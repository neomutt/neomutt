/* classes: h_files */

#ifndef RXNFAH
#define RXNFAH
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

#include "_rx.h"
#include "rxnode.h"


/* NFA
 *
 * A syntax tree is compiled into an NFA.  This page defines the structure
 * of that NFA.
 */

struct rx_nfa_state
{
  /* These are kept in a list as the NFA is being built. 
   * Here is the link.
   */
  struct rx_nfa_state *next;

  /* After the NFA is built, states are given integer id's.
   * States whose outgoing transitions are all either epsilon or 
   * side effect edges are given ids less than 0.  Other states
   * are given successive non-negative ids starting from 0.
   *
   * Here is the id for this state:
   */
  int id;

  /* The list of NFA edges that go from this state to some other. */
  struct rx_nfa_edge *edges;

  /* If you land in this state, then you implicitly land
   * in all other states reachable by only epsilon translations.
   * Call the set of maximal loop-less paths to such states the 
   * epsilon closure of this state.
   *
   * There may be other states that are reachable by a mixture of
   * epsilon and side effect edges.  Consider the set of maximal loop-less
   * paths of that sort from this state.  Call it the epsilon-side-effect
   * closure of the state.
   * 
   * The epsilon closure of the state is a subset of the epsilon-side-
   * effect closure.  It consists of all the paths that contain 
   * no side effects -- only epsilon edges.
   * 
   * The paths in the epsilon-side-effect closure  can be partitioned
   * into equivalance sets. Two paths are equivalant if they have the
   * same set of side effects, in the same order.  The epsilon-closure
   * is one of these equivalance sets.  Let's call these equivalance
   * sets: observably equivalant path sets.  That name is chosen
   * because equivalance of two paths means they cause the same side
   * effects -- so they lead to the same subsequent observations other
   * than that they may wind up in different target states.
   *
   * The superstate nfa, which is derived from this nfa, is based on
   * the observation that all of the paths in an observably equivalant
   * path set can be explored at the same time, provided that the
   * matcher keeps track not of a single nfa state, but of a set of
   * states.   In particular, after following all the paths in an
   * observably equivalant set, you wind up at a set of target states.
   * That set of target states corresponds to one state in the
   * superstate NFA.
   *
   * Staticly, before matching begins, it is convenient to analyze the
   * nfa.  Each state is labeled with a list of the observably
   * equivalant path sets who's union covers all the
   * epsilon-side-effect paths beginning in this state.  This list is
   * called the possible futures of the state.
   *
   * A trivial example is this NFA:
   *             s1
   *         A  --->  B
   *
   *             s2  
   *            --->  C
   *
   *             epsilon           s1
   *            --------->  D   ------> E
   * 
   * 
   * In this example, A has two possible futures.
   * One invokes the side effect `s1' and contains two paths,
   * one ending in state B, the other in state E.
   * The other invokes the side effect `s2' and contains only
   * one path, landing in state C.
   *
   * Here is a list of the possible futures of this state:
   */
  struct rx_possible_future *futures;
  int futures_computed:1;


  /* There is exactly one distinguished starting state in every NFA: */
  unsigned int is_start:1;

  int has_cset_edges:1;

  /* There may be many final states if the "cut" operator was used.
   * each will have a different non-0 value for this field:
   */
  int is_final;


  /* These are used internally during NFA construction... */
  unsigned int eclosure_needed:1;
  unsigned int mark:1;
};


/* An edge in an NFA is typed: 
 */
enum rx_nfa_etype
{
  /* A cset edge is labled with a set of characters one of which
   * must be matched for the edge to be taken.
   */
  ne_cset,

  /* An epsilon edge is taken whenever its starting state is
   * reached. 
   */
  ne_epsilon,

  /* A side effect edge is taken whenever its starting state is
   * reached.  Side effects may cause the match to fail or the
   * position of the matcher to advance.
   */
  ne_side_effect
};

struct rx_nfa_edge
{
  struct rx_nfa_edge *next;
  enum rx_nfa_etype type;
  struct rx_nfa_state *dest;
  union
  {
    rx_Bitset cset;
    void * side_effect;
  } params;
};



/* A possible future consists of a list of side effects
 * and a set of destination states.  Below are their
 * representations.  These structures are hash-consed so
 * that lists with the same elements share a representation
 * (their addresses are ==).
 */

struct rx_nfa_state_set
{
  struct rx_nfa_state * car;
  struct rx_nfa_state_set * cdr;
};

struct rx_se_list
{
  void * car;
  struct rx_se_list * cdr;
};

struct rx_possible_future
{
  struct rx_possible_future *next;
  struct rx_se_list * effects;
  struct rx_nfa_state_set * destset;
};




#ifdef __STDC__
extern struct rx_nfa_state * rx_nfa_state (struct rx *rx);
extern struct rx_nfa_edge * rx_nfa_edge (struct rx *rx,
					 enum rx_nfa_etype type,
					 struct rx_nfa_state *start,
					 struct rx_nfa_state *dest);
extern int rx_build_nfa (struct rx *rx,
			 struct rexp_node *rexp,
			 struct rx_nfa_state **start,
			 struct rx_nfa_state **end);
extern struct rx_possible_future * rx_state_possible_futures (struct rx * rx, struct rx_nfa_state * n);
extern void rx_free_nfa (struct rx *rx);

#else /* STDC */
extern struct rx_nfa_state * rx_nfa_state ();
extern struct rx_nfa_edge * rx_nfa_edge ();
extern int rx_build_nfa ();
extern struct rx_possible_future * rx_state_possible_futures ();
extern void rx_free_nfa ();

#endif /* STDC */











#endif  /* RXNFAH */
