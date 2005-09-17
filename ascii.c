/*
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA.
 * 
 */


/* 
 * Versions of the string comparison functions which are
 * locale-insensitive.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "ascii.h"

int ascii_isupper (int c)
{
  return (c >= 'A') && (c <= 'Z');
}

int ascii_islower (int c)
{
  return (c >= 'a') && (c <= 'z');
}

int ascii_toupper (int c)
{
  if (ascii_islower (c))
    return c & ~32;
  
  return c;
}

int ascii_tolower (int c)
{
  if (ascii_isupper (c))
    return c | 32;
  
  return c;
}

int ascii_strcasecmp (const char *a, const char *b)
{
  int i;
  
  if (a == b)
    return 0;
  if (a == NULL && b)
    return -1;
  if (b == NULL && a)
    return 1;
  
  for (; *a || *b; a++, b++)
  {
    if ((i = ascii_tolower (*a) - ascii_tolower (*b)))
      return i;
  }
  
  return 0;
}

int ascii_strncasecmp (const char *a, const char *b, int n)
{
  int i, j;
  
  if (a == b)
    return 0;
  if (a == NULL && b)
    return -1;
  if (b == NULL && a)
    return 1;
  
  for (j = 0; (*a || *b) && j < n; a++, b++, j++)
  {
    if ((i = ascii_tolower (*a) - ascii_tolower (*b)))
      return i;
  }
  
  return 0;
}
