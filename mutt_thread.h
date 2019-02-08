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

#ifndef MUTT_MUTT_THREAD_H
#define MUTT_MUTT_THREAD_H

#include <stdbool.h>

struct Context;
struct Email;
struct Mailbox;
struct MuttThread;

/* These Config Variables are only used in mutt_thread.c */
extern bool DuplicateThreads;
extern bool HideLimited;
extern bool HideMissing;
extern bool HideThreadSubject;
extern bool HideTopLimited;
extern bool HideTopMissing;
extern bool NarrowTree;
extern bool SortRe;
extern bool StrictThreads;
extern bool ThreadReceived;

#define MUTT_THREAD_COLLAPSE    (1 << 0)
#define MUTT_THREAD_UNCOLLAPSE  (1 << 1)
#define MUTT_THREAD_GET_HIDDEN  (1 << 2)
#define MUTT_THREAD_UNREAD      (1 << 3)
#define MUTT_THREAD_NEXT_UNREAD (1 << 4)
#define MUTT_THREAD_FLAGGED     (1 << 5)

int mutt_aside_thread(struct Email *e, bool forwards, bool subthreads);
#define mutt_next_thread(x)        mutt_aside_thread(x, true,  false)
#define mutt_previous_thread(x)    mutt_aside_thread(x, false, false)
#define mutt_next_subthread(x)     mutt_aside_thread(x, true,  true)
#define mutt_previous_subthread(x) mutt_aside_thread(x, false, true)

int mutt_traverse_thread(struct Context *ctx, struct Email *cur, int flag);
#define mutt_collapse_thread(x, y)         mutt_traverse_thread(x, y, MUTT_THREAD_COLLAPSE)
#define mutt_uncollapse_thread(x, y)       mutt_traverse_thread(x, y, MUTT_THREAD_UNCOLLAPSE)
#define mutt_get_hidden(x, y)              mutt_traverse_thread(x, y, MUTT_THREAD_GET_HIDDEN)
#define mutt_thread_contains_unread(x, y)  mutt_traverse_thread(x, y, MUTT_THREAD_UNREAD)
#define mutt_thread_contains_flagged(x, y) mutt_traverse_thread(x, y, MUTT_THREAD_FLAGGED)
#define mutt_thread_next_unread(x, y)      mutt_traverse_thread(x, y, MUTT_THREAD_NEXT_UNREAD)

bool mutt_link_threads(struct Email *parent, struct EmailList *children, struct Mailbox *m);
int mutt_messages_in_thread(struct Mailbox *m, struct Email *e, int flag);
void mutt_draw_tree(struct Context *ctx);

void mutt_clear_threads(struct Context *ctx);
struct MuttThread *mutt_sort_subthreads(struct MuttThread *thread, bool init);
void mutt_sort_threads(struct Context *ctx, bool init);
int mutt_parent_message(struct Context *ctx, struct Email *e, bool find_root);
void mutt_set_virtual(struct Context *ctx);
struct Hash *mutt_make_id_hash(struct Mailbox *m);

#endif /* MUTT_MUTT_THREAD_H */
