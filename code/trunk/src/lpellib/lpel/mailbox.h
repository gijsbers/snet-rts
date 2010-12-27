#ifndef _MAILBOX_H_
#define _MAILBOX_H_

#include <pthread.h>

#include "lpel.h"

#include "bool.h"


/*
 * worker msg body
 */
typedef enum {
  WORKER_MSG_TERMINATE,
  WORKER_MSG_WAKEUP,
  WORKER_MSG_ASSIGN,
} workermsg_type_t;

typedef struct {
  workermsg_type_t  type;
  lpel_task_t      *task;
} workermsg_t;



/* mailbox structures */

typedef struct mailbox_node_t {
  struct mailbox_node_t *next;
  workermsg_t body;
} mailbox_node_t;

typedef struct {
  pthread_mutex_t  lock_free;
  pthread_mutex_t  lock_inbox;
  pthread_cond_t   notempty;
  mailbox_node_t  *list_free;
  mailbox_node_t  *list_inbox;
} mailbox_t;


void MailboxInit( mailbox_t *mbox);
void MailboxCleanup( mailbox_t *mbox);
void MailboxSend( mailbox_t *mbox, workermsg_t *msg);
void MailboxRecv( mailbox_t *mbox, workermsg_t *msg);
bool MailboxHasIncoming( mailbox_t *mbox);


#endif /* _MAILBOX_H_ */