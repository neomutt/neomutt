/**
 * @file
 * XXX
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
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

#ifndef MUTT_AUTOCRYPT_AUTOCRYPT_PRIVATE_H
#define MUTT_AUTOCRYPT_AUTOCRYPT_PRIVATE_H

#include <sqlite3.h>

struct Address;
struct Buffer;

int mutt_autocrypt_account_init (void);

int mutt_autocrypt_db_init (int can_create);
void mutt_autocrypt_db_close (void);

struct AutocryptAccount *mutt_autocrypt_db_account_new (void);
void mutt_autocrypt_db_account_free (struct AutocryptAccount **account);
int mutt_autocrypt_db_account_get (struct Address *addr, struct AutocryptAccount **account);
int mutt_autocrypt_db_account_insert (struct Address *addr, const char *keyid,
                                      const char *keydata, int prefer_encrypt);

int mutt_autocrypt_schema_init (void);
int mutt_autocrypt_schema_update (void);

int mutt_autocrypt_gpgme_init (void);
int mutt_autocrypt_gpgme_create_key (struct Address *addr, struct Buffer *keyid, struct Buffer *keydata);

/* Prepared statements */
sqlite3_stmt *AccountGetStmt;
sqlite3_stmt *AccountInsertStmt;

#endif /* MUTT_AUTOCRYPT_AUTOCRYPT_PRIVATE_H */
