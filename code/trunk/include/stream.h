#ifndef STREAM_H
#define STREAM_H

typedef struct snet_stream_t snet_stream_t;

#include "info.h"
#include "stream.h"

/* Type for start-up functions generated by snet compiler: */
typedef snet_stream_t *(*snet_startup_fun_t)(snet_stream_t *, snet_info_t *, int);

#endif /* STREAM_H */
