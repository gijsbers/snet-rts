#ifndef _XCHANNEL_H
#define _XCHANNEL_H

/* Need intptr_t. */
#ifndef _STDINT_H
#include <stdint.h>
#endif

/* Number of bytes in a single channel node. */
#define CHAN_NODE_BYTES  128

/* Default number of data elements in a channel node. */
#define CHAN_DATA_SIZE  (CHAN_NODE_BYTES / sizeof(void*) - 2)

typedef struct chan_node        chan_node_t;
typedef struct chan_read        chan_read_t;
typedef struct chan_write       chan_write_t;
typedef struct channel          channel_t;

/* A data node for a channel with an unspecified number of elements */
struct chan_node {
  uintptr_t      first;
  void          *data[CHAN_DATA_SIZE];
  chan_node_t   *next;
};

/* The reader side of a channel */
struct chan_read {
  chan_node_t   *head;
  void         **get;
};

/* The writer side of a channel */
struct chan_write {
  chan_node_t   *tail;
  void         **put;
};

/* A channel (size equals 64 + CHAN_NODE_BYTES bytes.) */
struct channel {
  chan_read_t    reader;
  void          *padr[2];
  chan_node_t    node;
  void          *padw[2];
  chan_write_t   writer;
};


#endif
