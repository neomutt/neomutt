

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
#include "rxsuper.h"


/* The functions in the next several pages define the lazy-NFA-conversion used
 * by matchers.  The input to this construction is an NFA such as 
 * is built by compactify_nfa (rx.c).  The output is the superNFA.
 */

/* Match engines can use arbitrary values for opcodes.  So, the parse tree 
 * is built using instructions names (enum rx_opcode), but the superstate
 * nfa is populated with mystery opcodes (void *).
 *
 * For convenience, here is an id table.  The opcodes are == to their inxs
 *
 * The lables in re_search_2 would make good values for instructions.
 */

void * rx_id_instruction_table[rx_num_instructions] =
{
  (void *) rx_backtrack_point,
  (void *) rx_do_side_effects,
  (void *) rx_cache_miss,
  (void *) rx_next_char,
  (void *) rx_backtrack,
  (void *) rx_error_inx
};




/* The partially instantiated superstate graph has a transition 
 * table at every node.  There is one entry for every character.
 * This fills in the transition for a set.
 */
#ifdef __STDC__
static void 
install_transition (struct rx_superstate *super,
		    struct rx_inx *answer, rx_Bitset trcset) 
#else
static void 
install_transition (super, answer, trcset)
     struct rx_superstate *super;
     struct rx_inx *answer;
     rx_Bitset trcset;
#endif
{
  struct rx_inx * transitions = super->transitions;
  int chr;
  for (chr = 0; chr < 256; )
    if (!*trcset)
      {
	++trcset;
	chr += 32;
      }
    else
      {
	RX_subset sub = *trcset;
	RX_subset mask = 1;
	int bound = chr + 32;
	while (chr < bound)
	  {
	    if (sub & mask)
	      transitions [chr] = *answer;
	    ++chr;
	    mask <<= 1;
	  }
	++trcset;
      }
}


#ifdef __STDC__
static int
qlen (struct rx_superstate * q)
#else
static int
qlen (q)
     struct rx_superstate * q;
#endif
{
  int count = 1;
  struct rx_superstate * it;
  if (!q)
    return 0;
  for (it = q->next_recyclable; it != q; it = it->next_recyclable)
    ++count;
  return count;
}

#ifdef __STDC__
static void
check_cache (struct rx_cache * cache)
#else
static void
check_cache (cache)
     struct rx_cache * cache;
#endif
{
  struct rx_cache * you_fucked_up = 0;
  int total = cache->superstates;
  int semi = cache->semifree_superstates;
  if (semi != qlen (cache->semifree_superstate))
    check_cache (you_fucked_up);
  if ((total - semi) != qlen (cache->lru_superstate))
    check_cache (you_fucked_up);
}
#ifdef __STDC__
char *
rx_cache_malloc (struct rx_cache * cache, int size)
#else
char *
rx_cache_malloc (cache, size)
     struct rx_cache * cache;
     int size;
#endif
{
  char * answer;
  answer = (char *)malloc (size);
  if (answer)
    cache->bytes_used += size;
  return answer;
}


#ifdef __STDC__
void
rx_cache_free (struct rx_cache * cache, int size, char * mem)
#else
void
rx_cache_free (cache, size, mem)
     struct rx_cache * cache;
     int size;
     char * mem;
#endif
{
  free (mem);
  cache->bytes_used -= size;
}



/* When a superstate is old and neglected, it can enter a 
 * semi-free state.  A semi-free state is slated to die.
 * Incoming transitions to a semi-free state are re-written
 * to cause an (interpreted) fault when they are taken.
 * The fault handler revives the semi-free state, patches
 * incoming transitions back to normal, and continues.
 *
 * The idea is basicly to free in two stages, aborting 
 * between the two if the state turns out to be useful again.
 * When a free is aborted, the rescued superstate is placed
 * in the most-favored slot to maximize the time until it
 * is next semi-freed.
 *
 * Overall, this approximates truly freeing superstates in
 * lru order, but does not involve the cost of updating perfectly 
 * accurate timestamps every time a superstate is touched.
 */

#ifdef __STDC__
static void
semifree_superstate (struct rx_cache * cache)
#else
static void
semifree_superstate (cache)
     struct rx_cache * cache;
#endif
{
  int disqualified = cache->semifree_superstates;
  if (disqualified == cache->superstates)
    return;
  while (cache->lru_superstate->locks)
    {
      cache->lru_superstate = cache->lru_superstate->next_recyclable;
      ++disqualified;
      if (disqualified == cache->superstates)
	return;
    }
  {
    struct rx_superstate * it = cache->lru_superstate;
    it->next_recyclable->prev_recyclable = it->prev_recyclable;
    it->prev_recyclable->next_recyclable = it->next_recyclable;
    cache->lru_superstate = (it == it->next_recyclable
			     ? 0
			     : it->next_recyclable);
    if (!cache->semifree_superstate)
      {
	cache->semifree_superstate = it;
	it->next_recyclable = it;
	it->prev_recyclable = it;
      }
    else
      {
	it->prev_recyclable = cache->semifree_superstate->prev_recyclable;
	it->next_recyclable = cache->semifree_superstate;
	it->prev_recyclable->next_recyclable = it;
	it->next_recyclable->prev_recyclable = it;
      }
    {
      struct rx_distinct_future *df;
      it->is_semifree = 1;
      ++cache->semifree_superstates;
      df = it->transition_refs;
      if (df)
	{
	  df->prev_same_dest->next_same_dest = 0;
	  for (df = it->transition_refs; df; df = df->next_same_dest)
	    {
	      df->future_frame.inx = cache->instruction_table[rx_cache_miss];
	      df->future_frame.data = 0;
	      df->future_frame.data_2 = (void *) df;
	      /* If there are any NEXT-CHAR instruction frames that
	       * refer to this state, we convert them to CACHE-MISS frames.
	       */
	      if (!df->effects
		  && (df->edge->options->next_same_super_edge[0]
		      == df->edge->options))
		install_transition (df->present, &df->future_frame,
				    df->edge->cset);
	    }
	  df = it->transition_refs;
	  df->prev_same_dest->next_same_dest = df;
	}
    }
  }
}


#ifdef __STDC__
static void 
refresh_semifree_superstate (struct rx_cache * cache,
			     struct rx_superstate * super)
#else
static void 
refresh_semifree_superstate (cache, super)
     struct rx_cache * cache;
     struct rx_superstate * super;
#endif
{
  struct rx_distinct_future *df;

  if (super->transition_refs)
    {
      super->transition_refs->prev_same_dest->next_same_dest = 0; 
      for (df = super->transition_refs; df; df = df->next_same_dest)
	{
	  df->future_frame.inx = cache->instruction_table[rx_next_char];
	  df->future_frame.data = (void *) super->transitions;
	  df->future_frame.data_2 = (void *)(super->contents->is_final);
	  /* CACHE-MISS instruction frames that refer to this state,
	   * must be converted to NEXT-CHAR frames.
	   */
	  if (!df->effects
	      && (df->edge->options->next_same_super_edge[0]
		  == df->edge->options))
	    install_transition (df->present, &df->future_frame,
				df->edge->cset);
	}
      super->transition_refs->prev_same_dest->next_same_dest
	= super->transition_refs;
    }
  if (cache->semifree_superstate == super)
    cache->semifree_superstate = (super->prev_recyclable == super
				  ? 0
				  : super->prev_recyclable);
  super->next_recyclable->prev_recyclable = super->prev_recyclable;
  super->prev_recyclable->next_recyclable = super->next_recyclable;

  if (!cache->lru_superstate)
    (cache->lru_superstate
     = super->next_recyclable
     = super->prev_recyclable
     = super);
  else
    {
      super->next_recyclable = cache->lru_superstate;
      super->prev_recyclable = cache->lru_superstate->prev_recyclable;
      super->next_recyclable->prev_recyclable = super;
      super->prev_recyclable->next_recyclable = super;
    }
  super->is_semifree = 0;
  --cache->semifree_superstates;
}

#ifdef __STDC__
void
rx_refresh_this_superstate (struct rx_cache * cache,
			    struct rx_superstate * superstate)
#else
void
rx_refresh_this_superstate (cache, superstate)
     struct rx_cache * cache;
     struct rx_superstate * superstate;
#endif
{
  if (superstate->is_semifree)
    refresh_semifree_superstate (cache, superstate);
  else if (cache->lru_superstate == superstate)
    cache->lru_superstate = superstate->next_recyclable;
  else if (superstate != cache->lru_superstate->prev_recyclable)
    {
      superstate->next_recyclable->prev_recyclable
	= superstate->prev_recyclable;
      superstate->prev_recyclable->next_recyclable
	= superstate->next_recyclable;
      superstate->next_recyclable = cache->lru_superstate;
      superstate->prev_recyclable = cache->lru_superstate->prev_recyclable;
      superstate->next_recyclable->prev_recyclable = superstate;
      superstate->prev_recyclable->next_recyclable = superstate;
    }
}

#ifdef __STDC__
static void 
release_superset_low (struct rx_cache * cache,
		     struct rx_superset *set)
#else
static void 
release_superset_low (cache, set)
     struct rx_cache * cache;
     struct rx_superset *set;
#endif
{
  if (!--set->refs)
    {
      if (set->starts_for)
	set->starts_for->start_set = 0;

      if (set->cdr)
	release_superset_low (cache, set->cdr);

      rx_hash_free (&set->hash_item, &cache->superset_hash_rules);
      rx_cache_free (cache,
		     sizeof (struct rx_superset),
		     (char *)set);
    }
}

#ifdef __STDC__
void 
rx_release_superset (struct rx *rx,
		     struct rx_superset *set)
#else
void 
rx_release_superset (rx, set)
     struct rx *rx;
     struct rx_superset *set;
#endif
{
  release_superset_low (rx->cache, set);
}

/* This tries to add a new superstate to the superstate freelist.
 * It might, as a result, free some edge pieces or hash tables.
 * If nothing can be freed because too many locks are being held, fail.
 */

#ifdef __STDC__
static int
rx_really_free_superstate (struct rx_cache * cache)
#else
static int
rx_really_free_superstate (cache)
     struct rx_cache * cache;
#endif
{
  int locked_superstates = 0;
  struct rx_superstate * it;

  if (!cache->superstates)
    return 0;

  /* scale these */
  while ((cache->hits + cache->misses) > cache->superstates)
    {
      cache->hits >>= 1;
      cache->misses >>= 1;
    }

  /* semifree superstates faster than they are freed
   * so popular states can be rescued.
   */
  semifree_superstate (cache);
  semifree_superstate (cache);
  semifree_superstate (cache);

#if 0
  Redundant because semifree_superstate already 
    makes this check;


  while (cache->semifree_superstate && cache->semifree_superstate->locks)
    {
      refresh_semifree_superstate (cache, cache->semifree_superstate);
      ++locked_superstates;
      if (locked_superstates == cache->superstates)
	return 0;
    }


  if (cache->semifree_superstate)
    insert the "it =" block which follows this "if 0" code;
  else
    {
      while (cache->lru_superstate->locks)
	{
	  cache->lru_superstate = cache->lru_superstate->next_recyclable;
	  ++locked_superstates;
	  if (locked_superstates == cache->superstates)
	    return 0;
	}
      it = cache->lru_superstate;
      it->next_recyclable->prev_recyclable = it->prev_recyclable;
      it->prev_recyclable->next_recyclable = it->next_recyclable;
      cache->lru_superstate = ((it == it->next_recyclable)
				    ? 0
				    : it->next_recyclable);
    }
#endif

    if (!cache->semifree_superstate)
      return 0;

    {
      it = cache->semifree_superstate;
      it->next_recyclable->prev_recyclable = it->prev_recyclable;
      it->prev_recyclable->next_recyclable = it->next_recyclable;
      cache->semifree_superstate = ((it == it->next_recyclable)
				    ? 0
				    : it->next_recyclable);
      --cache->semifree_superstates;
    }


  if (it->transition_refs)
    {
      struct rx_distinct_future *df;
      for (df = it->transition_refs,
	   df->prev_same_dest->next_same_dest = 0;
	   df;
	   df = df->next_same_dest)
	{
	  df->future_frame.inx = cache->instruction_table[rx_cache_miss];
	  df->future_frame.data = 0;
	  df->future_frame.data_2 = (void *) df;
	  df->future = 0;
	}
      it->transition_refs->prev_same_dest->next_same_dest =
	it->transition_refs;
    }
  {
    struct rx_super_edge *tc = it->edges;
    while (tc)
      {
	struct rx_distinct_future * df;
	struct rx_super_edge *tct = tc->next;
	df = tc->options;
	df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
	while (df)
	  {
	    struct rx_distinct_future *dft = df;
	    df = df->next_same_super_edge[0];
	    
	    
	    if (dft->future && dft->future->transition_refs == dft)
	      {
		dft->future->transition_refs = dft->next_same_dest;
		if (dft->future->transition_refs == dft)
		  dft->future->transition_refs = 0;
	      }
	    dft->next_same_dest->prev_same_dest = dft->prev_same_dest;
	    dft->prev_same_dest->next_same_dest = dft->next_same_dest;
	    rx_cache_free (cache,
			   sizeof (struct rx_distinct_future),
			   (char *)dft);
	  }
	rx_cache_free (cache,
		       sizeof (struct rx_super_edge),
		       (char *)tc);
	tc = tct;
      }
  }
  
  if (it->contents->superstate == it)
    it->contents->superstate = 0;
  release_superset_low (cache, it->contents);
  rx_cache_free (cache,
		 (sizeof (struct rx_superstate)
		  + cache->local_cset_size * sizeof (struct rx_inx)),
		 (char *)it);
  --cache->superstates;
  return 1;
}


#ifdef __STDC__
static char *
rx_cache_malloc_or_get (struct rx_cache * cache, int size)
#else
static char *
rx_cache_malloc_or_get (cache, size)
     struct rx_cache * cache;
     int size;
#endif
{
  while (   (cache->bytes_used + size > cache->bytes_allowed)
	 && rx_really_free_superstate (cache))
    ;

  return rx_cache_malloc (cache, size);
}



#ifdef __STDC__
static int
supersetcmp (void * va, void * vb)
#else
static int
supersetcmp (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rx_superset * a = (struct rx_superset *)va;
  struct rx_superset * b = (struct rx_superset *)vb;
  return (   (a == b)
	  || (a && b && (a->id == b->id) && (a->car == b->car) && (a->cdr == b->cdr)));
}

#define rx_abs(A) (((A) > 0) ? (A) : -(A))


#ifdef __STDC__
static struct rx_hash_item *
superset_allocator (struct rx_hash_rules * rules, void * val)
#else
static struct rx_hash_item *
superset_allocator (rules, val)
     struct rx_hash_rules * rules;
     void * val;
#endif
{
  struct rx_cache * cache;
  struct rx_superset * template;
  struct rx_superset * newset;

  cache = ((struct rx_cache *)
	   ((char *)rules
	    - (unsigned long)(&((struct rx_cache *)0)->superset_hash_rules)));
  template = (struct rx_superset *)val;
  newset = ((struct rx_superset *)
	    rx_cache_malloc (cache, sizeof (*template)));
  if (!newset)
    return 0;
  {
    int cdrfinal;
    int cdredges;

    cdrfinal = (template->cdr
		? template->cdr->is_final
		: 0);
    cdredges = (template->cdr
		? template->cdr->has_cset_edges
		: 0);
    
    newset->is_final = (rx_abs (template->car->is_final) > rx_abs(cdrfinal)
			? template->car->is_final
			: template->cdr->is_final);
    newset->has_cset_edges = (template->car->has_cset_edges || cdredges);
  }
  newset->refs = 0;
  newset->id = template->id;
  newset->car = template->car;
  newset->cdr = template->cdr;
  rx_protect_superset (rx, template->cdr);
  newset->superstate = 0;
  newset->starts_for = 0;
  newset->hash_item.data = (void *)newset;
  newset->hash_item.binding = 0;
  return &newset->hash_item;
}

#ifdef __STDC__
static struct rx_hash * 
super_hash_allocator (struct rx_hash_rules * rules)
#else
static struct rx_hash * 
super_hash_allocator (rules)
     struct rx_hash_rules * rules;
#endif
{
  struct rx_cache * cache;
  cache = ((struct rx_cache *)
	   ((char *)rules
	    - (unsigned long)(&((struct rx_cache *)0)->superset_hash_rules)));
  return ((struct rx_hash *)
	  rx_cache_malloc (cache, sizeof (struct rx_hash)));
}


#ifdef __STDC__
static void
super_hash_liberator (struct rx_hash * hash, struct rx_hash_rules * rules)
#else
static void
super_hash_liberator (hash, rules)
     struct rx_hash * hash;
     struct rx_hash_rules * rules;
#endif
{
  struct rx_cache * cache
    = ((struct rx_cache *)
       (char *)rules - (long)(&((struct rx_cache *)0)->superset_hash_rules));
  rx_cache_free (cache, sizeof (struct rx_hash), (char *)hash);
}

#ifdef __STDC__
static void
superset_hash_item_liberator (struct rx_hash_item * it,
			      struct rx_hash_rules * rules)
#else
static void
superset_hash_item_liberator (it, rules)
     struct rx_hash_item * it;
     struct rx_hash_rules * rules;
#endif
{
}

int rx_cache_bound = 3;
static int rx_default_cache_got = 0;

static struct rx_cache default_cache = 
{
  {
    supersetcmp,
    super_hash_allocator,
    super_hash_liberator,
    superset_allocator,
    superset_hash_item_liberator,
  },
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  RX_DEFAULT_DFA_CACHE_SIZE,
  0,
  256,
  rx_id_instruction_table,
  {
    0,
    0,
    0,
    0,
    0
  }
};
struct rx_cache * rx_default_cache = &default_cache;

/* This adds an element to a superstate set.  These sets are lists, such
 * that lists with == elements are ==.  The empty set is returned by
 * superset_cons (rx, 0, 0) and is NOT equivelent to 
 * (struct rx_superset)0.
 */

#ifdef __STDC__
struct rx_superset *
rx_superset_cons (struct rx * rx,
		  struct rx_nfa_state *car, struct rx_superset *cdr)
#else
struct rx_superset *
rx_superset_cons (rx, car, cdr)
     struct rx * rx;
     struct rx_nfa_state *car;
     struct rx_superset *cdr;
#endif
{
  struct rx_cache * cache = rx->cache;
  if (!car && !cdr)
    {
      if (!cache->empty_superset)
	{
	  cache->empty_superset
	    = ((struct rx_superset *)
	       rx_cache_malloc (cache, sizeof (struct rx_superset)));
	  if (!cache->empty_superset)
	    return 0;
	  rx_bzero ((char *)cache->empty_superset, sizeof (struct rx_superset));
	  cache->empty_superset->refs = 1000;
	}
      return cache->empty_superset;
    }
  {
    struct rx_superset template;
    struct rx_hash_item * hit;
    template.car = car;
    template.cdr = cdr;
    template.id = rx->rx_id;
    rx_protect_superset (rx, template.cdr);
    hit = rx_hash_store (&cache->superset_table,
			 (unsigned long)car ^ car->id ^ (unsigned long)cdr,
			 (void *)&template,
			 &cache->superset_hash_rules);
    rx_protect_superset (rx, template.cdr);
    return (hit
	    ?  (struct rx_superset *)hit->data
	    : 0);
  }
}

/* This computes a union of two NFA state sets.  The sets do not have the
 * same representation though.  One is a RX_SUPERSET structure (part
 * of the superstate NFA) and the other is an NFA_STATE_SET (part of the NFA).
 */

#ifdef __STDC__
struct rx_superset *
rx_superstate_eclosure_union (struct rx * rx, struct rx_superset *set, struct rx_nfa_state_set *ecl) 
#else
struct rx_superset *
rx_superstate_eclosure_union (rx, set, ecl)
     struct rx * rx;
     struct rx_superset *set;
     struct rx_nfa_state_set *ecl;
#endif
{
  if (!ecl)
    return set;

  if (!set->car)
    return rx_superset_cons (rx, ecl->car,
			     rx_superstate_eclosure_union (rx, set, ecl->cdr));
  if (set->car == ecl->car)
    return rx_superstate_eclosure_union (rx, set, ecl->cdr);

  {
    struct rx_superset * tail;
    struct rx_nfa_state * first;

    if (set->car->id < ecl->car->id)
      {
	tail = rx_superstate_eclosure_union (rx, set->cdr, ecl);
	first = set->car;
      }
    else
      {
	tail = rx_superstate_eclosure_union (rx, set, ecl->cdr);
	first = ecl->car;
      }
    if (!tail)
      return 0;
    else
      {
	struct rx_superset * answer;
	answer = rx_superset_cons (rx, first, tail);
	if (!answer)
	  {
	    rx_protect_superset (rx, tail);
	    rx_release_superset (rx, tail);
	    return 0;
	  }
	else
	  return answer;
      }
  }
}




/*
 * This makes sure that a list of rx_distinct_futures contains
 * a future for each possible set of side effects in the eclosure
 * of a given state.  This is some of the work of filling in a
 * superstate transition. 
 */

#ifdef __STDC__
static struct rx_distinct_future *
include_futures (struct rx *rx,
		 struct rx_distinct_future *df, struct rx_nfa_state
		 *state, struct rx_superstate *superstate) 
#else
static struct rx_distinct_future *
include_futures (rx, df, state, superstate)
     struct rx *rx;
     struct rx_distinct_future *df;
     struct rx_nfa_state *state;
     struct rx_superstate *superstate;
#endif
{
  struct rx_possible_future *future;
  struct rx_cache * cache = rx->cache;
  for (future = rx_state_possible_futures (rx, state); future; future = future->next)
    {
      struct rx_distinct_future *dfp;
      struct rx_distinct_future *insert_before = 0;
      if (df)
	df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
      for (dfp = df; dfp; dfp = dfp->next_same_super_edge[0])
	if (dfp->effects == future->effects)
	  break;
	else
	  {
	    int order = rx->se_list_cmp (rx, dfp->effects, future->effects);
	    if (order > 0)
	      {
		insert_before = dfp;
		dfp = 0;
		break;
	      }
	  }
      if (df)
	df->next_same_super_edge[1]->next_same_super_edge[0] = df;
      if (!dfp)
	{
	  dfp
	    = ((struct rx_distinct_future *)
	       rx_cache_malloc (cache,
				sizeof (struct rx_distinct_future)));
	  if (!dfp)
	    return 0;
	  if (!df)
	    {
	      df = insert_before = dfp;
	      df->next_same_super_edge[0] = df->next_same_super_edge[1] = df;
	    }
	  else if (!insert_before)
	    insert_before = df;
	  else if (insert_before == df)
	    df = dfp;

	  dfp->next_same_super_edge[0] = insert_before;
	  dfp->next_same_super_edge[1]
	    = insert_before->next_same_super_edge[1];
	  dfp->next_same_super_edge[1]->next_same_super_edge[0] = dfp;
	  dfp->next_same_super_edge[0]->next_same_super_edge[1] = dfp;
	  dfp->next_same_dest = dfp->prev_same_dest = dfp;
	  dfp->future = 0;
	  dfp->present = superstate;
	  dfp->future_frame.inx = rx->instruction_table[rx_cache_miss];
	  dfp->future_frame.data = 0;
	  dfp->future_frame.data_2 = (void *) dfp;
	  dfp->side_effects_frame.inx
	    = rx->instruction_table[rx_do_side_effects];
	  dfp->side_effects_frame.data = 0;
	  dfp->side_effects_frame.data_2 = (void *) dfp;
	  dfp->effects = future->effects;
	}
    }
  return df;
}



/* This constructs a new superstate from its state set.  The only 
 * complexity here is memory management.
 */
#ifdef __STDC__
struct rx_superstate *
rx_superstate (struct rx *rx,
	       struct rx_superset *set)
#else
struct rx_superstate *
rx_superstate (rx, set)
     struct rx *rx;
     struct rx_superset *set;
#endif
{
  struct rx_cache * cache = rx->cache;
  struct rx_superstate * superstate = 0;

  /* Does the superstate already exist in the cache? */
  if (set->superstate)
    {
      if (set->superstate->rx_id != rx->rx_id)
	{
	  /* Aha.  It is in the cache, but belongs to a superstate
	   * that refers to an NFA that no longer exists.
	   * (We know it no longer exists because it was evidently
	   *  stored in the same region of memory as the current nfa
	   *  yet it has a different id.)
	   */
	  superstate = set->superstate;
	  if (!superstate->is_semifree)
	    {
	      if (cache->lru_superstate == superstate)
		{
		  cache->lru_superstate = superstate->next_recyclable;
		  if (cache->lru_superstate == superstate)
		    cache->lru_superstate = 0;
		}
	      {
		superstate->next_recyclable->prev_recyclable
		  = superstate->prev_recyclable;
		superstate->prev_recyclable->next_recyclable
		  = superstate->next_recyclable;
		if (!cache->semifree_superstate)
		  {
		    (cache->semifree_superstate
		     = superstate->next_recyclable
		     = superstate->prev_recyclable
		     = superstate);
		  }
		else
		  {
		    superstate->next_recyclable = cache->semifree_superstate;
		    superstate->prev_recyclable
		      = cache->semifree_superstate->prev_recyclable;
		    superstate->next_recyclable->prev_recyclable
		      = superstate;
		    superstate->prev_recyclable->next_recyclable
		      = superstate;
		    cache->semifree_superstate = superstate;
		  }
		++cache->semifree_superstates;
	      }
	    }
	  set->superstate = 0;
	  goto handle_cache_miss;
	}
      ++cache->hits;
      superstate = set->superstate;

      rx_refresh_this_superstate (cache, superstate);
      return superstate;
    }

 handle_cache_miss:

  /* This point reached only for cache misses. */
  ++cache->misses;
#if RX_DEBUG
  if (rx_debug_trace > 1)
    {
      struct rx_superset * setp = set;
      fprintf (stderr, "Building a superstet %d(%d): ", rx->rx_id, set);
      while (setp)
	{
	  fprintf (stderr, "%d ", setp->id);
	  setp = setp->cdr;
	}
      fprintf (stderr, "(%d)\n", set);
    }
#endif

  {
    int superstate_size;
    superstate_size = (  sizeof (*superstate)
		       + (  sizeof (struct rx_inx)
			  * rx->local_cset_size));
    superstate = ((struct rx_superstate *)
		  rx_cache_malloc_or_get (cache, superstate_size));
    ++cache->superstates;
  }							       

  if (!superstate)
    return 0;

  if (!cache->lru_superstate)
    (cache->lru_superstate
     = superstate->next_recyclable
     = superstate->prev_recyclable
     = superstate);
  else
    {
      superstate->next_recyclable = cache->lru_superstate;
      superstate->prev_recyclable = cache->lru_superstate->prev_recyclable;
      (  superstate->prev_recyclable->next_recyclable
       = superstate->next_recyclable->prev_recyclable
       = superstate);
    }
  superstate->rx_id = rx->rx_id;
  superstate->transition_refs = 0;
  superstate->locks = 0;
  superstate->is_semifree = 0;
  set->superstate = superstate;
  superstate->contents = set;
  rx_protect_superset (rx, set);
  superstate->edges = 0;
  {
    int x;
    /* None of the transitions from this superstate are known yet. */
    for (x = 0; x < rx->local_cset_size; ++x)
      {
	struct rx_inx * ifr = &superstate->transitions[x];
	ifr->inx = rx->instruction_table [rx_cache_miss];
	ifr->data = ifr->data_2 = 0;
      }
  }
  return superstate;
}


/* This computes the destination set of one edge of the superstate NFA.
 * Note that a RX_DISTINCT_FUTURE is a superstate edge.
 * Returns 0 on an allocation failure.
 */

#ifdef __STDC__
static int 
solve_destination (struct rx *rx, struct rx_distinct_future *df)
#else
static int 
solve_destination (rx, df)
     struct rx *rx;
     struct rx_distinct_future *df;
#endif
{
  struct rx_super_edge *tc = df->edge;
  struct rx_superset *nfa_state;
  struct rx_superset *nil_set = rx_superset_cons (rx, 0, 0);
  struct rx_superset *solution = nil_set;
  struct rx_superstate *dest;

  rx_protect_superset (rx, solution);
  /* Iterate over all NFA states in the state set of this superstate. */
  for (nfa_state = df->present->contents;
       nfa_state->car;
       nfa_state = nfa_state->cdr)
    {
      struct rx_nfa_edge *e;
      /* Iterate over all edges of each NFA state. */
      for (e = nfa_state->car->edges; e; e = e->next)
        /* If we find an edge that is labeled with 
	 * the characters we are solving for.....
	 */
	if ((e->type == ne_cset)
	    && rx_bitset_is_subset (rx->local_cset_size,
				    tc->cset,
				    e->params.cset))
	  {
	    struct rx_nfa_state *n = e->dest;
	    struct rx_possible_future *pf;
	    /* ....search the partial epsilon closures of the destination
	     * of that edge for a path that involves the same set of
	     * side effects we are solving for.
	     * If we find such a RX_POSSIBLE_FUTURE, we add members to the
	     * stateset we are computing.
	     */
	    for (pf = rx_state_possible_futures (rx, n); pf; pf = pf->next)
	      if (pf->effects == df->effects)
		{
		  struct rx_superset * old_sol;
		  old_sol = solution;
		  solution = rx_superstate_eclosure_union (rx, solution,
							   pf->destset);
		  if (!solution)
		    return 0;
		  rx_protect_superset (rx, solution);
		  rx_release_superset (rx, old_sol);
		}
	  }
    }
  /* It is possible that the RX_DISTINCT_FUTURE we are working on has 
   * the empty set of NFA states as its definition.  In that case, this
   * is a failure point.
   */
  if (solution == nil_set)
    {
      df->future_frame.inx = (void *) rx_backtrack;
      df->future_frame.data = 0;
      df->future_frame.data_2 = 0;
      return 1;
    }
  dest = rx_superstate (rx, solution);
  rx_release_superset (rx, solution);
  if (!dest)
    return 0;

  {
    struct rx_distinct_future *dft;
    dft = df;
    df->prev_same_dest->next_same_dest = 0;
    while (dft)
      {
	dft->future = dest;
	dft->future_frame.inx = rx->instruction_table[rx_next_char];
	dft->future_frame.data = (void *) dest->transitions;
	dft->future_frame.data_2 = (void *) dest->contents->is_final;
	dft = dft->next_same_dest;
      }
    df->prev_same_dest->next_same_dest = df;
  }
  if (!dest->transition_refs)
    dest->transition_refs = df;
  else
    {
      struct rx_distinct_future *dft;
      dft = dest->transition_refs->next_same_dest;
      dest->transition_refs->next_same_dest = df->next_same_dest;
      df->next_same_dest->prev_same_dest = dest->transition_refs;
      df->next_same_dest = dft;
      dft->prev_same_dest = df;
    }
  return 1;
}


/* This takes a superstate and a character, and computes some edges
 * from the superstate NFA.  In particular, this computes all edges
 * that lead from SUPERSTATE given CHR.   This function also 
 * computes the set of characters that share this edge set.
 * This returns 0 on allocation error.
 * The character set and list of edges are returned through 
 * the paramters CSETOUT and DFOUT.
 */

#ifdef __STDC__
static int 
compute_super_edge (struct rx *rx, struct rx_distinct_future **dfout,
			  rx_Bitset csetout, struct rx_superstate *superstate,
			  unsigned char chr)  
#else
static int 
compute_super_edge (rx, dfout, csetout, superstate, chr)
     struct rx *rx;
     struct rx_distinct_future **dfout;
     rx_Bitset csetout;
     struct rx_superstate *superstate;
     unsigned char chr;
#endif
{
  struct rx_superset *stateset = superstate->contents;

  /* To compute the set of characters that share edges with CHR, 
   * we start with the full character set, and subtract.
   */
  rx_bitset_universe (rx->local_cset_size, csetout);
  *dfout = 0;

  /* Iterate over the NFA states in the superstate state-set. */
  while (stateset->car)
    {
      struct rx_nfa_edge *e;
      for (e = stateset->car->edges; e; e = e->next)
	if (e->type == ne_cset)
	  {
	    if (!RX_bitset_member (e->params.cset, chr))
	      /* An edge that doesn't apply at least tells us some characters
	       * that don't share the same edge set as CHR.
	       */
	      rx_bitset_difference (rx->local_cset_size, csetout, e->params.cset);
	    else
	      {
		/* If we find an NFA edge that applies, we make sure there
		 * are corresponding edges in the superstate NFA.
		 */
		{
		  struct rx_distinct_future * saved;
		  saved = *dfout;
		  *dfout = include_futures (rx, *dfout, e->dest, superstate);
		  if (!*dfout)
		    {
		      struct rx_distinct_future * df;
		      df = saved;
		      if (df)
			df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
		      while (df)
			{
			  struct rx_distinct_future *dft;
			  dft = df;
			  df = df->next_same_super_edge[0];

			  if (dft->future && dft->future->transition_refs == dft)
			    {
			      dft->future->transition_refs = dft->next_same_dest;
			      if (dft->future->transition_refs == dft)
				dft->future->transition_refs = 0;
			    }
			  dft->next_same_dest->prev_same_dest = dft->prev_same_dest;
			  dft->prev_same_dest->next_same_dest = dft->next_same_dest;
			  rx_cache_free (rx->cache, sizeof (*dft), (char *)dft);
			}
		      return 0;
		    }
		}
		/* We also trim the character set a bit. */
		rx_bitset_intersection (rx->local_cset_size,
					csetout, e->params.cset);
	      }
	  }
      stateset = stateset->cdr;
    }
  return 1;
}


/* This is a constructor for RX_SUPER_EDGE structures.  These are
 * wrappers for lists of superstate NFA edges that share character sets labels.
 * If a transition class contains more than one rx_distinct_future (superstate
 * edge), then it represents a non-determinism in the superstate NFA.
 */

#ifdef __STDC__
static struct rx_super_edge *
rx_super_edge (struct rx *rx,
	       struct rx_superstate *super, rx_Bitset cset,
	       struct rx_distinct_future *df) 
#else
static struct rx_super_edge *
rx_super_edge (rx, super, cset, df)
     struct rx *rx;
     struct rx_superstate *super;
     rx_Bitset cset;
     struct rx_distinct_future *df;
#endif
{
  struct rx_super_edge *tc;
  int tc_size;

  tc_size = (  sizeof (struct rx_super_edge)
	     + rx_sizeof_bitset (rx->local_cset_size));

  tc = ((struct rx_super_edge *)
	rx_cache_malloc (rx->cache, tc_size));

  if (!tc)
    return 0;

  tc->next = super->edges;
  super->edges = tc;
  tc->rx_backtrack_frame.inx = rx->instruction_table[rx_backtrack_point];
  tc->rx_backtrack_frame.data = 0;
  tc->rx_backtrack_frame.data_2 = (void *) tc;
  tc->options = df;
  tc->cset = (rx_Bitset) ((char *) tc + sizeof (*tc));
  rx_bitset_assign (rx->local_cset_size, tc->cset, cset);
  if (df)
    {
      struct rx_distinct_future * dfp = df;
      df->next_same_super_edge[1]->next_same_super_edge[0] = 0;
      while (dfp)
	{
	  dfp->edge = tc;
	  dfp = dfp->next_same_super_edge[0];
	}
      df->next_same_super_edge[1]->next_same_super_edge[0] = df;
    }
  return tc;
}


/* There are three kinds of cache miss.  The first occurs when a
 * transition is taken that has never been computed during the
 * lifetime of the source superstate.  That cache miss is handled by
 * calling COMPUTE_SUPER_EDGE.  The second kind of cache miss
 * occurs when the destination superstate of a transition doesn't
 * exist.  SOLVE_DESTINATION is used to construct the destination superstate.
 * Finally, the third kind of cache miss occurs when the destination
 * superstate of a transition is in a `semi-free state'.  That case is
 * handled by UNFREE_SUPERSTATE.
 *
 * The function of HANDLE_CACHE_MISS is to figure out which of these
 * cases applies.
 */

#ifdef __STDC__
static void
install_partial_transition  (struct rx_superstate *super,
			     struct rx_inx *answer,
			     RX_subset set, int offset)
#else
static void
install_partial_transition  (super, answer, set, offset)
     struct rx_superstate *super;
     struct rx_inx *answer;
     RX_subset set;
     int offset;
#endif
{
  int start = offset;
  int end = start + 32;
  RX_subset pos = 1;
  struct rx_inx * transitions = super->transitions;
  
  while (start < end)
    {
      if (set & pos)
	transitions[start] = *answer;
      pos <<= 1;
      ++start;
    }
}


#ifdef __STDC__
struct rx_inx *
rx_handle_cache_miss (struct rx *rx, struct rx_superstate *super, unsigned char chr, void *data) 
#else
struct rx_inx *
rx_handle_cache_miss (rx, super, chr, data)
     struct rx *rx;
     struct rx_superstate *super;
     unsigned char chr;
     void *data;
#endif
{
  int offset = chr / RX_subset_bits;
  struct rx_distinct_future *df = data;

  if (!df)			/* must be the shared_cache_miss_frame */
    {
      /* Perhaps this is just a transition waiting to be filled. */
      struct rx_super_edge *tc;
      RX_subset mask = rx_subset_singletons [chr % RX_subset_bits];

      for (tc = super->edges; tc; tc = tc->next)
	if (tc->cset[offset] & mask)
	  {
	    struct rx_inx * answer;
	    df = tc->options;
	    answer = ((tc->options->next_same_super_edge[0] != tc->options)
		      ? &tc->rx_backtrack_frame
		      : (df->effects
			 ? &df->side_effects_frame
			 : &df->future_frame));
	    install_partial_transition (super, answer,
					tc->cset [offset], offset * 32);
	    return answer;
	  }
      /* Otherwise, it's a flushed or  newly encountered edge. */
      {
	char cset_space[1024];	/* this limit is far from unreasonable */
	rx_Bitset trcset;
	struct rx_inx *answer;

	if (rx_sizeof_bitset (rx->local_cset_size) > sizeof (cset_space))
	  return 0;		/* If the arbitrary limit is hit, always fail */
				/* cleanly. */
	trcset = (rx_Bitset)cset_space;
	rx_lock_superstate (rx, super);
	if (!compute_super_edge (rx, &df, trcset, super, chr))
	  {
	    rx_unlock_superstate (rx, super);
	    return 0;
	  }
	if (!df)		/* We just computed the fail transition. */
	  {
	    static struct rx_inx
	      shared_fail_frame = { 0, 0, (void *)rx_backtrack, 0 };
	    answer = &shared_fail_frame;
	  }
	else
	  {
	    tc = rx_super_edge (rx, super, trcset, df);
	    if (!tc)
	      {
		rx_unlock_superstate (rx, super);
		return 0;
	      }
	    answer = ((tc->options->next_same_super_edge[0] != tc->options)
		      ? &tc->rx_backtrack_frame
		      : (df->effects
			 ? &df->side_effects_frame
			 : &df->future_frame));
	  }
	install_partial_transition (super, answer,
				    trcset[offset], offset * 32);
	rx_unlock_superstate (rx, super);
	return answer;
      }
    }
  else if (df->future) /* A cache miss on an edge with a future? Must be
			* a semi-free destination. */
    {				
      if (df->future->is_semifree)
	refresh_semifree_superstate (rx->cache, df->future);
      return &df->future_frame;
    }
  else
    /* no future superstate on an existing edge */
    {
      rx_lock_superstate (rx, super);
      if (!solve_destination (rx, df))
	{
	  rx_unlock_superstate (rx, super);
	  return 0;
	}
      if (!df->effects
	  && (df->edge->options->next_same_super_edge[0] == df->edge->options))
	install_partial_transition (super, &df->future_frame,
				    df->edge->cset[offset], offset * 32);
      rx_unlock_superstate (rx, super);
      return &df->future_frame;
    }
}


