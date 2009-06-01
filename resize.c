/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#else
# ifdef HAVE_IOCTL_H
# include <ioctl.h>
# endif
#endif

/* this routine should be called after receiving SIGWINCH */
void mutt_resize_screen (void)
{
  char *cp;
  int fd;
  struct winsize w;
#ifdef HAVE_RESIZETERM
  int SLtt_Screen_Rows, SLtt_Screen_Cols;
#endif

  SLtt_Screen_Rows = -1;
  SLtt_Screen_Cols = -1;
  if ((fd = open ("/dev/tty", O_RDONLY)) != -1)
  {
    if (ioctl (fd, TIOCGWINSZ, &w) != -1)
    {
      SLtt_Screen_Rows = w.ws_row;
      SLtt_Screen_Cols = w.ws_col;
    }
    close (fd);
  }
  if (SLtt_Screen_Rows <= 0)
  {
    if ((cp = getenv ("LINES")) != NULL && mutt_atoi (cp, &SLtt_Screen_Rows) < 0)
      SLtt_Screen_Rows = 24;
  }
  if (SLtt_Screen_Cols <= 0)
  {
    if ((cp = getenv ("COLUMNS")) != NULL && mutt_atoi (cp, &SLtt_Screen_Cols) < 0)
      SLtt_Screen_Cols = 80;
  }
#ifdef USE_SLANG_CURSES
  delwin (stdscr);
  SLsmg_reset_smg ();
  SLsmg_init_smg ();
  stdscr = newwin (0, 0, 0, 0);
  keypad (stdscr, TRUE);
#else
  resizeterm (SLtt_Screen_Rows, SLtt_Screen_Cols);
#endif
}
