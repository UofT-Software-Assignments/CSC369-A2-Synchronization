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
 * CSC369 Assignment 2 - Error handling utilities header file.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#pragma once

#include <stdbool.h>


#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

/** Get current file and line as a string literal. */
#define CODE_LOCATION __FILE__ ":" STRINGIZE(__LINE__)


//NOTE: If compiled in debug mode (default), you should report errors to stderr.
// If compiled in non-debug mode (NDEBUG preprocessor macro is defined), this
// output should be disabled. In this case, we instead rely only on errno to
// report the error. This is what is normally expected from a system library -
// reporting errors is the responsibility of the program using the library. You
// should use the macros defined below for any error output.

/**
 * Print debugging information when an error is detected.
 *
 * Assumes that the current value of errno describes the error being reported.
 * This macro is basically an enhanced version of perror() and should be used in
 * a similar fashion (see "man 3 perror"). The message is suffixed with the code
 * location (file name and line number) where the macro is invoked. Supports
 * printf-style formatting. NOTE: this macro is only active in debug build (if
 * NDEBUG is not defined).
 *
 * @param format  message format string. Should include the name of the function
 *                that incurred the error (either current function or a failed
 *                syscall or a library function) and (optionally) any additional
 *                information about the error.
 * @param ...     arguments for the format string, if any.
 *
 * Example:
 * 	int func(int arg)
 * 	{
 * 		if (arg_is_invalid) {
 * 			errno = EINVAL;
 * 			report_error("func: invalid arg = %d", arg);
 * 			return -1;
 * 		}
 *
 * 		...
 *
 * 		if (some_syscall(...) < 0) {
 * 			report_error("some_syscall");
 * 			return -1;
 * 		}
 *
 * 		...
 *
 * 	}
 */
#ifndef NDEBUG
#define report_error(...) __report_error(CODE_LOCATION, false, __VA_ARGS__)
#else
#define report_error(...)
#endif

/**
 * Print debugging info for an error that could happen during normal operation.
 *
 * Examples include a non-blocking read or write call failing with EAGAIN, a
 * read call failing with EMSGSIZE when the message is larger than the buffer,
 * or a write call failing with EPIPE when there are no readers. NOTE: this
 * macro is only active if DEBUG_VERBOSE is defined and NDEBUG is not defined.
 *
 * @param format  message format string. See report_error() for details.
 * @param ...     arguments for the format string, if any.
 */
#if !defined(NDEBUG) && defined(DEBUG_VERBOSE)
#define report_info(...) __report_error(CODE_LOCATION, true, __VA_ARGS__)
#else
#define report_info(...)
#endif


/** Underlying implementation of error reporting macros. */
void __report_error(const char *location, bool info, const char *format, ...);
