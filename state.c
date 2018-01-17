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

#include "config.h"
#include <limits.h>
#include <stdarg.h>
#include <wchar.h>
#include "mutt/mutt.h"
#include "state.h"
#include "globals.h"

void state_mark_attach(struct State *s)
{
  if (!s || !s->fpout)
    return;
  if ((s->flags & MUTT_DISPLAY) && (mutt_str_strcmp(Pager, "builtin") == 0))
    state_puts(AttachmentMarker, s);
}

void state_attach_puts(const char *t, struct State *s)
{
  if (!t || !s || !s->fpout)
    return;

  if (*t != '\n')
    state_mark_attach(s);
  while (*t)
  {
    state_putc(*t, s);
    if (*t++ == '\n' && *t)
      if (*t != '\n')
        state_mark_attach(s);
  }
}

static int state_putwc(wchar_t wc, struct State *s)
{
  char mb[MB_LEN_MAX] = "";
  int rc;

  rc = wcrtomb(mb, wc, NULL);
  if (rc < 0)
    return rc;
  if (fputs(mb, s->fpout) == EOF)
    return -1;
  return 0;
}

int state_putws(const wchar_t *ws, struct State *s)
{
  const wchar_t *p = ws;

  while (p && *p != L'\0')
  {
    if (state_putwc(*p, s) < 0)
      return -1;
    p++;
  }
  return 0;
}

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

int state_printf(struct State *s, const char *fmt, ...)
{
  int rc;
  va_list ap;

  va_start(ap, fmt);
  rc = vfprintf(s->fpout, fmt, ap);
  va_end(ap);

  return rc;
}

void state_prefix_put(const char *d, size_t dlen, struct State *s)
{
  if (s->prefix)
    while (dlen--)
      state_prefix_putc(*d++, s);
  else
    fwrite(d, dlen, 1, s->fpout);
}
