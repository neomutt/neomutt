/**
 * @file
 * ctype(3) wrapper functions
 *
 * @authors
 * Copyright (C) 2025 Thomas Klausne <wiz@gatalith.at>
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

#include "ctype.h"
#include <ctype.h>

int mutt_isalnum(int arg)
{
  if (isascii(arg))
    return isalnum((unsigned char) arg);

  return 0;
}

int mutt_isalpha(int arg)
{
  if (isascii(arg))
    return isalpha((unsigned char) arg);

  return 0;
}

int mutt_isdigit(int arg)
{
  if (isascii(arg))
    return isdigit((unsigned char) arg);

  return 0;
}

int mutt_ispunct(int arg)
{
  if (isascii(arg))
    return ispunct((unsigned char) arg);

  return 0;
}

int mutt_isspace(int arg)
{
  if (isascii(arg))
    return isspace((unsigned char) arg);

  return 0;
}

int mutt_isxdigit(int arg)
{
  if (isascii(arg))
    return isxdigit((unsigned char) arg);

  return 0;
}

int mutt_tolower(int arg)
{
  if (isascii(arg))
    return tolower((unsigned char) arg);

  return arg;
}

int mutt_toupper(int arg)
{
  if (isascii(arg))
    return toupper((unsigned char) arg);

  return arg;
}
