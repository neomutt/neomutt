/* classes: h_files */

#ifndef RXCSETH
#define RXCSETH

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

/*  lord	Sun May  7 12:34:11 1995	*/


#include "rxbitset.h"


#ifdef __STDC__
extern rx_Bitset rx_cset (int size);
extern rx_Bitset rx_copy_cset (int size, rx_Bitset a);
extern void rx_free_cset (rx_Bitset c);

#else /* STDC */
extern rx_Bitset rx_cset ();
extern rx_Bitset rx_copy_cset ();
extern void rx_free_cset ();

#endif /* STDC */



#endif  /* RXCSETH */
