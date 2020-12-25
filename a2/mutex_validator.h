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
 * CSC369 Assignment 2 - Mutex Validator header file.
 *
 * This is a utility that can be used to test whether something is actually
 * being accessed exclusively by one thread at a time.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#pragma once

#include <stdbool.h>

#include "sync.h"


/**
 * Structure used in the mutually-exclusive access validation system.
 *
 * When you want to validate that only one thread at a time can actually access
 * a data structure, you can add a validator. A validator struct should be added
 * inside the data structure and initialized before use. Whenever a thread
 * performs an operation on the data structure that should be done mutually
 * exclusively, the thread should first enter. When done, it should exit. A
 * violation happens if two or more threads enter before one of them exits.
 */
typedef struct mutex_validator {
	/** Name of the data structure this validator is associated with. */
	const char *name;

	/**
	 * The pthread ID of the current owner thread, or 0 if the validator is
	 * currently not occupied by any thread.
	 *
	 * When a thread enters the validator, it stores its pthread ID in this
	 * field, and resets it to 0 when it exits the validator. If another thread
	 * has already entered the validator, the field retains its value, and the
	 * error message includes the pthread IDs of both threads. The pthread ID
	 * is returned by the pthread_self() function, and can also be seen in gdb,
	 * e.g. 0x7ffff7fd9740 in this example output:
	 * 	(gdb) info threads
	 * 	  Id   Target Id         Frame
	 * 	* 1    Thread 0x7ffff7fd9740 (LWP 30490) "a.out" main () at test.c:14
	 */
	pthread_t owner;

} mutex_validator;


#ifndef NDEBUG

/**
 * Add a mutex validator field to a structure.
 *
 * Use this macro to avoid adding the field in non-debug mode (when NDEBUG is
 * defined), since it won't be used. Example:
 * 	struct some_data_structure {
 * 		...
 * 		DEFINE_VALIDATOR(validator);
 * 	};
 */
#define DEFINE_VALIDATOR(name) mutex_validator name

/**
 * Initialize a mutex validator.
 *
 * @param validator  pointer to the validator.
 * @param name       name of the corresponding data structure.
 */
void validator_init(mutex_validator *validator, const char *name);

/** Destroy a mutex validator. */
void validator_destroy(mutex_validator *validator);

/**
 * Enter a mutex validator.
 *
 * @param validator  pointer to the validator.
 * @param delay      how long in nanoseconds to sleep immediately after
 *                   entering. Adding a delay period stretches out the critical
 *                   section, increasing the chance that a violation will occur
 *                   (in a buggy implementation).
 */
void validator_enter(mutex_validator *validator, unsigned long delay);

/** Exit a mutex validator. */
void validator_exit(mutex_validator *validator);

#else// NDEBUG

#define DEFINE_VALIDATOR(x)
#define validator_init(x, n)
#define validator_destroy(x)
#define validator_enter(x, d)
#define validator_exit(x)

#endif// NDEBUG


/** Sleep for the given number of nanoseconds. */
void nap(unsigned long nanoseconds);
