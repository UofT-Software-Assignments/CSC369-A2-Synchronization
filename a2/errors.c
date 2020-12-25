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
 * CSC369 Assignment 2 - Errors handling utilities implementation.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "errors.h"


/** Underlying implementation of error reporting macros. */
void __report_error(const char *location, bool info, const char *format, ...)
{
	// Preserve errno value that can be changed by vsnprintf() and fprintf()
	int e = errno;

	// Format the message into a temporary buffer
	//NOTE: messages longer than BUFFER_SIZE will be truncated
	#define BUFFER_SIZE 1024
	char msg[BUFFER_SIZE];
	va_list va_args;
	va_start(va_args, format);
	vsnprintf(msg, sizeof(msg), format, va_args);
	va_end(va_args);

	// Print the whole message to stderr
	fprintf(stderr, "%s[ERROR %d]: %s @ %s: %s\n",
	        info ? "[INFO] " : "", e, msg, location, strerror(e));

	// Restore the saved errno value
	errno = e;
}
