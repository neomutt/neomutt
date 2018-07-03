/**
 * @file
 * Create/manipulate threading in emails
 *
 * @authors
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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

/**
 * @page thread Create/manipulate threading in emails
 *
 * Create/manipulate threading in emails
 */

#include "config.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "header.h"

/**
 * is_descendant - Is one thread a descendant of another
 * @param a Parent thread
 * @param b Child thread
 * @retval 1 If b is a descendent of a (child, grandchild, etc)
 */
int is_descendant(struct MuttThread *a, struct MuttThread *b)
{
  while (a)
  {
    if (a == b)
      return 1;
    a = a->parent;
  }
  return 0;
}

/**
 * unlink_message - Break the message out of the thread
 * @param[in,out] old Root of thread
 * @param[in]     cur Child thread to separate
 *
 * Remove cur and its descendants from their current location.  Also make sure
 * ancestors of cur no longer are sorted by the fact that cur is their
 * descendant.
 */
void unlink_message(struct MuttThread **old, struct MuttThread *cur)
{
  struct MuttThread *tmp = NULL;

  if (cur->prev)
    cur->prev->next = cur->next;
  else
    *old = cur->next;

  if (cur->next)
    cur->next->prev = cur->prev;

  if (cur->sort_key)
  {
    for (tmp = cur->parent; tmp && tmp->sort_key == cur->sort_key; tmp = tmp->parent)
      tmp->sort_key = NULL;
  }
}

/**
 * insert_message - Insert a message into a thread
 * @param[in,out] new       New thread to add
 * @param[in]     newparent Parent of new thread
 * @param[in]     cur       Current thread to add after
 *
 * add cur as a prior sibling of *new, with parent newparent
 */
void insert_message(struct MuttThread **new, struct MuttThread *newparent,
                    struct MuttThread *cur)
{
  if (*new)
    (*new)->prev = cur;

  cur->parent = newparent;
  cur->next = *new;
  cur->prev = NULL;
  *new = cur;
}

/**
 * thread_hash_destructor - Hash Destructor callback
 * @param type Hash Type
 * @param obj  Object to free
 * @param data Data associated with the Hash
 */
void thread_hash_destructor(int type, void *obj, intptr_t data)
{
  FREE(&obj);
}

/**
 * find_virtual - Find an email with a Virtual message number
 * @param cur     Thread to search
 * @param reverse If true, reverse the direction of the search
 * @retval ptr Matching Header
 */
struct Header *find_virtual(struct MuttThread *cur, int reverse)
{
  struct MuttThread *top = NULL;

  if (cur->message && cur->message->virtual >= 0)
    return cur->message;

  top = cur;
  cur = cur->child;
  if (!cur)
    return NULL;

  while (reverse && cur->next)
    cur = cur->next;

  while (true)
  {
    if (cur->message && cur->message->virtual >= 0)
      return cur->message;

    if (cur->child)
    {
      cur = cur->child;

      while (reverse && cur->next)
        cur = cur->next;
    }
    else if (reverse ? cur->prev : cur->next)
      cur = reverse ? cur->prev : cur->next;
    else
    {
      while (!(reverse ? cur->prev : cur->next))
      {
        cur = cur->parent;
        if (cur == top)
          return NULL;
      }
      cur = reverse ? cur->prev : cur->next;
    }
    /* not reached */
  }
}

/**
 * clean_references - Update email references for a broken Thread
 * @param brk Broken thread
 * @param cur Current thread
 */
void clean_references(struct MuttThread *brk, struct MuttThread *cur)
{
  struct ListNode *ref = NULL;
  bool done = false;

  for (; cur; cur = cur->next, done = false)
  {
    /* parse subthread recursively */
    clean_references(brk, cur->child);

    if (!cur->message)
      break; /* skip pseudo-message */

    /* Looking for the first bad reference according to the new threading.
     * Optimal since NeoMutt stores the references in reverse order, and the
     * first loop should match immediately for mails respecting RFC2822. */
    for (struct MuttThread *p = brk; !done && p; p = p->parent)
    {
      for (ref = STAILQ_FIRST(&cur->message->env->references);
           p->message && ref; ref = STAILQ_NEXT(ref, entries))
      {
        if (mutt_str_strcasecmp(ref->data, p->message->env->message_id) == 0)
        {
          done = true;
          break;
        }
      }
    }

    if (done)
    {
      struct Header *h = cur->message;

      /* clearing the References: header from obsolete Message-ID(s) */
      struct ListNode *np;
      while ((np = STAILQ_NEXT(ref, entries)) != NULL)
      {
        STAILQ_REMOVE_AFTER(&cur->message->env->references, ref, entries);
        FREE(&np->data);
        FREE(&np);
      }

      h->env->refs_changed = h->changed = true;
    }
  }
}

/**
 * mutt_break_thread - Break the email Thread
 * @param hdr Email Header to break at
 */
void mutt_break_thread(struct Header *hdr)
{
  mutt_list_free(&hdr->env->in_reply_to);
  mutt_list_free(&hdr->env->references);
  hdr->env->irt_changed = hdr->env->refs_changed = hdr->changed = true;

  clean_references(hdr->thread, hdr->thread->child);
}
