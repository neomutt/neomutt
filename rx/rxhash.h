/* classes: h_files */

#ifndef RXHASHH
#define RXHASHH
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


#include "rxbitset.h"

/* giant inflatable hash trees */

struct rx_hash_item
{
  struct rx_hash_item * next_same_hash;
  struct rx_hash * table;
  unsigned long hash;
  void * data;
  void * binding;
};

struct rx_hash
{
  struct rx_hash * parent;
  int refs;
  RX_subset nested_p;
  void ** children[16];
};

struct rx_hash_rules;

/* rx_hash_eq should work like the == operator. */

#ifdef __STDC__
typedef int (*rx_hash_eq)(void *, void *);
typedef struct rx_hash * (*rx_alloc_hash)(struct rx_hash_rules *);
typedef void (*rx_free_hash)(struct rx_hash *,
			    struct rx_hash_rules *);
typedef struct rx_hash_item * (*rx_alloc_hash_item)(struct rx_hash_rules *,
						    void *);
typedef void (*rx_free_hash_item)(struct rx_hash_item *,
				 struct rx_hash_rules *);
typedef void (*rx_hash_freefn) (struct rx_hash_item * it);
#else
typedef int (*rx_hash_eq)();
typedef struct rx_hash * (*rx_alloc_hash)();
typedef void (*rx_free_hash)();
typedef struct rx_hash_item * (*rx_alloc_hash_item)();
typedef void (*rx_free_hash_item)();
typedef void (*rx_hash_freefn) ();
#endif

struct rx_hash_rules
{
  rx_hash_eq eq;
  rx_alloc_hash hash_alloc;
  rx_free_hash free_hash;
  rx_alloc_hash_item hash_item_alloc;
  rx_free_hash_item free_hash_item;
};


#ifdef __STDC__
extern struct rx_hash_item * rx_hash_find (struct rx_hash * table,
						   unsigned long hash,
						   void * value,
						   struct rx_hash_rules * rules);
extern struct rx_hash_item * rx_hash_store (struct rx_hash * table,
						    unsigned long hash,
						    void * value,
						    struct rx_hash_rules * rules);
extern void rx_hash_free (struct rx_hash_item * it, struct rx_hash_rules * rules);
extern void rx_free_hash_table (struct rx_hash * tab, rx_hash_freefn freefn,
					struct rx_hash_rules * rules);
extern int rx_count_hash_nodes (struct rx_hash * st);

#else /* STDC */
extern struct rx_hash_item * rx_hash_find ();
extern struct rx_hash_item * rx_hash_store ();
extern void rx_hash_free ();
extern void rx_free_hash_table ();
extern int rx_count_hash_nodes ();

#endif /* STDC */





#endif  /* RXHASHH */

