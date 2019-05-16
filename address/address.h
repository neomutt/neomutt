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
  struct Address *next;
};

struct AddressNode
{
  struct Address *addr;
  TAILQ_ENTRY(AddressNode) entries;
};
TAILQ_HEAD(AddressList, AddressNode);

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

void            mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials);
bool            mutt_addr_cmp(const struct Address *a, const struct Address *b);
struct Address *mutt_addr_copy(const struct Address *addr);
const char *    mutt_addr_for_display(const struct Address *a);
void            mutt_addr_free(struct Address **a);
size_t          mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display);

int             mutt_addr_mbox_to_udomain(const char *mbox, char **user, char **domain);
struct Address *mutt_addr_new(void);
int             mutt_addr_remove_from_list(struct AddressList *a, const char *mailbox);
void            mutt_addr_remove_xrefs(const struct AddressList *a, struct AddressList *b);
void            mutt_addr_set_intl(struct Address *a, char *intl_mailbox);
void            mutt_addr_set_local(struct Address *a, char *local_mailbox);
bool            mutt_addr_valid_msgid(const char *msgid);
struct Address *mutt_addrlist_dedupe(struct Address *addr);
int             mutt_addrlist_to_intl(struct Address *a, char **err);
int             mutt_addrlist_to_local(struct Address *a);

struct AddressList *mutt_addr_to_addresslist(struct Address *a);
struct AddressList *mutt_addresslist_new(void);
struct Address     *mutt_addresslist_to_addr(struct AddressList *al);
void                mutt_addresslist_append(struct AddressList *al, struct Address *a);
void                mutt_addresslist_prepend(struct AddressList *al, struct Address *a);
void                mutt_addresslist_copy(struct AddressList *dst, const struct AddressList *src, bool prune);
void                mutt_addresslist_clear(struct AddressList *al);
void                mutt_addresslist_free(struct AddressList **al);
void                mutt_addresslist_free_one(struct AddressList *al, struct AddressNode *anode);
void                mutt_addresslist_free_all(struct AddressList *al);
size_t              mutt_addresslist_write(char *buf, size_t buflen, const struct AddressList* addr, bool display);
int                 mutt_addresslist_parse(struct AddressList *top, const char *s);
int                 mutt_addresslist_parse2(struct AddressList *top, const char *s);
int                 mutt_addresslist_to_local(struct AddressList *al);
int                 mutt_addresslist_to_intl(struct AddressList *al, char **err);
void                mutt_addresslist_dedupe(struct AddressList *al);
void                mutt_addresslist_qualify(struct AddressList *al, const char *host);
struct Address*     mutt_addresslist_first(const struct AddressList *al);
bool                mutt_addresslist_search(const struct Address *needle, const struct AddressList *haystack);
int                 mutt_addresslist_has_recips(const struct AddressList *al);
bool                mutt_addresslist_equal(const struct AddressList *ala, const struct AddressList *alb);

#endif /* MUTT_EMAIL_ADDRESS_H */
