/* chat.c: an application to use the pub/sub library */

#include "stdio.h"
#include "mq/client.h"
#include "mq/thread.h"
#include "mq/string.h"

#include <time.h>
#include <unistd.h>

/* Prototypes */
void *listen_for_request (void *);

void usage()
{
  printf("\nUSAGE\n");
  printf("Use commands sub, unsub, and message to interact with server");
}

int main()
{
  char  *NAME = getenv("USER");
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
      char command[BUFSIZ];

      char option[BUFSIZ];
      char argument[BUFSIZ];
      char body[BUFSIZ];
      char *token;

      printf("\nMQ:: ");

      while (!fgets(command, BUFSIZ, stdin) && !feof(stdin));
      chomp(command);

      token = strtok(command, " ");
      strcpy(option, token);

      if (streq(option, "help"))
      {
        usage();
      }
      else if (streq(option, "subscribe"))
      {
        token = strtok(NULL, " ");
        strcpy(argument, token);
        mq_subscribe(mq, argument);
      }
      else if (streq(option, "unsubscribe"))
      {
        token = strtok(NULL, " ");
        strcpy(argument, token);
        mq_unsubscribe(mq, argument);
      }
      else if (streq(option, "exit"))
      {
        break;
      }
      else if (streq(option, "message"))
      {
        token = strtok(NULL, " ");
        strcpy(argument, token);

        token = strtok(NULL, " ");
        strcpy(body, token);

        mq_publish(mq, argument, body);
      }
  }

  mq_stop(mq);
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
    if (!body) break;
    printf("\nMessage recived: %s\n", body);
    printf("\nMQ:: ");
    free(body);
    fflush(stdout);
  }
  return NULL;
}
