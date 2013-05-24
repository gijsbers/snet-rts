#include "xchannel.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include "xchannel.c"

void *SNetMemAlloc(size_t s) { return malloc(s); }
void *SNetMemAlign(size_t s) { return malloc(s); }
void SNetMemFree(void *p) { return free(p); }

static int verb;

  typedef struct state {
    int get;
    int put;
    channel_t *chan;
    struct state *next, *prev;
  } state_t;

static void check(state_t *st)
{
  CHAN_CHECK(st->chan);
}

static state_t *new_state(void)
{
  state_t *st = SNetMemAlloc(sizeof(state_t));
  st->get = rand() & 4095;
  st->put = st->get;
  st->chan = SNetChannelCreate();
  st->next = NULL;
  st->prev = NULL;
  return st;
}

static void del_state(state_t *st)
{
  free(st);
}

static double prob(void)
{
  return (double) rand() / (double) RAND_MAX;
}

static void append(state_t *st)
{
  long *n;
  state_t *next = st;
  check(st);
  while (next->next) {
    next = next->next;
  }
  n = SNetNew(long);
  *n = next->put++;
  SNetChannelPut(st->chan, n);
}

static state_t *get_last(state_t *st)
{
  state_t *next = st;
  while (next->next) {
    next = next->next;
  }
  return next;
}

static bool reduce(state_t *st)
{
  check(st);
  while (st->get == st->put && st->next) {
    state_t *next = st->next;
    st->get = next->get;
    st->put = next->put;
    st->next = next->next;
    del_state(st);
  }
  if (st->get < st->put) {
    long *n = SNetChannelGet(st->chan);
    assert(*n == st->get);
    st->get++;
    free(n);
    return true;
  }
  return false;
}

static bool empty(state_t *st)
{
  return st->get == st->put && !st->next && !st->next;
}

static void merge(state_t *st, state_t *next)
{
  state_t *last;
  check(st);
  check(next);
  SNetChannelMerge(st->chan, next->chan);
  next->chan = NULL;
  last = get_last(st);
  last->next = next;
  next->prev = last;
}

static void test_wild(void)
{
  const int num = 128;
  state_t *arr[num];
  int i, j, k, nrec = 0, nst = 0, nmrg = 0;
  const int max_k = 10;

  for (i = 0; i < num; ++i) {
    arr[i] = NULL;
  }

  for (k = 0;; ++k) {
    if (k < max_k && prob() < 0.05) {
      for (i = 0; i < num; ++i) {
        if (arr[i] == NULL) {
          arr[i] = new_state();
          ++nst;
        }
      }
    }
    else if (k < max_k && prob() < 0.60) {
      for (i = rand() % num; i < num; ++i) {
        if (arr[i]) {
          append(arr[i]);
          ++nrec;
          break;
        }
      }
    }
    else if (prob() < 0.03) {
      for (i = rand() % num; i < num; ++i) {
        if (arr[i]) {
          for (j = rand() % num; j < num; ++j) {
            if (j != i && arr[j]) {
              merge(arr[i], arr[j]);
              arr[j] = NULL;
              --nst;
              ++nmrg;
              break;
            }
          }
          break;
        }
      }
    }
    else {
      for (i = rand() % num; i < num; ++i) {
        if (arr[i]) {
          if (reduce(arr[i])) {
            --nrec;
          }
          else {
            assert(empty(arr[i]));
          }
          if (empty(arr[i])) {
            del_state(arr[i]);
            arr[i] = NULL;
            --nst;
          }
          break;
        }
      }
    }
    if (!nst && !nrec && k >= max_k) {
      printf("done at k = %d\n", k);
      break;
    }
    if (k % 100 == 0) {
      printf("st %d, rec %d, merg %d\n", nst, nrec, nmrg);
    }
  }
  for (i = 0; i < num; ++i) {
    assert(arr[i] == NULL);
  }
}

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
    int aptake = rand() % (appends + 1);
    int get, gets = inserts + appends - intake - aptake;
    long in1, in2, ap1, ap2;
    int count;

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

    count = SNetChannelMerge(first, second);
    if (count != appends - aptake) {
      printf("count %d != appends %d - aptake %d\n",
             count, appends, aptake);
    }

    out = (in1 < in2) ? in1 : ap1;
    for (get = 1; get <= gets; ++get) {
      if ((in1 <= out && out < in2) || (ap1 <= out && out < ap2));else {
        printf("in1 %ld, out %ld, in2 %ld, ap1 %ld, out %ld, ap2 %ld\n",
              in1, out, in2, ap1, out, ap2);
        exit(1);
      }
      long *i = SNetChannelGet(first);
      assert(i);
      ++out;
      if (*i != out) {
        printf("%d: *i %ld != in %ld, in %ld, ins %d, aps %d, in1 %ld, in2 %ld, ap1 %ld, ap2 %ld\n",
               __LINE__, *i, out, in, inserts, appends, in1, in2, ap1, ap2);
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
  int merg = 0, sing = 0, wild = 1;
  unsigned seed = 0;

  while ((c = getopt(argc, argv, "ims:vw")) != EOF) {
    switch (c) {
      case 'v': verb = 1; break;
      case 's': sscanf(optarg, "%u", &seed); break;
      case 'i': sing = 1; break;
      case 'm': merg = 1; break;
      case 'w': wild = 1; break;
      default: printf("%s: bad option -%c\n", *argv, c); exit(1);
    }
  }

  if (!seed) {
    seed = (unsigned) (getpid() + time(NULL));
  }
  srand(seed);

  if (wild) {
    test_wild();
  }
  if (merg) {
    test_merge();
  }
  if (sing) {
    test_single();
  }

  return 0;
}

