/*
 * Copyright (C) 2010 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#include <stdio.h>
#include "lib.h"

/* NOTE: Currently there is no check in configure.ac for vasprintf(3).  the
 * undefined behavior of the error condition makes it difficult to write a safe
 * version using it.
 */

#ifdef HAVE_VASPRINTF
int safe_asprintf (char **strp, const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start (ap, fmt);
  n = vasprintf (strp, fmt, ap);
  va_end (ap);

  /* GNU libc man page for vasprintf(3) states that the value of *strp
   * is undefined when the return code is -1.
   */
  if (n < 0)
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }

  if (n == 0)
  {
    /* Mutt convention is to use NULL for 0-length strings */
    FREE (strp); /* __FREE_CHECKED__ */
  }

  return n;
}
#else
/* Allocate a C-string large enough to contain the formatted string.
 * This is essentially malloc+sprintf in one.
 */
int safe_asprintf (char **strp, const char *fmt, ...)
{
  int rlen = STRING;
  int n;

  *strp = safe_malloc (rlen);
  for (;;)
  {
    va_list ap;
    va_start (ap, fmt);
    n = vsnprintf (*strp, rlen, fmt, ap);
    va_end (ap);
    if (n < 0)
    {
      FREE (strp); /* __FREE_CHECKED__ */
      return n;
    }

    if (n < rlen)
    {
      /* reduce space to just that which was used.  note that 'n' does not
       * include the terminal nul char.
       */
      if (n == 0) /* convention is to use NULL for zero-length strings. */
	FREE (strp); /* __FREE_CHECKED__ */
      else if (n != rlen - 1)
	safe_realloc (strp, n + 1);
      return n;
    }
    /* increase size and try again */
    rlen = n + 1;
    safe_realloc (strp, rlen);
  }
  /* not reached */
}
#endif /* HAVE_ASPRINTF */

