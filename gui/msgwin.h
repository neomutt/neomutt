/**
 * @file
 * Message Window
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_MSGWIN_H
#define MUTT_GUI_MSGWIN_H

#include <stdbool.h>
#include "color/lib.h"

struct MuttWindow;

void               msgwin_clear_text(struct MuttWindow *win);
struct MuttWindow *msgwin_new       (bool interactive);
void               msgwin_add_text  (struct MuttWindow *win, const char *text, const struct AttrColor *ac_color);
void               msgwin_add_text_n(struct MuttWindow *win, const char *text, int bytes, const struct AttrColor *ac_color);
const char *       msgwin_get_text  (struct MuttWindow *win);
struct MuttWindow *msgwin_get_window(void);
void               msgwin_set_rows  (struct MuttWindow *win, short rows);
void               msgwin_set_text  (struct MuttWindow *win, const char *text, enum ColorId color);

#endif /* MUTT_GUI_MSGWIN_H */
