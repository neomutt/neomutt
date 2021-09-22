/**
 * @file
 * Set the terminal title/icon
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include <ncursesw/term.h> // IWYU pragma: keep
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/term.h> // IWYU pragma: keep
#endif

bool TsSupported; ///< Terminal Setting is supported

/* de facto standard escapes for tsl/fsl */
static const char *tsl = "\033]0;"; // Escape
static const char *fsl = "\007";    // Ctrl-G (BEL)

/**
 * mutt_ts_capability - Check terminal capabilities
 * @retval true Terminal is capable of having its title/icon set
 *
 * @note This must happen after the terminfo has been initialised.
 */
bool mutt_ts_capability(void)
{
  const char *known[] = {
    "color-xterm", "cygwin", "eterm",  "kterm", "nxterm",
    "putty",       "rxvt",   "screen", "xterm", NULL,
  };

  /* If tsl is set, then terminfo says that status lines work. */
  char *tcaps = tigetstr("tsl");
  if (tcaps && (tcaps != (char *) -1) && *tcaps)
  {
    /* update the static definitions of tsl/fsl from terminfo */
    tsl = tcaps;

    tcaps = tigetstr("fsl");
    if (tcaps && (tcaps != (char *) -1) && *tcaps)
      fsl = tcaps;

    return true;
  }

  /* If XT (boolean) is set, then this terminal supports the standard escape. */
  /* Beware: tigetflag returns -1 if XT is invalid or not a boolean. */
  use_extended_names(true);
  int tcapi = tigetflag("XT");
  if (tcapi == 1)
    return true;

  /* Check term types that are known to support the standard escape without
   * necessarily asserting it in terminfo. */
  const char *term = mutt_str_getenv("TERM");
  for (const char **termp = known; termp; termp++)
  {
    if (term && *termp && !mutt_istr_startswith(term, *termp))
      return true;
  }

  /* not reached */
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

  fprintf(stderr, "%s%s%s", tsl, str, fsl);
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
