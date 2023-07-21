/**
 * @file
 * Expando Data Domains
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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

#ifndef MUTT_EXPANDO_DEBUG_PRINT_H
#define MUTT_EXPANDO_DEBUG_PRINT_H

struct ExpandoNode;

void expando_tree_print(struct ExpandoNode **root);

void expando_print_color_string(const char *s);

#endif /* MUTT_EXPANDO_DEBUG_H */
