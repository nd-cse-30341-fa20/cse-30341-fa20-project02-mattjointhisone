/* chat.c: an application to use the pub/sub library */

#include "stdio.h"
#include "mq/client.h"
#include "mq/thread.h"
#include "mq/string.h"

#include <time.h>
#include <unistd.h>

/* Prototypes */
void *listen_for_request(void *);

void usage()
{
  printf("\nUSAGE\n");
  printf("Use commands subscribe, unsubscribe, and message to interact with server");
}

int main()
{
  char *NAME = getenv("USER");
  char *HOST = "localhost";
  char *PORT = "9620"; // server port
  Thread lis;

  MessageQueue *mq = mq_create(NAME, HOST, PORT);
  mq_start(mq);

  // send thread to listen to Requests
  thread_create(&lis, NULL, listen_for_request, mq);

  while (!feof(stdin))
  {
    char command[BUFSIZ];
    char option[BUFSIZ];
    char argument[BUFSIZ];
    char body[BUFSIZ];

    printf("\nMQ:: ");

    while (!fgets(command, BUFSIZ, stdin) && !feof(stdin))
      ;
    chomp(command);

    sscanf(command, "%s %s", option, argument);

    if (streq(option, "help"))
    {
      usage();
    }
    else if (streq(option, "subscribe"))
    {
      mq_subscribe(mq, argument);
    }
    else if (streq(option, "unsubscribe"))
    {
      mq_unsubscribe(mq, argument);
    }
    else if (streq(option, "exit"))
    {
      break;
    }
    // try to break up body
    sscanf(command, "%s %s %[^\t\n]", option, argument, body);
    if (streq(option, "message"))
    {
      mq_publish(mq, argument, body);
    }
  }

  mq_stop(mq);
  mq_delete(mq);
  return EXIT_SUCCESS;
}

void *listen_for_request(void *arg)
{
  MessageQueue *mq = (MessageQueue *)arg;
  char *body;
  while (true)
  {
    body = mq_retrieve(mq); // this blocks for us so we dont spin
    printf("%s", body);
    free(body);
    fflush(stdout);
  }
  return NULL;
}
