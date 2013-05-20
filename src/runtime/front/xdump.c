/*
 * This generates a graph of the static nodes in Dot file format
 * when the command line option "-G" is given to the executable.
 * The output is written to the file "snet_graph_out.dot", but this
 * can be changed by setting the environment variable SNETDUMPGRAPHFILE.
 *
 * To convert the output to something interesting issue a command like:
 * dot -Granksep=0.1 -Gmindist=0.1 -Gnodesep=0.2 -Grankdir=LR \
 *     -Nmargin=0.04 -Nheight=0.1 -Nwidth=0.1 -Nstyle=filled \
 *     -Tpng -O snet_graph_out.dot
 */

#include "node.h"
#include <stdlib.h>
#include <string.h>

#define FROM(x) SNetDumpNodeName(STREAM_FROM(x))
#define DEST(x) SNetDumpNodeName(STREAM_DEST(x))

/* Return a readable name for a node. */
static const char* SNetDumpNodeName(node_t* node)
{
#define NAME(x) #x
#define NODE(l,n) node->type == l ? n :
  return
  NODE(NODE_box, NODE_SPEC(node, box)->boxname)
  NODE(NODE_parallel, "|")
  NODE(NODE_star, "*")
  NODE(NODE_split, "!")
  NODE(NODE_feedback, "\\\\")
  NODE(NODE_sync, "S")
  NODE(NODE_filter, "F")
  NODE(NODE_nameshift, "^")
  NODE(NODE_collector, "C")
  NODE(NODE_input, "I")
  NODE(NODE_output, "O")
  NODE(NODE_zipper, "Z")
  NODE(NODE_identity, "=")
  NODE(NODE_garbage, "G")
  NODE(NODE_observer, "O")
  NODE(NODE_observer2, "2")
  NODE(NODE_dripback, "\\\\\\\\")
  NULL;
}

/* Output a stream as an edge to file. */
static void SNetDumpEdge(FILE* fp, snet_stream_t* stream)
{
    bool constraint = true;
    node_t* from = STREAM_FROM(stream);
    node_t* dest = STREAM_DEST(stream);
    snet_stream_t temp;

    if (from && dest && from->type != NODE_nameshift) {
      switch (dest->type) {
        case NODE_star:
          if (stream == NODE_SPEC(dest, star)->internal) {
            constraint = false;
          }
          break;
        case NODE_feedback:
          if (stream == NODE_SPEC(dest, feedback)->feedback) {
            constraint = false;
          }
          break;
        case NODE_dripback:
          if (stream == NODE_SPEC(dest, dripback)->dripback) {
            constraint = false;
          }
          break;
        case NODE_nameshift:
          while (dest->type == NODE_nameshift) {
            dest = STREAM_DEST(NODE_SPEC(dest, filter)->output);
          }
          STREAM_FROM(&temp) = from;
          STREAM_DEST(&temp) = dest;
          stream = &temp;
          break;
        default:
          break;
      }
      fprintf(fp, "%td [label=\"%s\"];\n", (ptrdiff_t) from, FROM(stream));
      fprintf(fp, "%td [label=\"%s\"];\n", (ptrdiff_t) dest, DEST(stream));
      fprintf(fp, "%td -> %td %s;\n",
              (ptrdiff_t) stream->from, (ptrdiff_t) stream->dest,
              (constraint == false) ? "[constraint=false]" : "");
    }
}

/* Mark a stream as visited in the hash table and it to the FIFO. */
static void SNetDumpAdd(hash_ptab_t* hash, fifo_t* fifo, snet_stream_t* stream)
{
  if (SNetHashPtrLookup(hash, stream) == NULL) {
    SNetHashPtrStore(hash, stream, stream->dest);
    SNetFifoPut(fifo, stream);
  }
}

/* Dump a set of streams. */
static void SNetDumpSubnet(
        FILE* fp,
        hash_ptab_t* hash,
        fifo_t* fifo)
{
  fifo_t                fifo2;
  snet_stream_t*        stream;
  snet_stream_t*        outg;
  node_t*               coll;
  node_t*               node;
  int                   i;

  while ((stream = SNetFifoGet(fifo)) != NULL) {
    SNetDumpEdge(fp, stream);

    node = stream->dest;
    switch (node->type) {
      case NODE_box:
        for (i = 0; i < NODE_SPEC(node, box)->concurrency; ++i) {
          SNetDumpAdd(hash, fifo, NODE_SPEC(node, box)->outputs[i]);
        }
        break;
      case NODE_parallel:
        coll = STREAM_DEST(NODE_SPEC(node, parallel)->collector);
        outg = NODE_SPEC(coll, collector)->output;
        SNetDumpAdd(hash, fifo, outg);
        SNetFifoInit(&fifo2);
        SNetFifoPut(&fifo2, NODE_SPEC(node, parallel)->collector);
        for (i = 0; i < NODE_SPEC(node, parallel)->num; ++i) {
          SNetFifoPut(&fifo2, NODE_SPEC(node, parallel)->outputs[i]);
        }
        fprintf(fp, "subgraph cluster%td {\n", (ptrdiff_t) stream);
        fprintf(fp, "color=blue;\n");
        SNetDumpSubnet(fp, hash, &fifo2);
        fprintf(fp, "}\n");
        SNetFifoDone(&fifo2);
        break;
      case NODE_star:
        if (stream == NODE_SPEC(node, star)->input) {
          coll = STREAM_DEST(NODE_SPEC(node, star)->collector);
          outg = NODE_SPEC(coll, collector)->output;
          SNetDumpAdd(hash, fifo, outg);
          SNetFifoInit(&fifo2);
          SNetFifoPut(&fifo2, NODE_SPEC(node, star)->instance);
          SNetFifoPut(&fifo2, NODE_SPEC(node, star)->collector);
          fprintf(fp, "subgraph cluster%td {\n", (ptrdiff_t) stream);
          fprintf(fp, "color=blue;\n");
          SNetDumpSubnet(fp, hash, &fifo2);
          fprintf(fp, "}\n");
          SNetFifoDone(&fifo2);
        }
        break;
      case NODE_split:
        coll = STREAM_DEST(NODE_SPEC(node, split)->collector);
        outg = NODE_SPEC(coll, collector)->output;
        SNetDumpAdd(hash, fifo, outg);
        SNetFifoInit(&fifo2);
        SNetFifoPut(&fifo2, NODE_SPEC(node, split)->instance);
        SNetFifoPut(&fifo2, NODE_SPEC(node, split)->collector);
        fprintf(fp, "subgraph cluster%td {\n", (ptrdiff_t) stream);
        fprintf(fp, "color=blue;\n");
        SNetDumpSubnet(fp, hash, &fifo2);
        fprintf(fp, "}\n");
        SNetFifoDone(&fifo2);
        break;
      case NODE_feedback:
        if (stream == NODE_SPEC(node, feedback)->input) {
          outg = NODE_SPEC(node, feedback)->output;
          SNetDumpAdd(hash, fifo, outg);
          SNetFifoInit(&fifo2);
          SNetFifoPut(&fifo2, NODE_SPEC(node, feedback)->instance);
          fprintf(fp, "subgraph cluster%td {\n", (ptrdiff_t) stream);
          fprintf(fp, "color=blue;\n");
          SNetDumpSubnet(fp, hash, &fifo2);
          fprintf(fp, "}\n");
          SNetFifoDone(&fifo2);
        }
        break;
      case NODE_dripback:
        if (stream == NODE_SPEC(node, dripback)->input) {
          outg = NODE_SPEC(node, dripback)->output;
          SNetDumpAdd(hash, fifo, outg);
          SNetFifoInit(&fifo2);
          SNetFifoPut(&fifo2, NODE_SPEC(node, dripback)->instance);
          fprintf(fp, "subgraph cluster%td {\n", (ptrdiff_t) stream);
          fprintf(fp, "color=blue;\n");
          SNetDumpSubnet(fp, hash, &fifo2);
          fprintf(fp, "}\n");
          SNetFifoDone(&fifo2);
        }
        break;
      case NODE_sync:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, sync)->output);
        break;
      case NODE_filter:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, filter)->output);
        break;
      case NODE_nameshift:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, filter)->output);
        break;
      case NODE_collector:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, collector)->output);
        break;
      case NODE_input:
        break;
      case NODE_output:
        break;
      case NODE_zipper:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, zipper)->output);
        break;
      case NODE_identity:
        break;
      case NODE_garbage:
        break;
      case NODE_observer:
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, observer)->output1);
        SNetDumpAdd(hash, fifo, NODE_SPEC(node, observer)->output2);
        break;
      case NODE_observer2:
        break;
    }
  }
}

/* Write the static entity graph to file. */
void SNetDumpNodeGraph(snet_stream_t* input, snet_stream_t* output)
{
  const char            def_name[] = "snet_graph_out.dot";
  const char*           env_name = getenv("SNETDUMPGRAPHFILE");
  const char*           name = env_name ? env_name : def_name;
  hash_ptab_t*          hash;
  FILE*                 fp;
  fifo_t                fifo;

  if ((fp = fopen(name, "w")) == NULL) {
    fprintf(stderr, "[%s]: Error opening file \"%s\": %s.\n",
              __func__, name, strerror(errno));
    return;
  }

  hash = SNetHashPtrTabCreate(10, true);
  SNetFifoInit(&fifo);
  SNetDumpAdd(hash, &fifo, input);

  fprintf(fp, "digraph snet_graph_dump {\n");
  SNetDumpSubnet(fp, hash, &fifo);
  fprintf(fp, "%td [shape=Mdiamond];\n", (ptrdiff_t) STREAM_FROM(input));
  fprintf(fp, "%td [shape=Msquare];\n", (ptrdiff_t) STREAM_DEST(output));
  fprintf(fp, "}\n");

  SNetFifoDone(&fifo);
  SNetHashPtrTabDestroy(hash);
  fclose(fp);
}

