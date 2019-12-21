/**
 * @file
 * GUI handle the resizing of the screen
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018      Ivan J. <parazyd@dyne.org>
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
 * @page resize GUI handle the resizing of the screen
 *
 * GUI handle the resizing of the screen
 */

#include "config.h"
#include <fcntl.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#ifdef USE_SLANG_CURSES
#include <stdbool.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#else
#ifdef HAVE_IOCTL_H
#include <ioctl.h>
#endif
#endif

/**
 * mutt_get_winsize - Use an ioctl to get the window size
 * @retval obj Window size
 */
static struct winsize mutt_get_winsize(void)
{
  struct winsize w = { 0 };
  int fd = open("/dev/tty", O_RDONLY);
  if (fd != -1)
  {
    ioctl(fd, TIOCGWINSZ, &w);
    close(fd);
  }
  return w;
}

#ifdef USE_SLANG_CURSES
/**
 * mutt_resize_screen - Update NeoMutt's opinion about the window size (SLANG)
 */
void mutt_resize_screen(void)
{
  struct winsize w = mutt_get_winsize();

  /* The following two variables are global to slang */
  SLtt_Screen_Rows = w.ws_row;
  SLtt_Screen_Cols = w.ws_col;

  if (SLtt_Screen_Rows <= 0)
  {
    const char *cp = mutt_str_getenv("LINES");
    if (cp && (mutt_str_atoi(cp, &SLtt_Screen_Rows) < 0))
      SLtt_Screen_Rows = 24;
  }

  if (SLtt_Screen_Cols <= 0)
  {
    const char *cp = mutt_str_getenv("COLUMNS");
    if (cp && (mutt_str_atoi(cp, &SLtt_Screen_Cols) < 0))
      SLtt_Screen_Cols = 80;
  }

  delwin(stdscr);
  SLsmg_reset_smg();
  SLsmg_init_smg();
  stdscr = newwin(0, 0, 0, 0);
  keypad(stdscr, true);
  mutt_window_set_root(w.ws_row, w.ws_col);
  mutt_window_reflow(NULL);
}
#else
/**
 * mutt_resize_screen - Update NeoMutt's opinion about the window size (CURSES)
 */
void mutt_resize_screen(void)
{
  struct winsize w = mutt_get_winsize();

  int screenrows = w.ws_row;
  int screencols = w.ws_col;

  if (screenrows <= 0)
  {
    const char *cp = mutt_str_getenv("LINES");
    if (cp && (mutt_str_atoi(cp, &screenrows) < 0))
      screenrows = 24;
  }

  if (screencols <= 0)
  {
    const char *cp = mutt_str_getenv("LINES");
    if (cp && (mutt_str_atoi(cp, &screencols) < 0))
      screencols = 80;
  }

  resizeterm(screenrows, screencols);
  mutt_window_set_root(w.ws_row, w.ws_col);
  mutt_window_reflow(NULL);
}
#endif
