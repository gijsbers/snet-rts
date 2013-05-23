/*
 * A single-malloc cache-efficient channel for unbounded streams.
 *
 * The channel allows a single malloc to complete initialization and in
 * addition allow for the reading and writing of at least 10 records.
 * When more records are needed then each additional malloc can
 * satisfy another 14 records.
 * 
 * Because readers and writers may concurrently use the same channel
 * the channel strictly separates their respective data structures.
 * The reader doesn't modify cache lines which are needed by the
 * writer and the writer only modifies a pointer to store new data.
 */

#include "xchannel.h"
#include "memfun.h"
#include <assert.h>
#include <string.h>
#include <sys/param.h>

/* Create a subsequent data node for a channel. */
static chan_node_t *SNetChannelNodeCreate(void)
{
  chan_node_t *node = (chan_node_t *) SNetMemAlloc(CHAN_NODE_BYTES);
  node->first = 0;
  node->next = -1 + CHAN_DATA_SIZE;
  return node;
}

/* Initialize a channel. */
void SNetChannelInit(channel_t *chan)
{
  chan_node_t *node = (chan_node_t *) &chan->node[0];
  node->first = 0;
  node->next = -1 - CHAN_DATA_OFFSET + CHAN_INIT_SIZE;
  chan->reader.head = node;
  chan->reader.get = &node->data[node->first];
  chan->reader.next = &node->data[node->next];
  chan->writer.tail = node;
  chan->writer.put = &node->data[node->first];
  chan->writer.next = &node->data[node->next];
}

/* Create a new channel */
channel_t *SNetChannelCreate(void)
{
  channel_t *chan = SNetNewAlign(channel_t);
  /* This code only compiles if channel_t has the required size. */
  switch (1) {
    case sizeof(channel_t) == CHAN_NODE_BYTES:
      SNetChannelInit(chan);
      break;
    case 0:
      break;
  }
  return chan;
}

/* Cleanup a channel. */
void SNetChannelDone(channel_t *chan)
{
  assert(chan->reader.head == chan->writer.tail);
  assert(chan->reader.get == chan->writer.put);
  assert(chan->reader.next == chan->writer.next);
  if (chan->reader.head != (chan_node_t *) &chan->node[0]) {
    SNetDelete(chan->reader.head);
  }
}

/* Delete a channel. */
void SNetChannelDestroy(channel_t *chan)
{
  SNetChannelDone(chan);
  SNetDelete(chan);
}

/* Append a new data node onto a full channel. */
static void SNetChannelExpand(channel_t *chan)
{
  chan_node_t *p = SNetChannelNodeCreate();
  *chan->writer.next = p;
  chan->writer.tail = p;
  chan->writer.put = &p->data[p->first];
  chan->writer.next = &p->data[p->next];
}

/* Append a new item at the tail of a channel. */
void SNetChannelPut(channel_t *chan, void *item)
{
  chan_write_t *w = &chan->writer;
  if (w->put == w->next) {
    SNetChannelExpand(chan);
  }
  assert(item && w->put < w->next);
  *w->put++ = item;
}

/* Move a reader to the next data node. */
static void SNetChannelAdvance(channel_t *chan)
{
  chan_node_t *p = (chan_node_t *) *chan->reader.next;
  if (chan->reader.head != (chan_node_t *) &chan->node[0]) {
    SNetMemFree(chan->reader.head);
  }
  chan->reader.head = p;
  chan->reader.get = &p->data[p->first];
  chan->reader.next = &p->data[p->next];
}

/* Retrieve an item from the head of a channel. */
void *SNetChannelGet(channel_t *chan)
{
  void         *item;
  chan_read_t  *r = &chan->reader;

  if (r->get == r->next) {
    SNetChannelAdvance(chan);
  }
  item = *r->get++;
  assert(item && r->get <= r->next);
  return item;
}

/* A fused send/recv if caller owns both sides. */
void *SNetChannelPutGet(channel_t *chan, void *item)
{
  if (chan->writer.tail == chan->reader.head &&
      chan->writer.put == chan->reader.get) {
    return item;
  } else {
    SNetChannelPut(chan, item);
    return SNetChannelGet(chan);
  }
}

/* Count the number of records in the channel. */
static int SNetChannelCount(channel_t *chan)
{
  chan_node_t  *node;
  int           count = 0;

  if (chan->reader.head == chan->writer.tail) {
    count += chan->writer.put - chan->reader.get;
  }
  else {
    count += chan->reader.next - chan->reader.get;
    node = (chan_node_t *) *chan->reader.next;
    while (node != chan->writer.tail) {
      count += node->next - node->first;
      node = (chan_node_t *) node->data[node->next];
    }
    count += chan->writer.put - &node->data[node->first];
  }

  return count;
}

/* Append the contents of the second channel onto the first.
 * Return the number of migrated records. */
int SNetChannelMerge(channel_t *first, channel_t *second)
{
  const int     count = SNetChannelCount(second);

  if (count > 0) {
    int done = 0;
    if (second->reader.head == (chan_node_t *) &second->node[0]) {
      int todo = (second->reader.head == second->writer.tail) ?
                 (second->writer.put - second->reader.get) :
                 (second->reader.next - second->reader.get);
      int left = first->writer.next - first->writer.put;
      size_t copy_bytes = todo * sizeof(void *);
      if (todo > left) {
        first->writer.next = first->writer.put;
        first->writer.tail->next = first->writer.put
                                 - first->writer.tail->data;
        SNetChannelExpand(first);
        left = first->writer.next - first->writer.put;
      }
      memcpy(first->writer.put, second->reader.get, copy_bytes);
      first->writer.put += todo;
      second->reader.get += todo;
      done += todo;
    }
    if (done < count) {
      first->writer.next = first->writer.put;
      first->writer.tail->next = first->writer.put
                               - first->writer.tail->data;
      if (second->reader.get == second->reader.next) {
        SNetChannelAdvance(second);
      }
      *first->writer.next = second->reader.head;
      first->writer = second->writer;
      second->reader.head->first = second->reader.get
                                 - second->reader.head->data;
    }
    SNetChannelInit(second);
  }

  return count;
}








