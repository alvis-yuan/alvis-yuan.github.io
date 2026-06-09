#include "fsm.h"

#include <assert.h>
#include <stdio.h>

#ifndef FSM_LOG_ERROR
#define FSM_LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef FSM_DEBUG
#define FSM_DEBUG DEBUG
#endif

#if FSM_DEBUG
#ifndef FSM_LOG_DEBUG
#define FSM_LOG_DEBUG(...) fprintf(stderr, __VA_ARGS__)
#endif
#endif

static const char *fsm_get_state_name(const fsm_t *fsm, unsigned int state)
{
    if (fsm == NULL || fsm->state_names == NULL || state >= fsm->state_count) {
        return "";
    }

    return fsm->state_names[state];
}

static const char *fsm_get_event_name(const fsm_t *fsm, unsigned int event)
{
    if (fsm == NULL || fsm->event_names == NULL || event >= fsm->event_count) {
        return "";
    }

    return fsm->event_names[event];
}

fsm_action_t fsm_get_action_for_event(const fsm_t *fsm, unsigned int event)
{
    if (fsm == NULL || fsm->states == NULL ||
        fsm->current_state >= fsm->state_count ||
        event >= fsm->event_count) {
        FSM_LOG_ERROR("%s:%d invalid state machine or event\n", __func__, __LINE__);
        return NULL;
    }

    return fsm->states[fsm->current_state].events[event].action;
}

unsigned int fsm_get_next_state_for_event(const fsm_t *fsm, unsigned int event)
{
    if (fsm == NULL || fsm->states == NULL ||
        fsm->current_state >= fsm->state_count ||
        event >= fsm->event_count) {
        FSM_LOG_ERROR("%s:%d invalid state machine or event\n", __func__, __LINE__);
        return 0;
    }

    return fsm->states[fsm->current_state].events[event].next_state;
}

size_t fsm_get_buffer_size(void)
{
    return sizeof(fsm_t);
}

fsm_t *fsm_create(unsigned int state_count,
                  unsigned int initial_state,
                  unsigned int event_count,
                  const fsm_state_t *states,
                  void *data,
                  const char *name,
                  const char **state_names,
                  const char **event_names,
                  uint8_t *fsm_buffer)
{
    fsm_t *fsm = (fsm_t *)fsm_buffer;

    if (fsm == NULL || states == NULL ||
        state_count == 0 || event_count == 0 ||
        initial_state >= state_count) {
        FSM_LOG_ERROR("%s:%d invalid create arguments\n", __func__, __LINE__);
        return NULL;
    }

    fsm->states = states;
    fsm->state_count = state_count;
    fsm->event_count = event_count;
    fsm->current_state = initial_state;
    fsm->current_event = event_count;
    fsm->data = data;
    fsm->name = name;
    fsm->state_names = state_names;
    fsm->event_names = event_names;

    return fsm;
}

void fsm_delete(fsm_t *fsm)
{
    (void)fsm;
}

void fsm_handle_event(fsm_t *fsm, unsigned int event)
{
    unsigned int tmp_event = event;

    if (fsm == NULL || fsm->states == NULL ||
        fsm->current_state >= fsm->state_count ||
        tmp_event >= fsm->event_count) {
        FSM_LOG_ERROR("%s:%d '%s': invalid state machine(%p) or event(%u:%s)\n",
                      __func__, __LINE__, (fsm != NULL && fsm->name != NULL ? fsm->name : ""),
                      (void *)fsm, tmp_event, fsm_get_event_name(fsm, tmp_event));
        return;
    }

    while (tmp_event < fsm->event_count) {
        fsm_action_t action = fsm_get_action_for_event(fsm, tmp_event);
        unsigned int orig_event = tmp_event;
#if FSM_DEBUG
        unsigned int orig_state = fsm->current_state;
#endif

        assert(fsm->states);

        if (action == NULL) {
            FSM_LOG_ERROR("%s:%d '%s'(%p): no handler for event(%u:%s) in state(%u:%s)\n",
                          __func__, __LINE__, (fsm->name != NULL ? fsm->name : ""),
                          (void *)fsm, tmp_event, fsm_get_event_name(fsm, tmp_event),
                          fsm->current_state, fsm_get_state_name(fsm, fsm->current_state));
            tmp_event = fsm->event_count;
            continue;
        }

        /* 动作函数可以通过 next_event 追加触发一次状态机事件。 */
        fsm->current_event = tmp_event;
        action(fsm, &tmp_event);
        fsm->current_state = fsm_get_next_state_for_event(fsm, orig_event);

#if FSM_DEBUG
        FSM_LOG_DEBUG("%s:%d '%s'(%p): event(%u:%s)->(%u:%s), state(%u:%s)->(%u:%s)\n",
                      __func__, __LINE__, (fsm->name != NULL ? fsm->name : ""), (void *)fsm,
                      orig_event, fsm_get_event_name(fsm, orig_event),
                      tmp_event, fsm_get_event_name(fsm, tmp_event),
                      orig_state, fsm_get_state_name(fsm, orig_state),
                      fsm->current_state, fsm_get_state_name(fsm, fsm->current_state));
#endif

        assert(fsm->current_state < fsm->state_count);
    }

    fsm->current_event = fsm->event_count;
}
