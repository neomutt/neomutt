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

#include "config.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lib/lib.h"
#include "mutt.h"
#include "thread.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "options.h"
#include "protos.h"
#include "sort.h"

static bool is_visible(struct Header *hdr, struct Context *ctx)
{
  return (hdr->virtual >= 0 || (hdr->collapsed && (!ctx->pattern || hdr->limited)));
}

/**
 * is_descendant - Is one thread a descendant of another
 */
static int is_descendant(struct MuttThread *a, struct MuttThread *b)
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
 * need_display_subject - Determines whether to display a message's subject
 */
static int need_display_subject(struct Context *ctx, struct Header *hdr)
{
  struct MuttThread *tmp = NULL, *tree = hdr->thread;

  /* if the user disabled subject hiding, display it */
  if (!option(OPT_HIDE_THREAD_SUBJECT))
    return 1;

  /* if our subject is different from our parent's, display it */
  if (hdr->subject_changed)
    return 1;

  /* if our subject is different from that of our closest previously displayed
   * sibling, display the subject */
  for (tmp = tree->prev; tmp; tmp = tmp->prev)
  {
    hdr = tmp->message;
    if (hdr && is_visible(hdr, ctx))
    {
      if (hdr->subject_changed)
        return 1;
      else
        break;
    }
  }

  /* if there is a parent-to-child subject change anywhere between us and our
   * closest displayed ancestor, display the subject */
  for (tmp = tree->parent; tmp; tmp = tmp->parent)
  {
    hdr = tmp->message;
    if (hdr)
    {
      if (is_visible(hdr, ctx))
        return 0;
      else if (hdr->subject_changed)
        return 1;
    }
  }

  /* if we have no visible parent or previous sibling, display the subject */
  return 1;
}

static void linearize_tree(struct Context *ctx)
{
  struct MuttThread *tree = ctx->tree;
  struct Header **array = ctx->hdrs + (Sort & SORT_REVERSE ? ctx->msgcount - 1 : 0);

  while (tree)
  {
    while (!tree->message)
      tree = tree->child;

    *array = tree->message;
    array += Sort & SORT_REVERSE ? -1 : 1;

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
  struct MuttThread *tmp = NULL, *tree = ctx->tree;
  int hide_top_missing = option(OPT_HIDE_TOP_MISSING) && !option(OPT_HIDE_MISSING);
  int hide_top_limited = option(OPT_HIDE_TOP_LIMITED) && !option(OPT_HIDE_LIMITED);
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
        tree->deep = !option(OPT_HIDE_LIMITED);
      }
    }
    else
    {
      tree->visible = false;
      tree->deep = !option(OPT_HIDE_MISSING);
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
      else
        tree = tree->prev;
    }
  }

  /* now fix up for the OPTHIDETOP* options if necessary */
  if (hide_top_limited || hide_top_missing)
  {
    tree = ctx->tree;
    while (true)
    {
      if (!tree->visible && tree->deep && tree->subtree_visible < 2 &&
          ((tree->message && hide_top_limited) || (!tree->message && hide_top_missing)))
        tree->deep = false;
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
        else
          tree = tree->next;
      }
    }
  }
}

/**
 * mutt_draw_tree - Draw a tree of threaded emails
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
  char corner = (Sort & SORT_REVERSE) ? MUTT_TREE_ULCORNER : MUTT_TREE_LLCORNER;
  char vtee = (Sort & SORT_REVERSE) ? MUTT_TREE_BTEE : MUTT_TREE_TTEE;
  int depth = 0, start_depth = 0, max_depth = 0, width = option(OPT_NARROW_TREE) ? 1 : 2;
  struct MuttThread *nextdisp = NULL, *pseudo = NULL, *parent = NULL, *tree = ctx->tree;

  /* Do the visibility calculations and free the old thread chars.
   * From now on we can simply ignore invisible subtrees
   */
  calculate_visibility(ctx, &max_depth);
  pfx = safe_malloc(width * max_depth + 2);
  arrow = safe_malloc(width * max_depth + 2);
  while (tree)
  {
    if (depth)
    {
      myarrow = arrow + (depth - start_depth - (start_depth ? 0 : 1)) * width;
      if (depth && start_depth == depth)
        myarrow[0] = nextdisp ? MUTT_TREE_LTEE : corner;
      else if (parent->message && !option(OPT_HIDE_LIMITED))
        myarrow[0] = MUTT_TREE_HIDDEN;
      else if (!parent->message && !option(OPT_HIDE_MISSING))
        myarrow[0] = MUTT_TREE_MISSING;
      else
        myarrow[0] = vtee;
      if (width == 2)
        myarrow[1] = pseudo ? MUTT_TREE_STAR :
                              (tree->duplicate_thread ? MUTT_TREE_EQUALS : MUTT_TREE_HLINE);
      if (tree->visible)
      {
        myarrow[width] = MUTT_TREE_RARROW;
        myarrow[width + 1] = 0;
        new_tree = safe_malloc((2 + depth * width));
        if (start_depth > 1)
        {
          strncpy(new_tree, pfx, (start_depth - 1) * width);
          strfcpy(new_tree + (start_depth - 1) * width, arrow,
                  (1 + depth - start_depth) * width + 2);
        }
        else
          strfcpy(new_tree, arrow, 2 + depth * width);
        tree->message->tree = new_tree;
      }
    }
    if (tree->child && depth)
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
 * make_subject_list - Create a list of all subjects in a thread
 *
 * since we may be trying to attach as a pseudo-thread a MuttThread that
 * has no message, we have to make a list of all the subjects of its
 * most immediate existing descendants.  we also note the earliest
 * date on any of the parents and put it in *dateptr.
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
      thisdate = option(OPT_THREAD_RECEIVED) ? cur->message->received :
                                               cur->message->date_sent;
      if (!*dateptr || thisdate < *dateptr)
        *dateptr = thisdate;
    }

    env = cur->message->env;
    if (env->real_subj && ((env->real_subj != env->subject) || (!option(OPT_SORT_RE))))
    {
      struct ListNode *np;
      STAILQ_FOREACH(np, subjects, entries)
      {
        rc = mutt_strcmp(env->real_subj, np->data);
        if (rc >= 0)
          break;
      }
      if (!np)
        mutt_list_insert_head(subjects, env->real_subj);
      else if (rc > 0)
        mutt_list_insert_after(subjects, np, env->real_subj);
    }

    while (!cur->next && cur != start)
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
 *
 * If there are multiple matches, the one which was sent the latest, but before
 * the current message, is used.
 */
static struct MuttThread *find_subject(struct Context *ctx, struct MuttThread *cur)
{
  struct HashElem *ptr = NULL;
  struct MuttThread *tmp = NULL, *last = NULL;
  struct ListHead subjects = STAILQ_HEAD_INITIALIZER(subjects);
  time_t date = 0;

  make_subject_list(&subjects, cur, &date);

  struct ListNode *np;
  STAILQ_FOREACH(np, &subjects, entries)
  {
    for (ptr = hash_find_bucket(ctx->subj_hash, np->data); ptr; ptr = ptr->next)
    {
      tmp = ((struct Header *) ptr->data)->thread;
      if (tmp != cur &&                    /* don't match the same message */
          !tmp->fake_thread &&             /* don't match pseudo threads */
          tmp->message->subject_changed && /* only match interesting replies */
          !is_descendant(tmp, cur) &&      /* don't match in the same thread */
          (date >= (option(OPT_THREAD_RECEIVED) ? tmp->message->received :
                                                  tmp->message->date_sent)) &&
          (!last || (option(OPT_THREAD_RECEIVED) ?
                         (last->message->received < tmp->message->received) :
                         (last->message->date_sent < tmp->message->date_sent))) &&
          tmp->message->env->real_subj &&
          (mutt_strcmp(np->data, tmp->message->env->real_subj) == 0))
      {
        last = tmp; /* best match so far */
      }
    }
  }

  mutt_list_clear(&subjects);
  return last;
}

/**
 * unlink_message - Break the message out of the thread
 *
 * Remove cur and its descendants from their current location.  Also make sure
 * ancestors of cur no longer are sorted by the fact that cur is their
 * descendant.
 */
static void unlink_message(struct MuttThread **old, struct MuttThread *cur)
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
 *
 * add cur as a prior sibling of *new, with parent newparent
 */
static void insert_message(struct MuttThread **new,
                           struct MuttThread *newparent, struct MuttThread *cur)
{
  if (*new)
    (*new)->prev = cur;

  cur->parent = newparent;
  cur->next = *new;
  cur->prev = NULL;
  *new = cur;
}

static struct Hash *make_subj_hash(struct Context *ctx)
{
  struct Header *hdr = NULL;
  struct Hash *hash = NULL;

  hash = hash_create(ctx->msgcount * 2, MUTT_HASH_ALLOW_DUPS);

  for (int i = 0; i < ctx->msgcount; i++)
  {
    hdr = ctx->hdrs[i];
    if (hdr->env->real_subj)
      hash_insert(hash, hdr->env->real_subj, hdr);
  }

  return hash;
}

/**
 * pseudo_threads - Thread messages by subject
 *
 * Thread by subject things that didn't get threaded by message-id
 */
static void pseudo_threads(struct Context *ctx)
{
  struct MuttThread *tree = ctx->tree, *top = tree;
  struct MuttThread *tmp = NULL, *cur = NULL, *parent = NULL, *curchild = NULL,
                    *nextchild = NULL;

  if (!ctx->subj_hash)
    ctx->subj_hash = make_subj_hash(ctx);

  while (tree)
  {
    cur = tree;
    tree = tree->next;
    parent = find_subject(ctx, cur);
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
        if (tmp == cur || (mutt_strcmp(tmp->message->env->real_subj,
                                       parent->message->env->real_subj) == 0))
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

        while (!tmp->next && tmp != cur)
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

void mutt_clear_threads(struct Context *ctx)
{
  for (int i = 0; i < ctx->msgcount; i++)
  {
    /* mailbox may have been only partially read */
    if (ctx->hdrs[i])
    {
      ctx->hdrs[i]->thread = NULL;
      ctx->hdrs[i]->threaded = false;
    }
  }
  ctx->tree = NULL;

  if (ctx->thread_hash)
    hash_destroy(&ctx->thread_hash, *free);
}

static int compare_threads(const void *a, const void *b)
{
  static sort_t *sort_func = NULL;

  if (a && b)
    return ((*sort_func)(&(*((struct MuttThread **) a))->sort_key,
                         &(*((struct MuttThread **) b))->sort_key));
  /* a hack to let us reset sort_func even though we can't
   * have extra arguments because of qsort
   */
  else
  {
    sort_func = mutt_get_sort_func(Sort);
    return (sort_func ? 1 : 0);
  }
}

struct MuttThread *mutt_sort_subthreads(struct MuttThread *thread, int init)
{
  struct MuttThread **array = NULL, *sort_key = NULL, *top = NULL, *tmp = NULL;
  struct Header *oldsort_key = NULL;
  int i, array_size, sort_top = 0;

  /* we put things into the array backwards to save some cycles,
   * but we want to have to move less stuff around if we're
   * resorting, so we sort backwards and then put them back
   * in reverse order so they're forwards
   */
  Sort ^= SORT_REVERSE;
  if (!compare_threads(NULL, NULL))
    return thread;

  top = thread;

  array = safe_calloc((array_size = 256), sizeof(struct MuttThread *));
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
            safe_realloc(&array, (array_size *= 2) * sizeof(struct MuttThread *));

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
          sort_key = (!(Sort & SORT_LAST) ^ !(Sort & SORT_REVERSE)) ? thread->child : tmp;

          /* we just sorted its children */
          thread->sort_children = false;

          oldsort_key = thread->sort_key;
          thread->sort_key = thread->message;

          if (Sort & SORT_LAST)
          {
            if (!thread->sort_key ||
                ((((Sort & SORT_REVERSE) ? 1 : -1) *
                  compare_threads((void *) &thread, (void *) &sort_key)) > 0))
              thread->sort_key = sort_key->sort_key;
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
        Sort ^= SORT_REVERSE;
        FREE(&array);
        return top;
      }
    }

    thread = thread->next;
  }
}

static void check_subjects(struct Context *ctx, int init)
{
  struct Header *cur = NULL;
  struct MuttThread *tmp = NULL;
  for (int i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    if (cur->thread->check_subject)
      cur->thread->check_subject = false;
    else if (!init)
      continue;

    /* figure out which messages have subjects different than their parents' */
    tmp = cur->thread->parent;
    while (tmp && !tmp->message)
    {
      tmp = tmp->parent;
    }

    if (!tmp)
      cur->subject_changed = true;
    else if (cur->env->real_subj && tmp->message->env->real_subj)
      cur->subject_changed =
          (mutt_strcmp(cur->env->real_subj, tmp->message->env->real_subj) != 0) ? true : false;
    else
      cur->subject_changed =
          (cur->env->real_subj || tmp->message->env->real_subj) ? true : false;
  }
}

void mutt_sort_threads(struct Context *ctx, int init)
{
  struct Header *cur = NULL;
  int i, oldsort, using_refs = 0;
  struct MuttThread *thread = NULL, *new = NULL, *tmp = NULL, top;
  memset(&top, 0, sizeof(top));
  struct ListNode *ref = NULL;

  /* set Sort to the secondary method to support the set sort_aux=reverse-*
   * settings.  The sorting functions just look at the value of
   * SORT_REVERSE
   */
  oldsort = Sort;
  Sort = SortAux;

  if (!ctx->thread_hash)
    init = 1;

  if (init)
    ctx->thread_hash = hash_create(ctx->msgcount * 2, MUTT_HASH_ALLOW_DUPS);

  /* we want a quick way to see if things are actually attached to the top of the
   * thread tree or if they're just dangling, so we attach everything to a top
   * node temporarily */
  top.parent = top.next = top.prev = NULL;
  top.child = ctx->tree;
  for (thread = ctx->tree; thread; thread = thread->next)
    thread->parent = &top;

  /* put each new message together with the matching messageless MuttThread if it
   * exists.  otherwise, if there is a MuttThread that already has a message, thread
   * new message as an identical child.  if we didn't attach the message to a
   * MuttThread, make a new one for it. */
  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];

    if (!cur->thread)
    {
      if ((!init || option(OPT_DUPLICATE_THREADS)) && cur->env->message_id)
        thread = hash_find(ctx->thread_hash, cur->env->message_id);
      else
        thread = NULL;

      if (thread && !thread->message)
      {
        /* this is a message which was missing before */
        thread->message = cur;
        cur->thread = thread;
        thread->check_subject = true;

        /* mark descendants as needing subject_changed checked */
        for (tmp = (thread->child ? thread->child : thread); tmp != thread;)
        {
          while (!tmp->message)
            tmp = tmp->child;
          tmp->check_subject = true;
          while (!tmp->next && tmp != thread)
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
        new = (option(OPT_DUPLICATE_THREADS) ? thread : NULL);

        thread = safe_calloc(1, sizeof(struct MuttThread));
        thread->message = cur;
        thread->check_subject = true;
        cur->thread = thread;
        hash_insert(ctx->thread_hash,
                    cur->env->message_id ? cur->env->message_id : "", thread);

        if (new)
        {
          if (new->duplicate_thread)
            new = new->parent;

          thread = cur->thread;

          insert_message(&new->child, new, thread);
          thread->duplicate_thread = true;
          thread->message->threaded = true;
        }
      }
    }
    else
    {
      /* unlink pseudo-threads because they might be children of newly
       * arrived messages */
      thread = cur->thread;
      for (new = thread->child; new;)
      {
        tmp = new->next;
        if (new->fake_thread)
        {
          unlink_message(&thread->child, new);
          insert_message(&top.child, &top, new);
          new->fake_thread = false;
        }
        new = tmp;
      }
    }
  }

  /* thread by references */
  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    if (cur->threaded)
      continue;
    cur->threaded = true;

    thread = cur->thread;
    using_refs = 0;

    while (true)
    {
      if (using_refs == 0)
      {
        /* look at the beginning of in-reply-to: */
        ref = STAILQ_FIRST(&cur->env->in_reply_to);
        if (ref)
          using_refs = 1;
        else
        {
          ref = STAILQ_FIRST(&cur->env->references);
          using_refs = 2;
        }
      }
      else if (using_refs == 1)
      {
        /* if there's no references header, use all the in-reply-to:
         * data that we have.  otherwise, use the first reference
         * if it's different than the first in-reply-to, otherwise use
         * the second reference (since at least eudora puts the most
         * recent reference in in-reply-to and the rest in references)
         */
        if (STAILQ_EMPTY(&cur->env->references))
          ref = STAILQ_NEXT(ref, entries);
        else
        {
          if (mutt_strcmp(ref->data, STAILQ_FIRST(&cur->env->references)->data) != 0)
            ref = STAILQ_FIRST(&cur->env->references);
          else
            ref = STAILQ_NEXT(STAILQ_FIRST(&cur->env->references), entries);

          using_refs = 2;
        }
      }
      else
        ref = STAILQ_NEXT(ref, entries); /* go on with references */

      if (!ref)
        break;

      new = hash_find(ctx->thread_hash, ref->data);
      if (!new)
      {
        new = safe_calloc(1, sizeof(struct MuttThread));
        hash_insert(ctx->thread_hash, ref->data, new);
      }
      else
      {
        if (new->duplicate_thread)
          new = new->parent;
        if (is_descendant(new, thread)) /* no loops! */
          continue;
      }

      if (thread->parent)
        unlink_message(&top.child, thread);
      insert_message(&new->child, new, thread);
      thread = new;
      if (thread->message || (thread->parent && thread->parent != &top))
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

  check_subjects(ctx, init);

  if (!option(OPT_STRICT_THREADS))
    pseudo_threads(ctx);

  if (ctx->tree)
  {
    ctx->tree = mutt_sort_subthreads(ctx->tree, init);

    /* restore the oldsort order. */
    Sort = oldsort;

    /* Put the list into an array. */
    linearize_tree(ctx);

    /* Draw the thread tree. */
    mutt_draw_tree(ctx);
  }
}

static struct Header *find_virtual(struct MuttThread *cur, int reverse)
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
 * _mutt_aside_thread - Find the next/previous (sub)thread
 * @param hdr        Search from this message
 * @param dir        Direction to search: 'true' forwards, 'false' backwards
 * @param subthreads Search subthreads: 'true' subthread, 'false' not
 * @retval n Index into the virtual email table
 */
int _mutt_aside_thread(struct Header *hdr, short dir, short subthreads)
{
  struct MuttThread *cur = NULL;
  struct Header *tmp = NULL;

  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled."));
    return hdr->virtual;
  }

  cur = hdr->thread;

  if (!subthreads)
  {
    while (cur->parent)
      cur = cur->parent;
  }
  else
  {
    if ((dir != 0) ^ ((Sort & SORT_REVERSE) != 0))
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

  if ((dir != 0) ^ ((Sort & SORT_REVERSE) != 0))
  {
    do
    {
      cur = cur->next;
      if (!cur)
        return -1;
      tmp = find_virtual(cur, 0);
    } while (!tmp);
  }
  else
  {
    do
    {
      cur = cur->prev;
      if (!cur)
        return -1;
      tmp = find_virtual(cur, 1);
    } while (!tmp);
  }

  return tmp->virtual;
}

int mutt_parent_message(struct Context *ctx, struct Header *hdr, int find_root)
{
  struct MuttThread *thread = NULL;
  struct Header *parent = NULL;

  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error(_("Threading is not enabled."));
    return hdr->virtual;
  }

  /* Root may be the current message */
  if (find_root)
    parent = hdr;

  for (thread = hdr->thread->parent; thread; thread = thread->parent)
  {
    hdr = thread->message;
    if (hdr)
    {
      parent = hdr;
      if (!find_root)
        break;
    }
  }

  if (!parent)
  {
    mutt_error(_("Parent message is not available."));
    return -1;
  }
  if (!is_visible(parent, ctx))
  {
    if (find_root)
      mutt_error(_("Root message is not visible in this limited view."));
    else
      mutt_error(_("Parent message is not visible in this limited view."));
    return -1;
  }
  return parent->virtual;
}

void mutt_set_virtual(struct Context *ctx)
{
  struct Header *cur = NULL;

  ctx->vcount = 0;
  ctx->vsize = 0;

  for (int i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    if (cur->virtual >= 0)
    {
      cur->virtual = ctx->vcount;
      ctx->v2r[ctx->vcount] = i;
      ctx->vcount++;
      ctx->vsize += cur->content->length + cur->content->offset - cur->content->hdr_offset;
      cur->num_hidden = mutt_get_hidden(ctx, cur);
    }
  }
}

int _mutt_traverse_thread(struct Context *ctx, struct Header *cur, int flag)
{
  struct MuttThread *thread = NULL, *top = NULL;
  struct Header *roothdr = NULL;
  int final, reverse = (Sort & SORT_REVERSE), minmsgno;
  int num_hidden = 0, new = 0, old = 0;
  bool flagged = false;
  int min_unread_msgno = INT_MAX, min_unread = cur->virtual;
#define CHECK_LIMIT (!ctx->pattern || cur->limited)

  if ((Sort & SORT_MASK) != SORT_THREADS && !(flag & MUTT_THREAD_GET_HIDDEN))
  {
    mutt_error(_("Threading is not enabled."));
    return cur->virtual;
  }

  final = cur->virtual;
  thread = cur->thread;
  while (thread->parent)
    thread = thread->parent;
  top = thread;
  while (!thread->message)
    thread = thread->child;
  cur = thread->message;
  minmsgno = cur->msgno;

  if (!cur->read && CHECK_LIMIT)
  {
    if (cur->old)
      old = 2;
    else
      new = 1;
    if (cur->msgno < min_unread_msgno)
    {
      min_unread = cur->virtual;
      min_unread_msgno = cur->msgno;
    }
  }

  if (cur->flagged && CHECK_LIMIT)
    flagged = true;

  if (cur->virtual == -1 && CHECK_LIMIT)
    num_hidden++;

  if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
  {
    cur->pair = 0; /* force index entry's color to be re-evaluated */
    cur->collapsed = flag & MUTT_THREAD_COLLAPSE;
    if (cur->virtual != -1)
    {
      roothdr = cur;
      if (flag & MUTT_THREAD_COLLAPSE)
        final = roothdr->virtual;
    }
  }

  if (thread == top && (thread = thread->child) == NULL)
  {
    /* return value depends on action requested */
    if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
      return final;
    else if (flag & MUTT_THREAD_UNREAD)
      return ((old && new) ? new : (old ? old : new));
    else if (flag & MUTT_THREAD_GET_HIDDEN)
      return num_hidden;
    else if (flag & MUTT_THREAD_NEXT_UNREAD)
      return min_unread;
    else if (flag & MUTT_THREAD_FLAGGED)
      return flagged;
  }

  while (true)
  {
    cur = thread->message;

    if (cur)
    {
      if (flag & (MUTT_THREAD_COLLAPSE | MUTT_THREAD_UNCOLLAPSE))
      {
        cur->pair = 0; /* force index entry's color to be re-evaluated */
        cur->collapsed = flag & MUTT_THREAD_COLLAPSE;
        if (!roothdr && CHECK_LIMIT)
        {
          roothdr = cur;
          if (flag & MUTT_THREAD_COLLAPSE)
            final = roothdr->virtual;
        }

        if (reverse && (flag & MUTT_THREAD_COLLAPSE) && (cur->msgno < minmsgno) && CHECK_LIMIT)
        {
          minmsgno = cur->msgno;
          final = cur->virtual;
        }

        if (flag & MUTT_THREAD_COLLAPSE)
        {
          if (cur != roothdr)
            cur->virtual = -1;
        }
        else
        {
          if (CHECK_LIMIT)
            cur->virtual = cur->msgno;
        }
      }

      if (!cur->read && CHECK_LIMIT)
      {
        if (cur->old)
          old = 2;
        else
          new = 1;
        if (cur->msgno < min_unread_msgno)
        {
          min_unread = cur->virtual;
          min_unread_msgno = cur->msgno;
        }
      }

      if (cur->flagged && CHECK_LIMIT)
        flagged = true;

      if (cur->virtual == -1 && CHECK_LIMIT)
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
    return final;
  else if (flag & MUTT_THREAD_UNREAD)
    return ((old && new) ? new : (old ? old : new));
  else if (flag & MUTT_THREAD_GET_HIDDEN)
    return num_hidden + 1;
  else if (flag & MUTT_THREAD_NEXT_UNREAD)
    return min_unread;
  else if (flag & MUTT_THREAD_FLAGGED)
    return flagged;

  return 0;
#undef CHECK_LIMIT
}

/**
 * mutt_messages_in_thread - Count the messages in a thread
 *
 * If flag is 0, we want to know how many messages are in the thread.
 * If flag is 1, we want to know our position in the thread.
 */
int mutt_messages_in_thread(struct Context *ctx, struct Header *hdr, int flag)
{
  struct MuttThread *threads[2];
  int rc;

  if ((Sort & SORT_MASK) != SORT_THREADS || !hdr->thread)
    return 1;

  threads[0] = hdr->thread;
  while (threads[0]->parent)
    threads[0] = threads[0]->parent;

  threads[1] = flag ? hdr->thread : threads[0]->next;

  for (int i = 0; i < ((flag || !threads[1]) ? 1 : 2); i++)
  {
    while (!threads[i]->message)
      threads[i] = threads[i]->child;
  }

  if (Sort & SORT_REVERSE)
    rc = threads[0]->message->msgno - (threads[1] ? threads[1]->message->msgno : -1);
  else
    rc = (threads[1] ? threads[1]->message->msgno : ctx->msgcount) -
         threads[0]->message->msgno;

  if (flag)
    rc += 1;

  return rc;
}

struct Hash *mutt_make_id_hash(struct Context *ctx)
{
  struct Header *hdr = NULL;
  struct Hash *hash = NULL;

  hash = hash_create(ctx->msgcount * 2, 0);

  for (int i = 0; i < ctx->msgcount; i++)
  {
    hdr = ctx->hdrs[i];
    if (hdr->env->message_id)
      hash_insert(hash, hdr->env->message_id, hdr);
  }

  return hash;
}

static void clean_references(struct MuttThread *brk, struct MuttThread *cur)
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
        if (mutt_strcasecmp(ref->data, p->message->env->message_id) == 0)
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

void mutt_break_thread(struct Header *hdr)
{
  mutt_list_free(&hdr->env->in_reply_to);
  mutt_list_free(&hdr->env->references);
  hdr->env->irt_changed = hdr->env->refs_changed = hdr->changed = true;

  clean_references(hdr->thread, hdr->thread->child);
}

static bool link_threads(struct Header *parent, struct Header *child, struct Context *ctx)
{
  if (child == parent)
    return false;

  mutt_break_thread(child);
  mutt_list_insert_head(&child->env->in_reply_to, safe_strdup(parent->env->message_id));
  mutt_set_flag(ctx, child, MUTT_TAG, 0);

  child->env->irt_changed = child->changed = true;
  return true;
}

int mutt_link_threads(struct Header *cur, struct Header *last, struct Context *ctx)
{
  bool changed = false;

  if (!last)
  {
    for (int i = 0; i < ctx->vcount; i++)
      if (ctx->hdrs[Context->v2r[i]]->tagged)
        changed |= link_threads(cur, ctx->hdrs[Context->v2r[i]], ctx);
  }
  else
    changed = link_threads(cur, last, ctx);

  return changed;
}
