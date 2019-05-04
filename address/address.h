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

#ifndef MUTT_EMAIL_ADDRESS_H
#define MUTT_EMAIL_ADDRESS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * struct Address - An email address
 */
struct Address
{
  char *personal; /**< real name of address */
  char *mailbox;  /**< mailbox and host address */
  bool group : 1; /**< group mailbox? */
  bool is_intl : 1;
  bool intl_checked : 1;
  struct Address *next;
};

/**
 * enum AddressError - possible values for AddressError
 */
enum AddressError
{
  ADDR_ERR_MEMORY = 1,     ///< Out of memory
  ADDR_ERR_MISMATCH_PAREN, ///< Mismatched parentheses
  ADDR_ERR_MISMATCH_QUOTE, ///< Mismatches quotes
  ADDR_ERR_BAD_ROUTE,      ///< Bad route
  ADDR_ERR_BAD_ROUTE_ADDR, ///< Bad route address
  ADDR_ERR_BAD_ADDR_SPEC,  ///< Bad address specifier
};

extern int AddressError;
extern const char *const AddressErrors[];
extern const char AddressSpecials[];

#define address_error(num) AddressErrors[num]

struct Address *mutt_addr_append(struct Address **a, struct Address *b, bool prune);
void            mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials);
bool            mutt_addr_cmp(struct Address *a, struct Address *b);
bool            mutt_addr_cmp_strict(const struct Address *a, const struct Address *b);
struct Address *mutt_addr_copy(struct Address *addr);
struct Address *mutt_addr_copy_list(struct Address *addr, bool prune);
const char *    mutt_addr_for_display(struct Address *a);
void            mutt_addr_free(struct Address **p);
int             mutt_addr_has_recips(struct Address *a);
bool            mutt_addr_is_intl(struct Address *a);
bool            mutt_addr_is_local(struct Address *a);
int             mutt_addr_mbox_to_udomain(const char *mbox, char **user, char **domain);
struct Address *mutt_addr_new(void);
struct Address *mutt_addr_parse_list(struct Address *top, const char *s);
struct Address *mutt_addr_parse_list2(struct Address *p, const char *s);
void            mutt_addr_qualify(struct Address *addr, const char *host);
int             mutt_addr_remove_from_list(struct Address **a, const char *mailbox);
struct Address *mutt_addr_remove_xrefs(struct Address *a, struct Address *b);
bool            mutt_addr_search(struct Address *a, struct Address *lst);
void            mutt_addr_set_intl(struct Address *a, char *intl_mailbox);
void            mutt_addr_set_local(struct Address *a, char *local_mailbox);
bool            mutt_addr_valid_msgid(const char *msgid);
size_t          mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display);
void            mutt_addr_write_single(char *buf, size_t buflen, struct Address *addr, bool display);
struct Address *mutt_addrlist_dedupe(struct Address *addr);
int             mutt_addrlist_to_intl(struct Address *a, char **err);
int             mutt_addrlist_to_local(struct Address *a);

#endif /* MUTT_EMAIL_ADDRESS_H */
