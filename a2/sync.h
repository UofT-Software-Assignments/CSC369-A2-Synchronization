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
 * CSC369 Assignment 2 - Synchronization primitives header file.
 *
 * NOTE: This file will be replaced when we run your code.
 *       DO NOT make any changes here.
 */

#pragma once

#include <pthread.h>


/** Mutex used in this assignment. Wraps pthread_mutex_t. */
typedef struct mutex {
	/** Underlying mutex. You are not allowed to access this directly. */
	pthread_mutex_t mutex;
} mutex_t;

/** Initialize a mutex. Does error checking. */
void mutex_init(mutex_t *mutex);

/** Destroy a mutex. Does error checking. */
void mutex_destroy(mutex_t *mutex);

/** Lock a mutex. Does error checking. */
void mutex_lock(mutex_t *mutex);

/** Unlock a mutex. Does error checking. */
void mutex_unlock(mutex_t *mutex);


/** Condition variable used in this assignment. Wraps pthread_cond_t. */
typedef struct cond {
	/** Underlying condition variable. You are not allowed to access this directly. */
	pthread_cond_t cond;
} cond_t;

/** Initialize a condition variable. Does error checking. */
void cond_init(cond_t *cond);

/** Destroy a condition variable. Does error checking. */
void cond_destroy(cond_t *cond);

/**
 * Block until a condition variable is signaled.
 *
 * The mutex that protects the condition variable must be already locked by the
 * calling thread.
 *
 * @param cond   pointer to the condition variable.
 * @param mutex  pointer to the mutex.
 */
void cond_wait(cond_t *cond, mutex_t *mutex);

/**
 * Signal a condition variable, waking up one thread that is blocked on it.
 *
 * The mutex that protects the condition variable must be already locked by the
 * calling thread.
 *
 * @param cond  pointer to the condition variable.
 */
void cond_signal(cond_t *cond);

/**
 * Broadcast a condition variable, waking up all threads that are blocked on it.
 *
 * The mutex that protects the condition variable must be already locked by the
 * calling thread.
 *
 * @param cond  pointer to the condition variable.
 */
void cond_broadcast(cond_t *cond);
