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
        strcpy(mq->name, name);
        strcpy(mq->host, host);
        strcpy(mq->port, port);

        mutex_init(&mq->lock, NULL);

        mq->shutdown = false;

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
    char uri[BUFSIZ];
    sprintf(uri, "/topic/%s", topic);
    Request *r = request_create("PUT", uri, body);
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

    if (r->body != NULL && !streq(r->body, SENTINEL))
    {
        char *body = strdup(r->body);
        request_delete(r);
        return body;
    }
    else
    {
        request_delete(r);
        return NULL;
    }
}

/**
 * Subscribe to specified topic.
 * @param   mq      Message Queue structure.
 * @param   topic   Topic string to subscribe to.
 **/
void mq_subscribe(MessageQueue *mq, const char *topic)
{
    char uri[BUFSIZ];
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
    char uri[BUFSIZ];
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
    thread_create(&mq->pusher, NULL, mq_pusher, mq);
    thread_create(&mq->puller, NULL, mq_puller, mq);

    mq_subscribe(mq, SENTINEL);
}

/**
 * Stop the message queue client by setting shutdown attribute and sending
 * sentinel messages
 * @param   mq      Message Queue structure.
 */
void mq_stop(MessageQueue *mq)
{
    // Send sentinel, puller needs something to pull
    mq_publish(mq, SENTINEL, SENTINEL);

    mutex_lock(&mq->lock);
    mq->shutdown = true;
    mutex_unlock(&mq->lock);

    thread_join(mq->pusher, NULL);
    thread_join(mq->puller, NULL);
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

        fs = socket_connect(mq->host, mq->port);

        if (!fs)
        {
            continue;
        }
        else
        {
            r = queue_pop(mq->outgoing);
            request_write(r, fs);
            // READ RESPONSE UNTIL EOF
            while (fgets(buf, BUFSIZ, fs))
            {
                buf[strlen(buf) - 1] = '\0';
            }

            request_delete(r);
            fclose(fs);
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

    while (!mq_shutdown(mq))
    {
        // make new request
        char uri[BUFSIZ];
        sprintf(uri, "/queue/%s", mq->name);
        Request *r = request_create("GET", uri, NULL);

        // connect to server
        fs = socket_connect(mq->host, mq->port);

        if (fs)
        {
            // write request
            request_write(r, fs);
            char buf[BUFSIZ];

            // // read response
            if (fgets(buf, BUFSIZ, fs) && strstr(buf, "200 OK"))
            {
                size_t length = 0;
                while (fgets(buf, BUFSIZ, fs) && !streq(buf, "\r\n"))
                {
                    sscanf(buf, "Content-Length: %ld", &length);
                }

                if (length > 0)
                {
                    r->body = calloc((length + 1), sizeof(char));
                    fread(r->body, length, 1, fs);
                }

                if (r->body)
                {
                    queue_push(mq->incoming, r);
                }
                else
                {
                    request_delete(r);
                }
            }
            else
            {
                request_delete(r);
                while (fgets(buf, BUFSIZ, fs))
                {
                    continue;
                }
            }
            fclose(fs);
        }
        else
        {
            request_delete(r);
        }
    }

    return NULL;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
