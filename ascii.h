/*
 * Copyright (C) 2001 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 59 Temple Place - Suite 330,
 *     Boston, MA 02111, USA.
 * 
 */


/* 
 * Versions of the string comparison functions which are
 * locale-insensitive.
 */

#ifndef _ASCII_H
# define _ASCII_H

int ascii_isupper (int c);
int ascii_islower (int c);
int ascii_toupper (int c);
int ascii_tolower (int c);
int ascii_strcasecmp (const char *a, const char *b);
int ascii_strncasecmp (const char *a, const char *b, int n);

#define ascii_strcmp(a,b) mutt_strcmp(a,b)
#define ascii_strncmp(a,b) mutt_strncmp(a,b)

#endif
