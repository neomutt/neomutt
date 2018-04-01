/**
 * @file
 * GUI handle the resizing of the screen
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#else
#ifdef HAVE_IOCTL_H
#include <ioctl.h>
#endif
#endif

/**
 * mutt_resize_screen - Called after receiving SIGWINCH
 */
void mutt_resize_screen(void)
{
  const char *cp = NULL;
  int fd;
  struct winsize w;
#if !defined(USE_SLANG_CURSES)
  int SLtt_Screen_Rows, SLtt_Screen_Cols;
#endif

  SLtt_Screen_Rows = -1;
  SLtt_Screen_Cols = -1;
  fd = open("/dev/tty", O_RDONLY);
  if (fd != -1)
  {
    if (ioctl(fd, TIOCGWINSZ, &w) != -1)
    {
      SLtt_Screen_Rows = w.ws_row;
      SLtt_Screen_Cols = w.ws_col;
    }
    close(fd);
  }
  if (SLtt_Screen_Rows <= 0)
  {
    cp = mutt_str_getenv("LINES");
    if (cp && (mutt_str_atoi(cp, &SLtt_Screen_Rows) < 0))
      SLtt_Screen_Rows = 24;
  }
  if (SLtt_Screen_Cols <= 0)
  {
    cp = mutt_str_getenv("COLUMNS");
    if (cp && (mutt_str_atoi(cp, &SLtt_Screen_Cols) < 0))
      SLtt_Screen_Cols = 80;
  }
#ifdef USE_SLANG_CURSES
  delwin(stdscr);
  SLsmg_reset_smg();
  SLsmg_init_smg();
  stdscr = newwin(0, 0, 0, 0);
  keypad(stdscr, true);
#else
  resizeterm(SLtt_Screen_Rows, SLtt_Screen_Cols);
#endif
  mutt_window_reflow();
}
