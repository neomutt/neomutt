/**
 * @file
 * ASCII string comparison routines
 *
 * @authors
 * Copyright (C) 2001-2002 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * Versions of the string comparison functions which are locale-insensitive.
 */

#ifndef _LIB_ASCII_H
#define _LIB_ASCII_H

int ascii_strcasecmp(const char *a, const char *b);
int ascii_strncasecmp(const char *a, const char *b, int n);
char *ascii_strlower(char *s);

#endif /* _LIB_ASCII_H */
