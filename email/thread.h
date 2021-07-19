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

#ifndef MUTT_EMAIL_THREAD_H
#define MUTT_EMAIL_THREAD_H

#include <stdbool.h>
#include <stdint.h>

struct Email;

/**
 * struct MuttThread - An Email conversation
 */
struct MuttThread
{
  bool fake_thread             : 1; ///< Emails grouped by Subject
  bool duplicate_thread        : 1; ///< Duplicated Email in Thread
  bool sort_children           : 1; ///< Sort the children
  bool check_subject           : 1; ///< Should the Subject be checked?
  bool visible                 : 1; ///< Is this Thread visible?
  bool deep                    : 1; ///< Is the Thread deeply nested?
  unsigned int subtree_visible : 2; ///< Is this Thread subtree visible?
  bool next_subtree_visible    : 1; ///< Is the next Thread subtree visible?

  struct MuttThread *parent;        ///< Parent of this Thread
  struct MuttThread *child;         ///< Child of this Thread
  struct MuttThread *next;          ///< Next sibling Thread
  struct MuttThread *prev;          ///< Previous sibling Thread
  struct Email *message;            ///< Email this Thread refers to
  struct Email *sort_thread_key;    ///< Email that controls how top thread sorts
  struct Email *sort_aux_key;       ///< Email that controls how subthread siblings sort
};

void          clean_references      (struct MuttThread *brk, struct MuttThread *cur);
struct Email *find_virtual          (struct MuttThread *cur, bool reverse);
void          insert_message        (struct MuttThread **add, struct MuttThread *parent, struct MuttThread *cur);
bool          is_descendant         (struct MuttThread *a, struct MuttThread *b);
void          mutt_break_thread     (struct Email *e);
void          thread_hash_destructor(int type, void *obj, intptr_t data);
void          unlink_message        (struct MuttThread **old, struct MuttThread *cur);

#endif /* MUTT_EMAIL_THREAD_H */
