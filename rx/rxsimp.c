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
#include "rxsimp.h"




/* Could reasonably hashcons instead of in rxunfa.c */

#ifdef __STDC__
int
rx_simple_rexp (struct rexp_node ** answer,
		int cset_size,
		struct rexp_node *node,
		struct rexp_node ** subexps)
#else
int
rx_simple_rexp (answer, cset_size, node, subexps)
     struct rexp_node ** answer;
     int cset_size;
     struct rexp_node *node;
     struct rexp_node ** subexps;
#endif
{
  int stat;

  if (!node)
    {
      *answer = 0;
      return 0;
    }

  if (!node->observed)
    {
      rx_save_rexp (node);
      *answer = node;
      return 0;
    }

  if (node->simplified)
    {
      rx_save_rexp (node->simplified);
      *answer = node->simplified;
      return 0;
    }

  switch (node->type)
    {
    default:
    case r_cset:
    case r_string:
    case r_cut:
      return -2;		/* an internal error, really */

    case r_parens:
      stat = rx_simple_rexp (answer, cset_size,
			     node->params.pair.left,
			     subexps);
      break;

    case r_context:
      if (isdigit (node->params.intval))
	 stat = rx_simple_rexp (answer, cset_size,
				subexps [node->params.intval - '0'],
				subexps);
      else
	{
	  *answer = 0;
	  stat = 0;
	}
      break;

    case r_concat:
    case r_alternate:
    case r_opt:
    case r_star:
    case r_plus:
    case r_interval:
      {
	struct rexp_node *n;
	n = rexp_node (node->type);
	if (!n)
	  return -1;

	if (node->params.cset)
	  {
	    n->params.cset = rx_copy_cset (cset_size,
					   node->params.cset);
	    if (!n->params.cset)
	      {
		rx_free_rexp (n);
		return -1;
	      }
	  }
	n->params.intval = node->params.intval;
	n->params.intval2 = node->params.intval2;
	{
	  int s;
    
	  s = rx_simple_rexp (&n->params.pair.left, cset_size,
			      node->params.pair.left, subexps);
	  if (!s)
	    s = rx_simple_rexp (&n->params.pair.right, cset_size,
				node->params.pair.right, subexps);
	  if (!s)
	    {
	      *answer = n;
	      stat = 0;
	    }
	  else
	    {
	      rx_free_rexp  (n);
	      stat = s;
	    }
	}
      }      
      break;
    }

  if (!stat)
    {
      node->simplified = *answer;
      rx_save_rexp (node->simplified);
    }
  return stat;
}  


