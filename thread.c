/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#include "mutt.h"
#include "sort.h"

#include <string.h>
#include <ctype.h>

#define VISIBLE(hdr, ctx) (hdr->virtual >= 0 || (hdr->collapsed && (!ctx->pattern || hdr->limited)))

/* determine whether a is a descendant of b */
static int is_descendant (THREAD *a, THREAD *b)
{
  while (a)
  {
    if (a == b)
      return (1);
    a = a->parent;
  }
  return (0);
}

/* Determines whether to display a message's subject. */
static int need_display_subject (CONTEXT *ctx, HEADER *hdr)
{
  THREAD *tmp, *tree = hdr->thread;

  /* if our subject is different from our parent's, display it */
  if (hdr->subject_changed)
    return (1);

  /* if our subject is different from that of our closest previously displayed
   * sibling, display the subject */
  for (tmp = tree->prev; tmp; tmp = tmp->prev)
  {
    hdr = tmp->message;
    if (hdr && VISIBLE (hdr, ctx))
    {
      if (hdr->subject_changed)
	return (1);
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
      if (VISIBLE (hdr, ctx))
	return (0);
      else if (hdr->subject_changed)
	return (1);
    }
  }
  
  /* if we have no visible parent or previous sibling, display the subject */
  return (1);
}

/* determines whether a later sibling or the child of a later 
 * sibling is displayed.  
 */

static int is_next_displayed (CONTEXT *ctx, THREAD *tree)
{
  int depth = 0;
  HEADER *hdr;

  if ((tree = tree->next) == NULL)
    return (0);

  FOREVER
  {
    hdr = tree->message;
    if (hdr && VISIBLE (hdr, ctx))
      return (1);

    if (tree->child)
    {
      tree = tree->child;
      depth++;
    }
    else
    {
      while (!tree->next && depth > 0)
      {
	tree = tree->parent;
	depth--;
      }
      if ((tree = tree->next) == NULL)
	break;
    }
  }
  return (0);
}


/* Since the graphics characters have a value >255, I have to resort to
 * using escape sequences to pass the information to print_enriched_string().
 * These are the macros M_TREE_* defined in mutt.h.
 *
 * ncurses should automatically use the default ASCII characters instead of
 * graphics chars on terminals which don't support them (see the man page
 * for curs_addch).
 */
void mutt_linearize_tree (CONTEXT *ctx, int linearize)
{
  char *pfx = NULL, *mypfx = NULL, *arrow = NULL, *myarrow = NULL;
  char corner = Sort & SORT_REVERSE ? M_TREE_ULCORNER : M_TREE_LLCORNER;
  int depth = 0, start_depth = 0, max_depth = 0, max_width = 0;
  int nextdisp = 0, visible;
  THREAD *tree = ctx->tree;
  HEADER **array = ctx->hdrs + (Sort & SORT_REVERSE ? ctx->msgcount - 1 : 0);
  HEADER *hdr;

  FOREVER
  {
    hdr = tree->message;

    if (hdr)
    {
      if ((visible = VISIBLE (hdr, ctx)) !=  0)
	hdr->display_subject = need_display_subject (ctx, hdr);

      safe_free ((void **) &hdr->tree);
    }
    else
      visible = 0;

    if (depth >= max_depth)
      safe_realloc ((void **) &pfx,
		    (max_depth += 32) * 2 * sizeof (char));

    if (depth - start_depth >= max_width)
      safe_realloc ((void **) &arrow,
		    (max_width += 16) * 2 * sizeof (char));

    if (depth)
    {
      myarrow = arrow + (depth - start_depth - (start_depth ? 0 : 1)) * 2;
      nextdisp = is_next_displayed (ctx, tree);
      
      if (depth && start_depth == depth)
	myarrow[0] = nextdisp ? M_TREE_LTEE : corner;
      else
	myarrow[0] = tree->parent->message ? M_TREE_HIDDEN : M_TREE_MISSING;
      myarrow[1] = (tree->fake_thread) ? M_TREE_STAR : M_TREE_HLINE;
      if (visible)
      {
	myarrow[2] = M_TREE_RARROW;
	myarrow[3] = 0;
      }

      if (visible)
      {
	hdr->tree = safe_malloc ((2 + depth * 2) * sizeof (char));
	if (start_depth > 1)
	{
	  strncpy (hdr->tree, pfx, (start_depth - 1) * 2);
	  strfcpy (hdr->tree + (start_depth - 1) * 2,
		   arrow, (2 + depth - start_depth) * 2);
	}
	else
	  strfcpy (hdr->tree, arrow, 2 + depth * 2);
      }
    }

    if (linearize && hdr)
    {
      *array = hdr;
      array += Sort & SORT_REVERSE ? -1 : 1;
    }

    if (tree->child)
    {
      if (depth)
      {
	mypfx = pfx + (depth - 1) * 2;
	mypfx[0] = nextdisp ? M_TREE_VLINE : M_TREE_SPACE;
	mypfx[1] = M_TREE_SPACE;
      }
      if (depth || !option (OPTHIDEMISSING)
	  || tree->message || tree->child->next)
	depth++;
      if (visible)
        start_depth = depth;
      tree = tree->child;
      hdr = tree->message;
    }
    else
    {
      while (!tree->next && tree->parent)
      {
	if (hdr && VISIBLE (hdr, ctx))
	  start_depth = depth;
	tree = tree->parent;
	hdr = tree->message;
	if (depth)
	{
	  if (start_depth == depth)
	    start_depth--;
	  depth--;
	}
      }
      if (hdr && VISIBLE (hdr, ctx))
	start_depth = depth;
      tree = tree->next;
      if (!tree)
	break;
    }
  }

  safe_free ((void **) &pfx);
  safe_free ((void **) &arrow);
}

/* inserts `msg' into the list `tree' using an insertion sort.  this function
 * assumes that `tree' is the first element in the list, and not some
 * element in the middle of the list.
 */

static void insert_message (THREAD **tree, THREAD *msg)
{
#if 0 /* XXX */
  THREAD *tmp;
#endif

  /* NOTE: we do NOT clear the `msg->child' link here because when we do
   * the pseudo-threading, we want to preserve any sub-threads.  So we clear
   * the `msg->child' in the main routine where we know it is safe to do.
   */

  /* if there are no elements in the list, just add it and return */
  if (!*tree)
  {
    msg->prev = msg->next = NULL;
    *tree = msg;
    return;
  }

  /* check to see if this message belongs at the beginning of the list */
#if 0 /* XXX */
  if (!sortFunc || sortFunc ((void *) &msg, (void *) tree) < 0)
  {
#endif
    (*tree)->prev = msg;
    msg->next = *tree;
    msg->prev = NULL;
    *tree = msg;
    return;
#if 0 /* XXX */
  }
  
  /* search for the correct spot in the list to insert */
  for (tmp = *tree; tmp->next; tmp = tmp->next)
    if (sortFunc ((void *) &msg, (void *) &tmp->next) < 0)
    {
      msg->prev = tmp;
      msg->next = tmp->next;
      tmp->next->prev = msg;
      tmp->next = msg;
      return;
    }

  /* did not insert yet, so add this message to the end of the list */
  tmp->next = msg;
  msg->prev = tmp;
  msg->next = NULL;
#endif
}

static LIST *make_subject_list (THREAD *cur, time_t *dateptr)
{
  THREAD *start = cur;
  ENVELOPE *env;
  time_t thisdate;
  LIST *curlist, *oldlist, *newlist, *subjects = NULL;
  int rc = 0;
  
  FOREVER
  {
    while (!cur->message)
      cur = cur->child;

    if (dateptr)
    {
      thisdate = option (OPTTHREADRECEIVED)
	? cur->message->received : cur->message->date_sent;
      if (!*dateptr || thisdate < *dateptr)
	*dateptr = thisdate;
    }

    env = cur->message->env;
    if (env->real_subj &&
	((env->real_subj != env->subject) || (!option (OPTSORTRE))))
    {
      for (curlist = subjects, oldlist = NULL;
	   curlist; oldlist = curlist, curlist = curlist->next)
      {
	rc = mutt_strcmp (env->real_subj, curlist->data);
	if (rc >= 0)
	  break;
      }
      if (!curlist || rc > 0)
      {
	newlist = safe_calloc (1, sizeof (LIST));
	newlist->data = env->real_subj;
	if (oldlist)
	{
	  newlist->next = oldlist->next;
	  oldlist->next = newlist;
	}
	else
	{
	  newlist->next = subjects;
	  subjects = newlist;
	}
      }
    }

    while (!cur->next && cur != start)
    {
      cur = cur->parent;
    }
    if (cur == start)
      break;
    cur = cur->next;
  }

  return (subjects);
}

/* find the best possible match for a parent mesage based upon subject.
 * if there are multiple matches, the one which was sent the latest, but
 * before the current message, is used. 
 */
static THREAD *find_subject (CONTEXT *ctx, THREAD *cur)
{
  struct hash_elem *ptr;
  THREAD *tmp, *last = NULL;
  int hash;
  LIST *subjects = NULL, *oldlist;
  time_t date = 0;  

  subjects = make_subject_list (cur, &date);

  while (subjects)
  {
    hash = hash_string ((unsigned char *) subjects->data, ctx->subj_hash->nelem);
    for (ptr = ctx->subj_hash->table[hash]; ptr; ptr = ptr->next)
    {
      tmp = ((HEADER *) ptr->data)->thread;
      if (tmp != cur &&			/* don't match the same message */
	  !tmp->fake_thread &&		/* don't match pseudo threads */
	  tmp->message->subject_changed &&	/* only match interesting replies */
	  !is_descendant (tmp, cur) &&	/* don't match in the same thread */
	  (date >= (option (OPTTHREADRECEIVED) ?
		    tmp->message->received :
		    tmp->message->date_sent)) &&
	  (!last ||
	   (option (OPTTHREADRECEIVED) ?
	    (last->message->received < tmp->message->received) :
	    (last->message->date_sent < tmp->message->date_sent))) &&
	  tmp->message->env->real_subj &&
	  mutt_strcmp (subjects->data, tmp->message->env->real_subj) == 0)
	last = tmp; /* best match so far */
    }

    oldlist = subjects;
    subjects = subjects->next;
    safe_free ((void **) &oldlist);
  }
  return (last);
}

static void unlink_message (THREAD **top, THREAD *cur)
{
  if (cur->prev)
  {
    cur->prev->next = cur->next;

    if (cur->next)
      cur->next->prev = cur->prev;
  }
  else
  {
    if (cur->next)
      cur->next->prev = NULL;
    *top = cur->next;
  }
}

static void pseudo_threads (CONTEXT *ctx)
{
  THREAD *tree = ctx->tree, *top = tree;
  THREAD *tmp, *cur, *parent, *curchild, *nextchild;

  if (!ctx->subj_hash)
    ctx->subj_hash = mutt_make_subj_hash (ctx);

  while (tree)
  {
    cur = tree;
    tree = tree->next;
    if ((parent = find_subject (ctx, cur)) != NULL)
    {
      /* detach this message from its current location */
      unlink_message (&top, cur);

      cur->fake_thread = 1;
      cur->parent = parent;
      insert_message (&parent->child, cur);
      
      tmp = cur;

      FOREVER
      {
	while (!tmp->message)
	  tmp = tmp->child;

	if (tmp == cur
	    || !mutt_strcmp (tmp->message->env->real_subj,
			     parent->message->env->real_subj))
	{
	  tmp->message->subject_changed = 0;

	  /* if the message we're attaching has pseudo-children, they
	   * need to be attached to its parent, so move them up a level.  */
	  for (curchild = tmp->child; curchild; )
	  {
	    nextchild = curchild->next;
	    if (curchild->fake_thread)
	    {
	      /* detach this message from its current location */
	      unlink_message (&tmp->child, curchild);
	      curchild->parent = parent;
	      /* we care in this case that insert_message inserts the
	       * message at the beginning of the list! */
	      insert_message (&parent->child, curchild);
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


void mutt_clear_threads (CONTEXT *ctx)
{
  int i;

  for (i = 0; i < ctx->msgcount; i++)
  {
    ctx->hdrs[i]->thread = NULL;
  }
  ctx->tree = NULL;

  if (ctx->thread_hash)
    hash_destroy (&ctx->thread_hash, *free);
}

int compare_threads (const void *a, const void *b)
{
  static sort_t *sort_func = NULL;

  if (a || b)
    return ((*sort_func) (&(*((THREAD **) a))->sort_key,
			  &(*((THREAD **) b))->sort_key));
  /* a hack to let us reset sort_func even though we can't
   * have extra arguments because of qsort
   */
  else
  {
    sort_func = NULL;
    sort_func = mutt_get_sort_func (Sort);
    return (sort_func ? 1 : 0);
  }
}

THREAD *mutt_sort_subthreads (THREAD *thread)
{
  THREAD **array, *sort_key;
  int i, array_size;
  
  Sort ^= SORT_REVERSE;
  if (!thread || !compare_threads (NULL, NULL))
    return thread;

  /* we put things into the array backwards to save some cycles,
   * but we want to have to move less stuff around if we're 
   * resorting, so we sort backwards and then put them back
   * in reverse order so they're forwards
   */

  array = safe_malloc ((array_size = 256) * sizeof (THREAD *));
  while (1)
  {
    while (thread->child)
    {
      thread->sort_key = thread->message;
      thread = thread->child;
    }

    while (thread->next && !thread->child)
    {
      thread->sort_key = thread->message;
      thread = thread->next;
    }

    if (thread->child)
      continue;

    thread->sort_key = thread->message;

    while (!thread->next)
    {
      if (thread->prev)
      {
	for (i = 0; thread; i++, thread = thread->prev)
	{
	  if (i >= array_size)
	    safe_realloc ((void **) &array, (array_size *= 2) * sizeof (THREAD *));

	  array[i] = thread;
	}

	qsort ((void *) array, i, sizeof (THREAD *), *compare_threads);

	array[0]->next = NULL;

	thread = array[i - 1];
	thread->prev = NULL;

	if (thread->parent)
	{
	  thread->parent->child = thread;
	  sort_key = array[(!(Sort & SORT_LAST) ^ !(Sort & SORT_REVERSE)) ? i - 1 : 0];
	}
	
	while (--i)
	{
	  array[i - 1]->prev = array[i];
	  array[i]->next = array[i - 1];
	}
      }
      else
	sort_key = thread;


      if (thread->parent)
      {
	if (Sort & SORT_LAST)
	{
	  if (!thread->parent->sort_key
	      || ((((Sort & SORT_REVERSE) ? 1 : -1)
		   * compare_threads ((void *) &thread->parent,
				      (void *) &sort_key))
		  > 0))
	    thread->parent->sort_key = sort_key->sort_key;
	}
	else if (!thread->parent->sort_key)
	  thread->parent->sort_key = sort_key->sort_key;

	thread = thread->parent;
      }
      else
      {
	Sort ^= SORT_REVERSE;
	return (thread);
      }
    }

    thread = thread->next;
  }
}

void mutt_sort_threads (CONTEXT *ctx, int init)
{
  HEADER *cur;
  int i, oldsort, using_refs = 0;
  HASH *id_hash;
  THREAD *thread, *new, *tmp;
  LIST *ref = NULL;
  
  /* set Sort to the secondary method to support the set sort_aux=reverse-*
   * settings.  The sorting functions just look at the value of
   * SORT_REVERSE
   */
  oldsort = Sort;
  Sort = SortAux;
  
  id_hash = hash_create (ctx->msgcount * 2);
  ctx->tree = safe_calloc (1, sizeof (THREAD));
  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    thread = safe_calloc (1, sizeof (THREAD));
    thread->message = cur;
    cur->thread = thread;
    hash_insert (id_hash, cur->env->message_id ? cur->env->message_id : "", thread, 1);
  }

  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    thread = cur->thread;
    using_refs = 0;

    while (1)
    {
      if (using_refs == 0)
      {
	/* look at the beginning of in-reply-to: */
	if ((ref = cur->env->in_reply_to) != NULL)
	{
	  using_refs = 1;
	}
	else
	{
	  ref = cur->env->references;
	  using_refs = 2;
	}
      }
      else if (using_refs == 1)
      {
	/* if there's no references header, use all the in-reply-to:
	 * data that we have.  otherwise, use the first reference
	 * if it's different than the first in-reply-to, otherwise use
	 * the second reference (since at least eudora puts the most
	 * recent reference in in-reply-to and the rest in references
	 */
	if (!cur->env->references)
	  ref = ref->next;
	else
	{
	  if (mutt_strcmp (ref->data, cur->env->references->data))
	    ref = cur->env->references;
	  else
	    ref = cur->env->references->next;
	  
	  using_refs = 2;
	}
      }
      else
	ref = ref->next;
      
      if (!ref)
	break;

      if ((new = hash_find (id_hash, ref->data)) == NULL)
      {
	new = safe_calloc (1, sizeof (THREAD));
	hash_insert (id_hash, ref->data, new, 1);
      }
      else if (is_descendant (new, thread)) /* no loops! */
	break;

      /* make new the parent of thread */
      if (thread->parent)
      {
	/* this can only happen if thread is attached directly to ctx->tree.
	 * detach it.
	 */
	if (thread->prev)
	  thread->prev->next = thread->next;
	if (thread->next)
	  thread->next->prev = thread->prev;
	if (thread->parent->child == thread)
	  thread->parent->child = thread->next;
      }

      thread->parent = new;
      thread->prev = NULL;
      if ((thread->next = new->child) != NULL)
	thread->next->prev = thread;
      new->child = thread;

      thread = new;

      if (thread->message || (thread->parent && thread->parent != ctx->tree))
	break;
    }

    if (!thread->parent)
    {
      if ((thread->next = ctx->tree->child) != NULL)
      {
	thread->next->prev = thread;
      }
      ctx->tree->child = thread;
      thread->parent = ctx->tree;
    }
  }

  for (thread = ctx->tree->child; thread; thread = thread->next)
  {
    thread->parent = NULL;
  }
  
  tmp = ctx->tree;
  ctx->tree = ctx->tree->child;
  safe_free ((void **) &tmp);

  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    tmp = cur->thread->parent;
    while (tmp && !tmp->message)
    {
      tmp = tmp->parent;
    }

    if (!tmp)
      cur->subject_changed = 1;
    else if (cur->env->real_subj && tmp->message->env->real_subj)
      cur->subject_changed = mutt_strcmp (cur->env->real_subj, tmp->message->env->real_subj) ? 1 : 0;
    else
      cur->subject_changed = (cur->env->real_subj || tmp->message->env->real_subj) ? 1 : 0;
  }

  if (!option (OPTSTRICTTHREADS))
    pseudo_threads (ctx);

  ctx->tree = mutt_sort_subthreads (ctx->tree);

  /* restore the oldsort order. */
  Sort = oldsort;

  /* Put the list into an array. */
  mutt_linearize_tree (ctx, 1);

  ctx->thread_hash = id_hash;
}

static HEADER *find_virtual (THREAD *cur)
{
  THREAD *top;

  if (cur->message && cur->message->virtual >= 0)
    return (cur->message);

  top = cur;
  if ((cur = cur->child) == NULL)
    return (NULL);

  FOREVER
  {
    if (cur->message && cur->message->virtual >= 0)
      return (cur->message);

    if (cur->child)
      cur = cur->child;
    else if (cur->next)
      cur = cur->next;
    else
    {
      while (!cur->next)
      {
	cur = cur->parent;
	if (cur == top)
	  return (NULL);
      }
      cur = cur->next;
    }
    /* not reached */
  }
}

int _mutt_aside_thread (HEADER *hdr, short dir, short subthreads)
{
  THREAD *cur;
  HEADER *tmp;

  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error _("Threading is not enabled.");
    return (hdr->virtual);
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
	return (-1);
      tmp = find_virtual (cur);
    } while (!tmp);
  }
  else
  {
    do
    { 
      cur = cur->prev;
      if (!cur)
	return (-1);
      tmp = find_virtual (cur);
    } while (!tmp);
  }

  return (tmp->virtual);
}

int mutt_parent_message (CONTEXT *ctx, HEADER *hdr)
{
  THREAD *thread;

  if ((Sort & SORT_MASK) != SORT_THREADS)
  {
    mutt_error _("Threading is not enabled.");
    return (hdr->virtual);
  }

  thread = hdr->thread;
  while ((thread = thread->parent))
  {
    hdr = thread->message;
    if (hdr && VISIBLE (hdr, ctx))
      return (hdr->virtual);
  }
  
  mutt_error _("Parent message is not available.");
  return (-1);
}

void mutt_set_virtual (CONTEXT *ctx)
{
  int i;
  HEADER *cur;

  ctx->vcount = 0;
  ctx->vsize = 0;

  for (i = 0; i < ctx->msgcount; i++)
  {
    cur = ctx->hdrs[i];
    if (cur->virtual >= 0)
    {
      cur->virtual = ctx->vcount;
      ctx->v2r[ctx->vcount] = i;
      ctx->vcount++;
      ctx->vsize += cur->content->length + cur->content->offset - cur->content->hdr_offset;
      cur->num_hidden = mutt_get_hidden (ctx, cur);
    }
  }
}

int _mutt_traverse_thread (CONTEXT *ctx, HEADER *cur, int flag)
{
  THREAD *thread, *top;
  HEADER *roothdr = NULL;
  int final, reverse = (Sort & SORT_REVERSE), minmsgno;
  int num_hidden = 0, new = 0, old = 0;
  int min_unread_msgno = INT_MAX, min_unread = cur->virtual;
#define CHECK_LIMIT (!ctx->pattern || cur->limited)

  if ((Sort & SORT_MASK) != SORT_THREADS && !(flag & M_THREAD_GET_HIDDEN))
  {
    mutt_error (_("Threading is not enabled."));
    return (cur->virtual);
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

  if (cur->virtual == -1 && CHECK_LIMIT)
    num_hidden++;

  if (flag & (M_THREAD_COLLAPSE | M_THREAD_UNCOLLAPSE))
  {
    cur->pair = 0; /* force index entry's color to be re-evaluated */
    cur->collapsed = flag & M_THREAD_COLLAPSE;
    if (cur->virtual != -1)
    {
      roothdr = cur;
      if (flag & M_THREAD_COLLAPSE)
	final = roothdr->virtual;
    }
  }

  if ((thread = thread->child) == NULL)
  {
    /* return value depends on action requested */
    if (flag & (M_THREAD_COLLAPSE | M_THREAD_UNCOLLAPSE))
      return (final);
    else if (flag & M_THREAD_UNREAD)
      return ((old && new) ? new : (old ? old : new));
    else if (flag & M_THREAD_GET_HIDDEN)
      return (num_hidden);
    else if (flag & M_THREAD_NEXT_UNREAD)
      return (min_unread);
  }
  
  FOREVER
  {
    cur = thread->message;

    if (cur)
    {
      if (flag & (M_THREAD_COLLAPSE | M_THREAD_UNCOLLAPSE))
      {
	cur->pair = 0; /* force index entry's color to be re-evaluated */
	cur->collapsed = flag & M_THREAD_COLLAPSE;
	if (!roothdr && CHECK_LIMIT)
	{
	  roothdr = cur;
	  if (flag & M_THREAD_COLLAPSE)
	    final = roothdr->virtual;
	}

	if (reverse && (flag & M_THREAD_COLLAPSE) && (cur->msgno < minmsgno) && CHECK_LIMIT)
	{
	  minmsgno = cur->msgno;
	  final = cur->virtual;
	}

	if (flag & M_THREAD_COLLAPSE)
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

      if (cur->virtual == -1 && CHECK_LIMIT)
	num_hidden++;
    }

    if (thread->child)
      thread = thread->child;
    else if (thread->next)
      thread = thread->next;
    else
    {
      int done = 0;
      while (!thread->next)
      {
	thread = thread->parent;
	if (thread == top)
	{
	  done = 1;
	  break;
	}
      }
      if (done)
	break;
      thread = thread->next;
    }
  }

  /* return value depends on action requested */
  if (flag & (M_THREAD_COLLAPSE | M_THREAD_UNCOLLAPSE))
    return (final);
  else if (flag & M_THREAD_UNREAD)
    return ((old && new) ? new : (old ? old : new));
  else if (flag & M_THREAD_GET_HIDDEN)
    return (num_hidden+1);
  else if (flag & M_THREAD_NEXT_UNREAD)
    return (min_unread);

  return (0);
#undef CHECK_LIMIT
}


/* if flag is 0, we want to know how many messages
 * are in the thread.  if flag is 1, we want to know
 * our position in the thread. */
int mutt_messages_in_thread (HEADER *hdr, int flag)
{
  THREAD *threads[2];
  int i;

  if ((Sort & SORT_MASK) != SORT_THREADS)
    return (1);

  threads[0] = hdr->thread;
  while (threads[0]->parent)
    threads[0] = threads[0]->parent;
  while (threads[0]->prev)
    threads[0] = threads[0]->prev;

  if (flag)
    threads[1] = hdr->thread;
  else
    threads[1] = threads[0]->next;

  for (i = 0; i < flag ? 1 : 2; i++)
  {
    while (!threads[i]->message)
      threads[i] = threads[i]->child;
  } 

  return (((Sort & SORT_REVERSE ? -1 : 1)
	   * threads[1]->message->msgno - threads[0]->message->msgno) + (flag ? 1 : 0));
}

HASH *mutt_make_id_hash (CONTEXT *ctx)
{
  int i;
  HEADER *hdr;
  HASH *hash;

  hash = hash_create (ctx->msgcount * 2);

  for (i = 0; i < ctx->msgcount; i++)
  {
    hdr = ctx->hdrs[i];
    if (hdr->env->message_id)
      hash_insert (hash, hdr->env->message_id, hdr, 0);
  }

  return hash;
}

HASH *mutt_make_subj_hash (CONTEXT *ctx)
{
  int i;
  HEADER *hdr;
  HASH *hash;

  hash = hash_create (ctx->msgcount * 2);

  for (i = 0; i < ctx->msgcount; i++)
  {
    hdr = ctx->hdrs[i];
    if (hdr->env->real_subj)
      hash_insert (hash, hdr->env->real_subj, hdr, 1);
  }

  return hash;
}
