/* classes: h_files */

#ifndef RXSPENCERH
#define RXSPENCERH
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



#include "rxproto.h"
#include "rxnode.h"
#include "rxunfa.h"
#include "rxanal.h"
#include "inst-rxposix.h"



#define RX_MANY_CASES 30


typedef enum rx_answers (*rx_vmfn)
     P((void * closure,
	unsigned const char ** burst, int * len, int * offset,
	int start, int end, int need));

typedef enum rx_answers (*rx_contextfn)
     P((void * closure,
	struct rexp_node * node,
	int start, int end,
	struct rx_registers * regs));


struct rx_solutions
{
  int step;

  int cset_size;
  struct rexp_node * exp;
  struct rexp_node ** subexps;
  struct rx_registers * regs;

  int start;
  int end;

  rx_vmfn vmfn;
  rx_contextfn contextfn;
  void * closure;

  struct rx_unfaniverse * verse;
  struct rx_unfa * dfa;
  struct rx_classical_system match_engine;
  struct rx_unfa * left_dfa;
  struct rx_classical_system left_match_engine;

  int split_guess;
  struct rx_solutions * left;
  struct rx_solutions * right;

  int interval_x;

  int saved_rm_so;
  int saved_rm_eo;

  int final_tag;
};

extern struct rx_solutions rx_no_solutions;


#ifdef __STDC__
extern struct rx_solutions * rx_make_solutions (struct rx_registers * regs, struct rx_unfaniverse * verse, struct rexp_node * expression, struct rexp_node ** subexps, int cset_size, int start, int end, rx_vmfn vmfn, rx_contextfn contextfn, void * closure);
extern void rx_free_solutions (struct rx_solutions * solns);
extern int rx_best_end_guess (struct rx_solutions * solns, struct rexp_node *  exp, int bound);
extern enum rx_answers rx_next_solution (struct rx_solutions * solns);

#else /* STDC */
extern struct rx_solutions * rx_make_solutions ();
extern void rx_free_solutions ();
extern int rx_best_end_guess ();
extern enum rx_answers rx_next_solution ();

#endif /* STDC */

#endif  /* RXSPENCERH */
