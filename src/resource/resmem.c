#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "resdefs.h"

void *xmalloc(size_t siz)
{
  void *p = malloc(siz);
  if (!p) pexit("malloc");
  return p;
}

void *xcalloc(size_t n, size_t siz)
{
  void *p = calloc(n, siz);
  if (!p) pexit("calloc");
  return p;
}

void *xrealloc(void *p, size_t siz)
{
  p = realloc(p, siz);
  if (!p) pexit("realloc");
  return p;
}

char* xstrdup(const char* str)
{
  char* p = strdup(str);
  if (!p) pexit("strdup");
  return p;
}

void xfree(void *p)
{
  free(p);
}

void xdel(void *p)
{
  free(p);
}

char* xdup(const char *s)
{
  char *p = strdup(s);
  if (!p) pexit("strdup");
  return p;
}
