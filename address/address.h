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
#include "mutt/lib.h"

/**
 * struct Address - An email address
 */
struct Address
{
  char *personal;               ///< Real name of address
  char *mailbox;                ///< Mailbox and host address
  bool group : 1;               ///< Group mailbox?
  bool is_intl : 1;             ///< International Domain Name
  bool intl_checked : 1;        ///< Checked for IDN?
  TAILQ_ENTRY(Address) entries; ///< Linked list
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

/**
 * typedef addr_predicate_t - Test an Address for some condition
 * @param a Address to test
 * @retval bool True if Address matches the test
 */
typedef bool (*addr_predicate_t)(const struct Address *a);

/* Utility functions that don't use struct Address or struct AddressList */
void mutt_addr_cat        (char *buf, size_t buflen, const char *value, const char *specials);
bool mutt_addr_valid_msgid(const char *msgid);

/* Functions that work on a single struct Address */
bool            mutt_addr_cmp        (const struct Address *a, const struct Address *b);
struct Address *mutt_addr_copy       (const struct Address *addr);
struct Address *mutt_addr_create     (const char *personal, const char *mailbox);
const char *    mutt_addr_for_display(const struct Address *a);
void            mutt_addr_free       (struct Address **ptr);
struct Address *mutt_addr_new        (void);
bool            mutt_addr_to_intl    (struct Address *a);
bool            mutt_addr_to_local   (struct Address *a);
bool            mutt_addr_uses_unicode(const char *str);
size_t          mutt_addr_write      (char *buf, size_t buflen, struct Address *addr, bool display);

/* Functions that work on struct AddressList */
void   mutt_addrlist_append      (struct AddressList *al, struct Address *a);
void   mutt_addrlist_clear       (struct AddressList *al);
void   mutt_addrlist_copy        (struct AddressList *dst, const struct AddressList *src, bool prune);
int    mutt_addrlist_count_recips(const struct AddressList *al);
void   mutt_addrlist_dedupe      (struct AddressList *al);
bool   mutt_addrlist_equal       (const struct AddressList *ala, const struct AddressList *alb);
int    mutt_addrlist_parse       (struct AddressList *al, const char *s);
int    mutt_addrlist_parse2      (struct AddressList *al, const char *s);
void   mutt_addrlist_prepend     (struct AddressList *al, struct Address *a);
void   mutt_addrlist_qualify     (struct AddressList *al, const char *host);
int    mutt_addrlist_remove      (struct AddressList *al, const char *mailbox);
void   mutt_addrlist_remove_xrefs(const struct AddressList *a, struct AddressList *b);
bool   mutt_addrlist_search      (const struct AddressList *haystack, const struct Address *needle);
int    mutt_addrlist_to_intl     (struct AddressList *al, char **err);
int    mutt_addrlist_to_local    (struct AddressList *al);
bool   mutt_addrlist_uses_unicode(const struct AddressList *al);
size_t mutt_addrlist_write       (const struct AddressList *al, char *buf, size_t buflen, bool display);
void   mutt_addrlist_write_file  (const struct AddressList *addr, FILE *fp, int linelen, bool display);
size_t mutt_addrlist_write_list  (const struct AddressList *al, struct ListHead *list);

#endif /* MUTT_EMAIL_ADDRESS_H */
