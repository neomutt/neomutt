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

#ifndef MUTT_AUTOCRYPT_AUTOCRYPT_H
#define MUTT_AUTOCRYPT_AUTOCRYPT_H

#include <sqlite3.h>
#include "where.h"

WHERE sqlite3 *AutocryptDB;

struct AutocryptAccount
{
  char *email_addr;
  char *keyid;
  char *keydata;
  int prefer_encrypt;    /* 0 = nopref, 1 = mutual */
  int enabled;
};

struct AutocryptPeer
{
  char *email_addr;
  sqlite3_int64 last_seen;
  sqlite3_int64 autocrypt_timestamp;
  char *keyid;
  char *keydata;
  int prefer_encrypt;    /* 0 = nopref, 1 = mutual */
  sqlite3_int64 gossip_timestamp;
  char *gossip_keyid;
  char *gossip_keydata;
};

struct AutocryptPeerHistory
{
  char *peer_email_addr;
  char *email_msgid;
  sqlite3_int64 timestamp;
  char *keydata;
};

int mutt_autocrypt_init (int);
void mutt_autocrypt_cleanup (void);
int mutt_autocrypt_process_autocrypt_header (struct Email *hdr, struct Envelope *env);

#endif /* MUTT_AUTOCRYPT_AUTOCRYPT_H */
