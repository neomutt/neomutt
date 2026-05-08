/**
 * @file
 * Keep track when processing files
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_STATE_H
#define MUTT_MUTT_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * enum StateFlag - Flags for State->flags
 */
enum StateFlag
{
  STATE_NONE           =       0,  ///< No flags are set
  STATE_DISPLAY        = 1U << 0,  ///< Output is displayed to the user
  STATE_VERIFY         = 1U << 1,  ///< Perform signature verification
  STATE_PENDINGPREFIX  = 1U << 2,  ///< Prefix to write, but character must follow
  STATE_WEED           = 1U << 3,  ///< Weed headers even when not in display mode
  STATE_CHARCONV       = 1U << 4,  ///< Do character set conversions
  STATE_PRINTING       = 1U << 5,  ///< Are we printing? - STATE_DISPLAY "light"
  STATE_REPLYING       = 1U << 6,  ///< Are we replying?
  STATE_FIRSTDONE      = 1U << 7,  ///< The first attachment has been done
  STATE_DISPLAY_ATTACH = 1U << 8,  ///< We are displaying an attachment
  STATE_PAGER          = 1U << 9,  ///< Output will be displayed in the Pager
};
typedef uint16_t StateFlags;

/**
 * struct State - Keep track when processing files
 */
struct State
{
  FILE       *fp_in;   ///< File to read from
  FILE       *fp_out;  ///< File to write to
  const char *prefix;  ///< String to add to the beginning of each output line
  StateFlags  flags;   ///< Flags, e.g. #STATE_DISPLAY
  int         wraplen; ///< Width to wrap lines to (when flags & #STATE_DISPLAY)
};

#define state_set_prefix(state)   ((state)->flags |= STATE_PENDINGPREFIX)
#define state_reset_prefix(state) ((state)->flags &= ~STATE_PENDINGPREFIX)
#define state_puts(STATE, STR) fputs(STR, (STATE)->fp_out)
#define state_putc(STATE, STR) fputc(STR, (STATE)->fp_out)

void state_attach_puts          (struct State *state, const char *t);
void state_mark_attach          (struct State *state);
void state_mark_protected_header(struct State *state);
void state_prefix_put           (struct State *state, const char *buf, size_t buflen);
void state_prefix_putc          (struct State *state, char c);
int  state_printf               (struct State *state, const char *fmt, ...)
                                  __attribute__((__format__(__printf__, 2, 3)));
int  state_putws                (struct State *state, const wchar_t *ws);

const char *state_attachment_marker(void);
const char *state_protected_header_marker(void);

#endif /* MUTT_MUTT_STATE_H */
