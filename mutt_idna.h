/*
 * Copyright (C) 2003,2005 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _MUTT_IDNA_H
# define _MUTT_IDNA_H

#include "rfc822.h"
#include "charset.h"

#ifdef HAVE_LIBIDN
#include <idna.h>
#endif

#define MI_MAY_BE_IRREVERSIBLE		(1 << 0)

/* Work around incompatibilities in the libidn API */

#ifdef HAVE_LIBIDN
int mutt_addrlist_to_idna (ADDRESS *, char **);
int mutt_addrlist_to_local (ADDRESS *);

void mutt_env_to_local (ENVELOPE *);
int mutt_env_to_idna (ENVELOPE *, char **, char **);

const char *mutt_addr_for_display (ADDRESS *a);

# if (!defined(HAVE_IDNA_TO_ASCII_8Z) && defined(HAVE_IDNA_TO_ASCII_FROM_UTF8))
#  define idna_to_ascii_8z(a,b,c) idna_to_ascii_from_utf8(a,b,(c)&1,((c)&2)?1:0)
# endif
# if (!defined(HAVE_IDNA_TO_ASCII_LZ) && defined(HAVE_IDNA_TO_ASCII_FROM_LOCALE))
#  define idna_to_ascii_lz(a,b,c) idna_to_ascii_from_locale(a,b,(c)&1,((c)&2)?1:0)
# endif
# if (!defined(HAVE_IDNA_TO_UNICODE_8Z8Z) && defined(HAVE_IDNA_TO_UNICODE_UTF8_FROM_UTF8))
#  define idna_to_unicode_8z8z(a,b,c) idna_to_unicode_utf8_from_utf8(a,b,(c)&1,((c)&2)?1:0)
# endif
#else

static inline int mutt_addrlist_to_idna (ADDRESS *addr, char **err)
{
  return 0;
}

static inline int mutt_addrlist_to_local (ADDRESS *addr)
{
  return 0;
}

static inline void mutt_env_to_local (ENVELOPE *env)
{
  return;
}

static inline int mutt_env_to_idna (ENVELOPE *env, char **tag, char **err)
{
  return 0;
}

static inline const char *mutt_addr_for_display (ADDRESS *a)
{
  return a->mailbox;
}

#endif /* HAVE_LIBIDN */

#endif
