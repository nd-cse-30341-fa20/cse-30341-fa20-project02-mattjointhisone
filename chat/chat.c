/* chat.c: an application to use the pub/sub library */

#include "stdio.h"
#include "mq/client.h"
#include "mq/thread.h"
#include "mq/string.h"

#include <time.h>
#include <unistd.h>

/* Prototypes */
void *listen_for_request (void *);
void usage();

int main()
{
  char  *NAME = getenv("USER");
  char  *TOPIC = "topic";
  char  *HOST = "localhost";
  char  *PORT = "9620";      // server port
  Thread lis;

  MessageQueue *mq = mq_create(NAME, HOST, PORT);
  mq_start(mq);

  // send thread to listen to Requests
  thread_create(&lis, NULL, listen_for_request, mq);
  // this never joins

  while (!feof(stdin))
  {
      char command[BUFSIZ] = "";
      char argument[BUFSIZ] = "";

      printf("\nMQ:: ");

      while (!fgets(command, BUFSIZ, stdin) && !feof(stdin));

      if (streq(command, "help"))
      {

      }
  }

  mq_delete(mq);
  return EXIT_SUCCESS;
}


void *listen_for_request (void *arg)
{
  MessageQueue *mq = (MessageQueue *)arg;
  char *body;
  while (true)
  {
    body = mq_retrieve(mq);    // this blocks for us so we dont spin
    printf("%s", body);
    // fflush(STDIN);
  }
  return NULL;
}
