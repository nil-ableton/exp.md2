#include "xxxx_tasks.h"

#include "xxxx_buf.h"

#include <windows.h>

#include <stdbool.h>

typedef struct Mutex
{
#if defined(_WIN32)
  CRITICAL_SECTION critical_section;
#endif
} Mutex;

void mtx_init(Mutex* mutex)
{
  InitializeCriticalSection(&mutex->critical_section);
}

void mtx_deinit(Mutex* mutex)
{
  DeleteCriticalSection(&mutex->critical_section);
}

void mtx_lock(Mutex* mutex)
{
  EnterCriticalSection(&mutex->critical_section);
}

void mtx_unlock(Mutex* mutex)
{
  LeaveCriticalSection(&mutex->critical_section);
}

typedef struct Task
{
  TaskHandle handle;
  TaskFn* task_function;
  void* task_data;
  TaskHandle* dependencies;
} Task;

static bool g_task_init;
static Mutex g_tasks_lock;
static Task* g_tasks = NULL;
static int g_worker_thread_mustrun = 0;
static HANDLE g_worker_thread;
static Mutex g_worker_thread_input_lock;
static TaskHandle* g_worker_thread_input = NULL;

enum
{
  TASK_INDEX_MASK = ((1 << 24) - 1),
  TASK_GENERATION_OFFSET = 24,
};

static Task* task__get(TaskHandle handle, Task* const tasks)
{
  uint32_t index = handle.id & TASK_INDEX_MASK;
  assert(index < buf_len(tasks));
  if (index >= buf_len(tasks))
    return NULL;

  Task* task = &tasks[index];
  assert(task->handle.id == handle.id);
  if (task->handle.id != handle.id)
    return NULL;

  return task;
}

static void task__run(Task task)
{
  task.task_function(task.task_data);
  for (TaskHandle* dep_f = &task.dependencies[0]; dep_f < buf_end(task.dependencies);
       dep_f++)
  {
    task_start(*dep_f);
  }
  buf_reset(task.dependencies);
}

static void task__free(Task* task)
{
  task->task_function = NULL;
}

static DWORD task__worker_thread_run(LPVOID lpParameter)
{
  (void)lpParameter;
  while (g_worker_thread_mustrun)
  {
    TaskHandle* pending = NULL;
    mtx_lock(&g_worker_thread_input_lock);
    for (TaskHandle* th_i = &g_worker_thread_input[0];
         th_i < buf_end(g_worker_thread_input); th_i++)
    {
      buf_push(pending, *th_i);
    }
    buf_reset(g_worker_thread_input);
    mtx_unlock(&g_worker_thread_input_lock);

    for (TaskHandle* th_i = &pending[0]; th_i < buf_end(pending); th_i++)
    {
      mtx_lock(&g_tasks_lock);
      Task task = *task__get(*th_i, g_tasks);
      mtx_unlock(&g_tasks_lock);

      task__run(task);

      mtx_lock(&g_tasks_lock);
      task__free(task__get(*th_i, g_tasks));
      mtx_unlock(&g_tasks_lock);
    }
    buf_free(pending), pending = NULL;
    Sleep(1);
  }

  return 0;
}

void task_init(void)
{
  assert(!g_task_init);
  mtx_init(&g_tasks_lock);
  g_task_init = true;

  mtx_init(&g_worker_thread_input_lock);
  g_worker_thread_mustrun = 1;
  g_worker_thread =
    CreateThread(NULL /* lpThreadAttributes: inherit */, 0 /* dwStackSize: default */,
                 task__worker_thread_run, NULL /* lpParameter */,
                 0 /* dwCreationFlags: thread runs immediately */, NULL /* lpThreadId */);
}

void task_deinit(void)
{
  assert(g_task_init);
  g_worker_thread_mustrun = 0;
  WaitForSingleObject(g_worker_thread, INFINITE);
  CloseHandle(g_worker_thread);
  mtx_deinit(&g_worker_thread_input_lock);
  mtx_deinit(&g_tasks_lock);
  buf_free(g_tasks);
  buf_free(g_worker_thread_input);
  g_task_init = false;
}

TaskHandle task_create(TaskFn* task_function, void* task_data)
{
  assert(g_task_init);
  mtx_lock(&g_tasks_lock);
  Task* free_task;
  for (free_task = &g_tasks[0]; free_task < buf_end(g_tasks) && free_task->task_function;
       free_task++)
    ;

  TaskHandle handle = {0};

  Task task = {
    .task_function = task_function,
    .task_data = task_data,
  };


  if (free_task == buf_end(g_tasks))
  {
    uint32_t index = (uint32_t)buf_len(g_tasks);
    index &= TASK_INDEX_MASK;
    assert((size_t)index == buf_len(g_tasks));
    handle.id = (0x01 << TASK_GENERATION_OFFSET) | index;
    task.handle = handle;
    buf_push(g_tasks, task);
  }
  else
  {
    uint32_t index = free_task->handle.id & TASK_INDEX_MASK;
    uint8_t generation = free_task->handle.id >> TASK_GENERATION_OFFSET;
    handle.id = (generation + 1) << TASK_GENERATION_OFFSET | index;
    task.handle = handle;
    *free_task = task;
  }

  // done_with_mutex:
  mtx_unlock(&g_tasks_lock);
  return handle;
}

/* Mark that `dependency` depends on `task`. Upon completion of `task` all its
 * dependencies are started. */
void task_depends(TaskHandle task, TaskHandle dependency)
{
  assert(task.id != dependency.id);

  mtx_lock(&g_tasks_lock);

  Task* parent = task__get(task, g_tasks);
  (void)task__get(dependency, g_tasks); // to check the handle

  buf_push(parent->dependencies, dependency);

  // done_with_mutex:
  mtx_unlock(&g_tasks_lock);
}

/* Schedule the task to run as soon as possible. */
void task_start(TaskHandle task_handle)
{
  {
    mtx_lock(&g_tasks_lock);
    (void)task__get(task_handle, g_tasks); // check task_handle
    mtx_unlock(&g_tasks_lock);
  }

  mtx_lock(&g_worker_thread_input_lock);
  buf_push(g_worker_thread_input, task_handle);
  mtx_unlock(&g_worker_thread_input_lock);
}

#include <stdio.h>

static void test_task_printf(void* data)
{
  char const* text = data;
  printf("%s", text);
}

static void test_task_end(void* data)
{
  int* i = data;
  (*i)--;
}

int test_task(int argc, char const** argv)
{
  (void)argc, (void)argv;
  task_init();
  int e1_n_storage = 1;
  int e2_n_storage = 1;
  TaskHandle a = task_create(test_task_printf, "a\n");
  TaskHandle b = task_create(test_task_printf, "b\n");
  TaskHandle c1 = task_create(test_task_printf, "c1\n");
  TaskHandle c2 = task_create(test_task_printf, "c2\n");

  int volatile* e1_n = &e1_n_storage;
  int volatile* e2_n = &e2_n_storage;

  TaskHandle e1 = task_create(test_task_end, (void*)e1_n);
  TaskHandle e2 = task_create(test_task_end, (void*)e2_n);

  task_depends(a, b);
  task_depends(b, c1);
  task_depends(b, c2);
  task_depends(c1, e1);
  task_depends(c2, e2);

  assert(*e1_n == 1 && *e2_n == 1);
  task_start(a);

  // join tasks
  while (*e1_n || *e2_n)
  {
    Sleep(1);
  }

  task_start(task_create(test_task_printf, "a\n"));

  task_deinit();

  return 0;
}
