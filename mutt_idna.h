/*
 * Copyright (C) 2003 Thomas Roessler <roessler@does-not-exist.org>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#ifndef _MUTT_IDNA_H
# define _MUTT_IDNA_H

#include "config.h"
#include "rfc822.h"
#include "charset.h"

#ifdef HAVE_LIBIDN
#include <idna.h>
#endif

#define MI_MAY_BE_IRREVERSIBLE		(1 << 0)

int mutt_idna_to_local (const char *, char **, int);
int mutt_local_to_idna (const char *, char **);

int mutt_addrlist_to_idna (ADDRESS *, char **);
int mutt_addrlist_to_local (ADDRESS *);

void mutt_env_to_local (ENVELOPE *);
int mutt_env_to_idna (ENVELOPE *, char **, char **);

const char *mutt_addr_for_display (ADDRESS *a);

#endif
