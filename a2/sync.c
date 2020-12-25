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
 * CSC369 Assignment 2 - Synchronization primitives implementation.
 *
 * NOTE: This file will be replaced when we run your code.
 *       DO NOT make any changes here.
 */

// Errors in all functions in this file are considered fatal. report_error()
// will always print to stderr.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <errno.h>

#include "errors.h"
#include "sync.h"


//NOTE: pthread functions don't set errno, they return the error code instead

void mutex_init(mutex_t *mutex)
{
	int ret = pthread_mutex_init(&mutex->mutex, NULL);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_mutex_init");
	}
}

void mutex_destroy(mutex_t *mutex)
{
	int ret = pthread_mutex_destroy(&mutex->mutex);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_mutex_destroy");
	}
}

void mutex_lock(mutex_t *mutex)
{
	int ret = pthread_mutex_lock(&mutex->mutex);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_mutex_lock");
	}
}

void mutex_unlock(mutex_t *mutex)
{
	int ret = pthread_mutex_unlock(&mutex->mutex);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_mutex_unlock");
	}
}


void cond_init(cond_t *cond)
{
	int ret = pthread_cond_init(&cond->cond, NULL);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_cond_init");
	}
}

void cond_destroy(cond_t *cond)
{
	int ret = pthread_cond_destroy(&cond->cond);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_cond_destroy");
	}
}

// Condition variable spurious wakeup time in nanoseconds (see below).
#ifndef COND_WAKEUP_TIME
#define COND_WAKEUP_TIME 1000
#endif

void cond_wait(cond_t *cond, mutex_t *mutex)
{
	// COND_AUTO_WAKEUP can be enabled to cause threads waiting on condition
	// variables to sporadically and automatically wake up. This is a debugging
	// feature might help in debugging certain synchronization issues, for
	// instance, the situation where the condition signal is "missed" by this
	// thread and it gets stuck in cond_wait() forever.
#ifndef COND_AUTO_WAKEUP
	int ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
#else
	struct timepsec t;
	t.secs = 0;
	t.nsecs = COND_WAKEUP_TIME;
	int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &t)
#endif

	if (ret != 0) {
		errno = ret;
		report_error("pthread_cond_wait");
	}
}

void cond_signal(cond_t *cond)
{
	int ret = pthread_cond_signal(&cond->cond);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_cond_signal");
	}
}

void cond_broadcast(cond_t *cond)
{
	int ret = pthread_cond_broadcast(&cond->cond);
	if (ret != 0) {
		errno = ret;
		report_error("pthread_cond_broadcast");
	}
}
