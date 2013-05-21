#include "xchannel.h"
#include "memfun.h"
#include <assert.h>
#include <string.h>

/* Create a new channel */
static chan_node_t *SNetChannelNodeCreate(void)
{
  chan_node_t *node = SNetNewAlign(chan_node_t);
  memset(node, 0, sizeof(chan_node_t));
  return node;
}

/* Initialize a channel */
static void SNetChannelInit(channel_t *chan)
{
  chan_node_t *node = &chan->node;
  chan->reader.head = node;
  chan->reader.get = &node->data[0];
  chan->writer.put = &node->data[0];
  chan->writer.tail = node;
  memset(node, 0, sizeof(chan_node_t));
}

/* Create a new channel */
channel_t *SNetChannelCreate(void)
{
  channel_t *chan = SNetNewAlign(channel_t);
  SNetChannelInit(chan);
  return chan;
}

/* Cleanup a channel. */
static void SNetChannelDone(channel_t *chan)
{
  assert(chan->reader.head == chan->writer.tail);
  assert(chan->reader.get == chan->writer.put);
  if (chan->reader.head != &chan->node) {
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
  chan_node_t  *n = w->tail;
  assert((char *) w->put <= (char *) &(n->data[CHAN_DATA_SIZE]));
  if (w->put == &(n->data[CHAN_DATA_SIZE])) {
    assert(!n->next);
    n = w->tail = n->next = SNetChannelNodeCreate();
    w->put = &(n->data[0]);
  }
  *(w->put)++ = item;
}

/* Retrieve an item from the head of a channel, or NULL if empty. */
void *SNetChannelGet(channel_t *chan)
{
  void         *item;
  chan_read_t  *r = &chan->reader;
  chan_node_t  *n = r->head;

  assert((char *) r->get <= (char *) &(n->data[CHAN_DATA_SIZE]));
  if (r->get == &(n->data[CHAN_DATA_SIZE])) {
    assert(n->next);
    r->head = n->next;
    if (n != &chan->node) {
      SNetMemFree(n);
    }
    n = r->head;
    r->get = &(n->data[0]);
  }
  item = *(r->get)++;
  assert(item);
  return item;
}

/* A fused send/recv if caller owns both sides. */
void *SNetChannelPutGet(channel_t *chan, void *item)
{
  SNetChannelPut(chan, item);
  return SNetChannelGet(chan);
}

