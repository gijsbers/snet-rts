#include <assert.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/param.h>

#include "resdefs.h"

int res_client_compare_local_workload_desc(const void *p, const void *q)
{
  const client_t* P = (const client_t *) p;
  const client_t* Q = (const client_t *) q;
  int A = P->local_workload;
  int B = Q->local_workload;

  return (A == B) ? (Q->stream.fd - P->stream.fd) : (A - B);
}

/* Each task can be run on a dedicated core. */
void res_rebalance_cores(intmap_t* map, int ncores, int nprocs)
{
  int i = 0, k = -1, count = res_map_count(map);
  client_t *client, **all = xmalloc(count * sizeof(client_t *));

  res_map_iter_init(map, &k);
  while ((client = res_map_iter_next(map, &k)) != NULL) {
    assert(i < count);
    all[i++] = client;
  }
  qsort(all, count, sizeof(client_t *), res_client_compare_local_workload_desc);

  for (i = 0; i < count; ++i) {

  }

  xfree(all);
}

void res_rebalance_procs(intmap_t* map)
{
}

void res_rebalance_proportional(intmap_t* map)
{
}

void res_rebalance_minimal(intmap_t* map)
{
}

void res_rebalance(intmap_t* map)
{
  client_t* client;
  int count = 0;
  int load = 0;
  int nprocs = res_local_procs();
  int ncores = res_local_cores();
  intmap_iter_t iter;

  res_map_iter_init(map, &iter);
  while ((client = res_map_iter_next(map, &iter)) != NULL) {
    if (client->local_workload >= 1) {
      ++count;
      load += client->local_workload;
    }
  }

  if (load <= ncores) {
    res_rebalance_cores(map, ncores, nprocs);
  }
  else if (load <= nprocs) {
    res_rebalance_procs(map);
  }
  else if (count < nprocs) {
    res_rebalance_proportional(map);
  }
  else {
    res_rebalance_minimal(map);
  }
}

void res_loop(int listen)
{
  fd_set        rset, wset, rout, wout;
  int           max, num, sock, bit;
  intmap_t*     sock_map = res_map_create();
  intmap_t*     client_map = res_map_create();
  bitmap_t      bitmap = 0;
  int           wcnt = 0, loops = 0;
  const int     max_loops = 10;

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_SET(listen, &rset);
  max = listen;

  while (++loops <= max_loops) {
    bool rebalance = false;

    rout = rset;
    wout = wset;
    num = select(max + 1, &rout, wcnt > 0 ? &wout : NULL, NULL, NULL);
    if (num <= 0) {
      pexit("select");
    }

    if (FD_ISSET(listen, &rout)) {
      FD_CLR(listen, &rout);
      --num;
      if ((sock = res_accept_socket(listen, true)) == -1) {
        break;
      } else {
        client_t *client = NULL;
        for (bit = 0; bit <= MAX_BIT; ++bit) {
          if (NOT(bitmap, bit)) {
            FD_SET(sock, &rset);
            max = MAX(max, sock);
            client = res_client_create(bit, sock);
            res_map_add(client_map, bit, client);
            res_map_add(sock_map, sock, client);
            break;
          }
        }
        if (client == NULL) {
          res_warn("Insufficient room.\n");
          res_socket_send(sock, "{ full } \n", 10);
          res_socket_close(sock);
        }
      }
    }

    for (sock = 1 + listen; num > 0 && sock <= max; ++sock) {
      if (FD_ISSET(sock, &rout) || FD_ISSET(sock, &wout)) {
        client_t* client = res_map_get(sock_map, sock);
        int io;
        --num;
        if (FD_ISSET(sock, &rout)) {
          FD_CLR(sock, &rset);
          io = res_client_read(client);
          if (io >= 0 && client->rebalance) {
            client->rebalance = false;
            rebalance = true;
          }
        } else {
          FD_CLR(sock, &wset);
          --wcnt;
          assert(wcnt >= 0);
          io = res_client_write(client);
        }
        if (io == -1) {
          res_client_destroy(client);
          res_map_del(client_map, client->bit);
          res_map_del(sock_map, sock);
          rebalance = true;
        } else {
          if (res_client_writing(client)) {
            FD_SET(sock, &wset);
            ++wcnt;
          } else {
            FD_SET(sock, &rset);
          }
        }
      }
    }

    if (rebalance) {
      res_rebalance(sock_map);
    }
  }
  res_info("%s: Maximum number of loops reached (%d).\n",
           __func__, max_loops);

  res_map_apply(client_map, (void (*)(void *)) res_client_destroy);
  res_map_destroy(client_map);
  res_map_destroy(sock_map);
}

void res_service(int port)
{
  int listen = res_listen_socket(port, true);
  if (listen < 0) {
    exit(1);
  } else {
    res_loop(listen);
    res_socket_close(listen);
  }
}

