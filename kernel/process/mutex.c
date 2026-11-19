
#include "mutex.h"
#include "process.h"

static mutex_logger_t mutexLogger = 0;

void mutex_set_logger(mutex_logger_t logger) {
    mutexLogger = logger;
}

void mutex_init(mutex_t *mutex) {
    if (mutex == 0) return;
    mutex->locked = 0;
}

void mutex_lock(mutex_t *mutex) {
    if (mutex == 0) return;

    int sudahMenunggu = 0;

    while (mutex->locked) {
        if (!sudahMenunggu) {
            if (mutexLogger)
                mutexLogger(MUTEX_EVENT_WAIT);
            sudahMenunggu = 1;
        }

        yield();
    }

    mutex->locked = 1;

    if (mutexLogger)
        mutexLogger(MUTEX_EVENT_LOCK);
}

void mutex_unlock(mutex_t *mutex) {
    if (mutex == 0) return;

    mutex->locked = 0;

    if (mutexLogger)
        mutexLogger(MUTEX_EVENT_UNLOCK);
}
