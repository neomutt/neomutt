#ifndef RXUNFAH
#define RXUNFAH

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



#include "_rx.h"


struct rx_unfaniverse 
{
  int delay;
  int delayed;
  struct rx_hash table;
  struct rx_cached_rexp * free_queue;
};


struct rx_unfa
{
  int refs;
  struct rexp_node * exp;
  struct rx * nfa;
  int cset_size;
  struct rx_unfaniverse * verse;
};

struct rx_cached_rexp
{
  struct rx_unfa unfa;
  struct rx_cached_rexp * next;
  struct rx_cached_rexp * prev;
  struct rx_hash_item * hash_item;
};



#ifdef __STDC__
extern struct rx_unfaniverse * rx_make_unfaniverse (int delay);
extern void rx_free_unfaniverse (struct rx_unfaniverse * it);
extern struct rx_unfa * rx_unfa (struct rx_unfaniverse * unfaniverse, struct rexp_node * exp, int cset_size);
extern void rx_free_unfa (struct rx_unfa * unfa);
extern void rx_save_unfa (struct rx_unfa * unfa);

#else /* STDC */
extern struct rx_unfaniverse * rx_make_unfaniverse ();
extern void rx_free_unfaniverse ();
extern struct rx_unfa * rx_unfa ();
extern void rx_free_unfa ();
extern void rx_save_unfa ();

#endif /* STDC */
#endif  /* RXUNFAH */
