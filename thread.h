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

struct Context;
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

int _mutt_aside_thread(struct Header *hdr, short dir, short subthreads);
#define mutt_next_thread(x)        _mutt_aside_thread(x, 1, 0)
#define mutt_previous_thread(x)    _mutt_aside_thread(x, 0, 0)
#define mutt_next_subthread(x)     _mutt_aside_thread(x, 1, 1)
#define mutt_previous_subthread(x) _mutt_aside_thread(x, 0, 1)

int _mutt_traverse_thread(struct Context *ctx, struct Header *cur, int flag);
#define mutt_collapse_thread(x, y)         _mutt_traverse_thread(x, y, MUTT_THREAD_COLLAPSE)
#define mutt_uncollapse_thread(x, y)       _mutt_traverse_thread(x, y, MUTT_THREAD_UNCOLLAPSE)
#define mutt_get_hidden(x, y)              _mutt_traverse_thread(x, y, MUTT_THREAD_GET_HIDDEN)
#define mutt_thread_contains_unread(x, y)  _mutt_traverse_thread(x, y, MUTT_THREAD_UNREAD)
#define mutt_thread_contains_flagged(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_FLAGGED)
#define mutt_thread_next_unread(x, y)      _mutt_traverse_thread(x, y, MUTT_THREAD_NEXT_UNREAD)

void mutt_break_thread(struct Header *hdr);
int mutt_link_threads(struct Header *cur, struct Header *last, struct Context *ctx);
int mutt_messages_in_thread(struct Context *ctx, struct Header *hdr, int flag);
void mutt_draw_tree(struct Context *ctx);

void mutt_clear_threads(struct Context *ctx);
struct MuttThread *mutt_sort_subthreads(struct MuttThread *thread, int init);
void mutt_sort_threads(struct Context *ctx, int init);
int mutt_parent_message(struct Context *ctx, struct Header *hdr, int find_root);
void mutt_set_virtual(struct Context *ctx);
struct Hash *mutt_make_id_hash(struct Context *ctx);

#endif /* _MUTT_THREAD_H */
