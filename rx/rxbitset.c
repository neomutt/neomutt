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
#include "rxbitset.h"


#ifdef __STDC__
int
rx_bitset_is_equal (int size, rx_Bitset a, rx_Bitset b)
#else
int
rx_bitset_is_equal (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  RX_subset s;

  if (size == 0)
    return 1;

  s = b[0];
  b[0] = ~a[0];

  for (x = rx_bitset_numb_subsets(size) - 1; a[x] == b[x]; --x)
    ;

  b[0] = s;
  return !x && (s == a[0]);
}


#ifdef __STDC__
int
rx_bitset_is_subset (int size, rx_Bitset a, rx_Bitset b)
#else
int
rx_bitset_is_subset (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  x = rx_bitset_numb_subsets(size) - 1;
  while (x-- && ((a[x] & b[x]) == a[x]));
  return x == -1;
}


#ifdef __STDC__
int
rx_bitset_empty (int size, rx_Bitset set)
#else
int
rx_bitset_empty (size, set)
     int size;
     rx_Bitset set;
#endif
{
  int x;
  RX_subset s;
  s = set[0];
  set[0] = 1;
  for (x = rx_bitset_numb_subsets(size) - 1; !set[x]; --x)
    ;
  set[0] = s;
  return !s;
}

#ifdef __STDC__
void
rx_bitset_null (int size, rx_Bitset b)
#else
void
rx_bitset_null (size, b)
     int size;
     rx_Bitset b;
#endif
{
  rx_bzero ((char *)b, rx_sizeof_bitset(size));
}


#ifdef __STDC__
void
rx_bitset_universe (int size, rx_Bitset b)
#else
void
rx_bitset_universe (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x = rx_bitset_numb_subsets (size);
  while (x--)
    *b++ = ~(RX_subset)0;
}


#ifdef __STDC__
void
rx_bitset_complement (int size, rx_Bitset b)
#else
void
rx_bitset_complement (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x = rx_bitset_numb_subsets (size);
  while (x--)
    {
      *b = ~*b;
      ++b;
    }
}


#ifdef __STDC__
void
rx_bitset_assign (int size, rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_assign (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] = b[x];
}


#ifdef __STDC__
void
rx_bitset_union (int size, rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_union (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] |= b[x];
}


#ifdef __STDC__
void
rx_bitset_intersection (int size,
			rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_intersection (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] &= b[x];
}


#ifdef __STDC__
void
rx_bitset_difference (int size, rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_difference (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] &=  ~ b[x];
}


#ifdef __STDC__
void
rx_bitset_revdifference (int size,
			 rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_revdifference (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] = ~a[x] & b[x];
}

#ifdef __STDC__
void
rx_bitset_xor (int size, rx_Bitset a, rx_Bitset b)
#else
void
rx_bitset_xor (size, a, b)
     int size;
     rx_Bitset a;
     rx_Bitset b;
#endif
{
  int x;
  for (x = rx_bitset_numb_subsets(size) - 1; x >=0; --x)
    a[x] ^= b[x];
}


#ifdef __STDC__
unsigned long
rx_bitset_hash (int size, rx_Bitset b)
#else
unsigned long
rx_bitset_hash (size, b)
     int size;
     rx_Bitset b;
#endif
{
  int x;
  unsigned long answer;

  answer = 0;

  for (x = 0; x < size; ++x)
    {
      if (RX_bitset_member (b, x))
	answer += (answer << 3) + x;
    }
  return answer;
}


RX_subset rx_subset_singletons [RX_subset_bits] = 
{
  0x1,
  0x2,
  0x4,
  0x8,
  0x10,
  0x20,
  0x40,
  0x80,
  0x100,
  0x200,
  0x400,
  0x800,
  0x1000,
  0x2000,
  0x4000,
  0x8000,
  0x10000,
  0x20000,
  0x40000,
  0x80000,
  0x100000,
  0x200000,
  0x400000,
  0x800000,
  0x1000000,
  0x2000000,
  0x4000000,
  0x8000000,
  0x10000000,
  0x20000000,
  0x40000000,
  0x80000000
};


/* 
 * (define l (let loop ((x 0) (l '())) (if (eq? x 256) l (loop (+ x 1) (cons x l)))))
 * (define lb (map (lambda (n) (number->string n 2)) l))
 * (define lc (map string->list lb))
 * (define ln (map (lambda (l) (map (lambda (c) (if (eq? c #\1) 1 0)) l)) lc))
 * (define lt (map (lambda (l) (apply + l)) ln))
 */

static int char_pops[256] = 
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

#define RX_char_population(C) (char_pops[C])

#ifdef __STDC__
int
rx_bitset_population (int size, rx_Bitset a)
#else
int
rx_bitset_population (size, a)
     int size;
     rx_Bitset a;
#endif
{
  int x;
  int total;
  unsigned char s;

  if (size == 0)
    return 0;

  total = 0;
  x = sizeof (RX_subset) * rx_bitset_numb_subsets(size) - 1;
  while (x >= 0)
    {
      s = ((unsigned char *)a)[x];
      --x;
      total = total + RX_char_population (s);
    }
  return total;
}     
