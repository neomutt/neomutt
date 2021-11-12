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
 * @page neo_resize GUI handle the resizing of the screen
 *
 * GUI handle the resizing of the screen
 */

#include "config.h"
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifndef HAVE_TCGETWINSIZE
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#else
#ifdef HAVE_IOCTL_H
#include <ioctl.h>
#endif
#endif
#endif /* HAVE_TCGETWINSIZE */

/**
 * mutt_get_winsize - Get the window size
 * @retval obj Window size
 */
static struct winsize mutt_get_winsize(void)
{
  struct winsize w = { 0 };

  int fd = open("/dev/tty", O_RDONLY);
  if (fd != -1)
  {
#ifdef HAVE_TCGETWINSIZE
    tcgetwinsize(fd, &w);
#else
    ioctl(fd, TIOCGWINSZ, &w);
#endif
    close(fd);
  }
  return w;
}

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
    if (cp && !mutt_str_atoi_full(cp, &screenrows))
      screenrows = 24;
  }

  if (screencols <= 0)
  {
    const char *cp = mutt_str_getenv("COLUMNS");
    if (cp && !mutt_str_atoi_full(cp, &screencols))
      screencols = 80;
  }

  resizeterm(screenrows, screencols);
  rootwin_set_size(screencols, screenrows);
  window_notify_all(NULL);
}
