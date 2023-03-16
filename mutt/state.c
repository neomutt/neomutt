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
 * @page mutt_state Keep track when processing files
 *
 * Keep track when processing files
 */

#include "config.h"
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>
#include "config/lib.h"
#include "core/lib.h"
#include "state.h"
#include "date.h"
#include "random.h"

/**
 * state_attachment_marker - Get a unique (per-run) ANSI string to mark PGP messages in an email
 * @retval ptr Marker
 */
const char *state_attachment_marker(void)
{
  static char marker[256] = { 0 };
  if (!marker[0])
  {
    snprintf(marker, sizeof(marker), "\033]9;%" PRIu64 "\a", mutt_rand64());
  }
  return marker;
}

/**
 * state_protected_header_marker - Get a unique (per-run) ANSI string to mark protected headers in an email
 * @retval ptr Marker
 */
const char *state_protected_header_marker(void)
{
  static char marker[256] = { 0 };
  if (!marker[0])
  {
    snprintf(marker, sizeof(marker), "\033]8;%lld\a", (long long) mutt_date_now());
  }
  return marker;
}

/**
 * state_mark_attach - Write a unique marker around content
 * @param state State to write to
 */
void state_mark_attach(struct State *state)
{
  if (!state || !state->fp_out)
    return;
  const char *const c_pager = cs_subset_string(NeoMutt->sub, "pager");
  if ((state->flags & MUTT_DISPLAY) && (!c_pager || mutt_str_equal(c_pager, "builtin")))
  {
    state_puts(state, state_attachment_marker());
  }
}

/**
 * state_mark_protected_header - Write a unique marker around protected headers
 * @param state State to write to
 */
void state_mark_protected_header(struct State *state)
{
  const char *const c_pager = cs_subset_string(NeoMutt->sub, "pager");
  if ((state->flags & MUTT_DISPLAY) && (!c_pager || mutt_str_equal(c_pager, "builtin")))
  {
    state_puts(state, state_protected_header_marker());
  }
}

/**
 * state_attach_puts - Write a string to the state
 * @param state State to write to
 * @param t     Text to write
 */
void state_attach_puts(struct State *state, const char *t)
{
  if (!state || !state->fp_out || !t)
    return;

  if (*t != '\n')
    state_mark_attach(state);
  while (*t)
  {
    state_putc(state, *t);
    if ((*t++ == '\n') && *t)
      if (*t != '\n')
        state_mark_attach(state);
  }
}

/**
 * state_putwc - Write a wide character to the state
 * @param state State to write to
 * @param wc    Wide character to write
 * @retval  0 Success
 * @retval -1 Error
 */
static int state_putwc(struct State *state, wchar_t wc)
{
  char mb[MB_LEN_MAX] = { 0 };
  int rc;

  rc = wcrtomb(mb, wc, NULL);
  if (rc < 0)
    return rc;
  if (fputs(mb, state->fp_out) == EOF)
    return -1;
  return 0;
}

/**
 * state_putws - Write a wide string to the state
 * @param state State to write to
 * @param ws    Wide string to write
 * @retval  0 Success
 * @retval -1 Error
 */
int state_putws(struct State *state, const wchar_t *ws)
{
  const wchar_t *p = ws;

  while (p && (*p != L'\0'))
  {
    if (state_putwc(state, *p) < 0)
      return -1;
    p++;
  }
  return 0;
}

/**
 * state_prefix_putc - Write a prefixed character to the state
 * @param state State to write to
 * @param c     Character to write
 */
void state_prefix_putc(struct State *state, char c)
{
  if (state->flags & MUTT_PENDINGPREFIX)
  {
    state_reset_prefix(state);
    if (state->prefix)
      state_puts(state, state->prefix);
  }

  state_putc(state, c);

  if (c == '\n')
    state_set_prefix(state);
}

/**
 * state_printf - Write a formatted string to the State
 * @param state State to write to
 * @param fmt   printf format string
 * @param ...   Arguments to formatting string
 * @retval num Number of characters written
 */
int state_printf(struct State *state, const char *fmt, ...)
{
  int rc;
  va_list ap;

  va_start(ap, fmt);
  rc = vfprintf(state->fp_out, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * state_prefix_put - Write a prefixed fixed-string to the State
 * @param state  State to write to
 * @param buf    String to write
 * @param buflen Length of string
 */
void state_prefix_put(struct State *state, const char *buf, size_t buflen)
{
  if (state->prefix)
  {
    while (buflen--)
      state_prefix_putc(state, *buf++);
  }
  else
    fwrite(buf, buflen, 1, state->fp_out);
}
