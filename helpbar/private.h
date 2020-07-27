/**
 * @file
 * Shared constants/structs that are private to the Help Bar
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_HELPBAR_PRIVATE_H
#define MUTT_HELPBAR_PRIVATE_H

struct MuttWindow;

/**
 * struct HelpbarWindowData - Help Bar Window data
 *
 * This is used to cache the data to generate the Help Bar text.
 */
struct HelpbarWindowData
{
  int                   help_menu; ///< Menu for key bindings, e.g. #MENU_PAGER
  const struct Mapping *help_data; ///< Data for the Help Bar
  char *                help_str;  ///< Formatted Help Bar string
};

void                      helpbar_wdata_free(struct MuttWindow *win, void **ptr);
struct HelpbarWindowData *helpbar_wdata_get (struct MuttWindow *win);
struct HelpbarWindowData *helpbar_wdata_new (void);

#endif /* MUTT_HELPBAR_PRIVATE_H */
