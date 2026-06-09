#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "utils/raii.h"

#define IO_WORKER_THREAD_NAME "io-worker"
#define TIMER_WORKER_THREAD_NAME "timer-worker"

enum {
    IO_WORKER_THREAD_NAME_MUST_FIT = 1 / (sizeof(IO_WORKER_THREAD_NAME) <= 16),
    TIMER_WORKER_THREAD_NAME_MUST_FIT = 1 / (sizeof(TIMER_WORKER_THREAD_NAME) <= 16),
};

struct worker_context {
    const char *thread_name;
    atomic_t running;
    int exit_event_fd;
};

static int read_eventfd_counter(int event_fd)
{
    uint64_t counter = 0;

    for (;;) {
        ssize_t bytes_read = read(event_fd, &counter, sizeof(counter));
        if (bytes_read == sizeof(counter)) {
            return 0;
        }

        if (bytes_read == -1 && errno == EINTR) {
            continue;
        }

        if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        return -1;
    }
}

static int notify_eventfd(int event_fd)
{
    uint64_t counter = 1;

    for (;;) {
        ssize_t bytes_written = write(event_fd, &counter, sizeof(counter));
        if (bytes_written == sizeof(counter)) {
            return 0;
        }

        if (bytes_written == -1 && errno == EINTR) {
            continue;
        }

        if (bytes_written == -1 && errno == EAGAIN) {
            return 0;
        }

        return -1;
    }
}

static void request_worker_stop(struct worker_context *context)
{
    if (context == NULL) {
        LogError("request_worker_stop called with NULL context");
        return;
    }

    atomic_set_release(&context->running, 0);

    if (notify_eventfd(context->exit_event_fd) == -1) {
        int saved_errno = errno;
        LogError("eventfd notify failed: %s", strerror(saved_errno));
    }
}

static void handle_worker_business(const struct worker_context *context)
{
    LogInfo("%s: business tick", context->thread_name);
}

static void *worker_thread_main(void *argument)
{
    struct worker_context *context = argument;

    if (context == NULL) {
        LogError("worker thread received NULL context");
        return NULL;
    }

    int set_name_result = pthread_setname_np(pthread_self(), context->thread_name);
    if (set_name_result != 0) {
        LogError("pthread_setname_np failed: %s", strerror(set_name_result));
    }

    while (atomic_read_acquire(&context->running)) {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(context->exit_event_fd, &read_fds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready_count = select(context->exit_event_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            int saved_errno = errno;
            LogError("worker select failed: %s", strerror(saved_errno));
            break;
        }

        if (ready_count == 0) {
            handle_worker_business(context);
            continue;
        }

        if (FD_ISSET(context->exit_event_fd, &read_fds)) {
            if (read_eventfd_counter(context->exit_event_fd) == -1) {
                int saved_errno = errno;
                LogError("eventfd read failed: %s", strerror(saved_errno));
                break;
            }

            int running = atomic_read_acquire(&context->running);
            LogInfo("%s: exit event received, running=%d", context->thread_name, running);
        }
    }

    LogInfo("%s: exit", context->thread_name);
    return NULL;
}

static int create_signal_fd(sigset_t *signal_mask)
{
    if (signal_mask == NULL) {
        LogError("create_signal_fd called with NULL signal_mask");
        return -1;
    }

    sigemptyset(signal_mask);
    sigaddset(signal_mask, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, signal_mask, NULL) != 0) {
        int saved_errno = errno;
        LogError("pthread_sigmask failed: %s", strerror(saved_errno));
        return -1;
    }

    int signal_fd = signalfd(-1, signal_mask, SFD_CLOEXEC);
    if (signal_fd == -1) {
        int saved_errno = errno;

        if (pthread_sigmask(SIG_UNBLOCK, signal_mask, NULL) != 0) {
            LogError("pthread_sigmask restore failed: %s", strerror(errno));
        }
        LogError("signalfd failed: %s", strerror(saved_errno));
        return -1;
    }

    return signal_fd;
}

static int wait_for_sigint(int signal_fd)
{
    for (;;) {
        fd_set read_fds;

        FD_ZERO(&read_fds);
        FD_SET(signal_fd, &read_fds);

        int ready_count = select(signal_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            int saved_errno = errno;
            LogError("main select failed: %s", strerror(saved_errno));
            return -1;
        }

        if (FD_ISSET(signal_fd, &read_fds)) {
            struct signalfd_siginfo signal_info;
            ssize_t bytes_read = read(signal_fd, &signal_info, sizeof(signal_info));

            if (bytes_read == -1 && errno == EINTR) {
                continue;
            }

            if (bytes_read != sizeof(signal_info)) {
                int saved_errno = errno;
                LogError("signalfd read failed: %s", strerror(saved_errno));
                return -1;
            }

            if (signal_info.ssi_signo == SIGINT) {
                LogInfo("main: SIGINT received");
                return 0;
            }
        }
    }
}

static void stop_workers(struct worker_context *contexts, int count)
{
    for (int index = 0; index < count; ++index) {
        request_worker_stop(&contexts[index]);
    }
}

static int join_workers(pthread_t *threads, int count)
{
    int result = 0;

    for (int index = 0; index < count; ++index) {
        int join_result = pthread_join(threads[index], NULL);
        if (join_result != 0) {
            LogError("pthread_join failed: %s", strerror(join_result));
            result = -1;
        }
    }

    return result;
}

static void close_worker_fds(struct worker_context *contexts, int count)
{
    for (int index = 0; index < count; ++index) {
        if (contexts[index].exit_event_fd != -1) {
            close(contexts[index].exit_event_fd);
            contexts[index].exit_event_fd = -1;
        }
    }
}

int main(void)
{
    enum { WORKER_COUNT = 2 };

    int exit_code = EXIT_FAILURE;
    int wait_result = -1;
    sigset_t signal_mask;
    int signal_fd = -1;
    struct worker_context contexts[WORKER_COUNT] = {
        {
            .thread_name = IO_WORKER_THREAD_NAME,
            .running = ATOMIC_INIT(1),
            .exit_event_fd = -1,
        },
        {
            .thread_name = TIMER_WORKER_THREAD_NAME,
            .running = ATOMIC_INIT(1),
            .exit_event_fd = -1,
        },
    };
    pthread_t worker_threads[WORKER_COUNT];
    int worker_count_created = 0;

    signal_fd = create_signal_fd(&signal_mask);
    if (signal_fd == -1) {
        goto out;
    }

    for (int index = 0; index < WORKER_COUNT; ++index) {
        contexts[index].exit_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (contexts[index].exit_event_fd == -1) {
            int saved_errno = errno;
            LogError("eventfd failed: %s", strerror(saved_errno));
            goto out_stop_workers;
        }

        int create_result = pthread_create(&worker_threads[index], NULL,
                                           worker_thread_main, &contexts[index]);
        if (create_result != 0) {
            LogError("pthread_create failed: %s", strerror(create_result));
            goto out_stop_workers;
        }

        ++worker_count_created;
    }

    wait_result = wait_for_sigint(signal_fd);

out_stop_workers:
    stop_workers(contexts, worker_count_created);

    if (join_workers(worker_threads, worker_count_created) != 0) {
        wait_result = -1;
    }

    close_worker_fds(contexts, WORKER_COUNT);
    if (signal_fd != -1) {
        close(signal_fd);
        signal_fd = -1;
    }

    exit_code = wait_result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
out:
    return exit_code;
}
