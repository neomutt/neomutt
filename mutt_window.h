/**
 * @file
 * Window management
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

#ifndef MUTT_MUTT_WINDOW_H
#define MUTT_MUTT_WINDOW_H

#include "config.h"

/**
 * struct MuttWindow - A division of the screen
 *
 * Windows for different parts of the screen
 */
struct MuttWindow
{
  int rows;
  int cols;
  int row_offset;
  int col_offset;
};

extern struct MuttWindow *MuttHelpWindow;
extern struct MuttWindow *MuttIndexWindow;
extern struct MuttWindow *MuttMessageWindow;
#ifdef USE_SIDEBAR
extern struct MuttWindow *MuttSidebarWindow;
#endif
extern struct MuttWindow *MuttStatusWindow;

// Functions that deal with the Window
void               mutt_window_copy_size          (const struct MuttWindow *win_src, struct MuttWindow *win_dst);
void               mutt_window_free               (struct MuttWindow **ptr);
void               mutt_window_free_all           (void);
void               mutt_window_get_coords         (struct MuttWindow *win, int *col, int *row);
void               mutt_window_init               (void);
struct MuttWindow *mutt_window_new                (void);
void               mutt_window_reflow             (void);
void               mutt_window_reflow_message_rows(int mw_rows);
int                mutt_window_wrap_cols          (int width, short wrap);

// Functions for drawing on the Window
int  mutt_window_addch    (int ch);
int  mutt_window_addnstr  (const char *str, int num);
int  mutt_window_addstr   (const char *str);
void mutt_window_clear_screen(void);
void mutt_window_clearline(struct MuttWindow *win, int row);
void mutt_window_clrtobot (void);
void mutt_window_clrtoeol (struct MuttWindow *win);
int  mutt_window_move     (struct MuttWindow *win, int row, int col);
void mutt_window_move_abs (int row, int col);
int  mutt_window_mvaddstr (struct MuttWindow *win, int row, int col, const char *str);
int  mutt_window_mvprintw (struct MuttWindow *win, int row, int col, const char *fmt, ...);
int  mutt_window_printf   (const char *format, ...);

#endif /* MUTT_MUTT_WINDOW_H */
