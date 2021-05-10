/**
 * @file
 * Private Menu functions
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

#ifndef MUTT_MENU_PRIVATE_H
#define MUTT_MENU_PRIVATE_H

#include "type.h"

struct Menu;
struct MuttWindow;

void         menu_free(struct Menu **ptr);
struct Menu *menu_new (enum MenuType type, struct MuttWindow *win);

#endif /* MUTT_MENU_PRIVATE_H */
