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
 * CSC369 Assignment 2 - Ring buffer implementation.
 *
 * The ring buffer is fully implemented and ready to use in your solution.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "errors.h"
#include "ring_buffer.h"


// Ring buffer mutex access validator delay in nanoseconds.
#ifndef RING_BUFFER_VALIDATOR_DELAY
#define RING_BUFFER_VALIDATOR_DELAY 1000
#endif


// Indicates that a byte of data in the buffer is considered "freed".
static const char poison = 0xDD;

// Marks that a segment of memory is considered "freed".
static void mark_free(char *start, size_t length)
{
	(void)start;
	(void)length;
	(void)poison;
#ifndef NDEBUG
	memset(start, poison, length);
#endif
}

// Checks that a segment of memory is considered "freed".
// Causes an assertion error if there is a byte not marked as "poison".
static void check_free(const char *start, size_t length)
{
	(void)start;
	for (size_t i = 0; i < length; ++i) {
		assert(start[i] == poison);
	}
}


int ring_buffer_init(ring_buffer *rb, size_t size)
{
	rb->buffer = malloc(size);
	if (!rb->buffer) {
		report_error("malloc");
		return -1;
	}

	mark_free(rb->buffer, size);

	rb->size = size;
	rb->head = 0;
	rb->tail = 0;
	rb->full = false;

	validator_init(&rb->validator, "ring_buffer");

	return 0;
}

void ring_buffer_destroy(ring_buffer *rb)
{
	free(rb->buffer);
	validator_destroy(&rb->validator);
}


static size_t __ring_buffer_used(ring_buffer *rb)
{
	size_t used_space;
	if (rb->full) {
		used_space = rb->size;
	} else if (rb->head >= rb->tail) {
		used_space = rb->head - rb->tail;
	} else {
		used_space = rb->size + rb->head - rb->tail;
	}

	return used_space;
}

static size_t __ring_buffer_free(ring_buffer *rb)
{
	return rb->size - __ring_buffer_used(rb);
}


size_t ring_buffer_used(ring_buffer *rb)
{
	validator_enter(&rb->validator, RING_BUFFER_VALIDATOR_DELAY);
	size_t used_space = __ring_buffer_used(rb);
	validator_exit(&rb->validator);
	return used_space;
}

size_t ring_buffer_free(ring_buffer *rb)
{
	validator_enter(&rb->validator, RING_BUFFER_VALIDATOR_DELAY);
	size_t size = __ring_buffer_free(rb);
	validator_exit(&rb->validator);
	return size;
}


// If remove is false, data stays in the buffer
static bool ring_buffer_read_common(ring_buffer *rb, void *buffer,
                                    size_t length, bool remove)
{
	assert(length != 0);
	if (length > __ring_buffer_used(rb)) {
		return false;
	}

	if (rb->tail + length <= rb->size) {
		memcpy(buffer, rb->buffer + rb->tail, length);
		if (remove) {
			mark_free(rb->buffer + rb->tail, length);
			rb->tail += length;
			// Read right up to the end, wrap around
			if (rb->tail == rb->size) {
				rb->tail = 0;
			}
		}

	} else {
		// Copy split into two parts

		size_t copy_len = rb->size - rb->tail;
		memcpy(buffer, rb->buffer + rb->tail, copy_len);
		if (remove) {
			mark_free(rb->buffer + rb->tail, copy_len);
			rb->tail = 0;
		}

		size_t next_copy_len = length - copy_len;
		memcpy(buffer + copy_len, rb->buffer, next_copy_len);
		if (remove) {
			mark_free(rb->buffer + rb->tail, next_copy_len);
			rb->tail = next_copy_len;
		}
	}

	// We know we removed something, so it can't be full
	if (remove) {
		rb->full = false;
	}
	return true;
}


bool ring_buffer_read(ring_buffer *rb, void *buffer, size_t length)
{
	validator_enter(&rb->validator, RING_BUFFER_VALIDATOR_DELAY);
	size_t result = ring_buffer_read_common(rb, buffer, length, true);
	validator_exit(&rb->validator);

	return result;
}

bool ring_buffer_peek(ring_buffer *rb, void *buffer, size_t length)
{
	validator_enter(&rb->validator, RING_BUFFER_VALIDATOR_DELAY);
	size_t result = ring_buffer_read_common(rb, buffer, length, false);
	validator_exit(&rb->validator);

	return result;
}


bool ring_buffer_write(ring_buffer *rb, const void *buffer, size_t length)
{
	validator_enter(&rb->validator, RING_BUFFER_VALIDATOR_DELAY);

	assert(length != 0);
	if (length > __ring_buffer_free(rb)) {
		validator_exit(&rb->validator);
		return false;
	}

	if (rb->head + length <= rb->size) {
		check_free(rb->buffer + rb->head, length);
		memcpy(rb->buffer + rb->head, buffer, length);
		rb->head += length;

		// Filled right up to the end, wrap around
		if (rb->head == rb->size) {
			rb->head = 0;
		}

	} else {
		// Copy split into two parts

		size_t copy_len = rb->size - rb->head;
		check_free(rb->buffer + rb->head, copy_len);
		memcpy(rb->buffer + rb->head, buffer, copy_len);
		rb->head = 0;

		size_t next_copy_len = length - copy_len;
		check_free(rb->buffer + rb->head, next_copy_len);
		memcpy(rb->buffer, buffer + copy_len, next_copy_len);
		rb->head = next_copy_len;
	}

	// We know we inserted something, so if head == tail, it must be full
	if (rb->head == rb->tail) {
		rb->full = true;
	}

	validator_exit(&rb->validator);
	return true;
}
