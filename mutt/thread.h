/**
 * @file
 * Create/manipulate threading in emails
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_THREAD_H
#define _MUTT_THREAD_H

#include <stdbool.h>
#include <stdint.h>

struct Header;

/**
 * struct MuttThread - An email conversation
 */
struct MuttThread
{
  bool fake_thread : 1;
  bool duplicate_thread : 1;
  bool sort_children : 1;
  bool check_subject : 1;
  bool visible : 1;
  bool deep : 1;
  unsigned int subtree_visible : 2;
  bool next_subtree_visible : 1;
  struct MuttThread *parent;
  struct MuttThread *child;
  struct MuttThread *next;
  struct MuttThread *prev;
  struct Header *message;
  struct Header *sort_key;
};

void           clean_references(struct MuttThread *brk, struct MuttThread *cur);
struct Header *find_virtual(struct MuttThread *cur, int reverse);
void           insert_message(struct MuttThread **new, struct MuttThread *newparent, struct MuttThread *cur);
int            is_descendant(struct MuttThread *a, struct MuttThread *b);
void           mutt_break_thread(struct Header *hdr);
void           thread_hash_destructor(int type, void *obj, intptr_t data);
void           unlink_message(struct MuttThread **old, struct MuttThread *cur);

#endif /* _MUTT_THREAD_H */
