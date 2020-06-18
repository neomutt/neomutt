/**
 * @file
 * Autocrypt end-to-end encryption
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

/**
 * @page autocrypt AUTOCRYPT: End-to-end encryption
 *
 * This is an implementation of Autocrypt Level 1.1. https://autocrypt.org/
 *
 * ## Still Todo
 *
 * * Setup message creation
 * * Setup message import
 *   These can both be added to the account menu, and perhaps the first-run too.
 *
 * ## Developer Notes
 *
 * ### `header->security | SEC_AUTOCRYPT`
 *
 * During message composition, SEC_AUTOCRYPT is mutually exclusive from
 * SEC_ENCRYPT and SEC_SIGN.  The former means that autocrypt will sign and
 * encrypt the email upon send, the latter means the normal keyring will do so.
 *
 * We keep these separate so that autocrypt can detect the normal keyring has
 * been turned on (manually, or by oppenc or something) and disable itself.
 *
 * Outside message composition the flags are not exclusive.  We can't tell a
 * message is an autocrypt message until we try to decrypt it.  Once we do so,
 * the flag is added to the existing flags.  The only relevance for decrypted
 * messages is when replying - in which case we want to force using autocrypt
 * in the reply.
 *
 * ### `header->security | SEC_AUTOCRYPT_OVERRIDE`
 *
 * I was loathe to use another bit for this, but unlike SEC_OPPENCRYPT,
 * SEC_AUTOCRYPT means the message *will* be encrypted, not that the option is
 * on.
 *
 * We need a way to distinguish between the user manually enabling autocrypt
 * and the recommendation engine doing so.  If this is not set, the engine can
 * turn SEC_AUTOCRYPT back off when the recipients change.  But if the user
 * manually set it, we don't want to do that.
 *
 * ### `mutt_autocrypt_init()`
 *
 * All public functions (in autocrypt.h) should call this function to make sure
 * everything is set up.  Nothing prevents the user from manually flipping the
 * option at runtime, but in that case the directory and such may not even
 * exist.
 *
 * Right now, I only allow "first run" initialization during startup.  Not all
 * calls are interactive, and we don't want to prompt the user while opening a
 * mailbox, for instance.
 *
 * ### Database schema version
 *
 * There is a "schema" table in the database, which records database version.
 * Any changes to the database should bump the schema version by adding a call
 * in mutt_autocrypt_schema_update().
 *
 * | File                            | Description                  |
 * | :------------------------------ | :--------------------------- |
 * | autocrypt/autocrypt.c           | @subpage autocrypt_autocrypt |
 * | autocrypt/autocrypt_acct_menu.c | @subpage autocrypt_account   |
 * | autocrypt/autocrypt_db.c        | @subpage autocrypt_db        |
 * | autocrypt/autocrypt_gpgme.c     | @subpage autocrypt_gpgme     |
 * | autocrypt/autocrypt_schema.c    | @subpage autocrypt_schema    |
 */

#ifndef MUTT_AUTOCRYPT_LIB_H
#define MUTT_AUTOCRYPT_LIB_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>

struct Email;
struct Envelope;

/**
 * struct AutocryptAccount - Autocrypt account
 */
struct AutocryptAccount
{
  char *email_addr;
  char *keyid;
  char *keydata;
  bool prefer_encrypt; ///< false = nopref, true = mutual
  bool enabled;
};

/**
 * struct AutocryptPeer - Autocrypt peer
 */
struct AutocryptPeer
{
  char *email_addr;
  sqlite3_int64 last_seen;
  sqlite3_int64 autocrypt_timestamp;
  char *keyid;
  char *keydata;
  bool prefer_encrypt; ///< false = nopref, true = mutual
  sqlite3_int64 gossip_timestamp;
  char *gossip_keyid;
  char *gossip_keydata;
};

/**
 * struct AutocryptPeerHistory - Autocrypt peer history
 */
struct AutocryptPeerHistory
{
  char *peer_email_addr;
  char *email_msgid;
  sqlite3_int64 timestamp;
  char *keydata;
};

/**
 * struct AutocryptGossipHistory - Autocrypt gossip history
 */
struct AutocryptGossipHistory
{
  char *peer_email_addr;
  char *sender_email_addr;
  char *email_msgid;
  sqlite3_int64 timestamp;
  char *gossip_keydata;
};

/**
 * enum AutocryptRec - Recommendation
 */
enum AutocryptRec
{
  AUTOCRYPT_REC_OFF,        ///< No recommendations
  AUTOCRYPT_REC_NO,         ///< Do no use Autocrypt
  AUTOCRYPT_REC_DISCOURAGE, ///< Prefer not to use Autocrypt
  AUTOCRYPT_REC_AVAILABLE,  ///< Autocrypt is available
  AUTOCRYPT_REC_YES,        ///< Autocrypt should be used
};

void              mutt_autocrypt_account_menu            (void);
void              mutt_autocrypt_cleanup                 (void);
int               mutt_autocrypt_generate_gossip_list    (struct Email *e);
int               mutt_autocrypt_init                    (bool can_create);
int               mutt_autocrypt_process_autocrypt_header(struct Email *e, struct Envelope *env);
int               mutt_autocrypt_process_gossip_header   (struct Email *e, struct Envelope *prot_headers);
int               mutt_autocrypt_set_sign_as_default_key (struct Email *e);
enum AutocryptRec mutt_autocrypt_ui_recommendation       (struct Email *e, char **keylist);
int               mutt_autocrypt_write_autocrypt_header  (struct Envelope *env, FILE *fp);
int               mutt_autocrypt_write_gossip_headers    (struct Envelope *env, FILE *fp);

#endif /* MUTT_AUTOCRYPT_LIB_H */
