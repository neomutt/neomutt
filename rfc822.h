/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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

#ifndef rfc822_h
#define rfc822_h

#include "config.h"

/* possible values for RFC822Error */
enum
{
  ERR_MEMORY = 1,
  ERR_MISMATCH_PAREN,
  ERR_MISMATCH_QUOTE,
  ERR_BAD_ROUTE,
  ERR_BAD_ROUTE_ADDR,
  ERR_BAD_ADDR_SPEC
};

typedef struct address_t
{
#ifdef EXACT_ADDRESS
  char *val;		/* value of address as parsed */
#endif
  char *personal;	/* real name of address */
  char *mailbox;	/* mailbox and host address */
  int group;		/* group mailbox? */
  struct address_t *next;
}
ADDRESS;

void rfc822_free_address (ADDRESS **);
void rfc822_qualify (ADDRESS *, const char *);
ADDRESS *rfc822_parse_adrlist (ADDRESS *, const char *s);
ADDRESS *rfc822_cpy_adr (ADDRESS *addr);
ADDRESS *rfc822_cpy_adr_real (ADDRESS *addr);
ADDRESS *rfc822_append (ADDRESS **a, ADDRESS *b);
void rfc822_write_address (char *, size_t, ADDRESS *);
void rfc822_write_address_single (char *, size_t, ADDRESS *);
void rfc822_write_list (char *, size_t, ADDRESS *);
void rfc822_free_address (ADDRESS **addr);
void rfc822_cat (char *, size_t, const char *, const char *);

extern int RFC822Error;
extern const char *RFC822Errors[];

#define rfc822_error(x) RFC822Errors[x]
#define rfc822_new_address() calloc(1,sizeof(ADDRESS))

#endif /* rfc822_h */
