/* classes: h_files */

#ifndef RXBITSETH
#define RXBITSETH

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




typedef unsigned long RX_subset;
#define RX_subset_bits	(8 * sizeof (RX_subset))
#define RX_subset_mask	(RX_subset_bits - 1)
typedef RX_subset * rx_Bitset;

#ifdef __STDC__
typedef void (*rx_bitset_iterator) (rx_Bitset, int member_index);
#else
typedef void (*rx_bitset_iterator) ();
#endif

/* Return the index of the word containing the Nth bit.
 */
#define rx_bitset_subset(N)  ((N) / RX_subset_bits)

/* Return the conveniently-sized supset containing the Nth bit.
 */
#define rx_bitset_subset_val(B,N)  ((B)[rx_bitset_subset(N)])


/* Genericly combine the word containing the Nth bit with a 1 bit mask
 * of the Nth bit position within that word.
 */
#define RX_bitset_access(B,N,OP) \
  ((B)[rx_bitset_subset(N)] OP rx_subset_singletons[(N) & RX_subset_mask])

#define RX_bitset_member(B,N)   RX_bitset_access(B, N, &)
#define RX_bitset_enjoin(B,N)   RX_bitset_access(B, N, |=)
#define RX_bitset_remove(B,N)   RX_bitset_access(B, N, &= ~)
#define RX_bitset_toggle(B,N)   RX_bitset_access(B, N, ^= )

/* How many words are needed for N bits?
 */
#define rx_bitset_numb_subsets(N) (((N) + RX_subset_bits - 1) / RX_subset_bits)

/* How much memory should be allocated for a bitset with N bits?
 */
#define rx_sizeof_bitset(N)	(rx_bitset_numb_subsets(N) * sizeof(RX_subset))


extern RX_subset rx_subset_singletons[];



#ifdef __STDC__
extern int rx_bitset_is_equal (int size, rx_Bitset a, rx_Bitset b);
extern int rx_bitset_is_subset (int size, rx_Bitset a, rx_Bitset b);
extern int rx_bitset_empty (int size, rx_Bitset set);
extern void rx_bitset_null (int size, rx_Bitset b);
extern void rx_bitset_universe (int size, rx_Bitset b);
extern void rx_bitset_complement (int size, rx_Bitset b);
extern void rx_bitset_assign (int size, rx_Bitset a, rx_Bitset b);
extern void rx_bitset_union (int size, rx_Bitset a, rx_Bitset b);
extern void rx_bitset_intersection (int size,
				    rx_Bitset a, rx_Bitset b);
extern void rx_bitset_difference (int size, rx_Bitset a, rx_Bitset b);
extern void rx_bitset_revdifference (int size,
				     rx_Bitset a, rx_Bitset b);
extern void rx_bitset_xor (int size, rx_Bitset a, rx_Bitset b);
extern unsigned long rx_bitset_hash (int size, rx_Bitset b);
extern int rx_bitset_population (int size, rx_Bitset a);

#else /* STDC */
extern int rx_bitset_is_equal ();
extern int rx_bitset_is_subset ();
extern int rx_bitset_empty ();
extern void rx_bitset_null ();
extern void rx_bitset_universe ();
extern void rx_bitset_complement ();
extern void rx_bitset_assign ();
extern void rx_bitset_union ();
extern void rx_bitset_intersection ();
extern void rx_bitset_difference ();
extern void rx_bitset_revdifference ();
extern void rx_bitset_xor ();
extern unsigned long rx_bitset_hash ();
extern int rx_bitset_population ();

#endif /* STDC */



#endif  /* RXBITSETH */
