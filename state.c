/**
 * @file
 * Keep track when processing files
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page state Keep track when processing files
 *
 * Keep track when processing files
 */

#include "config.h"
#include <limits.h>
#include <stdarg.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "state.h"
#include "mutt_globals.h"

/**
 * state_mark_attach - Write a unique marker around content
 * @param s State to write to
 */
void state_mark_attach(struct State *s)
{
  if (!s || !s->fp_out)
    return;
  if ((s->flags & MUTT_DISPLAY) &&
      (!C_Pager || mutt_str_equal(C_Pager, "builtin")))
  {
    state_puts(s, AttachmentMarker);
  }
}

/**
 * state_mark_protected_header - Write a unique marker around protected headers
 * @param s State to write to
 */
void state_mark_protected_header(struct State *s)
{
  if ((s->flags & MUTT_DISPLAY) &&
      (!C_Pager || mutt_str_equal(C_Pager, "builtin")))
  {
    state_puts(s, ProtectedHeaderMarker);
  }
}

/**
 * state_attach_puts - Write a string to the state
 * @param s State to write to
 * @param t Text to write
 */
void state_attach_puts(struct State *s, const char *t)
{
  if (!s || !s->fp_out || !t)
    return;

  if (*t != '\n')
    state_mark_attach(s);
  while (*t)
  {
    state_putc(s, *t);
    if ((*t++ == '\n') && *t)
      if (*t != '\n')
        state_mark_attach(s);
  }
}

/**
 * state_putwc - Write a wide character to the state
 * @param s  State to write to
 * @param wc Wide character to write
 * @retval  0 Success
 * @retval -1 Error
 */
static int state_putwc(struct State *s, wchar_t wc)
{
  char mb[MB_LEN_MAX] = { 0 };
  int rc;

  rc = wcrtomb(mb, wc, NULL);
  if (rc < 0)
    return rc;
  if (fputs(mb, s->fp_out) == EOF)
    return -1;
  return 0;
}

/**
 * state_putws - Write a wide string to the state
 * @param s  State to write to
 * @param ws Wide string to write
 * @retval  0 Success
 * @retval -1 Error
 */
int state_putws(struct State *s, const wchar_t *ws)
{
  const wchar_t *p = ws;

  while (p && (*p != L'\0'))
  {
    if (state_putwc(s, *p) < 0)
      return -1;
    p++;
  }
  return 0;
}

/**
 * state_prefix_putc - Write a prefixed character to the state
 * @param s State to write to
 * @param c Character to write
 */
void state_prefix_putc(struct State *s, char c)
{
  if (s->flags & MUTT_PENDINGPREFIX)
  {
    state_reset_prefix(s);
    if (s->prefix)
      state_puts(s, s->prefix);
  }

  state_putc(s, c);

  if (c == '\n')
    state_set_prefix(s);
}

/**
 * state_printf - Write a formatted string to the State
 * @param s   State to write to
 * @param fmt printf format string
 * @param ... Arguments to formatting string
 * @retval num Number of characters written
 */
int state_printf(struct State *s, const char *fmt, ...)
{
  int rc;
  va_list ap;

  va_start(ap, fmt);
  rc = vfprintf(s->fp_out, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * state_prefix_put - Write a prefixed fixed-string to the State
 * @param s      State to write to
 * @param buf    String to write
 * @param buflen Length of string
 */
void state_prefix_put(struct State *s, const char *buf, size_t buflen)
{
  if (s->prefix)
  {
    while (buflen--)
      state_prefix_putc(s, *buf++);
  }
  else
    fwrite(buf, buflen, 1, s->fp_out);
}
