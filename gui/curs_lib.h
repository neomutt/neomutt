/**
 * @file
 * GUI miscellaneous curses (window drawing) routines
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_GUI_CURS_LIB_H
#define MUTT_GUI_CURS_LIB_H

#include <stdbool.h>
#include <wchar.h>
#include "browser/lib.h"
#include "key/lib.h"

struct Buffer;
struct Mailbox;
struct MuttWindow;

/**
 * struct FileCompletionData - Input for the file completion function
 */
struct FileCompletionData
{
  bool            multiple; ///< Allow multiple selections
  struct Mailbox *mailbox;  ///< Mailbox
  char         ***files;    ///< List of files selected
  int            *numfiles; ///< Number of files selected
};

int          mutt_addwch(struct MuttWindow *win, wchar_t wc);
int          mutt_any_key_to_continue(const char *s);
void         mutt_beep(bool force);
int          mw_enter_fname(const char *prompt, struct Buffer *fname, bool mailbox, struct Mailbox *m, bool multiple, char ***files, int *numfiles, SelectFileFlags flags);
void         mutt_edit_file(const char *editor, const char *file);
void         mutt_endwin(void);
void         mutt_flushinp(void);
struct KeyEvent mutt_getch(GetChFlags flags);
void         mutt_need_hard_redraw(void);
void         mutt_paddstr(struct MuttWindow *win, int n, const char *s);
void         mutt_push_macro_event(int ch, int op);
void         mutt_query_exit(void);
void         mutt_refresh(void);
size_t       mutt_strwidth(const char *s);
size_t       mutt_strnwidth(const char *s, size_t len);
void         mutt_unget_ch(int ch);
void         mutt_unget_op(int op);
void         mutt_unget_string(const char *s);
size_t       mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);

#endif /* MUTT_GUI_CURS_LIB_H */
