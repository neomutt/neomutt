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

#define CHARSET_MAGIC "Mutt Character Set Definition 1.1\n"

#ifndef _GEN_CHARSETS

typedef int CHARSET_MAP[256];

typedef struct 
{
  CHARSET_MAP *map;
} 
CHARSET;

CHARSET *mutt_get_charset(const char *);
CHARSET_MAP *mutt_get_translation(const char *, const char *);

unsigned char mutt_display_char(unsigned char, CHARSET_MAP *);

int mutt_display_string(char *, CHARSET_MAP *);
int mutt_is_utf8(const char *);

void mutt_decode_utf8_string(char *, CHARSET *);

void state_fput_utf8(STATE *, char, CHARSET *);

#endif

#endif /* _CHARSET_H */
