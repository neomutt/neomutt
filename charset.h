/*
 * Copyright (C) 1998 Ruslan Ermilov <ru@ucb.crimea.ua>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _CHARSET_H
#define _CHARSET_H

typedef int UNICODE_MAP[128];

typedef struct 
{
  char *name;
  UNICODE_MAP *map;
} CHARSET;

#define CHARSET_MAGIC "Mutt Character Set Definition 1.0\n"

#ifndef _GEN_CHARSETS

CHARSET *mutt_get_charset(const char *);
UNICODE_MAP *mutt_get_translation(const char *, const char *);
int mutt_display_char(int, UNICODE_MAP *);
int mutt_display_string(char *, UNICODE_MAP *);

#endif

#endif /* _CHARSET_H */
