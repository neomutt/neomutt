/*	Copyright (C) 1995, 1996 Tom Lord
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */



#include "rxall.h"
#include "rxanal.h"
#include "rxbitset.h"
#include "rxsuper.h"




#ifdef __STDC__
int
rx_posix_analyze_rexp (struct rexp_node *** subexps,
		       size_t * re_nsub,
		       struct rexp_node * node,
		       int id)
#else
int
rx_posix_analyze_rexp (subexps, re_nsub, node, id)
     struct rexp_node *** subexps;
     size_t * re_nsub;
     struct rexp_node * node;
     int id;
#endif
{
  if (node)
    {
      size_t this_subexp;
      if (node->type == r_parens)
	{
	  if (node->params.intval >= 0)
	    {
	      this_subexp = *re_nsub;
	      ++*re_nsub;
	      if (!*subexps)
		*subexps = (struct rexp_node **)malloc (sizeof (struct rexp_node *) * *re_nsub);
	      else
		*subexps = (struct rexp_node **)realloc (*subexps,
							 sizeof (struct rexp_node *) * *re_nsub);
	    }
	}
      if (node->params.pair.left)
	id = rx_posix_analyze_rexp (subexps, re_nsub, node->params.pair.left, id);
      if (node->params.pair.right)
	id = rx_posix_analyze_rexp (subexps, re_nsub, node->params.pair.right, id);
      switch (node->type)
	{
	case r_cset:
	  node->len = 1;
	  node->observed = 0;
	  break;
 	case r_string:
 	  node->len = node->params.cstr.len;
 	  node->observed = 0;
 	  break;
	case r_cut:
	  node->len = 0;
	  node->observed = 0;
	  break;
	case r_concat:
	case r_alternate:
	  {
	    int lob, rob;
	    int llen, rlen;
	    lob = (!node->params.pair.left ? 0 : node->params.pair.left->observed);
	    rob = (!node->params.pair.right ? 0 : node->params.pair.right->observed);
	    llen = (!node->params.pair.left ? 0 : node->params.pair.left->len);
	    rlen = (!node->params.pair.right ? 0 : node->params.pair.right->len);
	    node->len = ((llen >= 0) && (rlen >= 0)
			 ? ((node->type == r_concat)
			    ? llen + rlen
			    : ((llen == rlen) ? llen : -1))
			 : -1);
	    node->observed = lob || rob;
	    break;
	  }
	case r_opt:
	case r_star:
	case r_plus:
	  node->len = -1;
	  node->observed = (node->params.pair.left
			    ? node->params.pair.left->observed
			    : 0);
	  break;

	case  r_interval:
	  node->len = -1;
	  node->observed = 1;
	  break;

	case r_parens:
	  if (node->params.intval >= 0)
	    {
	      node->observed = 1;
	      (*subexps)[this_subexp] = node;
	    }
	  else
	    node->observed = (node->params.pair.left
			      ? node->params.pair.left->observed
			      : 0);
	  node->len = (node->params.pair.left
		       ? node->params.pair.left->len
		       : 0);
	  break;
	case r_context:
	  switch (node->params.intval)
	    {
	    default:
	      node->observed = 1;
	      node->len = -1;
	      break;
	    case '^':
	    case '$':
	    case '=':
	    case '<':
	    case '>':
	    case 'b':
	    case 'B':
	    case '`':
	    case '\'':
	      node->observed = 1;
	      node->len = 0;
	      break;
	    }
	  break;
	}
      if (node->observed)
	node->id = id++;
      return id;
    }
  return id;
}

/* Returns 0 unless the pattern can match the empty string. */
#ifdef __STDC__
int
rx_fill_in_fastmap (int cset_size, unsigned char * map, struct rexp_node * exp)
#else
int
rx_fill_in_fastmap (cset_size, map, exp)
     int cset_size;
     unsigned char * map;
     struct rexp_node * exp;
#endif
{
  if (!exp)
    {
    can_match_empty:
      {
	int x;
	for (x = 0; x < cset_size; ++x)
	  map[x] = 1;
      }
      return 1;
    }
  
  switch (exp->type)
    {
    case r_cset:
      {
	int x;
	int most;
	
	most = exp->params.cset_size;
	for (x = 0; x < most; ++x)
	  if (RX_bitset_member (exp->params.cset, x))
	    map[x] = 1;
      }
      return 0;

    case r_string:
      if (exp->params.cstr.len)
 	{
	  map[exp->params.cstr.contents[0]] = 1;
	  return 0;
 	}
      else
	return 1;

    case r_cut:
      return 1;
      

    case r_concat:
      return rx_fill_in_fastmap (cset_size, map, exp->params.pair.left);

      /* Why not the right branch?  If the left branch
       * can't be empty it doesn't matter.  If it can, then
       * the fastmap is already saturated, and again, the
       * right branch doesn't matter.
       */


    case r_alternate:
      return (  rx_fill_in_fastmap (cset_size, map, exp->params.pair.left)
	      | rx_fill_in_fastmap (cset_size, map, exp->params.pair.right));

    case r_parens:
    case r_plus:
      return rx_fill_in_fastmap (cset_size, map, exp->params.pair.left);

    case r_opt:
    case r_star:
      goto can_match_empty;
      /* Why not the subtree?  These operators already saturate
       * the fastmap. 
       */

    case r_interval:
      if (exp->params.intval == 0)
	goto can_match_empty;
      else
	return rx_fill_in_fastmap (cset_size, map, exp->params.pair.left);
      
    case r_context:
      goto can_match_empty;
    }

  /* this should never happen but gcc seems to like it */
  return 0;
  
}


#ifdef __STDC__
int
rx_is_anchored_p (struct rexp_node * exp)
#else
int
rx_is_anchored_p (exp)
     struct rexp_node * exp;
#endif
{
  if (!exp)
    return 0;

  switch (exp->type)
    {
    case r_opt:
    case r_star:
    case r_cset:
    case r_string:
    case r_cut:
      return 0;

    case r_parens:
    case r_plus:
    case r_concat:
      return rx_is_anchored_p (exp->params.pair.left);

    case r_alternate:
      return (   rx_is_anchored_p (exp->params.pair.left)
	      && rx_is_anchored_p (exp->params.pair.right));


    case r_interval:
      if (exp->params.intval == 0)
	return 0;
      else
	return rx_is_anchored_p (exp->params.pair.left);
      
    case r_context:
      return (exp->params.intval == '^');
    }

  /* this should never happen but gcc seems to like it */
  return 0;
}



#ifdef __STDC__
enum rx_answers
rx_start_superstate (struct rx_classical_system * frame)
#else
enum rx_answers
rx_start_superstate (frame)
     struct rx_classical_system * frame;
#endif
{
  struct rx_superset * start_contents;
  struct rx_nfa_state_set * start_nfa_set;

  if (frame->rx->start_set)
    start_contents = frame->rx->start_set;
  else
    {
      {
	struct rx_possible_future * futures;
	futures = rx_state_possible_futures (frame->rx, frame->rx->start_nfa_states);

	if (!futures)
	  return rx_bogus;

	if (futures->next)
	  return rx_start_state_with_too_many_futures;

	start_nfa_set = futures->destset;
      }
      
      start_contents =
	rx_superstate_eclosure_union (frame->rx,
				      rx_superset_cons (frame->rx, 0, 0),
				      start_nfa_set);
      
      if (!start_contents)
	return rx_bogus;
	    
      start_contents->starts_for = frame->rx;
      frame->rx->start_set = start_contents;
    }

  if (   start_contents->superstate
      && (start_contents->superstate->rx_id == frame->rx->rx_id))
    {
      if (frame->state)
	{
	  rx_unlock_superstate (frame->rx, frame->state);
	}
      frame->state = start_contents->superstate;
      /* The cached superstate may be in a semifree state.
       * We need to lock it and preserve the invariant
       * that a locked superstate is never semifree.
       * So refresh it.
       */
      rx_refresh_this_superstate (frame->rx->cache, frame->state);
      rx_lock_superstate (frame->rx, frame->state);
      return rx_yes;
    }
  else
    {
      struct rx_superstate * state;

      rx_protect_superset (frame->rx, start_contents);
      state = rx_superstate (frame->rx, start_contents);
      rx_release_superset (frame->rx, start_contents);
      if (!state)
	return rx_bogus;
      if (frame->state)
	{
	  rx_unlock_superstate (frame->rx, frame->state);
	}
      frame->state = state;
      rx_lock_superstate (frame->rx, frame->state);
      return rx_yes;
    }
}




#ifdef __STDC__
enum rx_answers
rx_fit_p (struct rx_classical_system * frame, unsigned const char * burst, int len)
#else
enum rx_answers
rx_fit_p (frame, burst, len)
     struct rx_classical_system * frame;
     unsigned const char * burst;
     int len;
#endif
{
  struct rx_inx * inx_table;
  struct rx_inx * inx;

  if (!frame->state)
    return rx_bogus;

  if (!len)
    {
      frame->final_tag = frame->state->contents->is_final;
      return (frame->state->contents->is_final
	      ? rx_yes
	      : rx_no);
    }

  inx_table = frame->state->transitions;
  rx_unlock_superstate (frame->rx, frame->state);

  while (len--)
    {
      struct rx_inx * next_table;

      inx = inx_table + *burst;
      next_table = (struct rx_inx *)inx->data;
      while (!next_table)
	{
	  struct rx_superstate * state;
	  state = ((struct rx_superstate *)
		   ((char *)inx_table
		    - ((unsigned long)
		       ((struct rx_superstate *)0)->transitions)));

	  switch ((int)inx->inx)
	    {
	    case rx_backtrack:
	      /* RX_BACKTRACK means that we've reached the empty
	       * superstate, indicating that match can't succeed
	       * from this point.
	       */
	      frame->state = 0;
	      return rx_no;
	    
	    case rx_cache_miss:
	      /* Because the superstate NFA is lazily constructed,
	       * and in fact may erode from underneath us, we sometimes
	       * have to construct the next instruction from the hard way.
	       * This invokes one step in the lazy-conversion.
	       */
	      inx = 
		rx_handle_cache_miss
		  (frame->rx, state, *burst, inx->data_2);

	      if (!inx)
		{
		  frame->state = 0;
		  return rx_bogus;
		}
	      next_table = (struct rx_inx *)inx->data;
	      continue;
		

	      /* No other instructions are legal here.
	       * (In particular, this function does not handle backtracking
	       * or the related instructions.)
	       */
	    default:
	      frame->state = 0;
	      return rx_bogus;
	  }
	}
      inx_table = next_table;
      ++burst;
    }

  if (inx->data_2)		/* indicates a final superstate */
    {
      frame->final_tag = (int)inx->data_2;
      frame->state = ((struct rx_superstate *)
		      ((char *)inx_table
		       - ((unsigned long)
			  ((struct rx_superstate *)0)->transitions)));
      rx_lock_superstate (frame->rx, frame->state);
      return rx_yes;
    }
  frame->state = ((struct rx_superstate *)
		  ((char *)inx_table
		   - ((unsigned long)
		      ((struct rx_superstate *)0)->transitions)));
  rx_lock_superstate (frame->rx, frame->state);
  return rx_no;
}




#ifdef __STDC__
enum rx_answers
rx_advance (struct rx_classical_system * frame, unsigned const char * burst, int len)
#else
enum rx_answers
rx_advance (frame, burst, len)
     struct rx_classical_system * frame;
     unsigned const char * burst;
     int len;
#endif
{
  struct rx_inx * inx_table;

  if (!frame->state)
    return rx_bogus;

  if (!len)
    return rx_yes;

  inx_table = frame->state->transitions;
  rx_unlock_superstate (frame->rx, frame->state);

  while (len--)
    {
      struct rx_inx * inx;
      struct rx_inx * next_table;

      inx = inx_table + *burst;
      next_table = (struct rx_inx *)inx->data;
      while (!next_table)
	{
	  struct rx_superstate * state;
	  state = ((struct rx_superstate *)
		   ((char *)inx_table
		    - ((unsigned long)
		       ((struct rx_superstate *)0)->transitions)));

	  switch ((int)inx->inx)
	    {
	    case rx_backtrack:
	      /* RX_BACKTRACK means that we've reached the empty
	       * superstate, indicating that match can't succeed
	       * from this point.
	       */
	      frame->state = 0;
	      return rx_no;
	    
	    case rx_cache_miss:
	      /* Because the superstate NFA is lazily constructed,
	       * and in fact may erode from underneath us, we sometimes
	       * have to construct the next instruction from the hard way.
	       * This invokes one step in the lazy-conversion.
	       */
	      inx = 
		rx_handle_cache_miss
		  (frame->rx, state, *burst, inx->data_2);

	      if (!inx)
		{
		  frame->state = 0;
		  return rx_bogus;
		}
	      next_table = (struct rx_inx *)inx->data;
	      continue;
		

	      /* No other instructions are legal here.
	       * (In particular, this function does not handle backtracking
	       * or the related instructions.)
	       */
	    default:
	      frame->state = 0;
	      return rx_bogus;
	  }
	}
      inx_table = next_table;
      ++burst;
    }
  
  frame->state = ((struct rx_superstate *)
		  ((char *)inx_table
		   - ((unsigned long)
		      ((struct rx_superstate *)0)->transitions)));
  rx_lock_superstate (frame->rx, frame->state);
  return rx_yes;
}



#ifdef __STDC__
int
rx_advance_to_final (struct rx_classical_system * frame, unsigned const char * burst, int len)
#else
int
rx_advance_to_final (frame, burst, len)
     struct rx_classical_system * frame;
     unsigned const char * burst;
     int len;
#endif
{
  int initial_len;
  struct rx_inx * inx_table;
  struct rx_superstate * this_state;

  if (!frame->state)
    return 0;

  if (!len)
    {
      frame->final_tag = frame->state->contents->is_final;
      return 0;
    }

  inx_table = frame->state->transitions;

  initial_len = len;

  this_state = frame->state;

  while (len--)
    {
      struct rx_inx * inx;
      struct rx_inx * next_table;

      /* this_state holds the state for the position we're
       * leaving.  this_state is locked. 
       */
      inx = inx_table + *burst;
      next_table = (struct rx_inx *)inx->data;

      while (!next_table)
	{
	  struct rx_superstate * state;

	  state = ((struct rx_superstate *)
		   ((char *)inx_table
		    - ((unsigned long)
		       ((struct rx_superstate *)0)->transitions)));
	  
	  switch ((int)inx->inx)
	    {
	    case rx_backtrack:
	      /* RX_BACKTRACK means that we've reached the empty
	       * superstate, indicating that match can't succeed
	       * from this point.
	       *
	       * Return to the state for the position prior to what
	       * we failed at, and return that position.
	       */
	      frame->state = this_state;
	      frame->final_tag = this_state->contents->is_final;
	      return (initial_len - len) - 1;
	    
	    case rx_cache_miss:
	      /* Because the superstate NFA is lazily constructed,
	       * and in fact may erode from underneath us, we sometimes
	       * have to construct the next instruction from the hard way.
	       * This invokes one step in the lazy-conversion.
	       */
	      inx = rx_handle_cache_miss
		(frame->rx, state, *burst, inx->data_2);

	      if (!inx)
		{
		  rx_unlock_superstate (frame->rx, this_state);
		  frame->state = 0;
		  return -1;
		}
	      next_table = (struct rx_inx *)inx->data;
	      continue;
		

	      /* No other instructions are legal here.
	       * (In particular, this function does not handle backtracking
	       * or the related instructions.)
	       */
	    default:
	      rx_unlock_superstate (frame->rx, this_state);
	      frame->state = 0;
	      return -1;
	  }
	}

      /* Release the superstate for the preceeding position: */
      rx_unlock_superstate (frame->rx, this_state);

      /* Compute the superstate for the new position: */
      inx_table = next_table;
      this_state = ((struct rx_superstate *)
		    ((char *)inx_table
		     - ((unsigned long)
			((struct rx_superstate *)0)->transitions)));
      
      /* Lock it (see top-of-loop invariant): */
      rx_lock_superstate (frame->rx, this_state);
      
      /* Check to see if we should stop: */
      if (this_state->contents->is_final)
	{
	  frame->final_tag = this_state->contents->is_final;
	  frame->state = this_state;
	  return (initial_len - len);
	}
      
      ++burst;
    }

  /* Consumed all of the characters. */
  frame->state = this_state;
  frame->final_tag = this_state->contents->is_final;

  /* state already locked (see top-of-loop invariant) */
  return initial_len;
}




#ifdef __STDC__
void
rx_terminate_system (struct rx_classical_system * frame)
#else
void
rx_terminate_system (frame)
     struct rx_classical_system * frame;
#endif
{
  if (frame->state)
    {
      rx_unlock_superstate (frame->rx, frame->state);
      frame->state = 0;
    }
}









#ifdef __STDC__
void
rx_init_system (struct rx_classical_system * frame, struct rx * rx)
#else
void
rx_init_system (frame, rx)
     struct rx_classical_system * frame;
     struct rx * rx;
#endif
{
  frame->rx = rx;
  frame->state = 0;
}



