/* classes: h_files */

#ifndef RXSTRH
#define RXSTRH
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




#include "rxspencer.h"
#include "rxcontext.h"



  

struct rx_str_closure
{
  struct rx_context_rules rules;
  const unsigned char * str;
  int len;
};



#ifdef __STDC__
extern enum rx_answers rx_str_vmfn (void * closure, unsigned const char ** burstp, int * lenp, int * offsetp, int start, int end, int need);
extern enum rx_answers rx_str_contextfn (void * closure, struct rexp_node * node, int start, int end, struct rx_registers * regs);

#else /* STDC */
extern enum rx_answers rx_str_vmfn ();
extern enum rx_answers rx_str_contextfn ();

#endif /* STDC */






#endif  /* RXSTRH */
