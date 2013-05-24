#include "xchannel.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include "xchannel.c"

void *SNetMemAlloc(size_t s) { return malloc(s); }
void *SNetMemAlign(size_t s) { return malloc(s); }
void SNetMemFree(void *p) { return free(p); }

static int verb;

static void test_merge(void)
{
  int mg, merges = 100000;
  long in = 0, out = 0;

  for (mg = 1; mg <= merges; ++mg) {
    channel_t *first = SNetChannelCreate();
    channel_t *second = SNetChannelCreate();
    int ins, inserts = rand() & 255;
    int intake = rand() % (inserts + 1);
    int aps, appends = rand() & 255;
    int get, gets = inserts + appends;
    int aptake = rand() % (appends + 1);
    long in1, in2, ap1, ap2;

    in1 = in;
    for (ins = 1; ins <= inserts; ++ins) {
      long *i = malloc(sizeof(long));
      *i = ++in;
      SNetChannelPut(first, i);
    }
    in2 = in;

    for (get = 1; get <= intake; ++get) {
      long *i = SNetChannelGet(first);
      assert(i);
      if (*i != ++in1) {
        printf("*i %ld != in1 %ld, in %ld, ins %d, aps %d, in1 %ld, in2 %ld\n",
               *i, in1, in, inserts, appends, in1, in2);
        exit(1);
      }
      free(i);
    }

    ap1 = in;
    for (aps = 1; aps <= appends; ++aps) {
      long *i = malloc(sizeof(long));
      *i = ++in;
      SNetChannelPut(second, i);
    }
    ap2 = in;

    for (get = 1; get <= aptake; ++get) {
      long *i = SNetChannelGet(second);
      assert(i);
      if (*i != ++ap1) {
        printf("*i %ld != ap1 %ld, in %ld, ins %d, aps %d, in1 %ld, in2 %ld, ap1 %ld, ap2 %ld\n",
               *i, ap1, in, inserts, appends, in1, in2, ap1, ap2);
        exit(1);
      }
      free(i);
    }

    SNetChannelMerge(first, second);

    out = in1;
    for (get = 1; get <= gets; ++get) {
      long *i = SNetChannelGet(first);
      assert(i);
      ++out;
      if (*i != out) {
        printf("*i %ld != in %ld, in %ld, ins %d, aps %d, in1 %ld, in2 %ld, ap1 %ld, ap2 %ld\n",
               *i, out, in, inserts, appends, in1, in2, ap1, ap2);
        exit(1);
      }
      free(i);
      if (out == in2) {
        out = ap1;
      }
    }
    SNetChannelDestroy(first);
    SNetChannelDestroy(second);
  }
  printf("%ld puts and gets: OK\n", in);
}

static void test_single(void)
{
  int ch, channels = 1000;
  long out = 0;
  long in = 0;

  for (ch = 1; ch <= channels; ++ch) {
    channel_t *chan = SNetChannelCreate();
    int written = 0, reads = 0;
    int repeats = 1000;
    int rep;
    int get, gets;
    for (rep = 1; rep <= repeats; ++rep) {
      int put, puts = rand() & 63;
      for (put = 1; put <= puts; ++put) {
        long *l = malloc(sizeof(long));
        *l = ++out;
        SNetChannelPut(chan, l);
        ++written;
      }
      gets = (rep < repeats) ? (rand() & 63) : (written - reads);
      if (gets > written - reads) {
        gets = written - reads;
      }
      for (get = 1; get <= gets; ++get) {
        long *l = SNetChannelGet(chan);
        assert(l);
        ++in;
        if (*l != in) {
          printf("*l %ld != in %ld, wrn %d, rds %d\n", *l, in, written, reads);
          exit(1);
        }
        free(l);
        ++reads;
      }
    }
    SNetChannelDestroy(chan);
    if (verb) printf("Channel %4d done\n", ch);
  }
  printf("%ld puts and gets: OK\n", in);
}

int main(int argc, char **argv)
{
  int c;
  int merg = 1, sing = 1;
  unsigned seed = 0;

  while ((c = getopt(argc, argv, "vms:")) != EOF) {
    switch (c) {
      case 'v': verb = 1; break;
      case 's': sscanf(optarg, "%u", &seed); break;
      case 'm': merg = 1; sing = 0; break;
      default: printf("%s: bad option -%c\n", *argv, c); exit(1);
    }
  }

  if (!seed) {
    seed = (unsigned) (getpid() + time(NULL));
  }
  srand(seed);

  if (merg) {
    test_merge();
  }
  if (sing) {
    test_single();
  }

  return 0;
}

