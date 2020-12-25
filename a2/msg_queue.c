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
 * CSC369 Assignment 2 - Message queue implementation.
 *
 * You may not use the pthread library directly. Instead you must use the
 * functions and types available in sync.h.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "errors.h"
#include "list.h"
#include "msg_queue.h"
#include "ring_buffer.h"


//linked list entry, represents a thread waiting for an event in events to occur
typedef struct waiting_thread{
	cond_t is_ready; //this thread will wait until signaled that it is ready with events to report
	mutex_t poll_mutex; //threads editing the queue and this polling thread must aquire this lock 
} waiting_thread;

typedef struct waiting_thread_entry{
	waiting_thread *wt;
	int events;
	list_entry entry;
} waiting_thread_entry;

// Message queue implementation backend
typedef struct mq_backend {
	// Ring buffer for storing the messages
	ring_buffer buffer;

	// Reference count
	size_t refs;

	// Number of handles open for reads
	size_t readers;
	// Number of handles open for writes
	size_t writers;

	// Set to true when all the reader handles have been closed. Starts false
	// when they haven't been opened yet.
	bool no_readers;
	// Set to true when all the writer handles have been closed. Starts false
	// when they haven't been opened yet.
	bool no_writers;

	//TODO: add necessary synchronization primitives, as well as data structures
	//      needed to implement the msg_queue_poll() functionality
	
	mutex_t queue_mutex;
	cond_t empty;
	cond_t no_space;

	//bitwise OR of MQ_POLL_* constants that are true for this queue
	int qevents;

	list_head wait_queue;
	
} mq_backend;


static int mq_init(mq_backend *mq, size_t capacity)
{
	if (ring_buffer_init(&mq->buffer, capacity) < 0) {
		return -1;
	}

	mq->refs = 0;

	mq->readers = 0;
	mq->writers = 0;

	mq->no_readers = false;
	mq->no_writers = false;

	//TODO: initialize remaining fields (synchronization primitives, etc.)
	mutex_init(&mq->queue_mutex);
	cond_init(&mq->empty);
	cond_init(&mq->no_space);
	list_init(&mq->wait_queue);

	return 0;
}

static void mq_destroy(mq_backend *mq)
{
	assert(mq->refs == 0);
	assert(mq->readers == 0);
	assert(mq->writers == 0);

	ring_buffer_destroy(&mq->buffer);

	//TODO: cleanup remaining fields (synchronization primitives, etc.)
	mutex_destroy(&mq->queue_mutex);
	cond_destroy(&mq->empty);
	cond_destroy(&mq->no_space);
	list_destroy(&mq->wait_queue);
}


#define ALL_FLAGS (MSG_QUEUE_READER | MSG_QUEUE_WRITER | MSG_QUEUE_NONBLOCK)


// Message queue handle is a combination of the pointer to the queue backend and
// the handle flags. The pointer is always aligned on 8 bytes - its 3 least
// significant bits are always 0. This allows us to store the flags within the
// same word-sized value as the pointer by ORing the pointer with the flag bits.

// Get queue backend pointer from the queue handle
static mq_backend *get_backend(msg_queue_t queue)
{
	mq_backend *mq = (mq_backend*)(queue & ~ALL_FLAGS);
	assert(mq);
	return mq;
}

// Get handle flags from the queue handle
static int get_flags(msg_queue_t queue)
{
	return (int)(queue & ALL_FLAGS);
}

// Create a queue handle for given backend pointer and handle flags
static msg_queue_t make_handle(mq_backend *mq, int flags)
{
	assert(((uintptr_t)mq & ALL_FLAGS) == 0);
	assert((flags & ~ALL_FLAGS) == 0);
	return (uintptr_t)mq | flags;
}


static msg_queue_t mq_open(mq_backend *mq, int flags)
{
	++mq->refs;

	if (flags & MSG_QUEUE_READER) {
		++mq->readers;
		mq->no_readers = false;
	}
	if (flags & MSG_QUEUE_WRITER) {
		++mq->writers;
		mq->no_writers = false;
	}

	return make_handle(mq, flags);
}

// Returns true if this was the last handle
static bool mq_close(mq_backend *mq, int flags)
{
	assert(mq->refs != 0);
	assert(mq->refs >= mq->readers);
	assert(mq->refs >= mq->writers);

	if ((flags & MSG_QUEUE_READER) && (--mq->readers == 0)) {
		mq->no_readers = true;
	}
	if ((flags & MSG_QUEUE_WRITER) && (--mq->writers == 0)) {
		mq->no_writers = true;
	}

	if (--mq->refs == 0) {
		assert(mq->readers == 0);
		assert(mq->writers == 0);
		return true;
	}
	return false;
}


msg_queue_t msg_queue_create(size_t capacity, int flags)
{
	if (flags & ~ALL_FLAGS) {
		errno = EINVAL;
		report_error("msg_queue_create");
		return MSG_QUEUE_NULL;
	}

	mq_backend *mq = (mq_backend*)malloc(sizeof(mq_backend));
	if (!mq) {
		report_error("malloc");
		return MSG_QUEUE_NULL;
	}
	// Result of malloc() is always aligned on 8 bytes, allowing us to use the
	// 3 least significant bits of the handle to store the 3 bits of flags
	assert(((uintptr_t)mq & ALL_FLAGS) == 0);

	if (mq_init(mq, capacity) < 0) {
		// Preserve errno value that can be changed by free()
		int e = errno;
		free(mq);
		errno = e;
		return MSG_QUEUE_NULL;
	}

	return mq_open(mq, flags);
}

msg_queue_t msg_queue_open(msg_queue_t queue, int flags)
{
	if (!queue) {
		errno = EBADF;
		report_error("msg_queue_open");
		return MSG_QUEUE_NULL;
	}

	if (flags & ~ALL_FLAGS) {
		errno = EINVAL;
		report_error("msg_queue_open");
		return MSG_QUEUE_NULL;
	}

	mq_backend *mq = get_backend(queue);

	//TODO: add necessary synchronization

	mutex_lock(&mq->queue_mutex);
	msg_queue_t new_handle = mq_open(mq, flags);
	mutex_unlock(&mq->queue_mutex);
	return new_handle;	
}

int msg_queue_close(msg_queue_t *queue)
{
	if (!queue || !*queue) {
		errno = EBADF;
		report_error("msg_queue_close");
		return -1;
	}

	mq_backend *mq = get_backend(*queue);
	
	//TODO: add necessary synchronization
	mutex_lock(&mq->queue_mutex);

	if (mq_close(mq, get_flags(*queue))) {
		// Closed last handle; destroy the queue
		mutex_unlock(&mq->queue_mutex);
		mq_destroy(mq);
		free(mq);
		*queue = MSG_QUEUE_NULL;
		return 0;
	}

	

	//TODO: if this is the last reader (or writer) handle, notify all the writer
	//      (or reader) threads currently blocked in msg_queue_write() (or
	//      msg_queue_read()) and msg_queue_poll() calls for this queue.
	if(mq->no_readers){
		mq->qevents = mq->qevents | MQPOLL_WRITABLE | MQPOLL_NOREADERS;
		cond_broadcast(&mq->no_space); 

	}
	if(mq->no_writers){
		mq->qevents = mq->qevents | MQPOLL_READABLE | MQPOLL_NOWRITERS;
		cond_broadcast(&mq->empty); 
		
	}
	mutex_unlock(&mq->queue_mutex);

	*queue = MSG_QUEUE_NULL;
	return 0;
}


void trigger_event(msg_queue_t queue, int events){
	mq_backend *mq = get_backend(queue);
	mq->qevents = mq->qevents | events;
	list_entry *pos;
	list_for_each(pos, &mq->wait_queue){
		waiting_thread_entry *wt_entry = container_of(pos, waiting_thread_entry, entry);
		if(wt_entry->events & mq->qevents){ //requested event flag for this thread is true right now
			mutex_lock(&wt_entry->wt->poll_mutex);
			//signal waiting thread can report and return
			cond_signal(&wt_entry->wt->is_ready); 
			//give up lock shared with waiting thread so it can continue, wait until thread is done 
			//so queue state is preserved
			mutex_unlock(&wt_entry->wt->poll_mutex);
			//cond_wait(&wt_entry->wt->is_done, &wt_entry->wt->poll_mutex); 
			//mutex_unlock(&wt_entry->wt->poll_mutex); //done with this mutex at this point
			//mutex_destroy(&wt_entry->wt->poll_mutex);
		}
	}
	
}

ssize_t msg_queue_read(msg_queue_t queue, void *buffer, size_t length)
{
	
	if(!(get_flags(queue) & MSG_QUEUE_READER)){
		errno = EBADF;
		report_error("msg_queue_read"); 
		return -1;
	}

	mq_backend *mq = get_backend(queue);
	mutex_lock(&mq->queue_mutex); 
	//wait while ring buffer is empty and there are writers yet to be closed
	while(ring_buffer_used(&mq->buffer) == 0 && !mq->no_writers){
		if(get_flags(queue) & MSG_QUEUE_NONBLOCK){
			errno = EAGAIN;
			report_info("msg_queue_read");
			mutex_unlock(&mq->queue_mutex); 
			return -1;
		}
		cond_wait(&mq->empty, &mq->queue_mutex);
	}

	//if empty and no writers, return with 0
	if(ring_buffer_used(&mq->buffer) == 0 && mq->no_writers){
		trigger_event(queue, MQPOLL_NOWRITERS);
		mutex_unlock(&mq->queue_mutex);
		return 0;
	}

	//queue is non empty, can read
	size_t msg_size;
	ring_buffer_peek(&mq->buffer, &msg_size, sizeof(size_t));
	if(length < msg_size){
		errno = EMSGSIZE;
		report_info("msg_queue_read");
		mutex_unlock(&mq->queue_mutex); 
		return ~msg_size;
	}
	ring_buffer_read(&mq->buffer, &msg_size, sizeof(size_t));
	ring_buffer_read(&mq->buffer, buffer, msg_size);
	cond_signal(&mq->no_space);
	//if queue is empty and there are writers, MQPOLL_READABLE is false
	if(ring_buffer_used(&mq->buffer) == 0 && !mq->no_writers){
		mq->qevents = mq->qevents & ~MQPOLL_READABLE;
	}
	trigger_event(queue, MQPOLL_WRITABLE);

	mutex_unlock(&mq->queue_mutex); 
	return msg_size;
}

int msg_queue_write(msg_queue_t queue, const void *buffer, size_t length)
{
	//TODO
	
	if(!(get_flags(queue) & MSG_QUEUE_WRITER)){
		errno = EBADF;
		report_error("msg_queue_write");
		return -1;
	}

	if(length == 0){
		errno = EINVAL;
		report_error("msg_queue_write");
		return -1;
	}

	mq_backend *mq = get_backend(queue);
	mutex_lock(&mq->queue_mutex); 

	if(mq->buffer.size < (length + sizeof(size_t))){
		errno = EMSGSIZE;
		report_error("msg_queue_write");
		mutex_unlock(&mq->queue_mutex); 
		return -1;
	}

	while(ring_buffer_free(&mq->buffer) < (length + sizeof(size_t)) && !mq->no_readers){
		if(get_flags(queue) & MSG_QUEUE_NONBLOCK){
			errno = EAGAIN;
			report_info("msg_queue_write");
			mutex_unlock(&mq->queue_mutex); 
			return -1;
		}
		cond_wait(&mq->no_space, &mq->queue_mutex);
	}

	
	if(mq->no_readers){
		trigger_event(queue, MQPOLL_NOREADERS);
		errno = EPIPE;
		report_info("msg_queue_write");
		mutex_unlock(&mq->queue_mutex); 
		return -1;
	}
	

	ring_buffer_write(&mq->buffer, (const void *)&length, sizeof(size_t));
	ring_buffer_write(&mq->buffer, buffer, length);
	cond_signal(&mq->empty);

	//if ring_buffer full and there are still readers MQPOLL_WRITEABLE is false
	if(ring_buffer_free(&mq->buffer) == 0 && !mq->no_readers){
		mq->qevents = mq->qevents & ~MQPOLL_WRITABLE;
	}
	trigger_event(queue, MQPOLL_READABLE);

	mutex_unlock(&mq->queue_mutex); 
	return 0;
}

int report_events(msg_queue_pollfd *fds, size_t nfds){
	int num_ready = 0;
	for(unsigned int i = 0; i < nfds; ++i){
		if(fds[i].queue == MSG_QUEUE_NULL) continue;

		int curr_qevents = get_backend(fds[i].queue)->qevents;
		//populate revents
		if((fds[i].revents = curr_qevents & fds[i].events) > 0){
			num_ready++;
		}
	
		if(get_flags(fds[i].queue) & (MSG_QUEUE_READER | MSG_QUEUE_WRITER)){
			fds[i].revents = fds[i].revents | (curr_qevents & (MQPOLL_NOWRITERS | MQPOLL_NOREADERS));
		}
	}
	return num_ready;
}

#define ALLEVENTS (MQPOLL_NOWRITERS | MQPOLL_NOREADERS | MQPOLL_READABLE | MQPOLL_WRITABLE)

int msg_queue_poll(msg_queue_pollfd *fds, size_t nfds)
{
	//======================ARGUMENT VALIDATION===================================

	unsigned int num_null = 0;
	for(int unsigned i = 0; i < nfds; ++i){
		//skip null handles, setting revents to 0
		fds[i].revents = 0; //set to 0
		if(fds[i].queue == MSG_QUEUE_NULL){
			num_null++;
			continue; 
		} 

		if((fds[i].events & ~ALLEVENTS)														         \
			|| ((fds[i].events & MQPOLL_READABLE) && !(get_flags(fds[i].queue) & MSG_QUEUE_READER))  \
			|| ((fds[i].events & MQPOLL_WRITABLE) && !(get_flags(fds[i].queue) & MSG_QUEUE_WRITER))) \
		{
			errno = EINVAL;
			report_error("msg_queue_poll");
			return -1;
		}
		
	}
	if(num_null == nfds){ //check if nfds is 0 or if all handles are MSG_QUEUE_NULL
		errno = EINVAL;
		report_error("msg_queue_poll");
		return -1;
	}
	//=================================================================================
	//establish struct representing this thread (conditon variables and mutex)
	waiting_thread *wt = (waiting_thread *)malloc(sizeof(waiting_thread));
	//establish thread entry structs to append to the wait queues
	waiting_thread_entry *wt_entries = (waiting_thread_entry *)malloc(sizeof(waiting_thread_entry) * nfds);

	if(wt == NULL || wt_entries == NULL){
		report_error("malloc");
		return -1;
	}

	cond_init(&wt->is_ready);
	mutex_init(&wt->poll_mutex);
	mutex_lock(&wt->poll_mutex);
	//add thread to waiting queues
	for(unsigned int i = 0; i < nfds; ++i){
		if(fds[i].queue != MSG_QUEUE_NULL){
			wt_entries[i].wt = wt;
			wt_entries[i].events = fds[i].events;

			mq_backend *mq = get_backend(fds[i].queue);
			mutex_lock(&mq->queue_mutex);
			list_entry_init(&wt_entries[i].entry);
			list_add_tail(&mq->wait_queue, &wt_entries[i].entry);
			mutex_unlock(&mq->queue_mutex);
		}
	}
	//polling_thread is in wait queues, queues can not make read/write operations while polling thread awake

	int num_ready = report_events(fds, nfds);
	if(num_ready == 0){
		cond_wait(&wt->is_ready, &wt->poll_mutex);
		num_ready = report_events(fds, nfds);
	}
	mutex_unlock(&wt->poll_mutex);
	

	//been notified of event, clean up, report events, and return
	
	//remove thread from waiting queues
	//poll mutex and trigger_event function assures atomicity of this operation,
	//only one polling thread can be here at one time, since other "ready" polling threads will be waiting for the
	//queue to give up its own poll mutex, which wont happen until this thread signals on is_done
	for (unsigned int i = 0; i < nfds; ++i){
		if(fds[i].queue != MSG_QUEUE_NULL){
			mq_backend *mq = get_backend(fds[i].queue);
			mutex_lock(&mq->queue_mutex);
			list_del(&mq->wait_queue, &wt_entries[i].entry);
			mutex_unlock(&mq->queue_mutex);
		}
	}
	
	
	//signal all queues which are holding their state until this thread returns that they can continue
	cond_destroy(&wt->is_ready);
	mutex_unlock(&wt->poll_mutex); //destroy will happen in trigger_event to prevent error
	free(wt);
	free(wt_entries);
	return num_ready;
}
