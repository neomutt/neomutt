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
 * @page mutt_thread Create/manipulate threading in emails
 *
 * Create/manipulate threading in emails
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "mutt_thread.h"
#include "context.h"
#include "mutt_menu.h"
#include "mx.h"
#include "protos.h"
#include "sort.h"

/* These Config Variables are only used in mutt_thread.c */
bool C_DuplicateThreads; ///< Config: Highlight messages with duplicated message IDs
bool C_HideLimited; ///< Config: Don't indicate hidden messages, in the thread tree
bool C_HideMissing; ///< Config: Don't indicate missing messages, in the thread tree
bool C_HideThreadSubject; ///< Config: Hide subjects that are similar to that of the parent message
bool C_HideTopLimited; ///< Config: Don't indicate hidden top message, in the thread tree
bool C_HideTopMissing; ///< Config: Don't indicate missing top message, in the thread tree
bool C_NarrowTree; ///< Config: Draw a narrower thread tree in the index
bool C_SortRe;     ///< Config: Sort method for the sidebar
bool C_StrictThreads; ///< Config: Thread messages using 'In-Reply-To' and 'References' headers
bool C_ThreadReceived; ///< Config: Sort threaded messages by their received date

/**
 * is_visible - Is the message visible?
 * @param e   Email
 * @param ctx Mailbox
 * @retval true If the message is not hidden in some way
 */
static bool is_visible(struct Email *e, struct Context *ctx)
{
  return e->vnum >= 0 || (e->collapsed && (!ctx->pattern || e->limited));
}

/**
 * need_display_subject - Determines whether to display a message's subject
 * @param ctx Mailbox
 * @param e Email
 * @retval true If the subject should be displayed
 */
static bool need_display_subject(struct Context *ctx, struct Email *e)
{
  struct MuttThread *tmp = NULL;
  struct MuttThread *tree = e->thread;

  /* if the user disabled subject hiding, display it */
  if (!C_HideThreadSubject)
    return true;

  /* if our subject is different from our parent's, display it */
  if (e->subject_changed)
    return true;

  /* if our subject is different from that of our closest previously displayed
   * sibling, display the subject */
  for (tmp = tree->prev; tmp; tmp = tmp->prev)
  {
    e = tmp->message;
    if (e && is_visible(e, ctx))
    {
      if (e->subject_changed)
        return true;
      break;
    }
  }

  /* if there is a parent-to-child subject change anywhere between us and our
   * closest displayed ancestor, display the subject */
  for (tmp = tree->parent; tmp; tmp = tmp->parent)
  {
    e = tmp->message;
    if (e)
    {
      if (is_visible(e, ctx))
        return false;
      if (e->subject_changed)
        return true;
    }
  }

  /* if we have no visible parent or previous sibling, display the subject */
  return true;
}

/**
 * linearize_tree - Flatten an email thread
 * @param ctx Mailbox
 */
static void linearize_tree(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  struct MuttThread *tree = ctx->tree;
  struct Email **array = m->emails + ((C_Sort & SORT_REVERSE) ? m->msg_count - 1 : 0);

  while (tree)
  {
    while (!tree->message)
      tree = tree->child;

    *array = tree->message;
    array += (C_Sort & SORT_REVERSE) ? -1 : 1;

    if (tree->child)
      tree = tree->child;
    else
    {
      while (tree)
      {
        if (tree->next)
        {
          tree = tree->next;
          break;
        }
        else
          tree = tree->parent;
      }
    }
  }
}

/**
 * calculate_visibility - Are tree nodes visible
 * @param ctx       Mailbox
 * @param max_depth Maximum depth to check
 *
 * this calculates whether a node is the root of a subtree that has visible
 * nodes, whether a node itself is visible, whether, if invisible, it has
 * depth anyway, and whether any of its later siblings are roots of visible
 * subtrees.  while it's at it, it frees the old thread display, so we can
 * skip parts of the tree in mutt_draw_tree() if we've decided here that we
 * don't care about them any more.
 */
static void calculate_visibility(struct Context *ctx, int *max_depth)
{
  struct MuttThread *tmp = NULL;
  struct MuttThread *tree = ctx->tree;
  int hide_top_missing = C_HideTopMissing && !C_HideMissing;
  int hide_top_limited = C_HideTopLimited && !C_HideLimited;
  int depth = 0;

  /* we walk each level backwards to make it easier to compute next_subtree_visible */
  while (tree->next)
    tree = tree->next;
  *max_depth = 0;

  while (true)
  {
    if (depth > *max_depth)
      *max_depth = depth;

    tree->subtree_visible = 0;
    if (tree->message)
    {
      FREE(&tree->message->tree);
      if (is_visible(tree->message, ctx))
      {
        tree->deep = true;
        tree->visible = true;
        tree->message->display_subject = need_display_subject(ctx, tree->message);
        for (tmp = tree; tmp; tmp = tmp->parent)
        {
          if (tmp->subtree_visible)
          {
            tmp->deep = true;
            tmp->subtree_visible = 2;
            break;
          }
          else
            tmp->subtree_visible = 1;
        }
      }
      else
      {
        tree->visible = false;
        tree->deep = !C_HideLimited;
      }
    }
    else
    {
      tree->visible = false;
      tree->deep = !C_HideMissing;
    }
    tree->next_subtree_visible =
        tree->next && (tree->next->next_subtree_visible || tree->next->subtree_visible);
    if (tree->child)
    {
      depth++;
      tree = tree->child;
      while (tree->next)
        tree = tree->next;
    }
    else if (tree->prev)
      tree = tree->prev;
    else
    {
      while (tree && !tree->prev)
      {
        depth--;
        tree = tree->parent;
      }
      if (!tree)
        break;
      tree = tree->prev;
    }
  }

  /* now fix up for the OPTHIDETOP* options if necessary */
  if (hide_top_limited || hide_top_missing)
  {
    tree = ctx->tree;
    while (true)
    {
      if (!tree->visible && tree->deep && (tree->subtree_visible < 2) &&
          ((tree->message && hide_top_limited) || (!tree->message && hide_top_missing)))
      {
        tree->deep = false;
      }
      if (!tree->deep && tree->child && tree->subtree_visible)
        tree = tree->child;
      else if (tree->next)
        tree = tree->next;
      else
      {
        while (tree && !tree->next)
          tree = tree->parent;
        if (!tree)
          break;
        tree = tree->next;
      }
    }
  }
}

/**
 * mutt_draw_tree - Draw a tree of threaded emails
 * @param ctx Mailbox
 *
 * Since the graphics characters have a value >255, I have to resort to using
 * escape sequences to pass the information to print_enriched_string().  These
 * are the macros MUTT_TREE_* defined in mutt.h.
 *
 * ncurses should automatically use the default ASCII characters instead of
 * graphics chars on terminals which don't support them (see the man page for
 * curs_addch).
 */
void mutt_draw_tree(struct Context *ctx)
{
  char *pfx = NULL, *mypfx = NULL, *arrow = NULL, *myarrow = NULL, *new_tree = NULL;
  enum TreeChar corner = (C_Sort & SORT_REVERSE) ? MUTT_TREE_ULCORNER : MUTT_TREE_LLCORNER;
  enum TreeChar vtee = (C_Sort & SORT_REVERSE) ? MUTT_TREE_BTEE : MUTT_TREE_TTEE;
  int depth = 0, start_depth = 0, max_depth = 0, width = C_NarrowTree ? 1 : 2;
  struct MuttThread *nextdisp = NULL, *pseudo = NULL, *parent = NULL;
  struct MuttThread *tree = ctx->tree;

  /* Do the visibility calculations and free the old thread chars.
   * From now on we can simply ignore invisible subtrees */
  calculate_visibility(ctx, &max_depth);
  pfx = mutt_mem_malloc((width * max_depth) + 2);
  arrow = mutt_mem_malloc((width * max_depth) + 2);
  while (tree)
  {
    if (depth != 0)
    {
      myarrow = arrow + (depth - start_depth - ((start_depth != 0) ? 0 : 1)) * width;
      if (start_depth == depth)
        myarrow[0] = nextdisp ? MUTT_TREE_LTEE : corner;
      else if (parent->message && !C_HideLimited)
        myarrow[0] = MUTT_TREE_HIDDEN;
      else if (!parent->message && !C_HideMissing)
        myarrow[0] = MUTT_TREE_MISSING;
      else
        myarrow[0] = vtee;
      if (width == 2)
      {
        myarrow[1] = pseudo ? MUTT_TREE_STAR :
                              (tree->duplicate_thread ? MUTT_TREE_EQUALS : MUTT_TREE_HLINE);
      }
      if (tree->visible)
      {
        myarrow[width] = MUTT_TREE_RARROW;
        myarrow[width + 1] = 0;
        new_tree = mutt_mem_malloc(((size_t) depth * width) + 2);
        if (start_depth > 1)
        {
          strncpy(new_tree, pfx, (size_t) width * (start_depth - 1));
          mutt_str_copy(new_tree + (start_depth - 1) * width, arrow,
                        (1 + depth - start_depth) * width + 2);
        }
        else
          mutt_str_copy(new_tree, arrow, ((size_t) depth * width) + 2);
        tree->message->tree = new_tree;
      }
    }
    if (tree->child && (depth != 0))
    {
      mypfx = pfx + (depth - 1) * width;
      mypfx[0] = nextdisp ? MUTT_TREE_VLINE : MUTT_TREE_SPACE;
      if (width == 2)
        mypfx[1] = MUTT_TREE_SPACE;
    }
    parent = tree;
    nextdisp = NULL;
    pseudo = NULL;
    do
    {
      if (tree->child && tree->subtree_visible)
      {
        if (tree->deep)
          depth++;
        if (tree->visible)
          start_depth = depth;
        tree = tree->child;

        /* we do this here because we need to make sure that the first child thread
         * of the old tree that we deal with is actually displayed if any are,
         * or we might set the parent variable wrong while going through it. */
        while (!tree->subtree_visible && tree->next)
          tree = tree->next;
      }
      else
      {
        while (!tree->next && tree->parent)
        {
          if (tree == pseudo)
            pseudo = NULL;
          if (tree == nextdisp)
            nextdisp = NULL;
          if (tree->visible)
            start_depth = depth;
          tree = tree->parent;
          if (tree->deep)
          {
            if (start_depth == depth)
              start_depth--;
            depth--;
          }
        }
        if (tree == pseudo)
          pseudo = NULL;
        if (tree == nextdisp)
          nextdisp = NULL;
        if (tree->visible)
          start_depth = depth;
        tree = tree->next;
        if (!tree)
          break;
      }
      if (!pseudo && tree->fake_thread)
        pseudo = tree;
      if (!nextdisp && tree->next_subtree_visible)
        nextdisp = tree;
    } while (!tree->deep);
  }

  FREE(&pfx);
  FREE(&arrow);
}

/**
 * make_subject_list - Create a sorted list of all subjects in a thread
 * @param[out] subjects String List of subjects
 * @param[in]  cur      Email Thread
 * @param[out] dateptr  Earliest date found in thread
 *
 * Since we may be trying to attach as a pseudo-thread a MuttThread that has no
 * message, we have to make a list of all the subjects of its most immediate
 * existing descendants.
 */
static void make_subject_list(struct ListHead *subjects, struct MuttThread *cur, time_t *dateptr)
{
  struct MuttThread *start = cur;
  struct Envelope *env = NULL;
  time_t thisdate;
  int rc = 0;

  while (true)
  {
    while (!cur->message)
      cur = cur->child;

    if (dateptr)
    {
      thisdate = C_ThreadReceived ? cur->message->received : cur->message->date_sent;
      if ((*dateptr == 0) || (thisdate < *dateptr))
        *dateptr = thisdate;
    }

    env = cur->message->env;
    if (env->real_subj && ((env->real_subj != env->subject) || (!C_SortRe)))
    {
      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, subjects, entries)
      {
        rc = mutt_str_cmp(env->real_subj, np->data);
        if (rc >= 0)
          break;
      }
      if (!np)
        mutt_list_insert_head(subjects, env->real_subj);
      else if (rc > 0)
        mutt_list_insert_after(subjects, np, env->real_subj);
    }

    while (!cur->next && (cur != start))
    {
      cur = cur->parent;
    }
    if (cur == start)
      break;
    cur = cur->next;
  }
}

/**
 * find_subject - Find the best possible match for a parent based on subject
 * @param m   Mailbox
 * @param cur Email to match
 * @retval ptr Best match for a parent
 *
 * If there are multiple matches, the one which was sent the latest, but before
 * the current message, is used.
 */
static struct MuttThread *find_subject(struct Mailbox *m, struct MuttThread *cur)
{
  if (!m)
    return NULL;

  struct HashElem *ptr = NULL;
  struct MuttThread *tmp = NULL, *last = NULL;
  struct ListHead subjects = STAILQ_HEAD_INITIALIZER(subjects);
  time_t date = 0;

  make_subject_list(&subjects, cur, &date);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &subjects, entries)
  {
    for (ptr = mutt_hash_find_bucket(m->subj_hash, np->data); ptr; ptr = ptr->next)
    {
      tmp = ((struct Email *) ptr->data)->thread;
      if ((tmp != cur) &&                  /* don't match the same message */
          !tmp->fake_thread &&             /* don't match pseudo threads */
          tmp->message->subject_changed && /* only match interesting replies */
          !is_descendant(tmp, cur) &&      /* don't match in the same thread */
          (date >= (C_ThreadReceived ? tmp->message->received : tmp->message->date_sent)) &&
          (!last || (C_ThreadReceived ?
                         (last->message->received < tmp->message->received) :
                         (last->message->date_sent < tmp->message->date_sent))) &&
          tmp->message->env->real_subj &&
          mutt_str_equal(np->data, tmp->message->env->real_subj))
      {
        last = tmp; /* best match so far */
      }
    }
  }

  mutt_list_clear(&subjects);
  return last;
}

/**
 * make_subj_hash - Create a Hash Table for the email subjects
 * @param m Mailbox
 * @retval ptr Newly allocated Hash Table
 */
static struct HashTable *make_subj_hash(struct Mailbox *m)
{
  if (!m)
    return NULL;

  struct HashTable *hash = mutt_hash_new(m->msg_count * 2, MUTT_HASH_ALLOW_DUPS);

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e || !e->env)
      continue;
    if (e->env->real_subj)
      mutt_hash_insert(hash, e->env->real_subj, e);
  }

  return hash;
}

/**
 * pseudo_threads - Thread messages by subject
 * @param ctx Mailbox
 *
 * Thread by subject things that didn't get threaded by message-id
 */
static void pseudo_threads(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  struct MuttThread *tree = ctx->tree;
  struct MuttThread *top = tree;
  struct MuttThread *tmp = NULL, *cur = NULL, *parent = NULL, *curchild = NULL,
                    *nextchild = NULL;

  if (!m->subj_hash)
    m->subj_hash = make_subj_hash(ctx->mailbox);

  while (tree)
  {
    cur = tree;
    tree = tree->next;
    parent = find_subject(ctx->mailbox, cur);
    if (parent)
    {
      cur->fake_thread = true;
      unlink_message(&top, cur);
      insert_message(&parent->child, parent, cur);
      parent->sort_children = true;
      tmp = cur;
      while (true)
      {
        while (!tmp->message)
          tmp = tmp->child;

        /* if the message we're attaching has pseudo-children, they
         * need to be attached to its parent, so move them up a level.
         * but only do this if they have the same real subject as the
         * parent, since otherwise they rightly belong to the message
         * we're attaching. */
        if ((tmp == cur) || mutt_str_equal(tmp->message->env->real_subj,
                                           parent->message->env->real_subj))
        {
          tmp->message->subject_changed = false;

          for (curchild = tmp->child; curchild;)
          {
            nextchild = curchild->next;
            if (curchild->fake_thread)
            {
              unlink_message(&tmp->child, curchild);
              insert_message(&parent->child, parent, curchild);
            }
            curchild = nextchild;
          }
        }

        while (!tmp->next && (tmp != cur))
        {
          tmp = tmp->parent;
        }
        if (tmp == cur)
          break;
        tmp = tmp->next;
      }
    }
  }
  ctx->tree = top;
}

/**
 * mutt_clear_threads - Clear the threading of message in a mailbox
 * @param ctx Mailbox
 */
void mutt_clear_threads(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox || !ctx->mailbox->emails)
    return;

  struct Mailbox *m = ctx->mailbox;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    /* mailbox may have been only partially read */
    e->thread = NULL;
    e->threaded = false;
  }
  ctx->tree = NULL;

  mutt_hash_free(&ctx->thread_hash);
}

/**
 * compare_threads - Sorting function for email threads
 * @param a First thread to compare
 * @param b Second thread to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int compare_threads(const void *a, const void *b)
{
  static sort_t sort_func = NULL;

  if (a && b)
  {
    return (*sort_func)(&(*((struct MuttThread const *const *) a))->sort_key,
                        &(*((struct MuttThread const *const *) b))->sort_key);
  }
  /* a hack to let us reset sort_func even though we can't
   * have extra arguments because of qsort */
  else
  {
    sort_func = mutt_get_sort_func(C_Sort & SORT_MASK);
    return sort_func ? 1 : 0;
  }
}

/**
 * mutt_sort_subthreads - Sort the children of a thread
 * @param thread Thread to start at
 * @param init   If true, rebuild the thread
 * @retval ptr Sorted threads
 */
struct MuttThread *mutt_sort_subthreads(struct MuttThread *thread, bool init)
{
  struct MuttThread **array = NULL, *sort_key = NULL, *top = NULL, *tmp = NULL;
  struct Email *oldsort_key = NULL;
  int i, array_size, sort_top = 0;

  /* we put things into the array backwards to save some cycles,
   * but we want to have to move less stuff around if we're
   * resorting, so we sort backwards and then put them back
   * in reverse order so they're forwards */
  C_Sort ^= SORT_REVERSE;
  if (compare_threads(NULL, NULL) == 0)
    return thread;

  top = thread;

  array_size = 256;
  array = mutt_mem_calloc(array_size, sizeof(struct MuttThread *));
  while (true)
  {
    if (init || !thread->sort_key)
    {
      thread->sort_key = NULL;

      if (thread->parent)
        thread->parent->sort_children = true;
      else
        sort_top = 1;
    }

    if (thread->child)
    {
      thread = thread->child;
      continue;
    }
    else
    {
      /* if it has no children, it must be real. sort it on its own merits */
      thread->sort_key = thread->message;

      if (thread->next)
      {
        thread = thread->next;
        continue;
      }
    }

    while (!thread->next)
    {
      /* if it has siblings and needs to be sorted, sort it... */
      if (thread->prev && (thread->parent ? thread->parent->sort_children : sort_top))
      {
        /* put them into the array */
        for (i = 0; thread; i++, thread = thread->prev)
        {
          if (i >= array_size)
            mutt_mem_realloc(&array, (array_size *= 2) * sizeof(struct MuttThread *));

          array[i] = thread;
        }

        qsort((void *) array, i, sizeof(struct MuttThread *), *compare_threads);

        /* attach them back together.  make thread the last sibling. */
        thread = array[0];
        thread->next = NULL;
        array[i - 1]->prev = NULL;

        if (thread->parent)
          thread->parent->child = array[i - 1];
        else
          top = array[i - 1];

        while (--i)
        {
          array[i - 1]->prev = array[i];
          array[i]->next = array[i - 1];
        }
      }

      if (thread->parent)
      {
        tmp = thread;
        thread = thread->parent;

        if (!thread->sort_key || thread->sort_children)
        {
          /* make sort_key the first or last sibling, as appropriate */
          sort_key = ((!(C_Sort & SORT_LAST)) ^ (!(C_Sort & SORT_REVERSE))) ?
                         thread->child :
                         tmp;

          /* we just sorted its children */
          thread->sort_children = false;

          oldsort_key = thread->sort_key;
          thread->sort_key = thread->message;

          if (C_Sort & SORT_LAST)
          {
            if (!thread->sort_key ||
                ((((C_Sort & SORT_REVERSE) ? 1 : -1) *
                  compare_threads((void *) &thread, (void *) &sort_key)) > 0))
            {
              thread->sort_key = sort_key->sort_key;
            }
          }
          else if (!thread->sort_key)
            thread->sort_key = sort_key->sort_key;

          /* if its sort_key has changed, we need to resort it and siblings */
          if (oldsort_key != thread->sort_key)
          {
            if (thread->parent)
              thread->parent->sort_children = true;
            else
              sort_top = 1;
          }
        }
      }
      else
      {
        C_Sort ^= SORT_REVERSE;
        FREE(&array);
        return top;
      }
    }

    thread = thread->next;
  }
}

/**
 * check_subjects - Find out which emails' subjects differ from their parent's
 * @param m    Mailbox
 * @param init If true, rebuild the thread
 */
static void check_subjects(struct Mailbox *m, bool init)
{
  if (!m)
    return;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e || !e->thread)
      continue;

    if (e->thread->check_subject)
      e->thread->check_subject = false;
    else if (!init)
      continue;

    /* figure out which messages have subjects different than their parents' */
    struct MuttThread *tmp = e->thread->parent;
    while (tmp && !tmp->message)
    {
      tmp = tmp->parent;
    }

    if (!tmp)
      e->subject_changed = true;
    else if (e->env->real_subj && tmp->message->env->real_subj)
    {
      e->subject_changed = !mutt_str_equal(e->env->real_subj, tmp->message->env->real_subj);
    }
    else
    {
      e->subject_changed = (e->env->real_subj || tmp->message->env->real_subj);
    }
  }
}

/**
 * mutt_sort_threads - Sort email threads
 * @param ctx  Mailbox
 * @param init If true, rebuild the thread
 */
void mutt_sort_threads(struct Context *ctx, bool init)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  struct Email *e = NULL;
  int i, oldsort, using_refs = 0;
  struct MuttThread *thread = NULL, *tnew = NULL, *tmp = NULL;
  struct MuttThread top = { 0 };
  struct ListNode *ref = NULL;

  /* Set C_Sort to the secondary method to support the set sort_aux=reverse-*
   * settings.  The sorting functions just look at the value of SORT_REVERSE */
  oldsort = C_Sort;
  C_Sort = C_SortAux;

  if (!ctx->thread_hash)
    init = true;

  if (init)
  {
    ctx->thread_hash = mutt_hash_new(m->msg_count * 2, MUTT_HASH_ALLOW_DUPS);
    mutt_hash_set_destructor(ctx->thread_hash, thread_hash_destructor, 0);
  }

  /* we want a quick way to see if things are actually attached to the top of the
   * thread tree or if they're just dangling, so we attach everything to a top
   * node temporarily */
  top.parent = NULL;
  top.next = NULL;
  top.prev = NULL;

  top.child = ctx->tree;
  for (thread = ctx->tree; thread; thread = thread->next)
    thread->parent = &top;

  /* put each new message together with the matching messageless MuttThread if it
   * exists.  otherwise, if there is a MuttThread that already has a message, thread
   * new message as an identical child.  if we didn't attach the message to a
   * MuttThread, make a new one for it. */
  for (i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      continue;

    if (!e->thread)
    {
      if ((!init || C_DuplicateThreads) && e->env->message_id)
        thread = mutt_hash_find(ctx->thread_hash, e->env->message_id);
      else
        thread = NULL;

      if (thread && !thread->message)
      {
        /* this is a message which was missing before */
        thread->message = e;
        e->thread = thread;
        thread->check_subject = true;

        /* mark descendants as needing subject_changed checked */
        for (tmp = (thread->child ? thread->child : thread); tmp != thread;)
        {
          while (!tmp->message)
            tmp = tmp->child;
          tmp->check_subject = true;
          while (!tmp->next && (tmp != thread))
            tmp = tmp->parent;
          if (tmp != thread)
            tmp = tmp->next;
        }

        if (thread->parent)
        {
          /* remove threading info above it based on its children, which we'll
           * recalculate based on its headers.  make sure not to leave
           * dangling missing messages.  note that we haven't kept track
           * of what info came from its children and what from its siblings'
           * children, so we just remove the stuff that's definitely from it */
          do
          {
            tmp = thread->parent;
            unlink_message(&tmp->child, thread);
            thread->parent = NULL;
            thread->sort_key = NULL;
            thread->fake_thread = false;
            thread = tmp;
          } while (thread != &top && !thread->child && !thread->message);
        }
      }
      else
      {
        tnew = (C_DuplicateThreads ? thread : NULL);

        thread = mutt_mem_calloc(1, sizeof(struct MuttThread));
        thread->message = e;
        thread->check_subject = true;
        e->thread = thread;
        mutt_hash_insert(ctx->thread_hash,
                         e->env->message_id ? e->env->message_id : "", thread);

        if (tnew)
        {
          if (tnew->duplicate_thread)
            tnew = tnew->parent;

          thread = e->thread;

          insert_message(&tnew->child, tnew, thread);
          thread->duplicate_thread = true;
          thread->message->threaded = true;
        }
      }
    }
    else
    {
      /* unlink pseudo-threads because they might be children of newly
       * arrived messages */
      thread = e->thread;
      for (tnew = thread->child; tnew;)
      {
        tmp = tnew->next;
        if (tnew->fake_thread)
        {
          unlink_message(&thread->child, tnew);
          insert_message(&top.child, &top, tnew);
          tnew->fake_thread = false;
        }
        tnew = tmp;
      }
    }
  }

  /* thread by references */
  for (i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    if (e->threaded)
      continue;
    e->threaded = true;

    thread = e->thread;
    if (!thread)
      continue;
    using_refs = 0;

    while (true)
    {
      if (using_refs == 0)
      {
        /* look at the beginning of in-reply-to: */
        ref = STAILQ_FIRST(&e->env->in_reply_to);
        if (ref)
          using_refs = 1;
        else
        {
          ref = STAILQ_FIRST(&e->env->references);
          using_refs = 2;
        }
      }
      else if (using_refs == 1)
      {
        /* if there's no references header, use all the in-reply-to:
         * data that we have.  otherwise, use the first reference
         * if it's different than the first in-reply-to, otherwise use
         * the second reference (since at least eudora puts the most
         * recent reference in in-reply-to and the rest in references) */
        if (STAILQ_EMPTY(&e->env->references))
          ref = STAILQ_NEXT(ref, entries);
        else
        {
          if (!mutt_str_equal(ref->data, STAILQ_FIRST(&e->env->references)->data))
            ref = STAILQ_FIRST(&e->env->references);
          else
            ref = STAILQ_NEXT(STAILQ_FIRST(&e->env->references), entries);

          using_refs = 2;
        }
      }
      else
        ref = STAILQ_NEXT(ref, entries); /* go on with references */

      if (!ref)
        break;

      tnew = mutt_hash_find(ctx->thread_hash, ref->data);
      if (tnew)
      {
        if (tnew->duplicate_thread)
          tnew = tnew->parent;
        if (is_descendant(tnew, thread)) /* no loops! */
          continue;
      }
      else
      {
        tnew = mutt_mem_calloc(1, sizeof(struct MuttThread));
        mutt_hash_insert(ctx->thread_hash, ref->data, tnew);
      }

      if (thread->parent)
        unlink_message(&top.child, thread);
      insert_message(&tnew->child, tnew, thread);
      thread = tnew;
      if (thread->message || (thread->parent && (thread->parent != &top)))
        break;
    }

    if (!thread->parent)
      insert_message(&top.child, &top, thread);
  }

  /* detach everything from the temporary top node */
  for (thread = top.child; thread; thread = thread->next)
  {
    thread->parent = NULL;
  }
  ctx->tree = top.child;

  check_subjects(ctx->mailbox, init);

  if (!C_StrictThreads)
    pseudo_threads(ctx);

  if (ctx->tree)
  {
    ctx->tree = mutt_sort_subthreads(ctx->tree, init);

    /* restore the oldsort order. */
    C_Sort = oldsort;

    /* Put the list into an array. */
    linearize_tree(ctx);

    /* Draw the thread tree. */
    mutt_draw_tree(ctx);
  }
}

/**
 * mutt_aside_thread - Find the next/previous (sub)thread
 * @param e          Search from this Email
 * @param forwards   Direction to search: 'true' forwards, 'false' backwards
 * @param subthreads Search subthreads: 'true' subthread, 'false' not
 * @retval num Index into the virtual email table
 */
int mutt_aside_thread(struct Email *e, bool forwards, bool subthreads)
{
  struct MuttThread *cur = NULL;
  struct Email *e_tmp = NULL;

  if ((C_Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled"));
    return e->vnum;
  }

  cur = e->thread;

  if (subthreads)
  {
    if (forwards ^ ((C_Sort & SORT_REVERSE) != 0))
    {
      while (!cur->next && cur->parent)
        cur = cur->parent;
    }
    else
    {
      while (!cur->prev && cur->parent)
        cur = cur->parent;
    }
  }
  else
  {
    while (cur->parent)
      cur = cur->parent;
  }

  if (forwards ^ ((C_Sort & SORT_REVERSE) != 0))
  {
    do
    {
      cur = cur->next;
      if (!cur)
        return -1;
      e_tmp = find_virtual(cur, 0);
    } while (!e_tmp);
  }
  else
  {
    do
    {
      cur = cur->prev;
      if (!cur)
        return -1;
      e_tmp = find_virtual(cur, 1);
    } while (!e_tmp);
  }

  return e_tmp->vnum;
}

/**
 * mutt_parent_message - Find the parent of a message
 * @param ctx       Mailbox
 * @param e         Current Email
 * @param find_root If true, find the root message
 * @retval >=0 Virtual index number of parent/root message
 * @retval -1 Error
 */
int mutt_parent_message(struct Context *ctx, struct Email *e, bool find_root)
{
  if (!ctx || !e)
    return -1;

  struct MuttThread *thread = NULL;
  struct Email *e_parent = NULL;

  if ((C_Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled"));
    return e->vnum;
  }

  /* Root may be the current message */
  if (find_root)
    e_parent = e;

  for (thread = e->thread->parent; thread; thread = thread->parent)
  {
    e = thread->message;
    if (e)
    {
      e_parent = e;
      if (!find_root)
        break;
    }
  }

  if (!e_parent)
  {
    mutt_error(_("Parent message is not available"));
    return -1;
  }
  if (!is_visible(e_parent, ctx))
  {
    if (find_root)
      mutt_error(_("Root message is not visible in this limited view"));
    else
      mutt_error(_("Parent message is not visible in this limited view"));
    return -1;
  }
  return e_parent->vnum;
}

/**
 * mutt_set_vnum - Set the virtual index number of all the messages in a mailbox
 * @param ctx Mailbox
 */
void mutt_set_vnum(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Mailbox *m = ctx->mailbox;

  struct Email *e = NULL;

  m->vcount = 0;
  ctx->vsize = 0;
  int padding = mx_msg_padding_size(m);

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    if (e->vnum >= 0)
    {
      e->vnum = m->vcount;
      m->v2r[m->vcount] = i;
      m->vcount++;
      ctx->vsize += e->content->length + e->content->offset - e->content->hdr_offset + padding;
    }
  }
}

/**
 * mutt_traverse_thread - Recurse through an email thread, matching messages
 * @param ctx   Mailbox
 * @param e_cur Current Email
 * @param flag  Flag to set, see #MuttThreadFlags
 * @retval num Number of matches
 */
int mutt_traverse_thread(struct Context *ctx, struct Email *e_cur, MuttThreadFlags flag)
{
  struct MuttThread *thread = NULL, *top = NULL;
  struct Email *e_root = NULL;
  int final, reverse = (C_Sort & SORT_REVERSE), minmsgno;
  int num_hidden = 0, new_mail = 0, old_mail = 0;
  bool flagged = false;
  int min_unread_msgno = INT_MAX, min_unread = e_cur->vnum;
#define CHECK_LIMIT (!ctx->pattern || e_cur->limited)

  if ((C_Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled"));
    return e_cur->vnum;
  }

  if (!e_cur->thread)
  {
    return e_cur->vnum;
  }

  final = e_cur->vnum;
  thread = e_cur->thread;
  while (thread->parent)
    thread = thread->parent;
  top = thread;
  while (!thread->message)
    thread = thread->child;
  e_cur = thread->message;
  minmsgno = e_cur->msgno;

  if (!e_cur->read && CHECK_LIMIT)
  {
    if (e_cur->old)
      old_mail = 2;
    else
      new_mail = 1;
    if (e_cur->msgno < min_unread_msgno)
    {
      min_unread = e_cur->vnum;
      min_unread_msgno = e_cur->msgno;
    }
  }

  if (e_cur->flagged && CHECK_LIMIT)
    flagged = true;

  if ((e_cur->vnum == -1) && CHECK_LIMIT)
    num_hidden++;

  if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
  {
    e_cur->pair = 0; /* force index entry's color to be re-evaluated */
    e_cur->collapsed = flag & MUTT_THREAD_COLLAPSE;
    if (e_cur->vnum != -1)
    {
      e_root = e_cur;
      if (flag & MUTT_THREAD_COLLAPSE)
        final = e_root->vnum;
    }
  }

  if ((thread == top) && !(thread = thread->child))
  {
    /* return value depends on action requested */
    if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
    {
      if (e_root)
        e_root->num_hidden = num_hidden;
      return final;
    }
    if (flag & MUTT_THREAD_UNREAD)
      return (old_mail && new_mail) ? new_mail : (old_mail ? old_mail : new_mail);
    if (flag & MUTT_THREAD_NEXT_UNREAD)
      return min_unread;
    if (flag & MUTT_THREAD_FLAGGED)
      return flagged;
  }

  while (true)
  {
    e_cur = thread->message;

    if (e_cur)
    {
      if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
      {
        e_cur->pair = 0; /* force index entry's color to be re-evaluated */
        e_cur->collapsed = flag & MUTT_THREAD_COLLAPSE;
        if (!e_root && CHECK_LIMIT)
        {
          e_root = e_cur;
          if (flag & MUTT_THREAD_COLLAPSE)
            final = e_root->vnum;
        }

        if (reverse && (flag & MUTT_THREAD_COLLAPSE) && (e_cur->msgno < minmsgno) && CHECK_LIMIT)
        {
          minmsgno = e_cur->msgno;
          final = e_cur->vnum;
        }

        if (flag & MUTT_THREAD_COLLAPSE)
        {
          if (e_cur != e_root)
            e_cur->vnum = -1;
        }
        else
        {
          if (CHECK_LIMIT)
            e_cur->vnum = e_cur->msgno;
        }
      }

      if (!e_cur->read && CHECK_LIMIT)
      {
        if (e_cur->old)
          old_mail = 2;
        else
          new_mail = 1;
        if (e_cur->msgno < min_unread_msgno)
        {
          min_unread = e_cur->vnum;
          min_unread_msgno = e_cur->msgno;
        }
      }

      if (e_cur->flagged && CHECK_LIMIT)
        flagged = true;

      if ((e_cur->vnum == -1) && CHECK_LIMIT)
        num_hidden++;
    }

    if (thread->child)
      thread = thread->child;
    else if (thread->next)
      thread = thread->next;
    else
    {
      bool done = false;
      while (!thread->next)
      {
        thread = thread->parent;
        if (thread == top)
        {
          done = true;
          break;
        }
      }
      if (done)
        break;
      thread = thread->next;
    }
  }

  /* return value depends on action requested */
  if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
  {
    if (e_root)
      e_root->num_hidden = num_hidden + 1;
    return final;
  }
  if (flag & MUTT_THREAD_UNREAD)
    return (old_mail && new_mail) ? new_mail : (old_mail ? old_mail : new_mail);
  if (flag & MUTT_THREAD_NEXT_UNREAD)
    return min_unread;
  if (flag & MUTT_THREAD_FLAGGED)
    return flagged;

  return 0;
#undef CHECK_LIMIT
}

/**
 * mutt_messages_in_thread - Count the messages in a thread
 * @param m    Mailbox
 * @param e    Email
 * @param flag Flag, see notes below
 * @retval num Number of message / Our position
 *
 * If flag is 0, we want to know how many messages are in the thread.
 * If flag is 1, we want to know our position in the thread.
 */
int mutt_messages_in_thread(struct Mailbox *m, struct Email *e, int flag)
{
  if (!m || !e)
    return 1;

  struct MuttThread *threads[2];
  int rc;

  if (((C_Sort & SORT_MASK) != SORT_THREADS) || !e->thread)
    return 1;

  threads[0] = e->thread;
  while (threads[0]->parent)
    threads[0] = threads[0]->parent;

  threads[1] = flag ? e->thread : threads[0]->next;

  for (int i = 0; i < ((flag || !threads[1]) ? 1 : 2); i++)
  {
    while (!threads[i]->message)
      threads[i] = threads[i]->child;
  }

  if (C_Sort & SORT_REVERSE)
    rc = threads[0]->message->msgno - (threads[1] ? threads[1]->message->msgno : -1);
  else
  {
    rc = (threads[1] ? threads[1]->message->msgno : m->msg_count) -
         threads[0]->message->msgno;
  }

  if (flag)
    rc += 1;

  return rc;
}

/**
 * mutt_make_id_hash - Create a Hash Table for message-ids
 * @param m Mailbox
 * @retval ptr Newly allocated Hash Table
 */
struct HashTable *mutt_make_id_hash(struct Mailbox *m)
{
  struct HashTable *hash = mutt_hash_new(m->msg_count * 2, MUTT_HASH_NO_FLAGS);

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e || !e->env)
      continue;

    if (e->env->message_id)
      mutt_hash_insert(hash, e->env->message_id, e);
  }

  return hash;
}

/**
 * link_threads - Forcibly link messages together
 * @param parent Parent Email
 * @param child  Child Email
 * @param m      Mailbox
 * @retval true On success
 */
static bool link_threads(struct Email *parent, struct Email *child, struct Mailbox *m)
{
  if (child == parent)
    return false;

  mutt_break_thread(child);
  mutt_list_insert_head(&child->env->in_reply_to, mutt_str_dup(parent->env->message_id));
  mutt_set_flag(m, child, MUTT_TAG, false);

  child->changed = true;
  child->env->changed |= MUTT_ENV_CHANGED_IRT;
  return true;
}

/**
 * mutt_link_threads - Forcibly link threads together
 * @param parent   Parent Email
 * @param children List of children Emails
 * @param m        Mailbox
 * @retval true On success
 */
bool mutt_link_threads(struct Email *parent, struct EmailList *children, struct Mailbox *m)
{
  if (!parent || !children || !m)
    return false;

  bool changed = false;

  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, children, entries)
  {
    changed |= link_threads(parent, en->email, m);
  }

  return changed;
}
