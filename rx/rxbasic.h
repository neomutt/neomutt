/* classes: h_files */

#ifndef RXBASICH
#define RXBASICH
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



#include "rxcontext.h"
#include "rxnode.h"
#include "rxspencer.h"
#include "rxunfa.h"



#ifndef RX_DEFAULT_NFA_DELAY
/* This value bounds the number of NFAs kept in a cache of recent NFAs.
 * This value is used whenever the rx_basic_* entry points are used,
 * for example, when the Posix entry points are invoked.
 */
#define RX_DEFAULT_NFA_DELAY 64
#endif


#ifdef __STDC__
extern struct rx_unfaniverse * rx_basic_unfaniverse (void);
extern struct rx_solutions * rx_basic_make_solutions (struct rx_registers * regs, struct rexp_node * expression, struct rexp_node ** subexps, int start, int end, struct rx_context_rules * rules, const unsigned char * str);
extern void rx_basic_free_solutions (struct rx_solutions * solns);

#else /* STDC */
extern struct rx_unfaniverse * rx_basic_unfaniverse ();
extern struct rx_solutions * rx_basic_make_solutions ();
extern void rx_basic_free_solutions ();

#endif /* STDC */








#endif  /* RXBASICH */
