#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/select.h>

#include "node.h"
#include "pipemesg.h"
#include "debugtime.h"

/* Return worker ID from a Pthread. */
int SNetGetWorkerId(void)
{
  worker_t* worker = SNetThreadGetSelf();
  return worker ? worker->id : 0;
}

/* Create configuration which is shared by all workers. */
worker_config_t* SNetCreateWorkerConfig(
  int worker_count,
  int max_worker,
  int pipe_send,
  snet_stream_t *input,
  snet_stream_t *output)
{
  int id;
  worker_config_t *config = SNetNewAlign(worker_config_t);

  assert(input->from || SNetDistribGetNodeId());
  assert(output->dest || SNetDistribGetNodeId());

  /* Allocate globally shared worker configuration. */
  config->worker_count = worker_count;
  config->workers = SNetNewAlignN(1 + max_worker, worker_t*);
  config->thief_limit = SNetThreadingThieves();
  if (config->thief_limit > 1) {
    LOCK_INIT2(config->idle_lock, config->thief_limit);
  } else {
    LOCK_INIT(config->idle_lock);
  }
  config->pipe_send = pipe_send;
  config->input_node = input->from;
  config->output_node = output->dest;

  /* Set all workers to NULL to prevent premature stealing. */
  for (id = 0; id <= max_worker; ++id) {
    config->workers[id] = NULL;
  }
  return config;
}

void SNetDestroyWorkerConfig(worker_config_t* config, int max_worker)
{
  int id;
  for (id = 0; id <= max_worker; ++id) {
    if (config->workers[id]) {
      SNetWorkerDestroy(config->workers[id]);
    }
  }
  SNetMemFree(config->workers);
  SNetMemFree(config);
}

/* Transmit a message from a worker to the master. */
void SNetPipeSend(int fd, const pipe_mesg_t* mesg)
{
  int out = write(fd, mesg, sizeof(pipe_mesg_t));
  if (out != sizeof(pipe_mesg_t)) {
    SNetUtilDebugFatal("[%s]: Write pipe returns %d instead of %d bytes: %s\n",
                       __func__, out, (int) sizeof(mesg), strerror(errno));
  }
}

/* Receive a message from a worker by the master. */
void SNetPipeReceive(int fd, pipe_mesg_t* mesg)
{
  int in = read(fd, mesg, sizeof(pipe_mesg_t));
  if (in != sizeof(pipe_mesg_t)) {
    SNetUtilDebugFatal("[%s]: Read pipe returns %d instead of %d bytes: %s\n",
                       __func__, in, (int) sizeof(mesg), strerror(errno));
  }
}

/* Transmit a MesgDone to the master. */
static void SNetNodeWorkerDone(worker_t* worker)
{
  pipe_mesg_t   mesg;

  if (SNetVerbose()) {
    printf("Worker %2d done\n", worker->id);
  }

  mesg.type = MesgDone;
  mesg.id = worker->id;
  mesg.more = (worker->has_work || worker->has_input);
  SNetPipeSend(worker->config->pipe_send, &mesg);
}

/* Transmit a MesgBusy to the master. */
void SNetNodeWorkerBusy(worker_t* worker)
{
  pipe_mesg_t   mesg;

  if (SNetVerbose()) {
    printf("Worker %2d busy\n", worker->id);
  }

  mesg.type = MesgBusy;
  mesg.id = worker->id;
  mesg.more = true;
  SNetPipeSend(worker->config->pipe_send, &mesg);
}

/* Direct a newly created Pthread to work. */
void *SNetNodeThreadStart(void *arg)
{
  worker_t     *worker = (worker_t *) arg;

  SNetThreadSetSelf(worker);
  switch (worker->role) {
    case InputManager: {
      SNetInputManagerRun(worker);
    } break;

    case LoadManager: {
      SNetLoadManagerRun(worker);
    } break;

    case DataWorker: {
      if (SNetOptResource() && SNetDistribIsRootNode()) {
        SNetWorkerSlave(worker);
      } else {
        SNetWorkerRun(worker);
      }
    } break;

    default: assert(false);
  }
  SNetNodeWorkerDone(worker);
  return arg;
}

/* Start a number of workers and wait for all to finish. */
static void SNetMasterStatic(
  const int num_workers,
  const int num_managers,
  worker_config_t* config,
  const int recv)
{
  const int     total_workers = num_workers + num_managers;
  int           id, alive;
  pipe_mesg_t   mesg;

  /* Create a fixed number of data workers with new threads. */
  for (id = 1; id <= num_workers; ++id) {
    config->workers[id] = SNetWorkerCreate(id, DataWorker, config);
    SNetThreadCreate(SNetNodeThreadStart, config->workers[id], NO_PROC);
  }

  /* Wait for all to finish. */
  for (alive = total_workers; alive > 0; ) {
    SNetPipeReceive(recv, &mesg);
    switch (mesg.type) {
      case MesgDone: {
        --alive;
        if (SNetDebugTL()) {
          printf("[%s]: worker %d done, %d remain.\n", __func__, mesg.id, alive);
        }
      } break;
      default: SNetUtilDebugFatal("[%s]: Bad message %d", __func__, mesg.type);
    }
  }
}

/* Activate one new worker thread. */
void SNetMasterStartOne(int id, worker_config_t* config, int proc)
{
  if (config->workers[id] == NULL) {
    config->workers[id] = SNetWorkerCreate(id, DataWorker, config);
    if (config->worker_count < id) {
      config->worker_count = id;
    }
  } else {
    assert(config->workers[id]->is_idle == WorkerExit);
    config->workers[id]->is_idle = WorkerBusy;
  }
  SNetThreadCreate(SNetNodeThreadStart, config->workers[id], proc);
}

/* Return a bitmask of 1/2/3 when input is available within a given delay. */
int SNetWaitForInput(int pipe, int sock, double delay)
{
  struct timeval tv = { 0, 0 }, *tv_ptr = NULL;
  int num, max = MAX(pipe, sock);
  fd_set ins;
  FD_ZERO(&ins);
  if (pipe >= 0) {
    FD_SET(pipe, &ins);
  }
  if (sock >= 0) {
    FD_SET(sock, &ins);
  }
  if (delay >= 0) {
    SNetTimeFromDouble(&tv, delay);
    tv_ptr = &tv;
  }
  if ((num = select(max + 1, &ins, NULL, NULL, tv_ptr)) < 0) {
    SNetUtilDebugFatal("[%s]: select(%d,%d,%d,%ld,%ld): %s", __func__, max + 1,
                       pipe, sock, tv.tv_sec, tv.tv_usec, strerror(errno));
  }
  return ((num > 0) ?
         ((FD_ISSET(pipe, &ins) ? 1 : 0) +
          (FD_ISSET(sock, &ins) ? 2 : 0)) : 0);
}

/* Throttle number of workers by work load. */
void SNetMasterDynamic(worker_config_t* config, int recv)
{
  const int     worker_limit = config->worker_count;
  int           i, started = 0, wanted = 1, alive = 0, idlers = 0;
  pipe_mesg_t   mesg;
  char         *state = SNetMemCalloc(2 + worker_limit, sizeof(*state));
  enum state { SlaveDone, SlaveIdle, SlaveBusy };
  const double  begin = SNetRealTime();
  double        endtime = 0;

  /* Reset for now; increase as needed. */
  config->worker_count = 0;

  while (alive || started < wanted) {
    if (started < wanted) {
      const double now = SNetRealTime();
      const double slowdown = 0.001;
      const double delay = endtime + slowdown - now;
      if (delay <= 0 || SNetWaitForInput(recv, -1, delay) == false) {
        for (; started < wanted; ++started, ++alive, ++idlers) {
          for (i = 1; state[i]; ++i) {}
          assert(i <= worker_limit);
          SNetMasterStartOne(i, config, NO_PROC);
          state[i] = SlaveIdle;
        }
      }
    }

    SNetPipeReceive(recv, &mesg);
    assert(mesg.id >= 1 && mesg.id <= config->worker_count);
    switch (mesg.type) {

      case MesgDone: {
        --alive;
        --started;
        if (state[mesg.id] == SlaveIdle) {
          assert(idlers > 0 && started >= 0);
          --idlers;
          if (alive == 0) {
            --wanted;
          }
        } else {
          assert(state[mesg.id] == SlaveBusy);
          if (idlers != 0 || alive == 0) {
            --wanted;
          }
        }
        if (SNetDebugTL()) {
          printf("[%s,%.3f]: worker %d done, %d alive, %d idle, %d more.\n",
                 __func__, SNetRealTime() - begin, mesg.id,
                 alive, idlers, wanted - started);
        }
        state[mesg.id] = SlaveDone;
        endtime = SNetRealTime();
      } break;

      case MesgBusy: {
        --idlers;
        if (SNetDebugTL()) {
          printf("[%s,%.3f]: worker %d busy, %d alive, %d idle, %d more.\n",
                 __func__, SNetRealTime() - begin, mesg.id,
                 alive, idlers, wanted - started);
        }
        assert(state[mesg.id] == SlaveIdle);
        state[mesg.id] = SlaveBusy;
        if (alive < worker_limit && started == wanted) {
          ++wanted;
        }
        endtime = SNetRealTime();
      } break;

      default: SNetUtilDebugFatal("[%s]: Bad message %d", __func__, mesg.type);
    }
  }

  SNetMemFree(state);
}

/* Create workers and start them. */
void SNetNodeRun(snet_stream_t *input, snet_info_t *info, snet_stream_t *output)
{
  int                   mg = 0, fildes[2];
  const int             num_workers = SNetThreadingWorkers();
  const int             num_managers = SNetThreadedManagers();
  const int             total_workers = num_workers + num_managers;
  const int             worker_limit = SNetGetMaxProcs() + num_managers;
  const int             max_worker = MAX(worker_limit, total_workers);

  trace(__func__);

  /* Use a pipe to communicate the starting and exiting of a thread. */
  if (pipe(fildes)) {
    SNetUtilDebugFatal("[%s]: no pipe: %s", __func__, strerror(errno));
  } else {
    /* All workers share some common configuration. */
    worker_config_t* config =
      SNetCreateWorkerConfig(total_workers, max_worker, fildes[1], input, output);

    /* Create managers with new threads. */
    if (SNetDistribIsDistributed()) {
      /* Create the network input manager for the distribution layer. */
      int id = ++mg + num_workers;
      assert(id <= total_workers);
      config->workers[id] = SNetWorkerCreate(id, InputManager, config);
      SNetThreadCreate(SNetNodeThreadStart, config->workers[id], NO_PROC);
    }
    if (false && SNetOptResourceServer() != NULL) {
      /* Create the load manager to distribute load to external resources. */
      int id = ++mg + num_workers;
      assert(id <= total_workers);
      config->workers[id] = SNetWorkerCreate(id, LoadManager, config);
      SNetThreadCreate(SNetNodeThreadStart, config->workers[id], NO_PROC);
    }

    /* Check for dynamic resource management. */
    if (SNetOptResourceServer() && SNetDistribIsRootNode()) {
      /* Obtain compute resources dynamically from the resource server. */
      SNetMasterResource(config, fildes[0]);
    }
    else if (SNetOptResource() && SNetDistribIsRootNode()) {
      /* Throttle number of workers dynamically by work load. */
      SNetMasterDynamic(config, fildes[0]);
    } else {
      /* Start a fixed number of workers and wait for termination. */
      SNetMasterStatic(num_workers, num_managers, config, fildes[0]);
    }

    close(fildes[0]);
    close(fildes[1]);
    SNetDestroyWorkerConfig(config, max_worker);
  }
}

