/* queue.c: Concurrent Queue of Requests */

#include "mq/queue.h"

/**
 * Create queue structure.
 * @return  Newly allocated queue structure.
 */
Queue *queue_create()
{
    Queue *q = calloc(1, sizeof(Queue));
    if (q)
    {
        // Initialize mutex
        sem_init(&q->lock, 0, 1);

        // Initialize condition variable
        sem_init(&q->produced, 0, 0);
    }
    return q;
}

/**
 * Delete queue structure.
 * @param   q       Queue structure.
 */
void queue_delete(Queue *q)
{
    if (q)
    {
        sem_wait(&q->lock);
        Request *curr = q->head;
        if (curr)
        {
            Request *next = curr->next;
            while (next)
            {
                request_delete(curr);
                curr = next;
                next = next->next;
            }
            request_delete(curr);
        }

        sem_post(&q->lock);

        free(q);
    }
}

/**
 * Push request to the back of queue.
 * @param   q       Queue structure.
 * @param   r       Request structure.
 */
void queue_push(Queue *q, Request *r)
{
    if (q->size == 0)
    {
        q->head = r;
        q->tail = r;
    }
    else
    {
        q->tail->next = r;
        q->tail = r;
    }
    q->size++;
    sem_post(&q->produced);
}

/**
 * Pop request to the front of queue (block until there is something to return).
 * @param   q       Queue structure.
 * @return  Request structure.
 */
Request *queue_pop(Queue *q)
{
    sem_wait(&q->produced);
    sem_wait(&q->lock);

    Request *r = q->head;
    q->head = q->head->next;
    q->size--;

    sem_post(&q->lock);
    return r;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
