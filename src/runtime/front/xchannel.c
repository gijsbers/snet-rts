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

/* Append a new item at the tail of a channel. */
void SNetChannelPut(channel_t *chan, void *item)
{
  chan_write_t *w = &chan->writer;
  if (w->put == w->next) {
    chan_node_t *p = SNetChannelNodeCreate();
    *w->next = p;
    w->tail = p;
    w->put = &p->data[p->first];
    w->next = &p->data[p->next];
  }
  assert(item && w->put < w->next);
  *w->put++ = item;
}

/* Retrieve an item from the head of a channel. */
void *SNetChannelGet(channel_t *chan)
{
  void         *item;
  chan_read_t  *r = &chan->reader;

  if (r->get == r->next) {
    chan_node_t *p = *r->next;
    if (r->head != (chan_node_t *) &chan->node[0]) {
      SNetMemFree(r->head);
    }
    r->head = p;
    r->get = &p->data[p->first];
    r->next = &p->data[p->next];
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

