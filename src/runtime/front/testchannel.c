#include "xchannel.h"
#include "xchannel.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

void *SNetMemAlign(size_t s) { return malloc(s); }
void SNetMemFree(void *p) { return free(p); }

int main(int argc, char **argv)
{
  int channels = 1000;
  long out = 0;
  long in = 0;

  srand((unsigned) getpid() + time(NULL));

  while (--channels >= 0) {
    channel_t *chan = SNetChannelCreate();
    int repeats = 1000;
    int n = 0;
    while (--repeats >= 0) {
      int r = rand() & 63;
      while (--r >= 0) {
        long *l = malloc(sizeof(long));
        *l = ++out;
        SNetChannelPut(chan, l);
        ++n;
      }
      r = (repeats) ? (rand() & 63) : n;
      while (n > 0 && --r >= 0) {
        long *l = SNetChannelGet(chan);
        assert(l);
        ++in;
        assert(*l == in);
        free(l);
        --n;
      }
    }
    SNetChannelDestroy(chan);
  }
  printf("%ld puts and gets: OK\n", in);

  return 0;
}

