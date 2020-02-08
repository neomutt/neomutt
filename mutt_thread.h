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
#include <stdint.h>

struct Context;
struct Email;
struct EmailList;
struct Mailbox;
struct MuttThread;

/* These Config Variables are only used in mutt_thread.c */
extern bool C_DuplicateThreads;
extern bool C_HideLimited;
extern bool C_HideMissing;
extern bool C_HideThreadSubject;
extern bool C_HideTopLimited;
extern bool C_HideTopMissing;
extern bool C_NarrowTree;
extern bool C_SortRe;
extern bool C_StrictThreads;
extern bool C_ThreadReceived;

typedef uint8_t MuttThreadFlags;         ///< Flags, e.g. #MUTT_THREAD_COLLAPSE
#define MUTT_THREAD_NO_FLAGS          0  ///< No flags are set
#define MUTT_THREAD_COLLAPSE    (1 << 0) ///< Collapse an email thread
#define MUTT_THREAD_UNCOLLAPSE  (1 << 1) ///< Uncollapse an email thread
#define MUTT_THREAD_UNREAD      (1 << 2) ///< Count unread emails in a thread
#define MUTT_THREAD_NEXT_UNREAD (1 << 3) ///< Find the next unread email
#define MUTT_THREAD_FLAGGED     (1 << 4) ///< Count flagged emails in a thread

int mutt_traverse_thread(struct Context *ctx, struct Email *e, MuttThreadFlags flag);
#define mutt_collapse_thread(ctx, e)         mutt_traverse_thread(ctx, e, MUTT_THREAD_COLLAPSE)
#define mutt_uncollapse_thread(ctx, e)       mutt_traverse_thread(ctx, e, MUTT_THREAD_UNCOLLAPSE)
#define mutt_thread_contains_unread(ctx, e)  mutt_traverse_thread(ctx, e, MUTT_THREAD_UNREAD)
#define mutt_thread_contains_flagged(ctx, e) mutt_traverse_thread(ctx, e, MUTT_THREAD_FLAGGED)
#define mutt_thread_next_unread(ctx, e)      mutt_traverse_thread(ctx, e, MUTT_THREAD_NEXT_UNREAD)

int mutt_aside_thread(struct Email *e, bool forwards, bool subthreads);
#define mutt_next_thread(e)        mutt_aside_thread(e, true,  false)
#define mutt_previous_thread(e)    mutt_aside_thread(e, false, false)
#define mutt_next_subthread(e)     mutt_aside_thread(e, true,  true)
#define mutt_previous_subthread(e) mutt_aside_thread(e, false, true)

void               mutt_clear_threads     (struct Context *ctx);
void               mutt_draw_tree         (struct Context *ctx);
bool               mutt_link_threads      (struct Email *parent, struct EmailList *children, struct Mailbox *m);
struct HashTable * mutt_make_id_hash      (struct Mailbox *m);
int                mutt_messages_in_thread(struct Mailbox *m, struct Email *e, int flag);
int                mutt_parent_message    (struct Context *ctx, struct Email *e, bool find_root);
void               mutt_set_vnum          (struct Context *ctx);
struct MuttThread *mutt_sort_subthreads   (struct MuttThread *thread, bool init);
void               mutt_sort_threads      (struct Context *ctx, bool init);

#endif /* MUTT_MUTT_THREAD_H */
