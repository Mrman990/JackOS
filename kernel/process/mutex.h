
#ifndef MUTEX_H
#define MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int locked;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif
