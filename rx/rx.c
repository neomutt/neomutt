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



#include "rx.h"
#include "rxall.h"
#include "rxhash.h"
#include "rxnfa.h"
#include "rxsuper.h"



const char rx_version_string[] = "GNU Rx version 1.5";


#ifdef __STDC__
struct rx *
rx_make_rx (int cset_size)
#else
struct rx *
rx_make_rx (cset_size)
     int cset_size;
#endif
{
  static int rx_id = 0;
  struct rx * new_rx;
  new_rx = (struct rx *)malloc (sizeof (*new_rx));
  rx_bzero ((char *)new_rx, sizeof (*new_rx));
  new_rx->rx_id = rx_id++;
  new_rx->cache = rx_default_cache;
  new_rx->local_cset_size = cset_size;
  new_rx->instruction_table = rx_id_instruction_table;
  new_rx->next_nfa_id = 0;
  return new_rx;
}

#ifdef __STDC__
void
rx_free_rx (struct rx * rx)
#else
void
rx_free_rx (rx)
     struct rx * rx;
#endif
{
  if (rx->start_set)
    rx->start_set->starts_for = 0;
  rx_free_nfa (rx);
  free (rx);
}


#ifdef __STDC__
void
rx_bzero (char * mem, int size)
#else
void
rx_bzero (mem, size)
     char * mem;
     int size;
#endif
{
  while (size)
    {
      *mem = 0;
      ++mem;
      --size;
    }
}
