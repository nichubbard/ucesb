/* This file is part of DRASI - a data acquisition data pump.
 *
 * Copyright (C) 2017  Haakan T. Johansson  <f96hajo@chalmers.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef __PD_LINKED_LIST_H__
#define __PD_LINKED_LIST_H__

#include <stddef.h>

/* Many objects in the program must be kept track of.  And each object
 * at the same time often need to be part of several lists, depending
 * on it's current status.
 *
 * For simplicity all lists are double linked, to allow for simple
 * insertation and removal.  Also, a sentinel is used, as the list
 * head.
 *
 * In many cases, each item can only be part of one list of a specific
 * type/purpose at a time.  Then, it would be useful to make the list
 * entry part of the item itself.  Thus, no memory must be allocated
 * for a list item at insertation, and cache trashing is also reduced
 * a bit.
 *
 * Writing generic lists in C has the drawback (versus C++) of having
 * no type checking available (as would have been possible with
 * templates).  By some dirty tricks, type checking is still possible,
 * by using pointers.
 */

struct pd_ll_item_t;

typedef struct pd_ll_item_t
{
  struct pd_ll_item_t *_next;
  struct pd_ll_item_t *_prev;
} pd_ll_item;

#define PD_LL_SENTINEL(name) pd_ll_item name = { &name, &name }
#define PD_LL_SENTINEL_ITEM(name) pd_ll_item name

#define PD_LL_INIT(iter) ((iter)->_next = (iter)->_prev = (iter))

#define PD_LL_IS_EMPTY(iter) ((iter) == (iter)->_next)

#define PD_LL_FIRST(sentinel) ((sentinel)._next)

#define PD_LL_LAST(sentinel) ((sentinel)._prev)

#define PD_LL_NEXT(sentinel,iter) ((iter)->_next != (sentinel) ?	\
				   (iter)->_next : NULL)

#define PD_LL_FOREACH(sentinel,iter)   \
  for ((iter) = (sentinel)._next;      \
       (iter) != &(sentinel);          \
       (iter) = (iter)->_next)

#define PD_LL_FOREACH_TMP(sentinel,iter,tmp_iter)             \
  for ((iter) = (sentinel)._next, (tmp_iter) = (iter)->_next; \
       (iter) != &(sentinel);                                 \
       (iter) = (tmp_iter), (tmp_iter) = (tmp_iter)->_next)

/* Add at end of list if after is the sentinel. */
#define PD_LL_ADD_BEFORE(after,iter) \
  do {                               \
    (after)->_prev->_next = (iter);  \
    (iter)->_prev = (after)->_prev;  \
    (iter)->_next = (after);         \
    (after)->_prev = (iter);         \
  } while (0)

/* Add at beginning of list if before is the sentinel. */
#define PD_LL_ADD_AFTER(before,iter) \
  do {                               \
    (before)->_next->_prev = (iter); \
    (iter)->_next = (before)->_next; \
    (iter)->_prev = (before);        \
    (before)->_next = (iter);        \
  } while (0)

/* Add at beginning of list if before is the sentinel. */
#define PD_LL_REPLACE(before,after)	\
  do {					\
    (after)->_prev = (before)->_prev;	\
    (after)->_next = (before)->_next;	\
    (after)->_prev->_next = (after);	\
    (after)->_next->_prev = (after);	\
  } while (0)

/* Careful!  At most either first or second should have a sentinel. */
#define PD_LL_JOIN(first,second)              \
  do {                                        \
    pd_ll_item *__tmp = (first)->_prev;       \
    (first)->_prev->_next  = (second);        \
    (second)->_prev->_next = (first);         \
    (first)->_prev         = (second)->_prev; \
    (second)->_prev        = __tmp;           \
  } while (0)

#define PD_LL_REMOVE(iter)                \
  do {                                    \
    (iter)->_next->_prev = (iter)->_prev; \
    (iter)->_prev->_next = (iter)->_next; \
    PD_LL_INIT(iter);                     \
  } while (0)

#define PD_LL_ITEM(iter,type,listname) \
  ((type*) (((char*) (iter)) - offsetof(type,listname)))

#endif/*__PD_LINKED_LIST_H__*/
