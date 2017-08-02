/**
 * @file
 * ASCII string comparison routines
 *
 * @authors
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page ascii ASCII string comparison routines
 *
 * Simple ASCII string comparisons.
 *
 * | Function            | Description
 * | :------------------ | :---------------------------------
 * | ascii_strcasecmp()  | Compare strings, ignoring the case
 * | ascii_strlower()    | Lowercase a string
 * | ascii_strncasecmp() | Compare strings, ignoring the case
 */

/*
 * Versions of the string comparison functions which are
 * locale-insensitive.
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include "lib_ascii.h"

/**
 * ascii_strcasecmp - Compare strings, ignoring the case
 * @param a First string to compare
 * @param b Second string to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int ascii_strcasecmp(const char *a, const char *b)
{
  if (a == b)
    return 0;
  if (a == NULL && b)
    return -1;
  if (b == NULL && a)
    return 1;

  for (;; a++, b++)
  {
    int i = tolower(*a) - tolower(*b);
    if (i)
      return i;
    /* test for NUL here rather than in the for loop in order to detect unequal
     * length strings */
    if (!*a)
      break;
  }

  return 0;
}

/**
 * ascii_strncasecmp - Compare strings, ignoring the case
 * @param a First string to compare
 * @param b Second string to compare
 * @param n Maximum number of characters to compare
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int ascii_strncasecmp(const char *a, const char *b, int n)
{
  if (a == b)
    return 0;
  if (a == NULL && b)
    return -1;
  if (b == NULL && a)
    return 1;

  for (int j = 0; (*a || *b) && j < n; a++, b++, j++)
  {
    int i = tolower(*a) - tolower(*b);
    if (i)
      return i;
  }

  return 0;
}

/**
 * ascii_strlower - Lowercase a string
 * @param s String to transform
 * @retval ptr Transformed string
 *
 * The string is transformed in place.
 */
char *ascii_strlower(char *s)
{
  char *p = s;

  for (; *p; ++p)
    *p = tolower(*p);

  return s;
}
