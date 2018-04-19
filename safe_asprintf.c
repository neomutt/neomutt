/**
 * @file
 * Wrapper for vasprintf()/vsnprintf()
 *
 * @authors
 * Copyright (C) 2010 Michael R. Elkins <me@mutt.org>
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
 * @page safe_asprintf Wrapper for vasprintf()
 *
 * Wrapper for vasprintf()
 *
 * @note Currently there is no check in configure for vasprintf(3).  The
 * undefined behavior of the error condition makes it difficult to write a safe
 * version using it.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/mutt.h"

#ifdef HAVE_VASPRINTF
/**
 * safe_asprintf - Format a string, allocating space as necessary
 * @param strp New string saved here
 * @param fmt  Format string
 * @param ...  Format arguments
 * @retval num Characters written
 * @retval -1  Error
 */
int safe_asprintf(char **strp, const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vasprintf(strp, fmt, ap);
  va_end(ap);

  /* GNU libc man page for vasprintf(3) states that the value of *strp
   * is undefined when the return code is -1.
   */
  if (n < 0)
  {
    mutt_error(_("Out of memory!"));
    mutt_exit(1);
  }

  if (n == 0)
  {
    /* NeoMutt convention is to use NULL for 0-length strings */
    FREE(strp);
  }

  return n;
}
#else
/* Allocate a C-string large enough to contain the formatted string.
 * This is essentially malloc+sprintf in one.
 */
int safe_asprintf(char **strp, const char *fmt, ...)
{
  int rlen = STRING;

  *strp = mutt_mem_malloc(rlen);
  while (true)
  {
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(*strp, rlen, fmt, ap);
    va_end(ap);
    if (n < 0)
    {
      FREE(strp);
      return n;
    }

    if (n < rlen)
    {
      /* reduce space to just that which was used.  note that 'n' does not
       * include the terminal nul char.
       */
      if (n == 0) /* convention is to use NULL for zero-length strings. */
        FREE(strp);
      else if (n != rlen - 1)
        mutt_mem_realloc(strp, n + 1);
      return n;
    }
    /* increase size and try again */
    rlen = n + 1;
    mutt_mem_realloc(strp, rlen);
  }
  /* not reached */
}
#endif /* HAVE_ASPRINTF */
