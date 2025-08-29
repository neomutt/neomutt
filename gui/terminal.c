/**
 * @file
 * Set the terminal title/icon
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
 * @page gui_terminal Set the terminal title/icon
 *
 * Set the terminal title/icon
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "terminal.h"
#include "mutt_curses.h"
#ifdef HAVE_NCURSESW_NCURSES_H
#include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/term.h>
#endif

bool TsSupported; ///< Terminal Setting is supported

/* de facto standard escapes for tsl/fsl */
/// TSL: to_status_line - Sent before the terminal title
static const char *TSL = "\033]0;"; // Escape
/// FSL: from_status_line - Sent after the terminal title
static const char *FSL = "\007"; // Ctrl-G (BEL)

/**
 * mutt_tigetstr - Get terminal capabilities
 * @param name Name of capability
 * @retval str Capability
 *
 * @note Do not free the returned string
 */
const char *mutt_tigetstr(const char *name)
{
  char *cap = tigetstr(name);
  if (!cap || (cap == (char *) -1) || (*cap == '\0'))
    return NULL;

  return cap;
}

/**
 * mutt_ts_capability - Check terminal capabilities
 * @retval true Terminal is capable of having its title/icon set
 *
 * @note This must happen after the terminfo has been initialised.
 */
bool mutt_ts_capability(void)
{
  static const char *known[] = {
    "color-xterm", "cygwin", "eterm",  "kterm", "nxterm",
    "putty",       "rxvt",   "screen", "xterm", NULL,
  };

#ifdef HAVE_USE_EXTENDED_NAMES
  /* If tsl is set, then terminfo says that status lines work. */
  const char *tcaps = mutt_tigetstr("tsl");
  if (tcaps)
  {
    /* update the static definitions of tsl/fsl from terminfo */
    TSL = tcaps;

    tcaps = mutt_tigetstr("fsl");
    if (tcaps)
      FSL = tcaps;

    return true;
  }

  /* If XT (boolean) is set, then this terminal supports the standard escape. */
  /* Beware: tigetflag returns -1 if XT is invalid or not a boolean. */
  int tcapi = tigetflag("XT");
  if (tcapi == 1)
    return true;
#endif

  /* Check term types that are known to support the standard escape without
   * necessarily asserting it in terminfo. */
  const char *term = mutt_str_getenv("TERM");
  for (const char **termp = known; *termp; termp++)
  {
    if (term && !mutt_istr_startswith(term, *termp))
      return true;
  }

  return false;
}

/**
 * mutt_ts_status - Set the text of the terminal title bar
 * @param str Text to set
 *
 * @note To clear the text, set the title to a single space.
 */
void mutt_ts_status(char *str)
{
  if (!str || (*str == '\0'))
    return;

  fprintf(stderr, "%s%s%s", TSL, str, FSL);
}

/**
 * mutt_ts_icon - Set the icon in the terminal title bar
 * @param str Icon string
 *
 * @note To clear the icon, set it to a single space.
 */
void mutt_ts_icon(char *str)
{
  if (!str || (*str == '\0'))
    return;

  /* icon setting is not supported in terminfo, so hardcode the escape */
  fprintf(stderr, "\033]1;%s\007", str); // Escape
}
