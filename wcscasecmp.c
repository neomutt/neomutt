/**
 * @file
 * For systems lacking wcscasecmp()
 *
 * @authors
 * Copyright (C) 2009 Rocco Rutte <pdmef@gmx.net>
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
 * @page wcscasecmp For systems lacking wcscasecmp()
 *
 * For systems lacking wcscasecmp()
 */

#include "config.h"
#include <wchar.h>
#include <wctype.h>

/**
 * wcscasecmp - Compare two wide-character strings, ignoring case
 * @param a First string
 * @param b Second string
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int wcscasecmp(const wchar_t *a, const wchar_t *b)
{
  if (!a && !b)
    return 0;
  if (!a && b)
    return -1;
  if (a && !b)
    return 1;

  for (; *a || *b; a++, b++)
  {
    int i = towlower(*a);
    if ((i - towlower(*b)) != 0)
      return i;
  }
  return 0;
}
