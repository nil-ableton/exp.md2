#ifndef XXXX_QUEUE
#define XXXX_QUEUE

#include <stdbool.h>
#include <stdint.h>

/* single consumer single producer queue for cross thread work synchronization */

enum
{
  QUEUE_CAPACITY_POT = 256,
};

typedef struct Queue
{
  void* data[QUEUE_CAPACITY_POT];
  uint32_t writer_index;
  uint32_t reader_index;
} Queue;

typedef struct QueueWriter
{
  Queue* queue;
  void** writer_buffer;
} QueueWriter;

typedef struct QueueReader
{
  Queue* queue;
} QueueReader;

void queue_free(Queue* queue);
void queue_reader_free(QueueReader* queue);
void queue_writer_free(QueueWriter* queue);

void queue_push(QueueWriter* queue, void* data);
bool queue_flush(QueueWriter* queue);
void* queue_pull_next(QueueReader* queue);

#endif
