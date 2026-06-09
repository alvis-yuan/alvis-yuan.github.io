/**
 * @file fsm_demo.c
 * @brief 演示 fsm 的表驱动状态转换与 action 级联触发事件
 *
 * 编译运行：make fsm && out/bin/fsm
 */

#include "fsm.h"
#include "utils.h"

#include <stdint.h>
#include <stdlib.h>

enum fsm_demo_state {
    FSM_DEMO_LOCKED = 0,
    FSM_DEMO_UNLOCKED,
    FSM_DEMO_ALARM,
    FSM_DEMO_STATE_COUNT
};

enum fsm_demo_event {
    FSM_DEMO_AUTH = 0,
    FSM_DEMO_OPEN,
    FSM_DEMO_TIMEOUT,
    FSM_DEMO_FORCE_OPEN,
    FSM_DEMO_RESET,
    FSM_DEMO_EVENT_COUNT
};

typedef struct fsm_demo_ctx {
    unsigned int auth_count;
    unsigned int open_count;
    unsigned int denied_count;
    unsigned int alarm_count;
} fsm_demo_ctx_t;

typedef union fsm_demo_storage {
    uint8_t bytes[sizeof(fsm_t)];
    fsm_t   align;
} fsm_demo_storage_t;

static const char *g_fsm_demo_state_names[FSM_DEMO_STATE_COUNT] = {
    "LOCKED",
    "UNLOCKED",
    "ALARM",
};

static const char *g_fsm_demo_event_names[FSM_DEMO_EVENT_COUNT] = {
    "AUTH",
    "OPEN",
    "TIMEOUT",
    "FORCE_OPEN",
    "RESET",
};

static fsm_demo_ctx_t *fsm_demo_get_ctx(fsm_t *fsm)
{
    if (fsm == NULL || fsm->data == NULL) {
        LogError("fsm demo context is invalid");
        return NULL;
    }

    return (fsm_demo_ctx_t *)fsm->data;
}

static void fsm_demo_auth(fsm_t *fsm, unsigned int *next_event)
{
    fsm_demo_ctx_t *ctx = NULL;

    ctx = fsm_demo_get_ctx(fsm);
    if (ctx == NULL || next_event == NULL) {
        LogError("auth action argument is invalid");
        return;
    }

    ctx->auth_count++;
    LogInfo("auth accepted, queue OPEN event");
    *next_event = FSM_DEMO_OPEN;
}

static void fsm_demo_open(fsm_t *fsm, unsigned int *next_event)
{
    fsm_demo_ctx_t *ctx = NULL;

    ctx = fsm_demo_get_ctx(fsm);
    if (ctx == NULL || next_event == NULL) {
        LogError("open action argument is invalid");
        return;
    }

    ctx->open_count++;
    LogInfo("door opened, queue TIMEOUT event to lock it again");
    *next_event = FSM_DEMO_TIMEOUT;
}

static void fsm_demo_lock(fsm_t *fsm, unsigned int *next_event)
{
    (void)fsm;

    if (next_event == NULL) {
        LogError("lock action argument is invalid");
        return;
    }

    LogInfo("door locked");
    *next_event = FSM_DEMO_EVENT_COUNT;
}

static void fsm_demo_deny(fsm_t *fsm, unsigned int *next_event)
{
    fsm_demo_ctx_t *ctx = NULL;

    ctx = fsm_demo_get_ctx(fsm);
    if (ctx == NULL || next_event == NULL) {
        LogError("deny action argument is invalid");
        return;
    }

    ctx->denied_count++;
    LogInfo("event denied in current state");
    *next_event = FSM_DEMO_EVENT_COUNT;
}

static void fsm_demo_alarm(fsm_t *fsm, unsigned int *next_event)
{
    fsm_demo_ctx_t *ctx = NULL;

    ctx = fsm_demo_get_ctx(fsm);
    if (ctx == NULL || next_event == NULL) {
        LogError("alarm action argument is invalid");
        return;
    }

    ctx->alarm_count++;
    LogInfo("forced open detected, alarm raised");
    *next_event = FSM_DEMO_EVENT_COUNT;
}

static void fsm_demo_reset(fsm_t *fsm, unsigned int *next_event)
{
    (void)fsm;

    if (next_event == NULL) {
        LogError("reset action argument is invalid");
        return;
    }

    LogInfo("alarm reset");
    *next_event = FSM_DEMO_EVENT_COUNT;
}

static void fsm_demo_noop(fsm_t *fsm, unsigned int *next_event)
{
    (void)fsm;

    if (next_event == NULL) {
        LogError("noop action argument is invalid");
        return;
    }

    LogInfo("event ignored");
    *next_event = FSM_DEMO_EVENT_COUNT;
}

static const fsm_event_t g_fsm_demo_locked_events[FSM_DEMO_EVENT_COUNT] = {
    [FSM_DEMO_AUTH] = { fsm_demo_auth, FSM_DEMO_UNLOCKED },
    [FSM_DEMO_OPEN] = { fsm_demo_deny, FSM_DEMO_LOCKED },
    [FSM_DEMO_TIMEOUT] = { fsm_demo_noop, FSM_DEMO_LOCKED },
    [FSM_DEMO_FORCE_OPEN] = { fsm_demo_alarm, FSM_DEMO_ALARM },
    [FSM_DEMO_RESET] = { fsm_demo_noop, FSM_DEMO_LOCKED },
};

static const fsm_event_t g_fsm_demo_unlocked_events[FSM_DEMO_EVENT_COUNT] = {
    [FSM_DEMO_AUTH] = { fsm_demo_noop, FSM_DEMO_UNLOCKED },
    [FSM_DEMO_OPEN] = { fsm_demo_open, FSM_DEMO_UNLOCKED },
    [FSM_DEMO_TIMEOUT] = { fsm_demo_lock, FSM_DEMO_LOCKED },
    [FSM_DEMO_FORCE_OPEN] = { fsm_demo_alarm, FSM_DEMO_ALARM },
    [FSM_DEMO_RESET] = { fsm_demo_lock, FSM_DEMO_LOCKED },
};

static const fsm_event_t g_fsm_demo_alarm_events[FSM_DEMO_EVENT_COUNT] = {
    [FSM_DEMO_AUTH] = { fsm_demo_deny, FSM_DEMO_ALARM },
    [FSM_DEMO_OPEN] = { fsm_demo_deny, FSM_DEMO_ALARM },
    [FSM_DEMO_TIMEOUT] = { fsm_demo_noop, FSM_DEMO_ALARM },
    [FSM_DEMO_FORCE_OPEN] = { fsm_demo_noop, FSM_DEMO_ALARM },
    [FSM_DEMO_RESET] = { fsm_demo_reset, FSM_DEMO_LOCKED },
};

static const fsm_state_t g_fsm_demo_states[FSM_DEMO_STATE_COUNT] = {
    [FSM_DEMO_LOCKED] = { g_fsm_demo_locked_events },
    [FSM_DEMO_UNLOCKED] = { g_fsm_demo_unlocked_events },
    [FSM_DEMO_ALARM] = { g_fsm_demo_alarm_events },
};

static void fsm_demo_handle(fsm_t *fsm, enum fsm_demo_event event)
{
    if (fsm == NULL || event >= FSM_DEMO_EVENT_COUNT) {
        LogError("fsm demo handle argument is invalid");
        return;
    }

    LogInfo(">>> handle event %s while state is %s",
            g_fsm_demo_event_names[event],
            g_fsm_demo_state_names[fsm->current_state]);
    fsm_handle_event(fsm, (unsigned int)event);
    LogInfo("<<< current state is %s",
            g_fsm_demo_state_names[fsm->current_state]);
}

int main(void)
{
    fsm_demo_storage_t storage;
    fsm_demo_ctx_t     ctx = {0};
    fsm_t             *fsm = NULL;

    if (fsm_get_buffer_size() > sizeof(storage.bytes)) {
        LogError("fsm storage is too small");
        return EXIT_FAILURE;
    }

    fsm = fsm_create(FSM_DEMO_STATE_COUNT,
                     FSM_DEMO_LOCKED,
                     FSM_DEMO_EVENT_COUNT,
                     g_fsm_demo_states,
                     &ctx,
                     "door-fsm",
                     g_fsm_demo_state_names,
                     g_fsm_demo_event_names,
                     storage.bytes);
    if (fsm == NULL) {
        LogError("fsm_create failed");
        return EXIT_FAILURE;
    }

    LogInfo("fsm demo starts");

    fsm_demo_handle(fsm, FSM_DEMO_AUTH);
    fsm_demo_handle(fsm, FSM_DEMO_FORCE_OPEN);
    fsm_demo_handle(fsm, FSM_DEMO_OPEN);
    fsm_demo_handle(fsm, FSM_DEMO_RESET);

    LogInfo("summary: auth=%u open=%u denied=%u alarm=%u",
            ctx.auth_count, ctx.open_count, ctx.denied_count, ctx.alarm_count);

    fsm_delete(fsm);
    return EXIT_SUCCESS;
}
