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
 * CSC369 Assignment 2 - Ring buffer header file.
 *
 * The ring buffer is fully implemented and ready to use in your solution.
 *
 * NOTE: This file will be replaced when we run your code.
 *       DO NOT make any changes here.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mutex_validator.h"


/** Ring buffer implementation. */
typedef struct ring_buffer {
	/** Pointer to the underlying buffer. */
	char *buffer;
	/** Buffer capacity in bytes. */
	size_t size;
	/** Index of the byte that marks the head of the buffer. */
	size_t head;
	/** Index of the byte that marks the tail of the buffer. */
	size_t tail;
	/** Disambiguates between the full and empty states when head == tail. */
	bool full;

	/**
	 * Keeps track of threads that access the buffer, ensuring that they don't
	 * violate critical sections.
	 */
	DEFINE_VALIDATOR(validator);

} ring_buffer;


/**
 * Initialize a ring buffer.
 *
 * Allocates its own memory. Can fail with ENOMEM.
 *
 * @param rb    pointer to the ring buffer.
 * @param size  buffer capacity in bytes.
 * @return      0 on success, -1 on failure (with errno set).
 */
int ring_buffer_init(ring_buffer *rb, size_t size);

/**
 * Destroy a ring buffer.
 *
 * @param rb  pointer to the ring buffer.
 */
void ring_buffer_destroy(ring_buffer *rb);


/**
 * Get the amount of free space in a ring buffer.
 *
 * @param rb  pointer to the ring buffer.
 * @return    amount of free space in bytes.
 */
size_t ring_buffer_free(ring_buffer *rb);

/**
 * Get the amount of used space in a ring buffer.
 *
 * @param rb  pointer to the ring buffer.
 * @return    amount of used space in bytes.
 */
size_t ring_buffer_used(ring_buffer *rb);


/**
 * Retrieve and remove data from the tail of a ring buffer.
 *
 * @param rb      pointer to the ring buffer.
 * @param buffer  pointer to the buffer to copy data into.
 * @param length  number of bytes to read and remove.
 * @return        true on success, false if there was not enough data.
 */
bool ring_buffer_read(ring_buffer *rb, void *buffer, size_t length);

/**
 * Retrieve data from the tail of the ring buffer without removing it.
 *
 * @param rb      pointer to the ring buffer.
 * @param buffer  pointer to the buffer to copy data into.
 * @param length   number of bytes to read.
 * @return        true on success, false if there was not enough data.
 */
bool ring_buffer_peek(ring_buffer *rb, void *buffer, size_t length);

/**
 * Insert data into the head of a ring buffer.
 *
 * @param rb      pointer to the ring buffer.
 * @param buffer  pointer to the buffer to copy data from.
 * @param length  number of bytes to insert.
 * @return        true on success, false if there was not enough free space.
 */
bool ring_buffer_write(ring_buffer *rb, const void *buffer, size_t length);
