/* classes: src_files */

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



#define INITSIZE   8
#define EXPANDSIZE 8

#ifdef __STDC__
static int
rx_init_string(struct rx_string *thisone, char first)
#else
static int
rx_init_string(thisone, first)
     struct rx_string *thisone;
     char first;
#endif
{
  char *tmp;

  tmp = (char *) malloc (INITSIZE);

  if(!tmp)
    return -1;

  thisone->contents    = tmp;
  thisone->contents[0] = first;
  thisone->reallen     = INITSIZE;
  thisone->len         = 1;
  return 0;
}

#ifdef __STDC__
static void
rx_free_string (struct rx_string *junk)
#else
static void
rx_free_string (junk)
     struct rx_string *junk;
#endif
{
  free (junk->contents);
  junk->len = junk->reallen = 0;
  junk->contents = 0;
}


#ifdef __STDC__
int
rx_adjoin_string (struct rx_string *str, char c)
#else
int
rx_adjoin_string (str, c)
     struct rx_string *str;
     char c;
#endif
{
  if (!str->contents)
    return rx_init_string (str, c);

  if (str->len == str->reallen)
    {
      char *temp;
      temp = (char *) realloc (str->contents, str->reallen + EXPANDSIZE);

      if(!temp)
	return -1;

      str->contents = temp;
      str->reallen += EXPANDSIZE;
    }

  str->contents[str->len++] = c;
  return 0;
}


#ifdef __STDC__
static int
rx_copy_string (struct rx_string *to, struct rx_string *from)
#else
static int
rx_copy_string (to, from)
	struct rx_string *to;
	struct rx_string *from;
#endif
{
  char *tmp;

  if (from->len)
    {
      tmp = (char *) malloc (from->reallen);

      if (!tmp)
	return -1;
    }

  rx_free_string (to);
  to->len      = from->len;
  to->reallen  = from->reallen;
  to->contents = tmp;

  memcpy (to->contents, from->contents, from->reallen);

  return 0;
}


#ifdef __STDC__
static int
rx_compare_rx_strings (struct rx_string *a, struct rx_string *b)
#else
static int
rx_compare_rx_strings (a, b)
     struct rx_string *a;
     struct rx_string *b;
#endif
{
  if (a->len != b->len)
    return 0;

  if (a->len)
    return !memcmp (a->contents, b->contents, a->len);
  else
    return 1;			/* trivial case: "" == "" */
}


#ifdef __STDC__
static unsigned long
rx_string_hash (unsigned long seed, struct rx_string *str)
#else
static unsigned long
rx_string_hash (seed, str)
     unsigned long seed;
     struct rx_string *str;
#endif
{
  /* From Tcl: */
  unsigned long result;
  int c;
  char * string;
  int len;

  string = str->contents;
  len = str->len;
  result = seed;

  while (len--)
    {
      c = *string;
      string++;
      result += (result<<3) + c;
    }
  return result;
}




#ifdef __STDC__
struct rexp_node *
rexp_node (int type)
#else
struct rexp_node *
rexp_node (type)
     int type;
#endif
{
  struct rexp_node *n;

  n = (struct rexp_node *) malloc (sizeof (*n));
  rx_bzero ((char *)n, sizeof (*n));
  if (n)
    {
      n->type = type;
      n->id = -1;
      n->refs = 1;
    }
  return n;
}


/* free_rexp_node assumes that the bitset passed to rx_mk_r_cset
 * can be freed using rx_free_cset.
 */

#ifdef __STDC__
struct rexp_node *
rx_mk_r_cset (int type, int size, rx_Bitset b)
#else
struct rexp_node *
rx_mk_r_cset (type, size, b)
     int type;
     int size;
     rx_Bitset b;
#endif
{
  struct rexp_node * n;
  n = rexp_node (type);
  if (n)
    {
      n->params.cset = b;
      n->params.cset_size = size;
    }
  return n;
}


#ifdef __STDC__
struct rexp_node *
rx_mk_r_int (int type, int intval)
#else
struct rexp_node *
rx_mk_r_int (type, intval)
     int type;
     int intval;
#endif
{
  struct rexp_node * n;
  n = rexp_node (type);
  if (n)
    n->params.intval = intval;
  return n;
}


#ifdef __STDC__
struct rexp_node *
rx_mk_r_str (int type, char c)
#else
struct rexp_node *
rx_mk_r_str (type, c)
     int type;
     char c;
#endif
{
  struct rexp_node *n;
  n = rexp_node (type);
  if (n)
    rx_init_string (&(n->params.cstr), c);
  return n;
}


#ifdef __STDC__
struct rexp_node *
rx_mk_r_binop (int type, struct rexp_node * a, struct rexp_node * b)
#else
struct rexp_node *
rx_mk_r_binop (type, a, b)
     int type;
     struct rexp_node * a;
     struct rexp_node * b;
#endif
{
  struct rexp_node * n = rexp_node (type);
  if (n)
    {
      n->params.pair.left = a;
      n->params.pair.right = b;
    }
  return n;
}


#ifdef __STDC__
struct rexp_node *
rx_mk_r_monop (int type, struct rexp_node * a)
#else
struct rexp_node *
rx_mk_r_monop (type, a)
     int type;
     struct rexp_node * a;
#endif
{
  return rx_mk_r_binop (type, a, 0);
}


#ifdef __STDC__
void
rx_free_rexp (struct rexp_node * node)
#else
void
rx_free_rexp (node)
     struct rexp_node * node;
#endif
{
  if (node && !--node->refs)
    {
      if (node->params.cset)
	rx_free_cset (node->params.cset);
      if (node->params.cstr.reallen)
	rx_free_string (&(node->params.cstr));
      rx_free_rexp (node->params.pair.left);
      rx_free_rexp (node->params.pair.right);
      rx_free_rexp (node->simplified);
      free ((char *)node);
    }
}

#ifdef __STDC__
void
rx_save_rexp (struct rexp_node * node)
#else
void
rx_save_rexp (node)
     struct rexp_node * node;
#endif
{
  if (node)
    ++node->refs;
}


#ifdef __STDC__
struct rexp_node * 
rx_copy_rexp (int cset_size, struct rexp_node *node)
#else
struct rexp_node * 
rx_copy_rexp (cset_size, node)
     int cset_size;
     struct rexp_node *node;
#endif
{
  if (!node)
    return 0;
  else
    {
      struct rexp_node *n;
      n = rexp_node (node->type);
      if (!n)
	return 0;

      if (node->params.cset)
	{
	  n->params.cset = rx_copy_cset (cset_size,
					 node->params.cset);
	  if (!n->params.cset)
	    {
	      rx_free_rexp (n);
	      return 0;
	    }
	}

      if (node->params.cstr.reallen)
	if (rx_copy_string (&(n->params.cstr), &(node->params.cstr)))
	  {
	    rx_free_rexp(n);
	    return 0;
	  }

      n->params.intval = node->params.intval;
      n->params.intval2 = node->params.intval2;
      n->params.pair.left = rx_copy_rexp (cset_size, node->params.pair.left);
      n->params.pair.right = rx_copy_rexp (cset_size, node->params.pair.right);
      if (   (node->params.pair.left && !n->params.pair.left)
	  || (node->params.pair.right && !n->params.pair.right))
	{
	  rx_free_rexp  (n);
	  return 0;
	}
      n->id = node->id;
      n->len = node->len;
      n->observed = node->observed;
      return n;
    }
}



#ifdef __STDC__
struct rexp_node * 
rx_shallow_copy_rexp (int cset_size, struct rexp_node *node)
#else
struct rexp_node * 
rx_shallow_copy_rexp (cset_size, node)
     int cset_size;
     struct rexp_node *node;
#endif
{
  if (!node)
    return 0;
  else
    {
      struct rexp_node *n;
      n = rexp_node (node->type);
      if (!n)
	return 0;

      if (node->params.cset)
	{
	  n->params.cset = rx_copy_cset (cset_size,
					 node->params.cset);
	  if (!n->params.cset)
	    {
	      rx_free_rexp (n);
	      return 0;
	    }
	}

      if (node->params.cstr.reallen)
	if (rx_copy_string (&(n->params.cstr), &(node->params.cstr)))
	  {
	    rx_free_rexp(n);
	    return 0;
	  }

      n->params.intval = node->params.intval;
      n->params.intval2 = node->params.intval2;
      n->params.pair.left = node->params.pair.left;
      rx_save_rexp (node->params.pair.left);
      n->params.pair.right = node->params.pair.right;
      rx_save_rexp (node->params.pair.right);
      n->id = node->id;
      n->len = node->len;
      n->observed = node->observed;
      return n;
    }
}




#ifdef __STDC__
int
rx_rexp_equal (struct rexp_node * a, struct rexp_node * b)
#else
int
rx_rexp_equal (a, b)
     struct rexp_node * a;
     struct rexp_node * b;
#endif
{
  int ret;

  if (a == b)
    return 1;

  if ((a == 0) || (b == 0))
    return 0;

  if (a->type != b->type)
    return 0;

  switch (a->type)
    {
    case r_cset:
      ret = (   (a->params.cset_size == b->params.cset_size)
	     && rx_bitset_is_equal (a->params.cset_size,
				    a->params.cset,
				    b->params.cset));
      break;

    case r_string:
      ret = rx_compare_rx_strings (&(a->params.cstr), &(b->params.cstr));
      break;

    case r_cut:
      ret = (a->params.intval == b->params.intval);
      break;

    case r_concat:
    case r_alternate:
      ret = (   rx_rexp_equal (a->params.pair.left, b->params.pair.left)
	     && rx_rexp_equal (a->params.pair.right, b->params.pair.right));
      break;
    case r_opt:
    case r_star:
    case r_plus:
      ret = rx_rexp_equal (a->params.pair.left, b->params.pair.left);
      break;
    case r_interval:
      ret = (   (a->params.intval == b->params.intval)
	     && (a->params.intval2 == b->params.intval2)
	     && rx_rexp_equal (a->params.pair.left, b->params.pair.left));
      break;
    case r_parens:
      ret = (   (a->params.intval == b->params.intval)
	     && rx_rexp_equal (a->params.pair.left, b->params.pair.left));
      break;

    case r_context:
      ret = (a->params.intval == b->params.intval);
      break;
    default:
      return 0;
    }
  return ret;
}





#ifdef __STDC__
unsigned long
rx_rexp_hash (struct rexp_node * node, unsigned long seed)
#else
     unsigned long
     rx_rexp_hash (node, seed)
     struct rexp_node * node;
     unsigned long seed;
#endif
{
  if (!node)
    return seed;

  seed = rx_rexp_hash (node->params.pair.left, seed);
  seed = rx_rexp_hash (node->params.pair.right, seed);
  seed = rx_bitset_hash (node->params.cset_size, node->params.cset);
  seed = rx_string_hash (seed, &(node->params.cstr));
  seed += (seed << 3) + node->params.intval;
  seed += (seed << 3) + node->params.intval2;
  seed += (seed << 3) + node->type;
  seed += (seed << 3) + node->id;
#if 0
  seed += (seed << 3) + node->len;
  seed += (seed << 3) + node->observed;
#endif
  return seed;
}
