#ifndef _XCHANNEL_H
#define _XCHANNEL_H

/* Need offsetof */
#include <stddef.h>

/* Number of bytes in a single channel node. */
#define CHAN_NODE_BYTES         128

/* Offset of 'data' element in chan_node. */
#define CHAN_DATA_OFFSET        (int) offsetof(chan_node_t, data)

/* Number of void pointers in a node. */
#define VOID_PTRS(x)            ((x) / sizeof(void*))

/* Default number of data elements in a channel node. */
#define CHAN_DATA_SIZE          VOID_PTRS(CHAN_NODE_BYTES - CHAN_DATA_OFFSET)

/* Initial number of data elements in the first channel node. */
#define CHAN_INIT_SIZE          VOID_PTRS(CHAN_NODE_BYTES \
                                          - sizeof(chan_read_t) \
                                          - sizeof(chan_write_t))

typedef struct chan_node        chan_node_t;
typedef struct chan_read        chan_read_t;
typedef struct chan_write       chan_write_t;
typedef struct channel          channel_t;

/* A data node for a channel containing pointers to data.
 * 'first' indexes data and is the first item to read.
 * 'next' indexes data to point to a subsequent channel node,
 * if not NULL.
 */
struct chan_node {
  int            first, next;
  void          *data[];
};

/* The reader side of a channel */
struct chan_read {
  chan_node_t   *head;
  void         **get;
  void         **next;
};

/* The writer side of a channel */
struct chan_write {
  chan_node_t   *tail;
  void         **put;
  void         **next;
};

/* A channel (size must equal CHAN_NODE_BYTES). */
struct channel {
  chan_read_t    reader;
  void          *node[CHAN_INIT_SIZE];
  chan_write_t   writer;
};


#endif
