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
#include "rxnode.h"
#include "rxhash.h"

#ifdef __STDC__
static int
rexp_node_equal (void * va, void * vb)
#else
static int
rexp_node_equal (va, vb)
     void * va;
     void * vb;
#endif
{
  struct rexp_node * a;
  struct rexp_node * b;

  a = (struct rexp_node *)va;
  b = (struct rexp_node *)vb;

  return (   (va == vb)
	  || (   (a->type == b->type)
	      && (a->params.intval == b->params.intval)
	      && (a->params.intval2 == b->params.intval2)
	      && rx_bitset_is_equal (a->params.cset_size, a->params.cset, b->params.cset)
	      && rexp_node_equal (a->params.pair.left, b->params.pair.left)
	      && rexp_node_equal (a->params.pair.right, b->params.pair.right)));
}


