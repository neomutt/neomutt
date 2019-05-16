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
#include "mutt/mutt.h"
#include "state.h"
#include "globals.h"

/**
 * state_mark_attach - Write a unique marker around content
 * @param s State to write to
 */
void state_mark_attach(struct State *s)
{
  if (!s || !s->fp_out)
    return;
  if ((s->flags & MUTT_DISPLAY) && (mutt_str_strcmp(C_Pager, "builtin") == 0))
    state_puts(AttachmentMarker, s);
}

/**
 * state_mark_protected_header - Write a unique marker around protected headers
 * @param s State to write to
 */
void state_mark_protected_header(struct State *s)
{
  if ((s->flags & MUTT_DISPLAY) && (mutt_str_strcmp(C_Pager, "builtin") == 0))
    state_puts(ProtectedHeaderMarker, s);
}

/**
 * state_attach_puts - Write a string to the state
 * @param t Text to write
 * @param s State to write to
 */
void state_attach_puts(const char *t, struct State *s)
{
  if (!t || !s || !s->fp_out)
    return;

  if (*t != '\n')
    state_mark_attach(s);
  while (*t)
  {
    state_putc(*t, s);
    if ((*t++ == '\n') && *t)
      if (*t != '\n')
        state_mark_attach(s);
  }
}

/**
 * state_putwc - Write a wide character to the state
 * @param wc Wide character to write
 * @param s  State to write to
 * @retval  0 Success
 * @retval -1 Error
 */
static int state_putwc(wchar_t wc, struct State *s)
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
 * @param ws Wide string to write
 * @param s  State to write to
 * @retval  0 Success
 * @retval -1 Error
 */
int state_putws(const wchar_t *ws, struct State *s)
{
  const wchar_t *p = ws;

  while (p && (*p != L'\0'))
  {
    if (state_putwc(*p, s) < 0)
      return -1;
    p++;
  }
  return 0;
}

/**
 * state_prefix_putc - Write a prefixed character to the state
 * @param c Character to write
 * @param s State to write to
 */
void state_prefix_putc(char c, struct State *s)
{
  if (s->flags & MUTT_PENDINGPREFIX)
  {
    state_reset_prefix(s);
    if (s->prefix)
      state_puts(s->prefix, s);
  }

  state_putc(c, s);

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
 * @param buf    String to write
 * @param buflen Length of string
 * @param s    State to write to
 */
void state_prefix_put(const char *buf, size_t buflen, struct State *s)
{
  if (s->prefix)
  {
    while (buflen--)
      state_prefix_putc(*buf++, s);
  }
  else
    fwrite(buf, buflen, 1, s->fp_out);
}
