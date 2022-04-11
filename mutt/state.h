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

#ifndef MUTT_MUTT_STATE_H
#define MUTT_MUTT_STATE_H

#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

typedef uint16_t StateFlags;         ///< Flags for State->flags, e.g. #MUTT_DISPLAY
#define MUTT_STATE_NO_FLAGS       0  ///< No flags are set
#define MUTT_DISPLAY        (1 << 0) ///< Output is displayed to the user
#define MUTT_VERIFY         (1 << 1) ///< Perform signature verification
#define MUTT_PENDINGPREFIX  (1 << 2) ///< Prefix to write, but character must follow
#define MUTT_WEED           (1 << 3) ///< Weed headers even when not in display mode
#define MUTT_CHARCONV       (1 << 4) ///< Do character set conversions
#define MUTT_PRINTING       (1 << 5) ///< Are we printing? - MUTT_DISPLAY "light"
#define MUTT_REPLYING       (1 << 6) ///< Are we replying?
#define MUTT_FIRSTDONE      (1 << 7) ///< The first attachment has been done
#define MUTT_DISPLAY_ATTACH (1 << 8) ///< We are displaying an attachment

/**
 * struct State - Keep track when processing files
 */
struct State
{
  FILE      *fp_in;   ///< File to read from
  FILE      *fp_out;  ///< File to write to
  char      *prefix;  ///< String to add to the beginning of each output line
  StateFlags flags;   ///< Flags, e.g. #MUTT_DISPLAY
  int        wraplen; ///< Width to wrap lines to (when flags & #MUTT_DISPLAY)
};

#define state_set_prefix(state) ((state)->flags |= MUTT_PENDINGPREFIX)
#define state_reset_prefix(state) ((state)->flags &= ~MUTT_PENDINGPREFIX)
#define state_puts(STATE, STR) fputs(STR, (STATE)->fp_out)
#define state_putc(STATE, STR) fputc(STR, (STATE)->fp_out)

void state_attach_puts          (struct State *s, const char *t);
void state_mark_attach          (struct State *s);
void state_mark_protected_header(struct State *s);
void state_prefix_put           (struct State *s, const char *buf, size_t buflen);
void state_prefix_putc          (struct State *s, char c);
int  state_printf               (struct State *s, const char *fmt, ...);
int  state_putws                (struct State *s, const wchar_t *ws);

const char *state_attachment_marker(void);
const char *state_protected_header_marker(void);

#endif /* MUTT_MUTT_STATE_H */
