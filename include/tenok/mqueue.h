/**
 * @file
 */
#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include <sys/types.h>

#include <common/list.h>

typedef uint32_t mqd_t;

struct mq_attr {
    int mq_flags;   /* Flags: 0 or O_NONBLOCK */
    int mq_maxmsg;  /* Max number of the messages in the queue */
    int mq_msgsize; /* Byte size of the message */
    int mq_curmsgs; /* Number of the messages currently in the queue */
};

/**
 * @brief  Retrieve atributes of the message queue referred to by the message
 *         queue descriptor mqdes
 * @param  mqdes: The message queue descriptor to provide.
 * @param  newattr: Pointer to the new attribute.
 * @param  oldattr: Pointer to the memory space for storing old attributes.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mq_getattr(mqd_t mqdes, struct mq_attr *attr);

/**
 * @brief  Modify attributes of the message queue referred to by the message
 *         queue descriptor mqdes
 * @param  mqdes: The message queue descriptor to provide.
 * @param  attr: The memory space for retrieving message queue attributes.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mq_setattr(mqd_t mqdes,
               const struct mq_attr *newattr,
               struct mq_attr *oldattr);

/**
 * @brief  Create a new message queue or open an existing queue
 * @param  name: The name of the message queue.
 * @param  oflag: The flags for opening the message queue.
 * @param  attr: The attribute object for setting the message queue.
 * @retval mqd_t: The message queue descriptor to return.
 */
mqd_t mq_open(const char *name, int oflag, struct mq_attr *attr);

/**
 * @brief  Close the message queue descriptor
 * @param  mqdes: The message queue descriptor to provide.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mq_close(mqd_t mqdes);

int mq_unlink(const char *name);

/**
 * @brief  Remove the oldest message with highest priority from the message
 *         queue referred to by the message queue descriptor mqdes and places
 *         it in the buffer pointed to by msg_ptr
 * @param  mqdes: The message queue descriptor to provide.
 * @param  msg_ptr: The buffer for storing the received message.
 * @param  msg_len: The length of the buffer pointed to by msg_ptr.
 * @param  msg_prio: The priority of the received message.
 * @retval ssize_t: The size of the received message in bytes.
 */
ssize_t mq_receive(mqd_t mqdes,
                   char *msg_ptr,
                   size_t msg_len,
                   unsigned int *msg_prio);

/**
 * @brief  Add the message pointed to by msg_ptr to the message queue referred
 *         to by the message queue descriptor mqdes
 * @param  mqdes: The message queue descriptor to provide.
 * @param  msg_ptr: The message to send.
 * @param  msg_len: The size of the message in bytes.
 * @param  msg_prio: The priority of the message to send.
 * @retval int: 0 on success and nonzero error number on error.
 */
int mq_send(mqd_t mqdes,
            const char *msg_ptr,
            size_t msg_len,
            unsigned int msg_prio);

#endif
