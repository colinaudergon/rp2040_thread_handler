#ifndef _THREAD_HANDLER_H_
#define _THREAD_HANDLER_H_

#include "pico/stdlib.h"
#include "pico/malloc.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pico/multicore.h"
#include "pico/mem_ops.h"

#include "logger.h"


enum task_priority {
    HIGH = 0,
    MEDIUM,
    LOW
};

typedef struct __thread_task_t {
    void (*func)(void *arg);
    void *arg;
    const char *task_name;
    enum task_priority priority;
    bool task_is_single_shot;
    int task_id;    
} thread_task_t;

typedef struct __thread_handler_t {
    thread_task_t *task_list;
    uint8_t task_count;
    bool task_running;
} thread_handler_t;

int thread_handler_start();
int thread_handler_stop();

thread_task_t* thread_handler_create_task(void (*func)(void *arg), void *arg, const char *task_name, enum task_priority priority, bool task_is_single_shot);

int thread_handler_add_task(thread_task_t *task);
int tread_handler_remove_task(thread_task_t *task);

#endif /*_THREAD_HANDLER_H_*/