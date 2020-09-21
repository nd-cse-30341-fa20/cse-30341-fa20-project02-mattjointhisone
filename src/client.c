/* client.c: Message Queue Client */

#include "mq/client.h"
#include "mq/logging.h"
#include "mq/socket.h"
#include "mq/string.h"
#include "mq/thread.h"

/* Internal Constants */

#define SENTINEL "SHUTDOWN"

/* Internal Prototypes */

void *mq_pusher(void *);
void *mq_puller(void *);

/* External Functions */

/**
 * Create Message Queue withs specified name, host, and port.
 * @param   name        Name of client's queue.
 * @param   host        Address of server.
 * @param   port        Port of server.
 * @return  Newly allocated Message Queue structure.
 */
MessageQueue *mq_create(const char *name, const char *host, const char *port)
{
    MessageQueue *mq = calloc(1, sizeof(MessageQueue));

    if (mq)
    {
        if (name)
        {
            strcpy(mq->name, name);
        }
        if (host)
        {
            strcpy(mq->host, host);
        }
        if (port)
        {
            strcpy(mq->port, port);
        }

        mutex_init(&mq->lock, NULL);

        mq->outgoing = queue_create();
        mq->incoming = queue_create();
    }

    return mq;
}

/**
 * Delete Message Queue structure (and internal resources).
 * @param   mq      Message Queue structure.
 */
void mq_delete(MessageQueue *mq)
{
    if (mq)
    {
        queue_delete(mq->incoming);
        queue_delete(mq->outgoing);
        free(mq);
    }
}

/**
 * Publish one message to topic (by placing new Request in outgoing queue).
 * @param   mq      Message Queue structure.
 * @param   topic   Topic to publish to.
 * @param   body    Message body to publish.
 */
void mq_publish(MessageQueue *mq, const char *topic, const char *body)
{
    Request *r = request_create("PUT", topic, body);
    queue_push(mq->outgoing, r);
}

/**
 * Retrieve one message (by taking Request from incoming queue).
 * @param   mq      Message Queue structure.
 * @return  Newly allocated message body (must be freed).
 */
char *mq_retrieve(MessageQueue *mq)
{
    Request *r = queue_pop(mq->incoming);
    return r->body;
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic)
{
    char uri[1050];
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);

    queue_push(mq->outgoing, request_create("PUT", uri, NULL));
}

/**
 * Unubscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to unsubscribe from.
 **/
void mq_unsubscribe(MessageQueue *mq, const char *topic)
{
    char uri[1050];
    sprintf(uri, "/subscription/%s/%s", mq->name, topic);

    queue_push(mq->outgoing, request_create("DELETE", uri, NULL));
}

/**
 * Start running the background threads:
 *  1. First thread should continuously send requests from outgoing queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   mq      Message Queue structure.
 */
void mq_start(MessageQueue *mq)
{
    // BUI said do this, idk if it should be here
    mq_subscribe(mq, SENTINEL);

    thread_create(&mq->pusher, NULL, mq_pusher, mq);
    thread_create(&mq->puller, NULL, mq_puller, mq);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq)
{
    mq->shutdown = true;
    // publish to the SENTINEL
    // join threads

    size_t status;

    thread_join(mq->pusher, (void **)&status);
    thread_join(mq->puller, (void **)&status);
}

/**
 * Returns whether or not the message queue should be shutdown.
 * @param   mq      Message Queue structure.
 */
bool mq_shutdown(MessageQueue *mq)
{
    mutex_lock(&mq->lock);
    bool status = mq->shutdown;
    mutex_unlock(&mq->lock);

    return status;
}

/* Internal Functions */

/**
 * Pusher thread takes messages from outgoing queue and sends them to server.
*  1. First thread should continuously send requests from outgoing queue.
 * @param   arg     Message Queue structure.
 **/
void *mq_pusher(void *arg)
{
    MessageQueue *mq = (MessageQueue *)arg;
    Request *r;
    FILE *fs;
    char buf[BUFSIZ];

    while (!mq_shutdown(mq))
    {
        r = queue_pop(mq->outgoing);
        fs = socket_connect(mq->host, mq->port);
        if (!fs)
        {
            continue;
        }
        else
        {
            request_write(r, fs);
            // READ RESPONSE UNTIL EOF
            while (fgets(buf, BUFSIZ, fs))
            {
                buf[strlen(buf) - 1] = '\0';
            }

            request_delete(r);
        }
    }

    return NULL;
}

/**
 * Puller thread requests new messages from server and then puts them in
 * incoming queue.
 *  2. Second thread should continuously receive reqeusts to incoming queue.
 * @param   arg     Message Queue structure.
 **/
void *mq_puller(void *arg)
{
    MessageQueue *mq = (MessageQueue *)arg;
    FILE *fs;
    size_t length;
    char uri[1050];

    char buf[BUFSIZ];

    while (!mq_shutdown(mq))
    {
        // make new request
        sprintf(uri, "/queue/%s/", mq->name);
        Request *r = request_create("GET", uri, NULL);

        // connect to server
        fs = socket_connect(mq->host, mq->port);

        if (fs)
        {
            // write request
            request_write(r, fs);
        }
        else
        {
            continue;
        }

        // // read response
        if (!strstr(fgets(buf, BUFSIZ, fs), "200 OK"))
        {
            break; // bad request and whastever, handle this better
        }

        // check if there is a body
        while (fgets(buf, BUFSIZ, fs) && !streq(buf, "\r\n"))
        {
            sscanf(buf, "Content-Length: %ld", &length);
        }
        r->body = malloc((length + 1) * sizeof(char));
        fread(r->body, length, 1, fs);

        queue_push(mq->incoming, r);
        request_delete(r);
    }

    return NULL;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
