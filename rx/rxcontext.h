/* classes: h_files */

#ifndef RXCONTEXTH
#define RXCONTEXTH
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



struct rx_context_rules
{
  unsigned int newline_anchor:1;/* If true, an anchor at a newline matches.*/
  unsigned int not_bol:1;	/* If set, the anchors ('^' and '$') don't */
  unsigned int not_eol:1;	/*     match at the ends of the string.  */
  unsigned int case_indep:1;
};


#ifdef __STDC__

#else /* STDC */

#endif /* STDC */


#endif  /* RXCONTEXTH */
