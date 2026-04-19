/**
 * @file
 * Utility Window
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_GUI_UTILWIN_H
#define MUTT_GUI_UTILWIN_H

struct MuttWindow;

struct MuttWindow *utilwin_new     (void);
const char *       utilwin_get_text(struct MuttWindow *win);
void               utilwin_set_text(struct MuttWindow *win, const char *text);

#endif /* MUTT_GUI_UTILWIN_H */
