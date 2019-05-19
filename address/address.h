/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include "mutt/queue.h"

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
  TAILQ_ENTRY(Address) entries;
};
TAILQ_HEAD(AddressList, Address);

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

/* Utility functions that don't use struct Address or struct AddressList */
void            mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials);
bool            mutt_addr_valid_msgid(const char *msgid);

/* Functions that work on a single struct Address */
bool            mutt_addr_cmp(const struct Address *a, const struct Address *b);
struct Address *mutt_addr_copy(const struct Address *addr);
const char *    mutt_addr_for_display(const struct Address *a);
void            mutt_addr_free(struct Address **a);
size_t          mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display);
struct Address *mutt_addr_new(void);

/* Functions that work on struct AddressList */
void                mutt_addrlist_clear(struct AddressList *al);
void                mutt_addrlist_copy(struct AddressList *dst, const struct AddressList *src, bool prune);
size_t              mutt_addrlist_write(char *buf, size_t buflen, const struct AddressList *al, bool display);
int                 mutt_addrlist_parse(struct AddressList *al, const char *s);
int                 mutt_addrlist_parse2(struct AddressList *al, const char *s);
int                 mutt_addrlist_to_local(struct AddressList *al);
int                 mutt_addrlist_to_intl(struct AddressList *al, char **err);
void                mutt_addrlist_dedupe(struct AddressList *al);
void                mutt_addrlist_qualify(struct AddressList *al, const char *host);
bool                mutt_addrlist_search(const struct Address *needle, const struct AddressList *haystack);
int                 mutt_addrlist_count_recips(const struct AddressList *al);
bool                mutt_addrlist_equal(const struct AddressList *ala, const struct AddressList *alb);
int                 mutt_addrlist_remove(struct AddressList *al, const char *mailbox);
void                mutt_addrlist_remove_xrefs(const struct AddressList *a, struct AddressList *b);
void                mutt_addrlist_append(struct AddressList *al, struct Address *a);
void                mutt_addrlist_prepend(struct AddressList *al, struct Address *a);

#endif /* MUTT_EMAIL_ADDRESS_H */
