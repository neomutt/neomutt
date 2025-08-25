/**
 * @file
 * ctype(3) wrapper functions
 *
 * @authors
 * Copyright (C) 2025 Thomas Klausne <wiz@gatalith.at>
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

#ifndef MUTT_MUTT_CTYPE_H
#define MUTT_MUTT_CTYPE_H

int mutt_isalnum(int arg);
int mutt_isalpha(int arg);
int mutt_isdigit(int arg);
int mutt_ispunct(int arg);
int mutt_isspace(int arg);
int mutt_isxdigit(int arg);
int mutt_tolower(int arg);
int mutt_toupper(int arg);

#endif /* MUTT_MUTT_CTYPE_H */
