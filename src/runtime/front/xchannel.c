#include "xchannel.h"
#include "memfun.h"
#include <assert.h>
#include <string.h>

/* Create a subsequent data node for a channel. */
static chan_node_t *SNetChannelNodeCreate(void)
{
  chan_node_t *node = (chan_node_t *) SNetMemAlloc(CHAN_NODE_BYTES);
  node->first = 0;
  node->next = -1 + CHAN_DATA_SIZE;
  return node;
}

/* Initialize a new channel. */
static void SNetChannelInit(channel_t *chan)
{
  chan_node_t *node = (chan_node_t *) &chan->node[0];
  node->first = 0;
  node->next = -1 - CHAN_DATA_OFFSET + CHAN_INIT_SIZE;
  chan->reader.head = node;
  chan->reader.index = node->first;
  chan->reader.next = node->next;
  chan->writer.tail = node;
  chan->writer.index = node->first;
  chan->writer.next = node->next;
}

/* Create a new channel */
channel_t *SNetChannelCreate(void)
{
  channel_t *chan = SNetNew(channel_t);
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
static void SNetChannelDone(channel_t *chan)
{
  assert(chan->reader.head == chan->writer.tail);
  assert(chan->reader.index == chan->writer.index);
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
  if (w->index == w->next) {
    chan_node_t *p = SNetChannelNodeCreate();
    w->tail->data[w->next] = p;
    w->tail = p;
    w->index = p->first;
    w->next = p->next;
  }
  w->tail->data[w->index] = item;
  w->index++;
}

/* Retrieve an item from the head of a channel. */
void *SNetChannelGet(channel_t *chan)
{
  void         *item;
  chan_read_t  *r = &chan->reader;

  if (r->index == r->next) {
    chan_node_t *n = r->head;
    r->head = n->data[r->next];
    if (n != (chan_node_t *) &chan->node[0]) {
      SNetMemFree(n);
    }
    r->index = r->head->first;
    r->next = r->head->next;
  }
  item = r->head->data[r->index];
  r->index++;
  return item;
}

/* A fused send/recv if caller owns both sides. */
void *SNetChannelPutGet(channel_t *chan, void *item)
{
  if (chan->writer.tail == chan->reader.head &&
      chan->writer.index == chan->reader.index) {
    return item;
  } else {
    SNetChannelPut(chan, item);
    return SNetChannelGet(chan);
  }
}

