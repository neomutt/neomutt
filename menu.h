/**
 * @file
 * GUI present the user with a selectable list
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

#ifndef MUTT_MENU_H
#define MUTT_MENU_H

/* These Config Variables are only used in menu.c */
extern short MenuContext;
extern bool  MenuMoveOff; /**< allow menu to scroll past last entry */
extern bool  MenuScroll;  /**< scroll menu instead of implicit next-page */

#endif /* MUTT_MENU_H */
