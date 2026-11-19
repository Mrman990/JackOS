
#ifndef MUTEX_H
#define MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int locked;
} mutex_t;

typedef enum {
    MUTEX_EVENT_WAIT,
    MUTEX_EVENT_LOCK,
    MUTEX_EVENT_UNLOCK
} mutex_event_t;

typedef void (*mutex_logger_t)(mutex_event_t event);

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

void mutex_set_logger(mutex_logger_t logger);

#ifdef __cplusplus
}
#endif

#endif
