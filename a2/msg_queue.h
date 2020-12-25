/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Andrew Pelegris, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019, 2020 Karen Reid
 */

/**
 * CSC369 Assignment 2 - Message queue header file.
 *
 * You may not use the pthread library directly. Instead you must use the
 * functions and types available in sync.h.
 */

#pragma once

#include <stdint.h>
#include <sys/types.h>


/** Opaque handle to a message queue. */
typedef uintptr_t msg_queue_t;

/** Symbol for a null message queue handle. */
#define MSG_QUEUE_NULL ((msg_queue_t)0)

/**
 * Create a message queue.
 *
 * The returned message queue handle can also optionally be open for
 * reading and/or writing depending on the given flags.
 *
 * Errors:
 *   EINVAL  flags are invalid.
 *   ENOMEM  Not enough memory (NOTE: malloc automatically sets this).
 *
 * @param capacity  number of bytes of capacity that the queue will have.
 * @param flags     a bitwise OR of MSG_QUEUE_* constants that describe the
 *                  attributes of the opened reader and/or writer handle; set
 *                  to 0 if not opening a reader or writer handle.
 * @return          handle to the new message queue on success;
 *                  MSG_QUEUE_NULL on failure (with errno set).
 */
msg_queue_t msg_queue_create(size_t capacity, int flags);

/** Queue handle flag indicating that the handle is for reading messages. */
#define MSG_QUEUE_READER 0x01

/** Queue handle flag indicating that the handle is for writing messages. */
#define MSG_QUEUE_WRITER 0x02

/**
 * Queue handle flag indicating that the handle is used in non-blocking mode.
 *
 * If this flag is set, reading or writing from the queue handle will not block.
 * NOTE: in blocking mode (the default), read and write operation may block
 * indefinitely. In non-blocking mode, a thread performing a read or write
 * operation may still "block" (go to sleep) for a short period of time waiting
 * for a mutex owned by another thread, but it will never block indefinitely.
 */
#define MSG_QUEUE_NONBLOCK 0x04

/**
 * Open an additional handle to a message queue.
 *
 * Errors:
 *   EBADF   queue is not a valid message queue handle.
 *   EINVAL  flags are invalid.
 *
 * @param queue  handle to an existing message queue.
 * @param flags  a bitwise OR of MSG_QUEUE_* constants that describe the
 *               attributes of the opened reader and/or writer handle.
 * @return       new handle to the message queue on success;
 *               MSG_QUEUE_NULL on failure (with errno set).
 */
msg_queue_t msg_queue_open(msg_queue_t queue, int flags);

/**
 * Close a message queue handle and invalidate it (set to MSG_QUEUE_NULL).
 *
 * The queue is destroyed when the last handle is closed. If this is the last
 * reader (or writer) handle (but not the very last handle), must notify all the
 * writer (or reader) threads currently blocked in msg_queue_write() (or
 * msg_queue_read()) and msg_queue_poll() calls for this queue.
 *
 * Errors:
 *   EBADF   queue is not a pointer to a valid message queue handle.
 *
 * @param queue  pointer to the message queue handle.
 * @return       0 on success; -1 on failure (with errno set).
 */
int msg_queue_close(msg_queue_t *queue);


/**
 * Read a message from a message queue.
 *
 * Each message has two components: the size and the body. It first reads the
 * size, and then reads size bytes from the message queue. Blocks until the
 * queue contains at least one message, unless the handle is open in
 * non-blocking mode. If the buffer is too small, the message (including the
 * size component) stays in the queue, and negated message size is returned. If
 * the queue is empty and all the writer handles to the queue have been closed
 * ("end of file"), 0 is returned (NOTE: this does NOT include the case when a
 * newly created queue doesn't have any open writer handles yet).
 *
 * Errors:
 *   EAGAIN    The queue handle is non-blocking and the read would block.
 *   EBADF     queue is not a valid message queue handle open for reads.
 *   EMSGSIZE  The buffer is not large enough to hold the message.
 *
 * @param queue   message queue handle.
 * @param buffer  pointer to the buffer to write into.
 * @param length  maximum number of bytes to read.
 * @return        message size on success;
 *                negated message size if the buffer is too small
 *                  (with errno set to EMSGSIZE);
 *                0 if all the writer handles have been closed ("end of file");
 *                -1 in case of other failures (with errno set).
 */
ssize_t msg_queue_read(msg_queue_t queue, void *buffer, size_t length);

/**
 * Write a message into a message queue.
 *
 * Each message has two components: the size and the body, therefore we need two
 * write calls to the underlying ring buffer. It first writes the size of the
 * message (length), and then writes length bytes from the buffer. Blocks until
 * the queue has enough free space for the message, unless the handle is open in
 * non-blocking mode. If all the reader handles to the queue have been closed
 * ("broken pipe"), fails with EPIPE, even if the queue has enough free space
 * for the message (NOTE: this does NOT include the case when a newly created
 * queue doesn't have any open reader handles yet).
 *
 * Errors:
 *   EAGAIN    The queue handle is non-blocking and the write would block.
 *   EBADF     queue is not a valid message queue handle open for writes.
 *   EINVAL    Zero length message.
 *   EMSGSIZE  The capacity of the queue is not large enough for the message.
 *   EPIPE     All reader handles to the queue have been closed ("broken pipe").
 *
 * @param queue   message queue handle.
 * @param buffer  pointer to the buffer to read from.
 * @param length  number of bytes to write.
 * @return        0 on success; -1 on failure (with errno set).
 */
int msg_queue_write(msg_queue_t queue, const void *buffer, size_t length);


/** Describes a message queue to be monitored in a msg_queue_poll() call. */
typedef struct msg_queue_pollfd {

	/**
	 * Message queue handle to be monitored.
	 *
	 * If set to MSG_QUEUE_NULL, msg_queue_poll() will skip this entry: it must
	 * set the revents field to 0, ignoring the value of the events field.
	 */
	msg_queue_t queue;

	/**
	 * Requested events.
	 *
	 * A bitwise OR of MQPOLL_* constants. Set by the user before calling
	 * msg_queue_poll() and describes what events are subscribed to.
	 */
	int events;

	/**
	 * Returned events.
	 *
	 * A bitwise OR of MQPOLL_* constants. Set by the msg_queue_poll() call and
	 * indicates what events have actually occurred. If the queue handle is
	 * a reader (or writer) handle, MQPOLL_NOWRITERS (or MQPOLL_NOREADERS) may
	 * be reported, even if not explicitly subscribed to.
	 */
	int revents;

} msg_queue_pollfd;

/**
 * Event flag indicating that the queue is readable, i.e. the *very next*
 * msg_queue_read() call will not block.
 *
 * This means that either there is a message to be read, or all the writer
 * handles to the queue have been closed. *Very next* means that no other read
 * or write operations happen between the msg_queue_poll() call and the (*very
 * next*) msg_queue_read() call. This event can be generated by:
 * - a successful msg_queue_write() call in another tread;
 * - a msg_queue_close() call in another thread that closed the last writer
 *   handle to this queue (MQPOLL_NOWRITERS must also be reported in this case).
 */
#define MQPOLL_READABLE 0x01

/**
 * Event flag indicating that the queue is writable, i.e. the *very next*
 * msg_queue_write() call will not block.
 *
 * This means that either there is room (at least header size plus one byte) for
 * a message to be written, or all the reader handles to the queue have been
 * closed. *Very next* means that no other read or write operations happen
 * between the msg_queue_poll() call and the (*very next*) msg_queue_write()
 * call. This event can be generated by:
 * - a successful msg_queue_read() call in another tread;
 * - a msg_queue_close() call in another thread that closed the last reader
 *   handle to this queue (MQPOLL_NOREADERS must also be reported in this case).
 */
#define MQPOLL_WRITABLE 0x02

/**
 * Event flag indicating that all reader handles to the queue have been closed.
 *
 * NOTE: this does NOT include the case when a newly created queue doesn't have
 * any open reader handles yet. This event can be generated by:
 * - a msg_queue_close() call in another thread that closed the last reader
 *   handle to this queue.
 */
#define MQPOLL_NOREADERS 0x04

/**
 * Event flag indicating that all writer handles to the queue have been closed.
 *
 * NOTE: this does NOT include the case when a newly created queue doesn't have
 * any open writer handles yet. This event can be generated by:
 * - a msg_queue_close() call in another thread that closed the last writer
 *   handle to this queue.
 */
#define MQPOLL_NOWRITERS 0x08

/**
 * Wait for an event to occur in any message queue from a set of queues.
 *
 * Errors:
 *   EINVAL  events field in a pollfd entry is invalid, i.e. set to 0 or not a
 *           valid combination of MQPOLL_* constants).
 *   EINVAL  No events are subscribed to, i.e. nfds is 0, or all the pollfd
 *           entries have the queue field set to MSG_QUEUE_NULL.
 *   EINVAL  MQPOLL_READABLE requested for a non-reader queue handle or
 *           MQPOLL_WRITABLE requested for a non-writer queue handle.
 *   ENOMEM  Not enough memory (NOTE: malloc automatically sets this).
 *
 * @param fds   message queues to be monitored.
 * @param nfds  number of items in the fds array.
 * @return      number of queues ready for I/O (with non-zero revents)
 *              on success; -1 on failure (with errno set).
 */
int msg_queue_poll(msg_queue_pollfd *fds, size_t nfds);
