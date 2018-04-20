#include "xxxx_queue.h"

#include "xxxx_buf.h"

#include <string.h>

// Implementation notes:
// @url: https://fgiesen.wordpress.com/2010/12/14/ring-buffers-and-queues/
// @url: https://fgiesen.wordpress.com/2015/09/24/intervals-in-modular-arithmetic/

inline uint32_t modulo_pot_uint32(uint32_t x, uint32_t power_of_two)
{
  return x & (power_of_two - 1);
}

void queue_free(Queue* queue)
{
  assert(queue->debug_writer_n == 0);
  assert(queue->debug_reader_n == 0);
}

void queue_reader_free(QueueReader* queue)
{
  queue->queue->debug_reader_n--;
}

void queue_writer_free(QueueWriter* queue)
{
  buf_free(queue->writer_buffer);
  queue->queue->debug_writer_n--;
}

static bool queue__flush_up_to_n(QueueWriter* writer, uint32_t n)
{
  if (buf_len(writer->writer_buffer) == 0)
    return false;
  if (n == 0)
    return buf_len(writer->writer_buffer) > 0;

  Queue* queue = writer->queue;
  void** buffer = writer->writer_buffer;
  size_t index;
  uint32_t writer_index = queue->writer_index;
  for (index = 0; index < n && index < buf_len(buffer); index++)
  {
    queue->data[modulo_pot_uint32(writer_index + (uint32_t)index, QUEUE_CAPACITY_POT)] =
      buffer[index];
  }
  assert(index > 0);
  queue->writer_index += (uint32_t)(index - 0);

  size_t remaining = buf_len(buffer) - index;
  memcpy(&buffer[0], &buffer[index], (sizeof *buffer) * remaining);
  buf_truncate(buffer, remaining);
  writer->writer_buffer = buffer;
  return index > 0 || buf_len(writer->writer_buffer) > 0;
}

bool queue_flush(QueueWriter* writer)
{
  Queue* queue = writer->queue;

  // write region is in [writer_index,reader_index) modulo capacity
  // both indices can wrap-around due to unsigned int modulo
  uint32_t reader_index = queue->reader_index - 1;
  uint32_t writer_index = queue->writer_index;
  uint32_t queue_len = writer_index - reader_index;
  assert(queue_len <= QUEUE_CAPACITY_POT);

  uint32_t available_to_write = QUEUE_CAPACITY_POT - queue_len;
  return queue__flush_up_to_n(writer, available_to_write);
}

void queue_push(QueueWriter* writer, void* data)
{
  buf_push(writer->writer_buffer, data);
  queue_flush(writer);
}

void* queue_pull_next(QueueReader* reader)
{
  Queue* queue = reader->queue;
  // read region is in [reader_index,writer_index) modulo capacity
  // both indices can wrap-around due to unsigned int modulo
  uint32_t reader_index = queue->reader_index - 1;
  uint32_t writer_index = queue->writer_index;
  uint32_t queue_len = writer_index - reader_index;
  if (queue_len == 0)
    return NULL;
  assert(queue_len <= QUEUE_CAPACITY_POT);

  void* result = queue->data[modulo_pot_uint32(reader_index, QUEUE_CAPACITY_POT)];
  queue->reader_index++;
  return result;
}

int test_queue(int argc, char const** argv)
{
  (void)argc, (void)argv;

  Queue queue = {
    0,
  };
  QueueWriter writer = {.queue = &queue};
  QueueReader reader = {.queue = &queue};

  assert(queue_pull_next(&reader) == NULL);

  uintptr_t next_message_i = 1;
  while (buf_len(writer.writer_buffer) == 0)
  {
    queue_push(&writer, (void*)next_message_i++);
  }
  assert(buf_len(writer.writer_buffer) == 1);

  assert((uintptr_t)queue_pull_next(&reader) == 1);
  queue_push(&writer, (void*)next_message_i++);
  assert(buf_len(writer.writer_buffer) == 1);

  // read all
  void* read;
  uintptr_t last_read_message_i;
  do
  {
    read = queue_pull_next(&reader);
    if (read)
    {
      last_read_message_i = (uintptr_t)read;
    }
  } while (queue_flush(&writer) || read);
  assert(last_read_message_i == next_message_i - 1);

  queue_push(&writer, (void*)next_message_i++);
  assert(buf_len(writer.writer_buffer) == 0);
  return 0;
}
