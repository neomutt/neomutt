/**
 * @file
 * Singly-linked list type
 *
 * @authors
 * Copyright (C) 2017 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUTT_MUTT_LIST_H
#define MUTT_MUTT_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include "queue.h"

struct Buffer;

/**
 * struct ListNode - A List node for strings
 */
struct ListNode
{
  char *data;                     ///< String
  STAILQ_ENTRY(ListNode) entries; ///< Linked list
};
STAILQ_HEAD(ListHead, ListNode);

/**
 * @defgroup list_free_api List Data Free API
 *
 * Prototype for a function to free List data
 *
 * @param[out] ptr Data to free
 */
typedef void (*list_free_t)(void **ptr);

void             mutt_list_clear       (struct ListHead *h);
void             mutt_list_copy_tail   (struct ListHead *dst, const struct ListHead *src);
bool             mutt_list_equal       (const struct ListHead *ah, const struct ListHead *bh);
struct ListNode *mutt_list_find        (const struct ListHead *h, const char *data);
void             mutt_list_free        (struct ListHead *h);
void             mutt_list_free_type   (struct ListHead *h, list_free_t fn);
struct ListNode *mutt_list_insert_after(struct ListHead *h, struct ListNode *n, char *s);
struct ListNode *mutt_list_insert_head (struct ListHead *h, char *s);
struct ListNode *mutt_list_insert_tail (struct ListHead *h, char *s);
bool             mutt_list_match       (const char *s, struct ListHead *h);
size_t           mutt_list_str_split   (struct ListHead *head, const char *src, char sep);
size_t           mutt_list_write       (const struct ListHead *h, struct Buffer *buf);

#endif /* MUTT_MUTT_LIST_H */
