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
  unsigned seed = 0;

  while ((c = getopt(argc, argv, "vs:")) != EOF) {
    switch (c) {
      case 'v': verb = 1; break;
      case 's': sscanf(optarg, "%u", &seed); break;
      default: printf("%s: bad option -%c\n", *argv, c); exit(1);
    }
  }

  if (!seed) {
    seed = (unsigned) (getpid() + time(NULL));
  }
  srand(seed);

  test_single();

  return 0;
}

