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


/*
 * Tom Lord (lord@cygnus.com, lord@gnu.ai.mit.edu)
 */


#include "rxall.h"
#include "rxnfa.h"



#define remalloc(M, S) (M ? realloc (M, S) : malloc (S))


/* {Low Level Data Structure Code}
 */

/* Constructs a new nfa node. */
#ifdef __STDC__
struct rx_nfa_state *
rx_nfa_state (struct rx *rx)
#else
struct rx_nfa_state *
rx_nfa_state (rx)
     struct rx *rx;
#endif
{
  struct rx_nfa_state * n = (struct rx_nfa_state *)malloc (sizeof (*n));
  if (!n)
    return 0;
  rx_bzero ((char *)n, sizeof (*n));
  n->next = rx->nfa_states;
  rx->nfa_states = n;
  return n;
}


#ifdef __STDC__
static void
rx_free_nfa_state (struct rx_nfa_state * n)
#else
static void
rx_free_nfa_state (n)
  struct rx_nfa_state * n;
#endif
{
  free ((char *)n);
}


/* This adds an edge between two nodes, but doesn't initialize the 
 * edge label.
 */

#ifdef __STDC__
struct rx_nfa_edge * 
rx_nfa_edge (struct rx *rx,
	     enum rx_nfa_etype type,
	     struct rx_nfa_state *start,
	     struct rx_nfa_state *dest)
#else
struct rx_nfa_edge * 
rx_nfa_edge (rx, type, start, dest)
     struct rx *rx;
     enum rx_nfa_etype type;
     struct rx_nfa_state *start;
     struct rx_nfa_state *dest;
#endif
{
  struct rx_nfa_edge *e;
  e = (struct rx_nfa_edge *)malloc (sizeof (*e));
  if (!e)
    return 0;
  e->next = start->edges;
  start->edges = e;
  e->type = type;
  e->dest = dest;
  return e;
}


#ifdef __STDC__
static void
rx_free_nfa_edge (struct rx_nfa_edge * e)
#else
static void
rx_free_nfa_edge (e)
     struct rx_nfa_edge * e;
#endif
{
  free ((char *)e);
}


/* This constructs a POSSIBLE_FUTURE, which is a kind epsilon-closure
 * of an NFA.  These are added to an nfa automaticly by eclose_nfa.
 */  

#ifdef __STDC__
static struct rx_possible_future * 
rx_possible_future (struct rx * rx,
		 struct rx_se_list * effects)
#else
static struct rx_possible_future * 
rx_possible_future (rx, effects)
     struct rx * rx;
     struct rx_se_list * effects;
#endif
{
  struct rx_possible_future *ec;
  ec = (struct rx_possible_future *) malloc (sizeof (*ec));
  if (!ec)
    return 0;
  ec->destset = 0;
  ec->next = 0;
  ec->effects = effects;
  return ec;
}


#ifdef __STDC__
static void
rx_free_possible_future (struct rx_possible_future * pf)
#else
static void
rx_free_possible_future (pf)
     struct rx_possible_future * pf;
#endif
{
  free ((char *)pf);
}


#ifdef __STDC__
static void
rx_free_nfa_graph (struct rx *rx)
#else
static void
rx_free_nfa_graph (rx)
     struct rx *rx;
#endif
{
  while (rx->nfa_states)
    {
      while (rx->nfa_states->edges)
	{
	  switch (rx->nfa_states->edges->type)
	    {
	    case ne_cset:
	      rx_free_cset (rx->nfa_states->edges->params.cset);
	      break;
	    default:
	      break;
	    }
	  {
	    struct rx_nfa_edge * e;
	    e = rx->nfa_states->edges;
	    rx->nfa_states->edges = rx->nfa_states->edges->next;
	    rx_free_nfa_edge (e);
	  }
	} /* while (rx->nfa_states->edges) */
      {
	/* Iterate over the partial epsilon closures of rx->nfa_states */
	struct rx_possible_future * pf = rx->nfa_states->futures;
	while (pf)
	  {
	    struct rx_possible_future * pft = pf;
	    pf = pf->next;
	    rx_free_possible_future (pft);
	  }
      }
      {
	struct rx_nfa_state *n;
	n = rx->nfa_states;
	rx->nfa_states = rx->nfa_states->next;
	rx_free_nfa_state (n);
      }
    }
}



/* {Translating a Syntax Tree into an NFA}
 *
 */


/* This is the Thompson regexp->nfa algorithm. 
 * It is modified to allow for `side-effect epsilons.'  Those are
 * edges that are taken whenever a similarly placed epsilon edge 
 * would be, but which also imply that some side effect occurs 
 * when the edge is taken.
 *
 * Side effects are used to model parts of the pattern langauge 
 * that are not regular.
 */

#ifdef __STDC__
int
rx_build_nfa (struct rx *rx,
	      struct rexp_node *rexp,
	      struct rx_nfa_state **start,
	      struct rx_nfa_state **end)
#else
int
rx_build_nfa (rx, rexp, start, end)
     struct rx *rx;
     struct rexp_node *rexp;
     struct rx_nfa_state **start;
     struct rx_nfa_state **end;
#endif
{
  struct rx_nfa_edge *edge;

  /* Start & end nodes may have been allocated by the caller. */
  *start = *start ? *start : rx_nfa_state (rx);

  if (!*start)
    return 0;

  if (!rexp)
    {
      *end = *start;
      return 1;
    }

  *end = *end ? *end : rx_nfa_state (rx);

  if (!*end)
    {
      rx_free_nfa_state (*start);
      return 0;
    }

  switch (rexp->type)
    {
    case r_cset:
      edge = rx_nfa_edge (rx, ne_cset, *start, *end);
      (*start)->has_cset_edges = 1;
      if (!edge)
	return 0;
      edge->params.cset = rx_copy_cset (rx->local_cset_size,
					rexp->params.cset);
      if (!edge->params.cset)
	{
	  rx_free_nfa_edge (edge);
	  return 0;
	}
      return 1;

    case r_string:
      {
	if (rexp->params.cstr.len == 1)
	  {
	    edge = rx_nfa_edge (rx, ne_cset, *start, *end);
	    (*start)->has_cset_edges = 1;
	    if (!edge)
	      return 0;
	    edge->params.cset = rx_cset (rx->local_cset_size);
	    if (!edge->params.cset)
	      {
		rx_free_nfa_edge (edge);
		return 0;
	      }
	    RX_bitset_enjoin (edge->params.cset, rexp->params.cstr.contents[0]);
	    return 1;
	  }
	else
	  {
	    struct rexp_node copied;
	    struct rx_nfa_state * shared;

	    copied = *rexp;
	    shared = 0;
	    copied.params.cstr.len--;
	    copied.params.cstr.contents++;
	    if (!rx_build_nfa (rx, &copied, &shared, end))
	      return 0;
	    copied.params.cstr.len = 1;
	    copied.params.cstr.contents--;
	    return rx_build_nfa (rx, &copied, start, &shared);
	  }
      }
 
    case r_opt:
      return (rx_build_nfa (rx, rexp->params.pair.left, start, end)
	      && rx_nfa_edge (rx, ne_epsilon, *start, *end));

    case r_plus:
      {
	struct rx_nfa_state * star_start = 0;
	struct rx_nfa_state * star_end = 0;
	struct rx_nfa_state * shared;

	shared = 0;
	if (!rx_build_nfa (rx, rexp->params.pair.left, start, &shared))
	  return 0;
	return (rx_build_nfa (rx, rexp->params.pair.left,
			      &star_start, &star_end)
		&& star_start
		&& star_end
		&& rx_nfa_edge (rx, ne_epsilon, star_start, star_end)
		&& rx_nfa_edge (rx, ne_epsilon, shared, star_start)
		&& rx_nfa_edge (rx, ne_epsilon, star_end, *end)
		&& rx_nfa_edge (rx, ne_epsilon, star_end, star_start));
      }

    case r_interval:
    case r_star:
      {
	struct rx_nfa_state * star_start = 0;
	struct rx_nfa_state * star_end = 0;
	return (rx_build_nfa (rx, rexp->params.pair.left,
			      &star_start, &star_end)
		&& star_start
		&& star_end
		&& rx_nfa_edge (rx, ne_epsilon, star_start, star_end)
		&& rx_nfa_edge (rx, ne_epsilon, *start, star_start)
		&& rx_nfa_edge (rx, ne_epsilon, star_end, *end)

		&& rx_nfa_edge (rx, ne_epsilon, star_end, star_start));
      }

    case r_cut:
      {
	struct rx_nfa_state * cut_end = 0;

	cut_end = rx_nfa_state (rx);
	if (!(cut_end && rx_nfa_edge (rx, ne_epsilon, *start, cut_end)))
	  {
	    rx_free_nfa_state (*start);
	    rx_free_nfa_state (*end);
	    if (cut_end)
	      rx_free_nfa_state (cut_end);
	    return 0;
	  }
	cut_end->is_final = rexp->params.intval;
	return 1;
      }

    case r_parens:
      return rx_build_nfa (rx, rexp->params.pair.left, start, end);

    case r_concat:
      {
	struct rx_nfa_state *shared = 0;
	return
	  (rx_build_nfa (rx, rexp->params.pair.left, start, &shared)
	   && rx_build_nfa (rx, rexp->params.pair.right, &shared, end));
      }

    case r_alternate:
      {
	struct rx_nfa_state *ls = 0;
	struct rx_nfa_state *le = 0;
	struct rx_nfa_state *rs = 0;
	struct rx_nfa_state *re = 0;
	return (rx_build_nfa (rx, rexp->params.pair.left, &ls, &le)
		&& rx_build_nfa (rx, rexp->params.pair.right, &rs, &re)
		&& rx_nfa_edge (rx, ne_epsilon, *start, ls)
		&& rx_nfa_edge (rx, ne_epsilon, *start, rs)
		&& rx_nfa_edge (rx, ne_epsilon, le, *end)
		&& rx_nfa_edge (rx, ne_epsilon, re, *end));
      }

    case r_context:
      edge = rx_nfa_edge (rx, ne_side_effect, *start, *end);
      if (!edge)
	return 0;
      edge->params.side_effect = (void *)rexp->params.intval;
      return 1;
    }

  /* this should never happen */
  return 0;
}


/* {Low Level Data Structures for the Static NFA->SuperNFA Analysis}
 *
 * There are side effect lists -- lists of side effects occuring
 * along an uninterrupted, acyclic path of side-effect epsilon edges.
 * Such paths are collapsed to single edges in the course of computing
 * epsilon closures.  The resulting single edges are labled with a list 
 * of all the side effects from the original multi-edge path.  Equivalent
 * lists of side effects are made == by the constructors below.
 *
 * There are also nfa state sets.  These are used to hold a list of all
 * states reachable from a starting state for a given type of transition
 * and side effect list.   These are also hash-consed.
 */



/* The next several functions compare, construct, etc. lists of side
 * effects.  See ECLOSE_NFA (below) for details.
 */

/* Ordering of rx_se_list
 * (-1, 0, 1 return value convention).
 */

#ifdef __STDC__
static int 
se_list_cmp (void * va, void * vb)
#else
static int 
se_list_cmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_se_list * a = (struct rx_se_list *)va;
  struct rx_se_list * b = (struct rx_se_list *)vb;

  return ((va == vb)
	  ? 0
	  : (!va
	     ? -1
	     : (!vb
		? 1
		: ((long)a->car < (long)b->car
		   ? 1
		   : ((long)a->car > (long)b->car
		      ? -1
		      : se_list_cmp ((void *)a->cdr, (void *)b->cdr))))));
}


#ifdef __STDC__
static int 
se_list_equal (void * va, void * vb)
#else
static int 
se_list_equal (va, vb)
     void * va;
     void * vb;
#endif
{
  return !(se_list_cmp (va, vb));
}

static struct rx_hash_rules se_list_hash_rules = { se_list_equal, 0, 0, 0, 0 };


#ifdef __STDC__
static struct rx_se_list * 
side_effect_cons (struct rx * rx,
		  void * se, struct rx_se_list * list)
#else
static struct rx_se_list * 
side_effect_cons (rx, se, list)
     struct rx * rx;
     void * se;
     struct rx_se_list * list;
#endif
{
  struct rx_se_list * l;
  l = ((struct rx_se_list *) malloc (sizeof (*l)));
  if (!l)
    return 0;
  l->car = se;
  l->cdr = list;
  return l;
}


#ifdef __STDC__
static struct rx_se_list *
hash_cons_se_prog (struct rx * rx,
		   struct rx_hash * memo,
		   void * car, struct rx_se_list * cdr)
#else
static struct rx_se_list *
hash_cons_se_prog (rx, memo, car, cdr)
     struct rx * rx;
     struct rx_hash * memo;
     void * car;
     struct rx_se_list * cdr;
#endif
{
  long hash = (long)car ^ (long)cdr;
  struct rx_se_list template;

  template.car = car;
  template.cdr = cdr;
  {
    struct rx_hash_item * it = rx_hash_store (memo, hash,
					      (void *)&template,
					      &se_list_hash_rules);
    if (!it)
      return 0;
    if (it->data == (void *)&template)
      {
	struct rx_se_list * consed;
	consed = (struct rx_se_list *) malloc (sizeof (*consed));
	*consed = template;
	it->data = (void *)consed;
      }
    return (struct rx_se_list *)it->data;
  }
}
     

#ifdef __STDC__
static struct rx_se_list *
hash_se_prog (struct rx * rx, struct rx_hash * memo, struct rx_se_list * prog)
#else
static struct rx_se_list *
hash_se_prog (rx, memo, prog)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_se_list * prog;
#endif
{
  struct rx_se_list * answer = 0;
  while (prog)
    {
      answer = hash_cons_se_prog (rx, memo, prog->car, answer);
      if (!answer)
	return 0;
      prog = prog->cdr;
    }
  return answer;
}



/* {Constructors, etc. for NFA State Sets}
 */

#ifdef __STDC__
static int 
nfa_set_cmp (void * va, void * vb)
#else
static int 
nfa_set_cmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_nfa_state_set * a = (struct rx_nfa_state_set *)va;
  struct rx_nfa_state_set * b = (struct rx_nfa_state_set *)vb;

  return ((va == vb)
	  ? 0
	  : (!va
	     ? -1
	     : (!vb
		? 1
		: (a->car->id < b->car->id
		   ? 1
		   : (a->car->id > b->car->id
		      ? -1
		      : nfa_set_cmp ((void *)a->cdr, (void *)b->cdr))))));
}

#ifdef __STDC__
static int 
nfa_set_equal (void * va, void * vb)
#else
static int 
nfa_set_equal (va, vb)
     void * va;
     void * vb;
#endif
{
  return !nfa_set_cmp (va, vb);
}

static struct rx_hash_rules nfa_set_hash_rules = { nfa_set_equal, 0, 0, 0, 0 };


#ifdef __STDC__
static struct rx_nfa_state_set * 
nfa_set_cons (struct rx * rx,
	      struct rx_hash * memo, struct rx_nfa_state * state,
	      struct rx_nfa_state_set * set)
#else
static struct rx_nfa_state_set * 
nfa_set_cons (rx, memo, state, set)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_nfa_state * state;
     struct rx_nfa_state_set * set;
#endif
{
  struct rx_nfa_state_set template;
  struct rx_hash_item * node;
  template.car = state;
  template.cdr = set;
  node = rx_hash_store (memo,
			(((long)state) >> 8) ^ (long)set,
			&template, &nfa_set_hash_rules);
  if (!node)
    return 0;
  if (node->data == &template)
    {
      struct rx_nfa_state_set * l;
      l = (struct rx_nfa_state_set *) malloc (sizeof (*l));
      node->data = (void *) l;
      if (!l)
	return 0;
      *l = template;
    }
  return (struct rx_nfa_state_set *)node->data;
}


#ifdef __STDC__
static struct rx_nfa_state_set * 
nfa_set_enjoin (struct rx * rx,
		struct rx_hash * memo, struct rx_nfa_state * state,
		struct rx_nfa_state_set * set)
#else
static struct rx_nfa_state_set * 
nfa_set_enjoin (rx, memo, state, set)
     struct rx * rx;
     struct rx_hash * memo;
     struct rx_nfa_state * state;
     struct rx_nfa_state_set * set;
#endif
{
  if (!set || (state->id < set->car->id))
    return nfa_set_cons (rx, memo, state, set);
  if (state->id == set->car->id)
    return set;
  else
    {
      struct rx_nfa_state_set * newcdr
	= nfa_set_enjoin (rx, memo, state, set->cdr);
      if (newcdr != set->cdr)
	set = nfa_set_cons (rx, memo, set->car, newcdr);
      return set;
    }
}


/* {Computing Epsilon/Side-effect Closures.}
 */

struct eclose_frame
{
  struct rx_se_list *prog_backwards;
};


/* This is called while computing closures for "outnode".
 * The current node in the traversal is "node".
 * "frame" contains a list of a all side effects between 
 * "outnode" and "node" from most to least recent.
 *
 * Explores forward from "node", adding new possible
 * futures to outnode.
 *
 * Returns 0 on allocation failure.
 */

#ifdef __STDC__
static int 
eclose_node (struct rx *rx, struct rx_nfa_state *outnode,
	     struct rx_nfa_state *node, struct eclose_frame *frame)
#else
static int 
eclose_node (rx, outnode, node, frame)
     struct rx *rx;
     struct rx_nfa_state *outnode;
     struct rx_nfa_state *node;
     struct eclose_frame *frame;
#endif
{
  struct rx_nfa_edge *e = node->edges;

  /* For each node, we follow all epsilon paths to build the closure.
   * The closure omits nodes that have only epsilon edges.
   * The closure is split into partial closures -- all the states in
   * a partial closure are reached by crossing the same list of
   * of side effects (though not necessarily the same path).
   */
  if (node->mark)
    return 1;
  node->mark = 1;

  /* If "node" has more than just epsilon and 
   * and side-effect transitions (id >= 0), or is final,
   * then it has to be added to the possible futures
   * of "outnode".
   */
  if (node->id >= 0 || node->is_final)
    {
      struct rx_possible_future **ec;
      struct rx_se_list * prog_in_order;
      int cmp;

      prog_in_order = ((struct rx_se_list *)hash_se_prog (rx,
							  &rx->se_list_memo,
							  frame->prog_backwards));

      ec = &outnode->futures;

      while (*ec)
	{
	  cmp = se_list_cmp ((void *)(*ec)->effects, (void *)prog_in_order);
	  if (cmp <= 0)
	    break;
	  ec = &(*ec)->next;
	}

      if (!*ec || (cmp < 0))
	{
	  struct rx_possible_future * pf;
	  pf = rx_possible_future (rx, prog_in_order);
	  if (!pf)
	    return 0;
	  pf->next = *ec;
	  *ec = pf;
	}
      if (node->id >= 0)
	{
	  (*ec)->destset = nfa_set_enjoin (rx, &rx->set_list_memo,
					   node, (*ec)->destset);
	  if (!(*ec)->destset)
	    return 0;
	}
    }

  /* Recurse on outgoing epsilon and side effect nodes.
   */
  while (e)
    {
      switch (e->type)
	{
	case ne_epsilon:
	  if (!eclose_node (rx, outnode, e->dest, frame))
	    return 0;
	  break;
	case ne_side_effect:
	  {
	    frame->prog_backwards = side_effect_cons (rx, 
						      e->params.side_effect,
						      frame->prog_backwards);
	    if (!frame->prog_backwards)
	      return 0;
	    if (!eclose_node (rx, outnode, e->dest, frame))
	      return 0;
	    {
	      struct rx_se_list * dying = frame->prog_backwards;
	      frame->prog_backwards = frame->prog_backwards->cdr;
	      free ((char *)dying);
	    }
	    break;
	  }
	default:
	  break;
	}
      e = e->next;
    }
  node->mark = 0;
  return 1;
}


#ifdef __STDC__
struct rx_possible_future *
rx_state_possible_futures (struct rx * rx, struct rx_nfa_state * n)
#else
struct rx_possible_future *
rx_state_possible_futures (rx, n)
     struct rx * rx;
     struct rx_nfa_state * n;
#endif
{
  if (n->futures_computed)
    return n->futures;
  else
    {
      struct eclose_frame frame;
      frame.prog_backwards = 0;
      if (!eclose_node (rx, n, n, &frame))
	return 0;
      else
	{
	  n->futures_computed = 1;
	  return n->futures;
	}
    }
}



/* {Storing the NFA in a Contiguous Region of Memory}
 */



#ifdef __STDC__
static void 
se_memo_freer (struct rx_hash_item * node)
#else
static void 
se_memo_freer (node)
     struct rx_hash_item * node;
#endif
{
  free ((char *)node->data);
}


#ifdef __STDC__
static void 
nfa_set_freer (struct rx_hash_item * node)
#else
static void 
nfa_set_freer (node)
     struct rx_hash_item * node;
#endif
{
  free ((char *)node->data);
}

#ifdef __STDC__
void
rx_free_nfa (struct rx *rx)
#else
void
rx_free_nfa (rx)
     struct rx *rx;
#endif
{
  rx_free_hash_table (&rx->se_list_memo, se_memo_freer, &se_list_hash_rules);
  rx_bzero ((char *)&rx->se_list_memo, sizeof (rx->se_list_memo));
  rx_free_hash_table (&rx->set_list_memo, nfa_set_freer, &nfa_set_hash_rules);
  rx_bzero ((char *)&rx->set_list_memo, sizeof (rx->set_list_memo));
  rx_free_nfa_graph (rx);
}
