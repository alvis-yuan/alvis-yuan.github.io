#ifndef PATTERN_FSM_H
#define PATTERN_FSM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct fsm;

/**
 * @brief 状态机事件动作函数。
 *
 * @param fsm 当前状态机实例。
 * @param next_event 输出下一次要处理的事件；写入 >= event_count 表示不继续触发事件。
 */
typedef void (*fsm_action_t)(struct fsm *fsm, unsigned int *next_event);

typedef struct fsm_event {
    fsm_action_t action;
    unsigned int next_state;
} fsm_event_t;

typedef struct fsm_state {
    const fsm_event_t *events;
} fsm_state_t;

typedef struct fsm {
    const fsm_state_t *states;
    unsigned int state_count;
    unsigned int event_count;
    unsigned int current_state;
    unsigned int current_event;
    void *data;
    const char *name;
    const char **state_names;
    const char **event_names;
} fsm_t;

fsm_action_t fsm_get_action_for_event(const fsm_t *fsm, unsigned int event);
unsigned int fsm_get_next_state_for_event(const fsm_t *fsm, unsigned int event);
size_t fsm_get_buffer_size(void);
fsm_t *fsm_create(unsigned int state_count,
                  unsigned int initial_state,
                  unsigned int event_count,
                  const fsm_state_t *states,
                  void *data,
                  const char *name,
                  const char **state_names,
                  const char **event_names,
                  uint8_t *fsm_buffer);
void fsm_delete(fsm_t *fsm);
void fsm_handle_event(fsm_t *fsm, unsigned int event);

#ifdef __cplusplus
}
#endif

#endif
