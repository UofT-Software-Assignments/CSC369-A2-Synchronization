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
 * Copyright (c) 2019 Karen Reid
 */

/**
 * CSC369 Assignment 2 - Linked list header file.
 *
 * Circular doubly-linked list implementation. Adapted from the Linux kernel.
 *
 * NOTE: This file will be replaced when we run your code.
 * DO NOT make any changes here.
 */

#pragma once

#include <stddef.h>

#include "mutex_validator.h"


// List mutex access validator delay in nanoseconds.
#ifndef LIST_DELAY
#define LIST_DELAY 1000
#endif


//NOTE: This linked list implementation is different from what you might be used
// to. To make a linked list of items of type struct S, instead of storing a
// struct S* pointer in the list entry struct, we embed a struct with the next
// and prev pointers in struct S (as one of its fields).
//
// Providing and managing storage for list entries is the responsibility of the
// user of this API. Keep in mind that an entry allocated on the stack (a local
// variable in a function) becomes invalid when the function returns.


/**
 * A node in a doubly-linked list.
 *
 * Should be a member of the struct that is linked into a list.
 */
typedef struct list_entry {
	struct list_entry *next;
	struct list_entry *prev;
} list_entry;

/**
 * Cast a member of a structure out to the containing structure.
 *
 * Useful for getting a pointer to the structure containing an embedded list
 * entry given a pointer to the embedded list entry.
 *
 * @param   ptr pointer to the member.
 * @param   type type of the container struct this is embedded in.
 * @param   member name of the member within the struct.
 * @return  pointer to the containing struct.
 */
#define container_of(ptr, type, member) ({            \
	const typeof(((type*)0)->member) *__mptr = (ptr); \
	(type*)((char*)__mptr - offsetof(type, member));  \
})

/** Doubly-linked list head. */
typedef struct list_head {
	struct list_entry head;

	/**
	 * Keeps track of threads that access the list, ensuring that they don't
	 * violate critical sections.
	 */
	DEFINE_VALIDATOR(validator);

} list_head;


/** Initialize a list head. */
static inline void list_init(list_head *list)
{
	list_entry *head = &list->head;
	head->next = head;
	head->prev = head;
	validator_init(&list->validator, "list_head");
}

/** Destroy a list head. */
static inline void list_destroy(list_head *list)
{
	validator_destroy(&list->validator);
	(void)list;
}

/**
 * Initialize a list entry into the unlinked state.
 *
 * NULL value in the next and prev fields means that the entry is not linked
 * into any list.
 *
 * @param entry  pointer to the list entry.
 */
static inline void list_entry_init(list_entry *entry)
{
	entry->next = NULL;
	entry->prev = NULL;
}

/** Determine if a list entry is linked into any list. */
static inline bool list_entry_is_linked(list_entry *entry)
{
	return entry->next != NULL;
}

/** Insert a new list entry between two known consecutive entries. */
static inline void __list_insert(list_head *list, list_entry *entry,
                                 list_entry *prev, list_entry *next)
{
	(void)list;

	validator_enter(&list->validator, LIST_DELAY);
	next->prev = entry;
	entry->next = next;
	entry->prev = prev;
	prev->next = entry;
	validator_exit(&list->validator);
}

/**
 * Add a new entry at the head of the list.
 *
 * Insert a new entry after the specified head. Useful for implementing stacks.
 *
 * @param list   pointer to the list head.
 * @param entry  pointer to the new entry to be added.
 */
static inline void list_add_head(list_head *list, list_entry *entry)
{
	__list_insert(list, entry, &list->head, list->head.next);
}

/**
 * Add a new entry at the tail of the list.
 *
 * Insert a new entry before the specified head. Useful for implementing queues.
 *
 * @param list   pointer to the list head.
 * @param entry  pointer to the new entry to be added.
 */
static inline void list_add_tail(list_head *list, list_entry *entry)
{
	__list_insert(list, entry, list->head.prev, &list->head);
}

/**
 * Delete an entry from a list.
 *
 * @param list   pointer to the list head.
 * @param entry  pointer to the entry to delete from the list.
 */
static inline void list_del(list_head *list, list_entry *entry)
{
	(void)list;

	validator_enter(&list->validator, LIST_DELAY);
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	validator_exit(&list->validator);

	list_entry_init(entry);
}

/**
 * Helper for list_for_each below.
 *
 * This ensures that the mutex validator enters. We make this into a function to
 * avoid errors and warnings when compiling in NDEBUG mode.
 */
static inline void __list_for_each_init(list_head *list)
{
	(void)list;
	validator_enter(&(list)->validator, LIST_DELAY);
}

/**
 * Helper for list_for_each below.
 *
 * This ensures that the mutex validator exits when done iterating.
 */
static inline bool __list_for_each_cond(list_head *list, list_entry *entry)
{
	bool ended = entry == &list->head;
	if (ended) {
		validator_exit(&list->validator);
	}
	return !ended;
}

/**
 * Iterate over a list.
 *
 * NOTE: Do not attempt to modify the structure of the list while iterating
 * through it with this macro (you can still modify other fields of the struct
 * containing the list entry). This will likely trigger a mutex access validator
 * violation. Even if the validator is disabled, if you delete the current list
 * entry from the list, pos->next will be invalidated, leading to a segmentation
 * fault in the next iteration.
 *
 * @param pos   struct list_entry pointer to use as a loop cursor.
 * @param list  pointer to the list.
 */
#define list_for_each(pos, list)                              \
	for (__list_for_each_init(list), pos = (list)->head.next; \
	     __list_for_each_cond((list), pos);                   \
	     pos = pos->next)
