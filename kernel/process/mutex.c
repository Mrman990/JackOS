
#include "mutex.h"
#include "process.h"

void mutex_init(mutex_t *mutex) {
    if (mutex == 0) return;

    mutex->locked = 0;
}

void mutex_lock(mutex_t *mutex) {
    if (mutex == 0) return;

    while (mutex->locked) {
        yield();
    }

    mutex->locked = 1;
}

void mutex_unlock(mutex_t *mutex) {
    if (mutex == 0) return;

    mutex->locked = 0;
}
