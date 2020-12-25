/**
 * CSC369 Assignment 2 - Simulates a very simple producer and
 * consumer thread interaction.
 *
 * In this program, the producer sends a bunch of numbers to the consumer and
 * the consumer adds them. To verify correctness, the sums from both producer
 * and consumer are compared.
 *
 * There is just one queue and each message is sizeof(int). The execution
 * consists of 3 phases:
 *
 *   Start Up: The producer and consumer threads are created. The producer
 *   sends a sizeof(int) message indicating how many numbers will be sent.
 *
 *   Running Sum: The producer sends numbers to the consumer while both
 *   calculate the sum.
 *
 *   Shut Down: Producer and consumer close their connections to the queue.
 *
 */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "errors.h"
#include "msg_queue.h"


// Holds information useful for the producer thread
typedef struct ProducerContext {

	// Write handle to the queue.
	msg_queue_t queue;

	// Quantity of numbers intended to send over.
	int numberCount;

	// Sum of all numbers actually sent and received.
	int sum;
} ProducerContext;

/*
* Function run by the producer thread
*/
void *producer(void *contextPtr)
{
	ProducerContext *context = (ProducerContext*)contextPtr;
	msg_queue_t queue = context->queue;

	// We tell the consumer how many numbers we intend to create.
	int count = context->numberCount;
	int ret = msg_queue_write(queue, &count, sizeof(int));
	assert(ret == 0);
	(void)ret;

	// We send over each of the numbers.
	int sum = 0;
	for (int i = 0; i < count; i++) { 
		int number = rand() % 64;
		ret = msg_queue_write(queue, &number, sizeof(int));
		if (ret != 0) {
			if (errno == EPIPE) {
				printf("Broken pipe. Consumer closed.\n");
				fflush(NULL);
				break;
			} else {
				report_error("producer: write failed");
			}
		}

		sum += number;
	}
	context->sum = sum;

	// Close "my" end of the queue.
	ret = msg_queue_close(&queue);
	assert(ret == 0);

	printf("Producer done\n");
	fflush(NULL);

	return NULL;
}

/**
 * This is the main function of the program and essentially the consumer thread.
 */
int main(int argc, char *argv[])
{
	// Generate a random number generator seed based on the current time.
	srand(time(NULL));

	// Read arguments.
	int queueCapacity; // Number of bytes of queue capacity.
	int numberCount; // How many numbers to transfer.
	if (argc >= 3) {

		// The 0th arg is the program name.
		char *pEnd;
		queueCapacity = strtol(argv[1], &pEnd, 10);
		numberCount =  strtol(argv[2], &pEnd, 10);
	} else {
		fprintf(stderr, "Usage: %s queue_capacity message_count\n", argv[0]);
		return 1;
	}

	msg_queue_t queue = msg_queue_create(queueCapacity, MSG_QUEUE_READER);
	if (!queue) {
		return 1;
	}

	// Create the producer thread. ProducerContext is used to pass
	// information to it.
	ProducerContext context;
	context.queue = msg_queue_open(queue, MSG_QUEUE_WRITER);
	context.numberCount = numberCount;

	pthread_t thread;
	int ret = pthread_create(&thread, NULL, producer, &context);
	if (ret != 0) {
		errno = ret;
		perror("pthread_create" CODE_LOCATION);
	}

	// Read how many numbers will be sent over. This thread got the message
	// count originally as an argument.
	int count;
	size_t len = msg_queue_read(queue, &count, sizeof(int));
	assert(len == sizeof(int));
	(void)len;
	assert(count == numberCount);

	// Read the numbers that were sent and add to running sum. 
	int sum = 0;
	for (int i = 0; i < count; i++ ) { 
		int value;
		ssize_t result = msg_queue_read(queue, &value, sizeof(int));
		if (result == 0) {
			printf("Broken pipe. Producer closed.\n");
			fflush(NULL);
			break;
		} else if (result < 0) {
			report_error("main: read number error");
		}

		sum += value;
	}

	// Close "my" end of the queue.
	ret = msg_queue_close(&queue);
	assert(ret == 0);

	// Wait for the producer thread to join (finish execution).
	void *returnValue;
	ret = pthread_join(thread, &returnValue);
	if (ret != 0) {
		errno = ret;
		perror("pthread_join" CODE_LOCATION);
	}

	// Since join means that the producer thread finished execution and since
	// join synchronizes memory, it is safe to grab the producer's sum.
	int expectedSum = context.sum;

	printf("Producer's sum: %d; My sum: %d\n", expectedSum, sum);
	if (expectedSum != sum) {
		fprintf(stderr, "Producer's sum (%d) and my sum (%d) don't match!\n",
				expectedSum, sum);
		assert(false);
	}

	printf("Finished\n");
	fflush(NULL);

	return 0;
}
