/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_ADDRESS_H
#define _MUTT_ADDRESS_H

#include <stdbool.h>

/**
 * struct Address - An email address
 */
struct Address
{
  char *personal; /**< real name of address */
  char *mailbox;  /**< mailbox and host address */
  int group;      /**< group mailbox? */
  struct Address *next;
  bool is_intl : 1;
  bool intl_checked : 1;
};

/**
 * enum AddressError - possible values for AddressError
 */
enum AddressError
{
  ERR_MEMORY = 1,
  ERR_MISMATCH_PAREN,
  ERR_MISMATCH_QUOTE,
  ERR_BAD_ROUTE,
  ERR_BAD_ROUTE_ADDR,
  ERR_BAD_ADDR_SPEC
};

extern int AddressError;
extern const char *const AddressErrors[];
extern const char AddressSpecials[];

#define address_error(x) AddressErrors[x]

struct Address *mutt_addr_append(struct Address **a, struct Address *b, bool prune);
void            mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials);
bool            mutt_addr_cmp_strict(const struct Address *a, const struct Address *b);
bool            mutt_addr_cmp(struct Address *a, struct Address *b);
struct Address *mutt_addr_copy_list(struct Address *addr, bool prune);
struct Address *mutt_addr_copy(struct Address *addr);
void            mutt_addr_free(struct Address **p);
int             mutt_addr_has_recips(struct Address *a);
struct Address *mutt_addr_new(void);
struct Address *mutt_addr_parse_list2(struct Address *p, const char *s);
struct Address *mutt_addr_parse_list(struct Address *top, const char *s);
void            mutt_addr_qualify(struct Address *addr, const char *host);
int             mutt_addr_remove_from_list(struct Address **a, const char *mailbox);
bool            mutt_addr_search(struct Address *a, struct Address *lst);
bool            mutt_addr_valid_msgid(const char *msgid);

#endif /* _MUTT_ADDRESS_H */
