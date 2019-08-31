/**
 * @file
 * Shared constants/structs that are private to Autocrypt
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
#include <stdbool.h>

struct Address;
struct AddressList;
struct AutocryptAccount;
struct AutocryptGossipHistory;
struct AutocryptPeer;
struct AutocryptPeerHistory;
struct Buffer;

extern sqlite3 *AutocryptDB;

int                            mutt_autocrypt_account_init(bool prompt);
void                           mutt_autocrypt_scan_mailboxes(void);

int                            mutt_autocrypt_db_account_delete(struct AutocryptAccount *acct);
void                           mutt_autocrypt_db_account_free(struct AutocryptAccount **ptr);
int                            mutt_autocrypt_db_account_get(struct Address *addr, struct AutocryptAccount **account);
int                            mutt_autocrypt_db_account_get_all(struct AutocryptAccount ***accounts, int *num_accounts);
int                            mutt_autocrypt_db_account_insert(struct Address *addr, const char *keyid, const char *keydata, bool prefer_encrypt);
struct AutocryptAccount *      mutt_autocrypt_db_account_new(void);
int                            mutt_autocrypt_db_account_update(struct AutocryptAccount *acct);
void                           mutt_autocrypt_db_close(void);
void                           mutt_autocrypt_db_gossip_history_free(struct AutocryptGossipHistory **ptr);
int                            mutt_autocrypt_db_gossip_history_insert(struct Address *addr, struct AutocryptGossipHistory *gossip_hist);
struct AutocryptGossipHistory *mutt_autocrypt_db_gossip_history_new(void);
int                            mutt_autocrypt_db_init(bool can_create);
void                           mutt_autocrypt_db_normalize_addr(struct Address *a);
void                           mutt_autocrypt_db_normalize_addrlist(struct AddressList *al);
void                           mutt_autocrypt_db_peer_free(struct AutocryptPeer **ptr);
int                            mutt_autocrypt_db_peer_get(struct Address *addr, struct AutocryptPeer **peer);
void                           mutt_autocrypt_db_peer_history_free(struct AutocryptPeerHistory **ptr);
int                            mutt_autocrypt_db_peer_history_insert(struct Address *addr, struct AutocryptPeerHistory *peerhist);
struct AutocryptPeerHistory *  mutt_autocrypt_db_peer_history_new(void);
int                            mutt_autocrypt_db_peer_insert(struct Address *addr, struct AutocryptPeer *peer);
struct AutocryptPeer *         mutt_autocrypt_db_peer_new(void);
int                            mutt_autocrypt_db_peer_update(struct AutocryptPeer *peer);

int                            mutt_autocrypt_schema_init(void);
int                            mutt_autocrypt_schema_update(void);

int                            mutt_autocrypt_gpgme_create_key(struct Address *addr, struct Buffer *keyid, struct Buffer *keydata);
int                            mutt_autocrypt_gpgme_import_key(const char *keydata, struct Buffer *keyid);
int                            mutt_autocrypt_gpgme_init(void);
bool                           mutt_autocrypt_gpgme_is_valid_key(const char *keyid);
int                            mutt_autocrypt_gpgme_select_key(struct Buffer *keyid, struct Buffer *keydata);
int                            mutt_autocrypt_gpgme_select_or_create_key(struct Address *addr, struct Buffer *keyid, struct Buffer *keydata);

#endif /* MUTT_AUTOCRYPT_AUTOCRYPT_PRIVATE_H */
