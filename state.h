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

#ifndef _MUTT_STATE_H
#define _MUTT_STATE_H

#include <stdio.h>
#include <stddef.h>

/**
 * struct State - Keep track when processing files
 */
struct State
{
  FILE *fpin;
  FILE *fpout;
  char *prefix;
  int flags;
};

/* flags for the State struct */
#define MUTT_DISPLAY       (1 << 0) /**< output is displayed to the user */
#define MUTT_VERIFY        (1 << 1) /**< perform signature verification */
#define MUTT_PENDINGPREFIX (1 << 2) /**< prefix to write, but character must follow */
#define MUTT_WEED          (1 << 3) /**< weed headers even when not in display mode */
#define MUTT_CHARCONV      (1 << 4) /**< Do character set conversions */
#define MUTT_PRINTING      (1 << 5) /**< are we printing? - MUTT_DISPLAY "light" */
#define MUTT_REPLYING      (1 << 6) /**< are we replying? */
#define MUTT_FIRSTDONE     (1 << 7) /**< the first attachment has been done */

#define state_set_prefix(s) ((s)->flags |= MUTT_PENDINGPREFIX)
#define state_reset_prefix(s) ((s)->flags &= ~MUTT_PENDINGPREFIX)
#define state_puts(x, y) fputs(x, (y)->fpout)
#define state_putc(x, y) fputc(x, (y)->fpout)

void state_mark_attach(struct State *s);
void state_attach_puts(const char *t, struct State *s);
void state_prefix_putc(char c, struct State *s);
int state_printf(struct State *s, const char *fmt, ...);
int state_putws(const wchar_t *ws, struct State *s);
void state_prefix_put(const char *d, size_t dlen, struct State *s);

#endif /* _MUTT_STATE_H */
