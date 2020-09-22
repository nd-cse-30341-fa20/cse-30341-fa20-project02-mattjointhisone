/* chat.c: an application to use the pub/sub library */

#include "stdio.h"
#include "mq/client.h"

#include <assert.h>
#include <time.h>
#include <unistd.h>

/* GLOBALS */
const char  *HOST = "localhost";
const char  *PORT = "9620";      // server port

int main()
{
  char  *NAME = getenv("USER");

  MessageQueue *mq = mq_create(NAME, HOST, PORT);

  mq_subscribe(mq, "Noodles");
  mq_start(mq);
  //mq_subscribe(mq, "Pasta");
  mq_stop(mq);

  mq_delete(mq);

  printf("And here we went ...\n");
  return 0;
}
