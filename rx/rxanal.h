/* classes: h_files */

#ifndef RXANALH
#define RXANALH
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



#include "rxcset.h"
#include "rxnode.h"
#include "rxsuper.h"




enum rx_answers
{
  rx_yes = 0,
  rx_no = 1,
  rx_bogus = -1,
  rx_start_state_with_too_many_futures = rx_bogus - 1
  /* n < 0 -- error */
};

struct rx_classical_system
{
  struct rx * rx;
  struct rx_superstate * state;
  int final_tag;
};



#ifdef __STDC__
extern int rx_posix_analyze_rexp (struct rexp_node *** subexps,
				  size_t * re_nsub,
				  struct rexp_node * node,
				  int id);
extern int rx_fill_in_fastmap (int cset_size, unsigned char * map, struct rexp_node * exp);
extern int rx_is_anchored_p (struct rexp_node * exp);
extern enum rx_answers rx_start_superstate (struct rx_classical_system * frame);
extern enum rx_answers rx_fit_p (struct rx_classical_system * frame, unsigned const char * burst, int len);
extern enum rx_answers rx_advance (struct rx_classical_system * frame, unsigned const char * burst, int len);
extern int rx_advance_to_final (struct rx_classical_system * frame, unsigned const char * burst, int len);
extern void rx_terminate_system (struct rx_classical_system * frame);
extern void rx_init_system (struct rx_classical_system * frame, struct rx * rx);

#else /* STDC */
extern int rx_posix_analyze_rexp ();
extern int rx_fill_in_fastmap ();
extern int rx_is_anchored_p ();
extern enum rx_answers rx_start_superstate ();
extern enum rx_answers rx_fit_p ();
extern enum rx_answers rx_advance ();
extern int rx_advance_to_final ();
extern void rx_terminate_system ();
extern void rx_init_system ();

#endif /* STDC */



#endif  /* RXANALH */
