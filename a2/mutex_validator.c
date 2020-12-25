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
 * CSC369 Assignment 2 - Mutex Validator implementation.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#include <assert.h>
#include <errno.h>
#include <time.h>

#include "errors.h"
#include "mutex_validator.h"


#ifndef NDEBUG

//NOTE: The implementation assumes that pthread_t is a scalar integral type,
// which is true at least for gcc on Linux, but this assumption is not portable.

void validator_init(mutex_validator *validator, const char *name)
{
	assert(name);
	validator->name = name;
	validator->owner = 0;
}

void validator_destroy(mutex_validator *validator)
{
	// Should not be occupied: all threads must finish their operations with the
	// data structure by the time it is destroyed
	assert(!validator->owner);
}

void validator_enter(mutex_validator *validator, unsigned long delay)
{
	// Use atomic CAS to check that the validator is not already occupied and to
	// store the current pthread ID as the new owner
	pthread_t self = pthread_self();
	pthread_t owner = __sync_val_compare_and_swap(&validator->owner, 0, self);
	if (owner != 0) {
		errno = EPERM;
		//NOTE: violations can be reported twice: here and in validator_exit()
		report_error("validator_enter: %lx failed to enter validator %s already"
		             " occupied by %lx", (unsigned long)self, validator->name,
		             (unsigned long)owner);
	}

	// Add a delay to increase the chance of triggering a violation
	nap(delay);
}

void validator_exit(mutex_validator *validator)
{
	// Use atomic CAS to check if the validator is not already occupied and to
	// store the current pthread ID as the new owner
	pthread_t self = pthread_self();
	pthread_t owner = __sync_val_compare_and_swap(&validator->owner, self, 0);
	if (owner != self) {
		errno = EPERM;
		//NOTE: violations can be reported twice: here and in validator_enter()
		report_error("validator_exit: %lx failed to exit validator %s actually"
		             " occupied by %lx", (unsigned long)self, validator->name,
		             (unsigned long)owner);
	}
}

#endif// NDEBUG


void nap(unsigned long nanoseconds)
{
	if (!nanoseconds) {
		return;
	}

	// How long we want to sleep
	struct timespec required;
	required.tv_sec = 0;
	required.tv_nsec = nanoseconds;
	// Set to the time remaining if sleep was interrupted
	struct timespec remainder;

	// Keep sleeping for remaining time if interrupted
	int result;
	do {
		result = nanosleep(&required, &remainder);
		required = remainder;
	} while (result == -1 && errno == EINTR);

	if (result != 0) {
		report_error("nanosleep");
	}
}
