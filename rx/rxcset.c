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



#include "rxall.h"
#include "rxcset.h"
/* Utilities for manipulating bitset represntations of characters sets. */

#ifdef __STDC__
rx_Bitset
rx_cset (int size)
#else
rx_Bitset
rx_cset (size)
     int size;
#endif
{
  rx_Bitset b;
  b = (rx_Bitset) malloc (rx_sizeof_bitset (size));
  if (b)
    rx_bitset_null (size, b);
  return b;
}


#ifdef __STDC__
rx_Bitset
rx_copy_cset (int size, rx_Bitset a)
#else
rx_Bitset
rx_copy_cset (size, a)
     int size;
     rx_Bitset a;
#endif
{
  rx_Bitset cs;
  cs = rx_cset (size);

  if (cs)
    rx_bitset_union (size, cs, a);

  return cs;
}


#ifdef __STDC__
void
rx_free_cset (rx_Bitset c)
#else
void
rx_free_cset (c)
     rx_Bitset c;
#endif
{
  if (c)
    free ((char *)c);
}

