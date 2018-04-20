#ifndef XXXX_TASKS
#define XXXX_TASKS

typedef struct TaskHandle
{
  int id; // null task has id = 0
} TaskHandle;

typedef void(TaskFn)(void* task_data);

void task_init();
void task_deinit();

/* Create a new task and returns its handle. A null handle (all bits to zero)
 * denotes an allocation error. */
TaskHandle task_create(TaskFn* task_function, void* task_data);

/* Mark that `dependency` depends on `task`. Upon completion of `task` all its
 * dependencies are started. */
void task_depends(TaskHandle task, TaskHandle dependency);

/* Schedule the task to run as soon as possible. */
void task_start(TaskHandle task);

/*
 * Always assign task workloads that, on average, justify the overhead of
 * scheduling a task, however minimal that overhead may be.
 * The other major benefit of working on bigger chunks of data is obviously
 * coherent memory access. The only benefit of finer-grained tasks is better
 * load balancing (and you don't need too much granularity for that) or when
 * finer grained tasks lead to finer grainer dependencies, letting work start
 * much sooner than otherwise. Even if your data has a natural granularity (a
 * million entities), you might not want to kick off a task for each. Batch them
 * up!
 *
 * This is the equivalent of parallel_foreach and similar things, but adapted
 * for the characteristics of your data.
 *
 * Don't try to make batching the job of the task system. The right strategy is
 * tied up with your data design.
 */

#endif
