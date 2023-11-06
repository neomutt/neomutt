/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
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
 * @page neo_mutt_signal Signal handling
 *
 * Signal handling
 */

#include "config.h"
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "attach/lib.h"
#include "globals.h"
#include "protos.h"
#if defined(USE_DEBUG_GRAPHVIZ) || defined(USE_DEBUG_BACKTRACE)
#include "debug/lib.h"
#endif

/// Ncurses function isendwin() has been called
static int IsEndwin = 0;

/**
 * curses_signal_handler - Catch signals and relay the info to the main program - Implements ::sig_handler_t - @ingroup sig_handler_api
 * @param sig Signal number, e.g. SIGINT
 */
static void curses_signal_handler(int sig)
{
  int save_errno = errno;
  enum MuttCursorState old_cursor = MUTT_CURSOR_VISIBLE;

  switch (sig)
  {
    case SIGTSTP: /* user requested a suspend */
    {
      const bool c_suspend = cs_subset_bool(NeoMutt->sub, "suspend");
      if (!c_suspend)
        break;
      IsEndwin = isendwin();
      old_cursor = mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
      if (!IsEndwin)
        endwin();
      kill(0, SIGSTOP);
    }
      FALLTHROUGH;

    case SIGCONT:
      if (!IsEndwin)
        refresh();
      mutt_curses_set_cursor(old_cursor);
      /* We don't receive SIGWINCH when suspended; however, no harm is done by
       * just assuming we received one, and triggering the 'resize' anyway. */
      SigWinch = true;
      break;

    case SIGWINCH:
      SigWinch = true;
      break;

    case SIGINT:
      SigInt = true;
      break;
  }
  errno = save_errno;
}

/**
 * curses_exit_handler - Notify the user and shutdown gracefully - Implements ::sig_handler_t - @ingroup sig_handler_api
 * @param sig Signal number, e.g. SIGTERM
 */
static void curses_exit_handler(int sig)
{
  mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
  endwin(); /* just to be safe */
  mutt_temp_attachments_cleanup();
  mutt_sig_exit_handler(sig); /* DOES NOT RETURN */
}

/**
 * curses_segv_handler - Catch a segfault and print a backtrace - Implements ::sig_handler_t - @ingroup sig_handler_api
 * @param sig Signal number, e.g. SIGSEGV
 */
static void curses_segv_handler(int sig)
{
  mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
  endwin(); /* just to be safe */
#ifdef USE_DEBUG_BACKTRACE
  show_backtrace();
#endif
#ifdef USE_DEBUG_GRAPHVIZ
  dump_graphviz("segfault", NULL);
#endif

  struct sigaction act = { 0 };
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = SIG_DFL;
  sigaction(sig, &act, NULL);
  // Re-raise the signal to give outside handlers a chance to deal with it
  raise(sig);
}

/**
 * mutt_signal_init - Initialise the signal handling
 */
void mutt_signal_init(void)
{
  mutt_sig_init(curses_signal_handler, curses_exit_handler, curses_segv_handler);
}
