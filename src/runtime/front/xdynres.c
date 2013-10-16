#include <sys/param.h>
#include "node.h"
#include "pipemesg.h"
#include "debugtime.h"

#if ENABLE_RESSERV
#include "resdefs.h"
#include "resconf.h"
#endif

#define WAIT_FOREVER    (-1.0)

void SNetBindLogicalProc(int proc)
{
#if ENABLE_RESSERV
  if (proc >= 0) {
    res_bind_logical_proc(proc);
  }
#endif
}

void SNetLoadManagerRun(worker_t* worker)
{
}

/* Dynamic resource management via the resource server. */
void SNetMasterResource(worker_config_t* config, int recv)
{
#if ENABLE_RESSERV
  const int     worker_limit = config->worker_count;
  int           worker_count = 0, remotes = 0;
  int           i, started = 0, wanted = 1, idlers = 0, granted = 0, managed = 0;
  pipe_mesg_t   mesg;
  char         *state = SNetMemCalloc(2 + worker_limit, sizeof(*state));
  int          *procs = SNetMemCalloc(2 + worker_limit, sizeof(*procs));
  enum state { SlaveDone, SlaveIdle, SlaveBusy };
  const double  begin = SNetRealTime(), load_check_period = 0.1;
  double        endtime = 0, loadtime = 0;
  server_t     *server;
  bitmap_t      revokes = BITMAP_ZERO;
  bitmap_t      hostmap = BITMAP_ZERO;
  res_client_conf_t client_spec;

  /* Initialize the resource management library. */
  res_set_program_name(SNetGetProgramName());
  res_set_verbose(SNetVerbose());
  res_set_debug(SNetDebugRS());
  res_topo_create();
  res_init_client_conf(&client_spec);
  res_get_client_config(SNetOptResourceServer(), &client_spec);
  if (client_spec.slowdown <= 0 || client_spec.slowdown > 1) {
    res_error("[%s]: Invalid slowdown %g.\n", __func__, client_spec.slowdown);
  }

  /* Create a connection with the resource server. */
  server = res_server_connect(client_spec.server_addr, client_spec.server_port);
  if (server == NULL) {
    res_topo_destroy();
    return;
  }

  /* Check for management workers which have already started. */
  for (i = 0; i <= worker_limit; ++i) {
    if (config->workers[i]) {
      assert(config->workers[i]->id == i);
      assert(config->workers[i]->role == InputManager);
      state[i] = SlaveBusy;
      procs[i] = NO_PROC;
      ++managed;
    }
  }

  /* Loop for as long as there are threads. */
  while (started > 0 || wanted > 0 || managed > 0) {
    const int sock = res_server_get_socket(server);
    const int bit_sock = 1;
    const int bit_recv = 0;
    int input = 0;
    bitmap_t mask = BITMAP_ZERO;

    if (wanted != res_server_get_local(server)) {
      /* Update resource server about the local workload. */
      res_server_set_local(server, wanted);
    }
    if (granted != res_server_get_granted(server)) {
      /* Update our notion about how many resources we can use. */
      granted = res_server_get_granted(server);
    }
    /* Check for new revokes. */
    res_server_get_revoke_mask(server, &mask);
    if (BITMAP_NEQ(revokes, mask)) {
      int i, k;
      for (i = 0; i < MAX_BIT; ++i) {
        if (HAS(mask, i)) {
          if (NOT(revokes, i)) {
            SET(revokes, i);
            /* Request worker to stop. */
            for (k = 1; k <= worker_count; ++k) {
              if (procs[k] == i) {
                if (config->workers[k]->proc_revoked == false) {
                  config->workers[k]->proc_revoked = true;
                }
                break;
              }
            }
            assert(k <= worker_count);
          }
        }
        else if (HAS(revokes, i)) {
          /* Remove stop request from worker. */
          for (k = 1; k <= worker_count; ++k) {
            if (procs[k] == i) {
              if (config->workers[k]->proc_revoked == true) {
                config->workers[k]->proc_revoked = false;
              }
              break;
            }
          }
          assert(k <= worker_count);
        }
      }
    }
    /* Check for new systems. */
    res_server_get_systems_mask(server, &mask);
    if (BITMAP_NEQ(hostmap, mask)) {
      for (int i = 0; i < MAX_BIT; ++i) {
        if (HAS(mask, i) && NOT(hostmap, i)) {
          SET(hostmap, i);
          if (i > 0) {
            const char* name = res_topo_get_hostname(i);
            printf("New system %s.\n", name);
            if (SNetDistribIsDistributed()) {
              int node = 0;
              if (SNetInputManagerGetNode(name, &node)) {
                printf("Known system %s as %d.\n", name, node);
                res_server_access(server, i);
                ++remotes;
                if (remotes == 1) {
                  loadtime = SNetRealTime();
                }
              } else {
                printf("System %s unknown.\n", name);
              }
            }
          }
        }
      }
    }

    /* Start new threads when needed, given enough resources. */
    if (started < MIN(wanted, granted)) {
      const double now = SNetRealTime();
      const double slowdown = client_spec.slowdown;
      const double delay = endtime + slowdown - now;
      if (delay <= 0 || (input = SNetWaitForInput(recv, sock, delay)) == 0) {
        do {
          int proc = res_server_allocate_proc(server);
          for (i = 1; state[i]; ++i) {}
          assert(i <= worker_limit);
          SNetMasterStartOne(i, config, proc);
          state[i] = SlaveIdle;
          procs[i] = proc;
          if (worker_count < i) worker_count = i;
        } while (++idlers, ++started < MIN(wanted, granted));
      }
    }

    if (!input) {
      double wait = WAIT_FOREVER;
      if (remotes > 0) {
        const double now = SNetRealTime();
        wait = (loadtime > now) ? (loadtime - now) : 0;
      }
      input = SNetWaitForInput(recv, sock, wait);
      assert(input >= 0 && input <= 3);
    }

    if (HAS(input, bit_sock)) {
      res_server_read(server);
    }

    if (HAS(input, bit_recv)) {
      SNetPipeReceive(recv, &mesg);
      assert(mesg.id >= 1 && mesg.id <= worker_limit);
      switch (mesg.type) {

        case MesgDone: {
          if (mesg.id <= worker_count) {
            --started;
            assert(started >= 0);
            if (state[mesg.id] == SlaveIdle) {
              --idlers;
              assert(idlers >= 0);
              if (started + managed == 0 && mesg.more == false) {
                --wanted;
              }
            } else {
              assert(state[mesg.id] == SlaveBusy);
              if (mesg.more == false && (idlers != 0 || started + managed == 0)) {
                --wanted;
              }
            }
            if (wanted > started + 1) {
              wanted = started + 1;
            }
            if (idlers > 0 && wanted > started) {
              wanted = started;
            }
            res_server_release_proc(server, procs[mesg.id]);
            if (HAS(revokes, procs[mesg.id])) {
              CLR(revokes, procs[mesg.id]);
              if (config->workers[mesg.id]->proc_revoked) {
                config->workers[mesg.id]->proc_revoked = false;
              }
            }
            if (SNetDebugTL()) {
              printf("[%s,%.3f]: worker %d done, %d alive, %d idle, %d more.\n",
                     __func__, SNetRealTime() - begin, mesg.id,
                     started, idlers, wanted - started);
            }
            state[mesg.id] = SlaveDone;
            endtime = SNetRealTime();
          } else {
            assert(mesg.id > worker_count && mesg.id <= worker_limit);
            assert(config->workers[mesg.id]->role == InputManager);
            assert(managed > 0);
            managed -= 1;
          }
        } break;

        case MesgBusy: {
          --idlers;
          assert(idlers >= 0);
          if (SNetDebugTL()) {
            printf("[%s,%.3f]: worker %d busy, %d alive, %d idle, %d more.\n",
                   __func__, SNetRealTime() - begin, mesg.id,
                   started, idlers, wanted - started);
          }
          assert(state[mesg.id] == SlaveIdle);
          state[mesg.id] = SlaveBusy;
          if (started < worker_limit && started == wanted && idlers == 0) {
            ++wanted;
          }
          endtime = SNetRealTime();
        } break;

        default: SNetUtilDebugFatal("[%s]: Bad message %d", __func__, mesg.type);
      }
    }

    if (remotes > 0) {
      const double now = SNetRealTime();
      if (now >= loadtime) {
        loadtime = now + load_check_period;
      }
    }
  }

  SNetMemFree(state);
  SNetMemFree(procs);
  res_server_destroy(server);
  res_topo_destroy();
#else
  SNetUtilDebugFatal("[%s]: Resource management was not configured.", __func__);
#endif
}

