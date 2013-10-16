#ifndef _PIPE_MESG_H_
#define _PIPE_MESG_H_


enum pipe_mesg_type {
  MesgDone = 10,
  MesgBusy = 20,
};

struct pipe_mesg {
  int   type;
  int   id;
  int   more;
};

#endif
