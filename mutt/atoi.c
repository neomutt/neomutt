/**
 * @file
 * Parse a number in a string
 *
 * @authors
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
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
 * @page mutt_atoi Parse a number in a string
 *
 * Parse a number in a string.
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * str_atol_clamp - Convert ASCII string to a long and clamp
 * @param[in]  str String to read
 * @param[in]  lmin Lower bound
 * @param[in]  lmax Upper bound
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * This is a strtol() wrapper with range checking.
 * errno may be set on error, e.g. ERANGE
 */
static const char *str_atol_clamp(const char *str, long *dst, long lmin, long lmax)
{
  if (dst)
  {
    *dst = 0;
  }

  if (!str || (*str == '\0'))
  {
    return NULL;
  }

  char *e = NULL;
  errno = 0;
  long res = strtol(str, &e, 10);
  if ((e == str) || (((res == LONG_MIN) || (res == LONG_MAX)) && (errno == ERANGE)) ||
      (res < lmin) || (res > lmax))
  {
    return NULL;
  }

  if (dst)
  {
    *dst = res;
  }

  return e;
}

/**
 * str_atoull_clamp - Convert ASCII string to an unsigned long long and clamp
 * @param[in]  str String to read
 * @param[in]  ullmax Upper bound
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * This is a strtoull() wrapper with range checking.
 * errno may be set on error, e.g. ERANGE
 */
static const char *str_atoull_clamp(const char *str, unsigned long long *dst,
                                    unsigned long long ullmax)
{
  if (dst)
  {
    *dst = 0;
  }

  if (!str || (*str == '\0'))
  {
    return str;
  }

  char *e = NULL;
  errno = 0;
  unsigned long long res = strtoull(str, &e, 10);
  if ((e == str) || ((res == ULLONG_MAX) && (errno == ERANGE)) || (res > ullmax))
  {
    return NULL;
  }

  if (dst)
  {
    *dst = res;
  }

  return e;
}

/**
 * mutt_str_atol - Convert ASCII string to a long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * This is a strtol() wrapper with range checking.
 * errno may be set on error, e.g. ERANGE
 */
const char *mutt_str_atol(const char *str, long *dst)
{
  return str_atol_clamp(str, dst, LONG_MIN, LONG_MAX);
}

/**
 * mutt_str_atos - Convert ASCII string to a short
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Error, overflow
 *
 * This is a strtol() wrapper with range checking.
 * If @a dst is NULL, the string will be tested only (without conversion).
 *
 * errno may be set on error, e.g. ERANGE
 */
const char *mutt_str_atos(const char *str, short *dst)
{
  long l;
  const char *res = str_atol_clamp(str, &l, SHRT_MIN, SHRT_MAX);
  if (dst)
  {
    *dst = res ? l : 0;
  }
  return res;
}

/**
 * mutt_str_atoi - Convert ASCII string to an integer
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * This is a strtol() wrapper with range checking.
 * If @a dst is NULL, the string will be tested only (without conversion).
 * errno may be set on error, e.g. ERANGE
 */
const char *mutt_str_atoi(const char *str, int *dst)
{
  long l;
  const char *res = str_atol_clamp(str, &l, INT_MIN, INT_MAX);
  if (dst)
  {
    *dst = res ? l : 0;
  }
  return res;
}

/**
 * mutt_str_atoui - Convert ASCII string to an unsigned integer
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
const char *mutt_str_atoui(const char *str, unsigned int *dst)
{
  unsigned long long l;
  const char *res = str_atoull_clamp(str, &l, UINT_MAX);
  if (dst)
  {
    *dst = res ? l : 0;
  }
  return res;
}

/**
 * mutt_str_atoul - Convert ASCII string to an unsigned long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
const char *mutt_str_atoul(const char *str, unsigned long *dst)
{
  unsigned long long l;
  const char *res = str_atoull_clamp(str, &l, ULONG_MAX);
  if (dst)
  {
    *dst = res ? l : 0;
  }
  return res;
}

/**
 * mutt_str_atous - Convert ASCII string to an unsigned short
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
const char *mutt_str_atous(const char *str, unsigned short *dst)
{
  unsigned long long l;
  const char *res = str_atoull_clamp(str, &l, USHRT_MAX);
  if (dst)
  {
    *dst = res ? l : 0;
  }
  return res;
}

/**
 * mutt_str_atoull - Convert ASCII string to an unsigned long long
 * @param[in]  str String to read
 * @param[out] dst Store the result
 * @retval endptr
 *
 * endptr    == NULL -> no conversion happened, or overflow
 * endptr[0] == '\0' -> str was fully converted
 * endptr[0] != '\0' -> endptr points to first non converted char in str
 *
 * @note This function's return value differs from the other functions.
 *       They return -1 if there is input beyond the number.
 */
const char *mutt_str_atoull(const char *str, unsigned long long *dst)
{
  return str_atoull_clamp(str, dst, ULLONG_MAX);
}

