#ifndef _REMAILER_H
#define _REMAILER_H

/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/*
 * Mixmaster support for Mutt
 */

#ifdef MIXMASTER

#define MIX_CAP_COMPRESS  (1 << 0)
#define MIX_CAP_MIDDLEMAN (1 << 1)
#define MIX_CAP_NEWSPOST  (1 << 2)
#define MIX_CAP_NEWSMAIL  (1 << 3)

/* Mixmaster's maximum chain length.  Don't change this. */

#define MAXMIXES 19

struct type2
{
  int num;
  char *shortname;
  char *addr;
  char *ver;
  int caps;
};

typedef struct type2 REMAILER;


struct mixchain
{
  size_t cl;
  int ch[MAXMIXES];
};

typedef struct mixchain MIXCHAIN;

int mix_send_message (LIST *, const char *);
int mix_check_message (HEADER *msg);
void mix_make_chain (LIST **, int *);

#endif /* MIXMASTER */

#endif /* _REMAILER_H */
