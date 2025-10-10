/**
 * @file
 * Color private Module data
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

#ifndef MUTT_COLOR_MODULE_DATA_H
#define MUTT_COLOR_MODULE_DATA_H

#include "attr.h"
#include "color.h"
#include "curses2.h"
#include "regex4.h"

/**
 * struct ColorModuleData - Color private Module data
 */
struct ColorModuleData
{
  struct Notify          *notify;                       ///< Notifications
  struct CursesColorList  curses_colors;                ///< List of all Curses colours
  int                     num_curses_colors;            ///< Number of ncurses colours left to allocate
  struct AttrColorList    merged_colors;                ///< Array of user colours
  struct Notify          *colors_notify;                ///< Notifications: #ColorId, #EventColor
  int                     num_quoted_colors;            ///< Number of colours for quoted email text
  struct AttrColor        simple_colors[MT_COLOR_MAX];  ///< Array of Simple colours
  struct RegexColorList   attach_list;                  ///< List of colours applied to the attachment headers
  struct RegexColorList   body_list;                    ///< List of colours applied to the email body
  struct RegexColorList   header_list;                  ///< List of colours applied to the email headers
  struct RegexColorList   status_list;                  ///< List of colours applied to the status bar
};

#endif /* MUTT_COLOR_MODULE_DATA_H */
