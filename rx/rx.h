/* classes: h_files */

#ifndef RXH
#define RXH
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



#include "rxhash.h"




extern const char rx_version_string[];


#ifdef __STDC__
extern struct rx * rx_make_rx (int cset_size);
extern void rx_free_rx (struct rx * rx);
extern void rx_bzero (char * mem, int size);

#else /* STDC */
extern struct rx * rx_make_rx ();
extern void rx_free_rx ();
extern void rx_bzero ();

#endif /* STDC */





#endif  /* RXH */
