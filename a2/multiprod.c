/**
 * CSC369 Assignment 2 - Test for msg_queue_poll().
 *
 * Creates one consumer threads and P producer threads.
 * Each producer is assigned one queue to which it writes a total of M messages.
 *
 * The consumer is assigned all the queues and reads from them all in a
 * non-blocking fashion:
 * - Uses msg_queue_poll() to wait until at least one of the queues is readable
 *   (and to determine the set of currently readable queues).
 * - Reads one message from each readable queue.
 *
 * The test measures how much time consumer threads spend in msg_queue_poll()
 * calls and msg_queue_read() calls. Poll time is always expected to be much
 * longer than read time.  To see this effect, you need to uncomment the NDEBUG
 * flag in the first line of the Makefile
 */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "msg_queue.h"


static uint64_t time_diff_ns(const struct timespec *t1, const struct timespec *t0)
{
	return (t1->tv_sec - t0->tv_sec) * 1000000000ll + (t1->tv_nsec - t0->tv_nsec);
}


typedef struct message {
	int data;
} message;


typedef struct prod_args {
	// Queue handle to write to
	msg_queue_t mq;
	// Number of messages to send
	size_t nmsgs;
} prod_args;

// Sends requested number of messages with random delays in between
// Closes the queue handle before exiting; returns the sum of sent message values
static void *prod_thread_f(void *arg)
{
	prod_args *args = (prod_args*)arg;

	long sum = 0;
	for (size_t i = 0; i < args->nmsgs; ++i) {
		// Introduce a delay between sending messages.
		nanosleep(&(struct timespec){ 0, (rand() * 1000) % 1000000 }, NULL);

		message msg = { rand() };
		int ret = msg_queue_write(args->mq, &msg, sizeof(msg));
		(void)ret;
		assert(ret == 0);
		sum += msg.data;
	}

	int ret = msg_queue_close(&args->mq);
	assert(ret == 0);
	(void)ret;

	return (void*)sum;
}


typedef struct cons_args {
	// Total time spent in poll calls (out argument)
	unsigned long poll_time_ns;
	// Total time spent in read calls (out argument)
	unsigned long read_time_ns;
	// Number of queues to read from
	size_t nmqs;
	// Queue handles (embedded array)
	msg_queue_t mqs[];
} cons_args;


/*  Polls and reads from all the queues until all of them are closed by the 
 *  writers. Closes all queue handles before exiting; returns the sum of 
 *  received message values.  We expect most of the time to be spent in poll
 *  calls, not in read calls. (Remember to compile with NDEBUG when timing.)
 */
static void *cons_thread_f(void *arg)
{
	cons_args *args = (cons_args*)arg;

	msg_queue_pollfd fds[args->nmqs];
	for (size_t i = 0; i < args->nmqs; ++i) {
		fds[i] = (msg_queue_pollfd){ args->mqs[i], MQPOLL_READABLE, 0 };
	}

	unsigned long poll_time_ns = 0;
	unsigned long read_time_ns = 0;
	long sum = 0;

	size_t done = 0, iter = 0;
	while (done < args->nmqs) {

		// Measure the time spent in the poll call
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);

		int ready = msg_queue_poll(fds, args->nmqs);

		clock_gettime(CLOCK_MONOTONIC, &t1);
		poll_time_ns += time_diff_ns(&t1, &t0);
		assert(ready > 0);

		// Check which queue(s) have events
		for (size_t i = 0; i < args->nmqs; ++i) {
			int e = fds[i].revents;
			assert((e == 0) || (e == MQPOLL_READABLE) ||
			       (e == (MQPOLL_READABLE | MQPOLL_NOWRITERS)));

			if (e & MQPOLL_READABLE) {
				message msg;
				// Measure the time spent in the read call
				clock_gettime(CLOCK_MONOTONIC, &t0);

				ssize_t len = msg_queue_read(args->mqs[i], &msg, sizeof(msg));

				clock_gettime(CLOCK_MONOTONIC, &t1);
				read_time_ns += time_diff_ns(&t1, &t0);

				if (len > 0) {
					// Read a message
					assert(len == sizeof(msg));
					sum += msg.data;
				} else if (len == 0) {
					// "End of file" - last writer has closed its queue handle
					// Verify that with an extra poll() call
					msg_queue_pollfd fd = { args->mqs[i], MQPOLL_NOWRITERS, 0 };
					int r = msg_queue_poll(&fd, 1);
					assert(r == 1);
					(void)r;
					assert(fd.revents == MQPOLL_NOWRITERS);

					done++;
					// Will invalidate (set to NULL) the queue handle; this
					// entry will be ignored in future msg_queue_poll() calls
					int ret = msg_queue_close(&fds[i].queue);
					assert(!fds[i].queue);
					assert(ret == 0);
					(void)ret;
				} else {
					// Another thread won the race and read the last message
					//NOTE: queue handle is non-blocking
					assert(errno == EAGAIN);
				}

				// No need to check the rest of the pollfd array
				if (--ready == 0) {
					break;
				}
			}
		}
		++iter;
	}

	args->poll_time_ns = poll_time_ns;
	args->read_time_ns = read_time_ns;
	return (void*)sum;
}


int main(int argc, char *argv[])
{
	srand(time(NULL));

	if (argc < 4) {
		fprintf(stderr, "Usage: %s queue_sizef producers messages\n",
		        argv[0]);
		return 1;
	}

	size_t size  = strtoul(argv[1], NULL, 10);
	size_t nprod = strtoul(argv[2], NULL, 10);
	size_t nmsgs = strtoul(argv[3], NULL, 10);

	// Create the queues; handles are open without read/write permissions
	msg_queue_t mqs[nprod];
	for (size_t i = 0; i < nprod; ++i) {
		mqs[i] = msg_queue_create(size * (sizeof(message) + sizeof(size_t)), 0);
		assert(mqs[i]);
	}

	// Each producer is assigned one queue
	// and the consumer is assigned all queues

	// Allocate consumer thread's arguments on stack
	size_t max_cons_args_size = sizeof(cons_args) + nprod * sizeof(void*);
	char arg_buf[max_cons_args_size];

	// Start consumer thread
	pthread_t consumer;
	cons_args *args = (cons_args*)(arg_buf);
	args->nmqs = nprod;
	for (size_t j = 0; j < nprod; ++j) {
		// Because there is only one reader, we don't need to worrry about
		// race conditions between calling poll and reading the last message
		// from a queue, so we can make the read blocking.
		args->mqs[j] = msg_queue_open(mqs[j], MSG_QUEUE_READER);
	}
	int ret = pthread_create(&consumer, NULL, cons_thread_f, args);
	assert(ret == 0);
	(void)ret;

	// Start producer threads
	prod_args pargs[nprod];
	pthread_t producers[nprod];
	for (size_t i = 0; i < nprod; ++i) {
		pargs[i] = (prod_args){ msg_queue_open(mqs[i % nprod], MSG_QUEUE_WRITER), nmsgs };
		int ret = pthread_create(&producers[i], NULL, prod_thread_f, &pargs[i]);
		assert(ret == 0);
		(void)ret;
	}

	// Wait for producer threads to finish and compute the sum of their results
	long prod_sum = 0;
	for (size_t i = 0; i < nprod; ++i) {
		long sum;
		int ret = pthread_join(producers[i], (void**)&sum);
		assert(ret == 0);
		(void)ret;
		prod_sum += sum;
	}

	// Wait for consumer threads to finish and compute the sum of their results
	long cons_sum = 0;
	ret = pthread_join(consumer, (void**)&cons_sum);
	assert(ret == 0);
	(void)ret;

	// Check that all reader/writer queue handles have been closed
	msg_queue_pollfd fds[nprod];
	for (size_t i = 0; i < nprod; ++i) {
		fds[i] = (msg_queue_pollfd){ mqs[i], MQPOLL_NOREADERS | MQPOLL_NOWRITERS, 0 };
		assert(fds[i].queue != 0);
	}
	int ready = msg_queue_poll(fds, nprod);
	assert((size_t)ready == nprod);
	(void)ready;
	for (size_t i = 0; i < nprod; ++i) {
		assert(fds[i].revents == (MQPOLL_NOREADERS | MQPOLL_NOWRITERS));
	}

	// Destroy the queues
	for (size_t i = 0; i < nprod; ++i) {
		int ret = msg_queue_close(&mqs[i]);
		assert(ret == 0);
		(void)ret;
		assert(!mqs[i]);
	}

	// Check that no messages were lost
	if (prod_sum != cons_sum) {
		fprintf(stderr, "Sum mismatch: produced %ld != %ld consumed\n",
		        prod_sum, cons_sum);
		return 1;
	}

	printf("poll time: %lu ns, read time: %lu ns\n",
	       args->poll_time_ns, args->read_time_ns);

	return 0;
}
