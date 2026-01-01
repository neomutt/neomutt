/**
 * @file
 * ctype(3) wrapper functions
 *
 * @authors
 * Copyright (C) 2025 Thomas Klausner <wiz@gatalith.at>
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page mutt_ctype ctype(3) wrapper functions
 *
 * The arguments to ctype(3) functions need to be EOF or representable
 * as an unsigned char. These replacement functions avoid replicating
 * the checks for valid arguments.
 */

#include <ctype.h>
#include <stdbool.h>

/**
 * mutt_isalnum - Wrapper for isalnum(3)
 * @param arg Character to test
 * @retval true Character is alphanumeric
 */
bool mutt_isalnum(int arg)
{
  if (isascii(arg))
    return isalnum(arg);

  return false;
}

/**
 * mutt_isalpha - Wrapper for isalpha(3)
 * @param arg Character to test
 * @retval true Character is alphabetic
 */
bool mutt_isalpha(int arg)
{
  if (isascii(arg))
    return isalpha(arg);

  return false;
}

/**
 * mutt_isdigit - Wrapper for isdigit(3)
 * @param arg Character to test
 * @retval true Character is a digit (0 through 9)
 */
bool mutt_isdigit(int arg)
{
  if (isascii(arg))
    return isdigit(arg);

  return false;
}

/**
 * mutt_ispunct - Wrapper for ispunct(3)
 * @param arg Character to test
 * @retval true Character is printable but is not a space or alphanumeric
 */
bool mutt_ispunct(int arg)
{
  if (isascii(arg))
    return ispunct(arg);

  return false;
}

/**
 * mutt_isspace - Wrapper for isspace(3)
 * @param arg Character to test
 * @retval true Character is white-space
 *
 * In the "C" and "POSIX" locales, these are: space, form-feed ('\\f'),
 * newline ('\\n'), carriage return ('\\r'), horizontal tab ('\\t'),
 * and vertical tab ('\\v').
 */
bool mutt_isspace(int arg)
{
  if (isascii(arg))
    return isspace(arg);

  return false;
}

/**
 * mutt_isxdigit - Wrapper for isxdigit(3)
 * @param arg Character to test
 * @retval true Character is a hexadecimal digits
 *
 * That is, one of 0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F.
 */
bool mutt_isxdigit(int arg)
{
  if (isascii(arg))
    return isxdigit(arg);

  return false;
}

/**
 * mutt_tolower - Wrapper for tolower(3)
 * @param arg Character to lowercase
 * @retval num Success: Lower-case character
 * @retval arg Failure: Original character
 *
 */
int mutt_tolower(int arg)
{
  if (isascii(arg))
    return tolower(arg);

  return arg;
}

/**
 * mutt_toupper - Wrapper for toupper(3)
 * @param arg Character to uppercase
 * @retval num Success: Upper-case character
 * @retval arg Failure: Original character
 */
int mutt_toupper(int arg)
{
  if (isascii(arg))
    return toupper(arg);

  return arg;
}
