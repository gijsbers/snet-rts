#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include "resdefs.h"

typedef enum proc_state proc_state_t;
typedef struct proc proc_t;
typedef struct core core_t;
typedef struct cache cache_t;
typedef struct numa numa_t;
typedef struct host host_t;
typedef struct remt remt_t;

#include "resource.h"
#include "restopo.h"
#include "resparse.h"

static remt_t res_remt;
static host_t *res_local;

void res_topo_create(void)
{
  res_remt.nhosts = 0;
  res_remt.hosts = NULL;
  res_remt.ncores = 0;
  res_remt.nprocs = 0;
}

void res_topo_add_host(host_t *host)
{
  if (host->index > 0) {
    if (host->index >= res_remt.nhosts) {
      int i, newmax = host->index + 1;
      res_remt.hosts = xrealloc(res_remt.hosts, newmax * sizeof(host_t*));
      for (i = res_remt.nhosts; i < newmax; ++i) {
        res_remt.hosts[i] = NULL;
      }
      res_remt.nhosts = newmax;
    }
    res_remt.hosts[host->index] = host;
    res_remt.ncores += host->ncores;
    res_remt.nprocs += host->nprocs;
  }
  else {
    assert(host->index == 0);
    assert(res_local == NULL);
    res_local = host;
  }
}

host_t* res_topo_get_host(int sysid)
{
  assert(sysid >= 0);

  if (sysid == LOCAL_HOST) {
    return res_local;
  }
  else if (sysid < res_remt.nhosts) {
    return res_remt.hosts[sysid];
  }
  return NULL;
}

const char* res_topo_get_hostname(int sysid)
{
  host_t* host = res_topo_get_host(sysid);
  return host ? host->hostname : NULL;
}

remt_t* res_topo_get_remt(void)
{
  return &res_remt;
}

host_t* res_local_host(void)
{
  return res_local;
}

resource_t* res_host_get_root(host_t* host)
{
  return host->root;
}

resource_t* res_local_root(void)
{
  host_t* host = res_local_host();
  return res_host_get_root(host);
}

int res_local_cores(void)
{
  host_t* host = res_local_host();
  int count = host->ncores;
  assert(count > 0);
  return count;
}

int res_local_procs(void)
{
  host_t* host = res_local_host();
  int count = host->nprocs;
  assert(count > 0);
  return count;
}

int res_remote_hosts(void)
{
  assert(res_remt.nhosts >= 0);
  return res_remt.nhosts;
}

int res_remote_cores(void)
{
  assert(res_remt.ncores >= 0);
  return res_remt.ncores;
}

int res_remote_procs(void)
{
  assert(res_remt.nprocs >= 0);
  return res_remt.nprocs;
}

int res_host_unassigned_cores(host_t* host)
{
  int n, count = 0;
  for (n = 0; n < host->ncores; ++n) {
    if (NOT(host->coreassign, n)) {
      ++count;
    }
  }
  return count;
}

int res_host_unassigned_procs(host_t* host)
{
  int n, count = 0;
  for (n = 0; n < host->nprocs; ++n) {
    if (NOT(host->procassign, n)) {
      ++count;
    }
  }
  return count;
}

resource_t* res_topo_get_root(int sysid)
{
  host_t* host = res_topo_get_host(sysid);
  if (host) {
    return res_host_get_root(host);
  } else {
    return NULL;
  }
}

void res_topo_get_host_list(intlist_t* list)
{
  int i;
  res_list_reset(list);
  if (res_local) {
    res_list_append(list, LOCAL_HOST);
  }
  for (i = 1; i < res_remt.nhosts; ++i) {
    if (res_remt.hosts[i]) {
      res_list_append(list, i);
    }
  }
}

static proc_t* res_proc_create(resource_t* res)
{
  proc_t* proc = xnew(proc_t);
  proc->logical = res->logical;
  proc->physical = res->physical;
  proc->state = ProcAvail;
  proc->clientbit = 0;
  assert(res->kind == Proc);
  return proc;
}

static void res_core_collect(core_t* core, resource_t* res)
{
  int   i;

  switch (res->kind) {

    case Proc:
      core->nprocs += 1;
      core->procs = xrealloc(core->procs, core->nprocs * sizeof(proc_t *));
      core->procs[core->nprocs - 1] = res_proc_create(res);
      core->procs[core->nprocs - 1]->core = core;
      break;

    default:
      for (i = 0; i < res->num_children; ++i) {
        res_core_collect(core, res->children[i]);
      }
  }
}

static core_t* res_core_create(resource_t* res)
{
  core_t* core = xnew(core_t);
  core->logical = res->logical;
  core->physical = res->physical;
  core->hyper = 0;
  core->nprocs = 0;
  core->procs = NULL;
  core->assigned = 0;
  assert(res->kind >= Core);
  res_core_collect(core, res);
  if (core->nprocs >= 2) {
    core->hyper = core->nprocs;
  }
  assert(core->nprocs >= 1);
  return core;
}

static void res_cache_collect(cache_t* cache, resource_t* res)
{
  int   i;

  switch (res->kind) {

    case Core:
    case Proc:
      cache->ncores += 1;
      cache->cores = xrealloc(cache->cores, cache->ncores * sizeof(core_t *));
      cache->cores[cache->ncores - 1] = res_core_create(res);
      cache->cores[cache->ncores - 1]->cache = cache;
      cache->nprocs += cache->cores[cache->ncores - 1]->nprocs;
      break;

    default:
      for (i = 0; i < res->num_children; ++i) {
        res_cache_collect(cache, res->children[i]);
      }
  }
}

static cache_t* res_cache_create(resource_t* res)
{
  cache_t* cache = xnew(cache_t);
  cache->logical = res->logical;
  cache->physical = res->physical;
  cache->ncores = 0;
  cache->cores = NULL;
  cache->nprocs = 0;
  cache->assigned = 0;
  res_cache_collect(cache, res);
  assert(cache->ncores >= 1);
  return cache;
}

static void res_numa_collect(numa_t* numa, resource_t* res)
{
  int   i;

  switch (res->kind) {

    case Cache:
    case Core:
      numa->ncaches += 1;
      numa->caches = xrealloc(numa->caches, numa->ncaches * sizeof(cache_t *));
      numa->caches[numa->ncaches - 1] = res_cache_create(res);
      numa->caches[numa->ncaches - 1]->numa = numa;
      numa->nprocs += numa->caches[numa->ncaches - 1]->nprocs;
      numa->ncores += numa->caches[numa->ncaches - 1]->ncores;
      break;

    default:
      for (i = 0; i < res->num_children; ++i) {
        res_numa_collect(numa, res->children[i]);
      }
  }
}

static numa_t* res_numa_create(resource_t* res)
{
  numa_t* numa = xnew(numa_t);
  numa->logical = res->logical;
  numa->physical = res->physical;
  numa->ncaches = 0;
  numa->caches = NULL;
  numa->ncores = 0;
  numa->nprocs = 0;
  numa->assigned = 0;
  res_numa_collect(numa, res);
  assert(numa->ncaches >= 1);
  return numa;
}

static void res_host_collect(host_t* host, resource_t* res)
{
  int   i;

  switch (res->kind) {

    case System:
    case Machine:
      for (i = 0; i < res->num_children; ++i) {
        res_host_collect(host, res->children[i]);
      }
      break;

    default:
      host->nnumas += 1;
      host->numa = xrealloc(host->numa, host->nnumas * sizeof(numa_t *));
      host->numa[host->nnumas - 1] = res_numa_create(res);
      host->numa[host->nnumas - 1]->host = host;
      host->ncores += host->numa[host->nnumas - 1]->ncores;
      host->nprocs += host->numa[host->nnumas - 1]->nprocs;
  }
}

host_t* res_host_create(char* hostname, int index, resource_t* root)
{
  int n, a, o, p, core_index, proc_index;

  host_t* host = xnew(host_t);
  memset(host, 0, sizeof(host_t));
  host->hostname = hostname;
  host->root = root;
  host->index = index;

  res_host_collect(host, root);

  /* Also collect all cores/procs of a host in a single array. */
  host->cores = xcalloc(host->ncores, sizeof(core_t *));
  host->procs = xcalloc(host->nprocs, sizeof(proc_t *));
  core_index = proc_index = 0;
  for (n = 0; n < host->nnumas; ++n) {
    numa_t* numa = host->numa[n];
    for (a = 0; a < numa->ncaches; ++a) {
      cache_t* cache = numa->caches[a];
      for (o = 0; o < cache->ncores; ++o) {
        core_t* core = cache->cores[o];
        assert(core_index == core->logical);
        host->cores[core_index++] = core;
        for (p = 0; p < core->nprocs; ++p) {
          proc_t* proc = core->procs[p];
          assert(proc_index == proc->logical);
          host->procs[proc_index++] = proc;
        }
      }
    }
  }

  if (res_get_debug()) {
    res_host_dump(host);
  }

  return host;
}

void res_host_destroy(host_t* host)
{
  int n, a, o, p;

  if (host->numa) {
    for (n = 0; n < host->nnumas; ++n) {
      numa_t* numa = host->numa[n];
      if (numa) {
        if (numa->caches) {
          for (a = 0; a < numa->ncaches; ++a) {
            cache_t* cache = numa->caches[a];
            if (cache) {
              if (cache->cores) {
                for (o = 0; o < cache->ncores; ++o) {
                  core_t* core = cache->cores[o];
                  if (core) {
                    if (core->procs) {
                      for (p = 0; p < core->nprocs; ++p) {
                        proc_t* proc = core->procs[p];
                        xfree(proc);
                      }
                      xfree(core->procs);
                    }
                    xfree(core);
                  }
                }
                xfree(cache->cores);
              }
              xfree(cache);
            }
          }
          xfree(numa->caches);
        }
        xfree(numa);
      }
    }
    xfree(host->numa);
  }
  xfree(host->hostname);
  xfree(host->procs);
  xfree(host->cores);
  if (host->root) {
    res_resource_free(host->root);
  }
  xfree(host);
}

void res_host_dump(host_t* host)
{
  int n, a, o, p;

  printf("host %d, name %s, %d numas, %d cores (%#lx), %d procs (%#lx)\n",
         host->index, host->hostname, host->nnumas,
         host->ncores, host->coreassign, host->nprocs, host->procassign);

  for (n = 0; n < host->nnumas; ++n) {
    numa_t* numa = host->numa[n];
    printf("    numa %d, %d caches, %d cores, %d procs, assigned %d\n",
           n, numa->ncaches, numa->ncores, numa->nprocs, numa->assigned);
    for (a = 0; a < numa->ncaches; ++a) {
      cache_t* cache = numa->caches[a];
      printf("        cache %d, %d cores, %d procs, assigned %d\n",
             a, cache->ncores, cache->nprocs, cache->assigned);
      for (o = 0; o < cache->ncores; ++o) {
        core_t* core = cache->cores[o];
        printf("            core %d, %d procs, hyper %d, assigned %d\n",
               o, core->nprocs, core->hyper, core->assigned);
        for (p = 0; p < core->nprocs; ++p) {
          proc_t* proc = core->procs[p];
          printf("                proc %d, id %d, state %d, bit %d\n",
                 p, proc->logical, proc->state, proc->clientbit);
        }
      }
    }
  }
  fflush(stdout);
}

bool res_topo_init(void)
{
  resource_t   *local_root = res_resource_init();
  host_t       *local_host;

  if (local_root) {
    local_host = res_host_create(res_hostname(), LOCAL_HOST, local_root);

    res_topo_create();
    res_topo_add_host(local_host);
  }

  return (local_root != NULL);
}

void res_topo_destroy(void)
{
  int i;
  if (res_local) {
    res_host_destroy(res_local);
    res_local = NULL;
  }
  for (i = 1; i < res_remt.nhosts; ++i) {
    if (res_remt.hosts[i]) {
      res_host_destroy(res_remt.hosts[i]);
      res_remt.hosts[i] = NULL;
    }
  }
  if (res_remt.hosts) {
    xfree(res_remt.hosts);
    memset(&res_remt, 0, sizeof(res_remt));
  }
}

char* res_system_resource_string(int id)
{
  host_t* host = res_topo_get_host(id);
  if (host) {
    resource_t* root = res_host_get_root(host);
    if (root) {
      int len, size = 10*1024;
      char *str = xmalloc(size);
      str[0] = '\0';
      snprintf(str, size, "{ hardware %d host %s children 1 \n", id, host->hostname);
      len = strlen(str);
      str = res_resource_object_string(root, str, len, &size);
      len += strlen(str + len);
      if (len + 10 > size) {
        size = len + 10;
        str = xrealloc(str, size);
      }
      snprintf(str + len, size - len, "} \n");
      return str;
    } else {
      return NULL;
    }
  } else {
    return NULL;
  }
}

char* res_system_host_string(int id)
{
  host_t* host = res_topo_get_host(id);
  if (host) {
    int n, a, o, p;
    int len, size = 10*1024;
    char *str = xmalloc(size);
    str[0] = '\0';

    assert(id == host->index);
    snprintf(str, size,
             "{ system %d hostname %s children %d \n",
             id, host->hostname, host->nnumas);
    len = strlen(str);

    for (n = 0; n < host->nnumas; ++n) {
      numa_t* numa = host->numa[n];
      if (len + 100 > size) {
        size = 3 * size / 2;
        str = xrealloc(str, size);
      }
      snprintf(str + len, size,
               "  { numa %d children %d \n",
               n, numa->ncaches);
      len += strlen(str + len);
      for (a = 0; a < numa->ncaches; ++a) {
        cache_t* cache = numa->caches[a];
        if (len + 100 > size) {
          size = 3 * size / 2;
          str = xrealloc(str, size);
        }
        snprintf(str + len, size,
                 "    { cache %d children %d \n",
                 a, cache->ncores);
        len += strlen(str + len);
        for (o = 0; o < cache->ncores; ++o) {
          core_t* core = cache->cores[o];
          if (len + 100 > size) {
            size = 3 * size / 2;
            str = xrealloc(str, size);
          }
          snprintf(str + len, size,
                   "      { core %d logical %d physical %d children %d \n",
                   o, core->logical, core->physical, core->nprocs);
          len += strlen(str + len);
          for (p = 0; p < core->nprocs; ++p) {
            proc_t* proc = core->procs[p];
            if (len + 100 > size) {
              size = 3 * size / 2;
              str = xrealloc(str, size);
            }
            snprintf(str + len, size,
                     "        { proc %d logical %d physical %d } \n",
                     p, proc->logical, proc->physical);
            len += strlen(str + len);
          }
          snprintf(str + len, size, "      } \n");
          len += strlen(str + len);
        }
        snprintf(str + len, size, "    } \n");
        len += strlen(str + len);
      }
      snprintf(str + len, size, "  } \n");
      len += strlen(str + len);
    }
    snprintf(str + len, size, "} \n");
    len += strlen(str + len);
    return str;
  } else {
    return NULL;
  }
}

void res_parse_system(stream_t* stream, host_t** host_ptr)
{
  char* hostname = NULL;
  int n, nnumas = 0, a, ncaches = 0, o, ncores = 0, p, nprocs = 0;
  int logical = 0, physical = 0, sysid = 0, maxcores = 0, maxprocs = 0;
  host_t* host;
  numa_t* numa;
  cache_t* cache;
  core_t* core;
  proc_t* proc;

  assert(host_ptr && !*host_ptr);

  res_parse_expect(stream, Number, &sysid);
  res_parse_expect(stream, Hostname, NULL);
  res_parse_expect(stream, String, &hostname);
  res_parse_expect(stream, Children, NULL);
  res_parse_expect(stream, Number, &nnumas);

  host = *host_ptr = xnew(host_t);
  host->hostname = hostname;
  host->root = NULL;
  host->index = sysid;
  host->nnumas = nnumas;
  host->numa = xcalloc(nnumas, sizeof(numa_t*));
  host->ncores = 0;
  host->nprocs = 0;
  host->cores = NULL;
  host->procs = NULL;

  for (n = 0; n < nnumas; ++n) {
    res_parse_expect(stream, Left, NULL);
    res_parse_expect(stream, Numa, NULL);
    res_parse_expect(stream, Number, NULL);
    res_parse_expect(stream, Children, NULL);
    res_parse_expect(stream, Number, &ncaches);

    numa = host->numa[n] = xnew(numa_t);
    numa->logical = 0;
    numa->physical = 0;
    numa->ncaches = ncaches;
    numa->caches = xcalloc(ncaches, sizeof(cache_t*));
    numa->ncores = 0;
    numa->nprocs = 0;
    numa->assigned = 0;
    numa->host = host;

    for (a = 0; a < ncaches; ++a) {
      res_parse_expect(stream, Left, NULL);
      res_parse_expect(stream, Cache, NULL);
      res_parse_expect(stream, Number, NULL);
      res_parse_expect(stream, Children, NULL);
      res_parse_expect(stream, Number, &ncores);

      cache = numa->caches[a] = xnew(cache_t);
      cache->logical = 0;
      cache->physical = 0;
      cache->ncores = ncores;
      cache->cores = xcalloc(ncores, sizeof(core_t*));
      cache->nprocs = 0;
      cache->assigned = 0;
      cache->numa = numa;

      for (o = 0; o < ncores; ++o) {
        res_parse_expect(stream, Left, NULL);
        res_parse_expect(stream, Core, NULL);
        res_parse_expect(stream, Number, NULL);
        res_parse_expect(stream, Logical, NULL);
        res_parse_expect(stream, Number, &logical);
        res_parse_expect(stream, Physical, NULL);
        res_parse_expect(stream, Number, &physical);
        res_parse_expect(stream, Children, NULL);
        res_parse_expect(stream, Number, &nprocs);

        core = cache->cores[o] = xnew(core_t);
        core->logical = logical;
        core->physical = physical;
        core->hyper = 0;
        core->nprocs = nprocs;
        core->procs = xcalloc(nprocs, sizeof(proc_t*));
        core->assigned = 0;
        core->cache = cache;

        for (p = 0; p < nprocs; ++p) {
          res_parse_expect(stream, Left, NULL);
          res_parse_expect(stream, Proc, NULL);
          res_parse_expect(stream, Number, NULL);
          res_parse_expect(stream, Logical, NULL);
          res_parse_expect(stream, Number, &logical);
          res_parse_expect(stream, Physical, NULL);
          res_parse_expect(stream, Number, &physical);

          proc = core->procs[p] = xnew(proc_t);
          proc->logical = logical;
          proc->physical = physical;
          proc->state = ProcAvail;
          proc->clientbit = 0;
          proc->core = core;

          if (host->nprocs + 1 > maxprocs) {
            maxprocs = MAX(4, 3 * maxprocs / 2);
            host->procs = xrealloc(host->procs, maxprocs * sizeof(proc_t*));
          }
          host->procs[host->nprocs + p] = proc;

          res_parse_expect(stream, Right, NULL);
        }

        cache->nprocs += nprocs;
        numa->nprocs += nprocs;
        host->nprocs += nprocs;

        if (host->ncores + 1 > maxcores) {
          maxcores = MAX(4, 3 * maxcores / 2);
          host->cores = xrealloc(host->cores, maxcores * sizeof(core_t*));
        }
        host->cores[host->ncores + o] = core;

        res_parse_expect(stream, Right, NULL);
      }

      numa->ncores += ncores;
      host->ncores += ncores;

      res_parse_expect(stream, Right, NULL);
    }
    res_parse_expect(stream, Right, NULL);
  }
}

bool res_parse_topology(int sysid, char* text)
{
  stream_t* stream = res_stream_from_string(text);
  host_t* host = NULL;

  if (setjmp(res_parse_exception_context)) {
    res_host_destroy(host);
    return false;
  } else {
    res_parse_expect(stream, Left, NULL);
    res_parse_expect(stream, System, NULL);
    res_parse_system(stream, &host);
    res_parse_expect(stream, Right, NULL);

    host->index = sysid;
    res_topo_add_host(host);
    return true;
  }
}

