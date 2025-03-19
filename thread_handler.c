#include "thread_handler.h"

#define THREAD_HANDLER_MAX_TASKS_BASE 10

typedef enum
{
    ADD_TASK = 0,
    REMOVE_TASK,
    TASK_DONE,
    START_THREAD,
    STOP_THREAD
} thread_handler_cmd_t;

static int thread_handler_internal_init(thread_handler_t *thread_handler);
static int thread_handler_destroy(thread_handler_t *thread_handler);
static int thread_handler_add_task_internal(thread_task_t *task, thread_handler_t *thread_handler);
static int thread_handler_remove_task_internal(thread_task_t *task, thread_handler_t *thread_handler);

static void thread_handler_core_1()
{
    static thread_handler_t _thread_handler;
    thread_handler_internal_init(&_thread_handler);
    while (_thread_handler.task_running)
    {
        int32_t command;
        if (multicore_fifo_rvalid())
        {
            // Process the command   
            command = multicore_fifo_pop_blocking();
            if (command == ADD_TASK)
            {
                thread_task_t *task = (thread_task_t *)multicore_fifo_pop_blocking();
                thread_handler_add_task_internal(task, &_thread_handler);
            }
            else if (command == REMOVE_TASK)
            {
                thread_task_t *task = (thread_task_t *)multicore_fifo_pop_blocking();
                thread_handler_remove_task_internal(task, &_thread_handler);
            }

            if (command == STOP_THREAD)
            {
                thread_handler_destroy(&_thread_handler);
                break;
            }
        }

        for (int i = 0; i < _thread_handler.task_count; i++)
        {
            if (_thread_handler.task_list[i].priority == HIGH)
            {
                _thread_handler.task_list[i].func(_thread_handler.task_list[i].arg);
            }

            if (_thread_handler.task_list[i].task_is_single_shot)
            {
                thread_handler_remove_task_internal(&_thread_handler.task_list[i], &_thread_handler);
            }
        }

        for (int i = 0; i < _thread_handler.task_count; i++)
        {
            if (_thread_handler.task_list[i].priority == MEDIUM)
            {
                _thread_handler.task_list[i].func(_thread_handler.task_list[i].arg);
            }
            if (_thread_handler.task_list[i].task_is_single_shot)
            {
                thread_handler_remove_task_internal(&_thread_handler.task_list[i], &_thread_handler);
            }
        }

        for (int i = 0; i < _thread_handler.task_count; i++)
        {
            if (_thread_handler.task_list[i].priority == LOW)
            {
                _thread_handler.task_list[i].func(_thread_handler.task_list[i].arg);
            }
            if (_thread_handler.task_list[i].task_is_single_shot)
            {
                thread_handler_remove_task_internal(&_thread_handler.task_list[i], &_thread_handler);
            }
        }
    }
}

static int thread_handler_internal_init(thread_handler_t *thread_handler)
{
    thread_handler->task_list = (thread_task_t *)malloc(sizeof(thread_task_t) * THREAD_HANDLER_MAX_TASKS_BASE);
    if (!thread_handler->task_list)
    {
        LOG_ERROR("Failed to allocate memory for task list\n");
        return -ENOMEM;
    }
    thread_handler->task_running = true;
    return 0;
}

static int thread_handler_destroy(thread_handler_t *thread_handler)
{
    free(thread_handler->task_list); // Free task list
    thread_handler->task_running = false;
    return 0;
}

int thread_handler_start()
{
    multicore_launch_core1(thread_handler_core_1);
    return 0;
}

int thread_handler_stop()
{
    // Put a flag to stop the thread
    multicore_fifo_push_blocking(STOP_THREAD);
    return 0;
}

int thread_handler_add_task(thread_task_t *task)
{
    if (task == NULL)
    {
        LOG_ERROR("task is NULL\n");
        return -EINVAL;
    }

    multicore_fifo_push_blocking(ADD_TASK);
    multicore_fifo_push_blocking((uintptr_t)task);
}

int tread_handler_remove_task(thread_task_t *task)
{
    if (task == NULL)
    {
        LOG_ERROR("task is NULL\n");
        return -EINVAL;
    }

    multicore_fifo_push_blocking(REMOVE_TASK);
    multicore_fifo_push_blocking((uintptr_t)task);
    return 0;
}

static int thread_handler_add_task_internal(thread_task_t *task, thread_handler_t *thread_handler)
{
    if (task == NULL || thread_handler == NULL)
    {
        return -EINVAL;
    }

    if (thread_handler->task_count >= THREAD_HANDLER_MAX_TASKS_BASE)
    {
        // Resize task list if needed
        int new_size = THREAD_HANDLER_MAX_TASKS_BASE * 2; // Double capacity
        thread_task_t *new_list = realloc(thread_handler->task_list, new_size * sizeof(thread_task_t));
        if (!new_list)
        {
            LOG_ERROR("Failed to allocate memory for new task list\n");
            return -ENOMEM;
        }
        thread_handler->task_list = new_list;

        // Initialize the newly allocated memory to avoid uninitialized values
        memset(&thread_handler->task_list[thread_handler->task_count], 0, (new_size - thread_handler->task_count) * sizeof(thread_task_t));
    }

    thread_handler->task_list[thread_handler->task_count++] = *task;
    return 0;
}

static int thread_handler_remove_task_internal(thread_task_t *task, thread_handler_t *thread_handler)
{
    for (int i = 0; i < thread_handler->task_count; i++)
    {
        if (thread_handler->task_list[i].task_id == task->task_id)
        { // Compare by task_id
            for (int j = i; j < thread_handler->task_count - 1; j++)
            {
                thread_handler->task_list[j] = thread_handler->task_list[j + 1];
            }
            thread_handler->task_count--;
            return 0;
        }
    }
    return -EEXIST; // Task not found
}

thread_task_t *thread_handler_create_task(void (*func)(void *arg), void *arg, const char *task_name, enum task_priority priority, bool task_is_single_shot)
{
    if (func == NULL || task_name == NULL)
    {
        return NULL;
    }

    thread_task_t *task = (thread_task_t *)malloc(sizeof(thread_task_t));
    if (task == NULL)
    {
        LOG_ERROR("failed to allocate memory for task\n");
        return NULL;
    }
    task->func = func;
    task->arg = arg;
    task->task_name = task_name;
    task->priority = priority;
    task->task_is_single_shot = task_is_single_shot;
    task->task_id = rand();
    return task;
}
