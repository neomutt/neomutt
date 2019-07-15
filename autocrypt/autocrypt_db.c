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

#include "config.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "autocrypt_private.h"
#include "address/lib.h"
#include "mutt.h"
#include "autocrypt.h"
#include "globals.h"

/* Prepared statements */
static sqlite3_stmt *AccountGetStmt;
static sqlite3_stmt *AccountInsertStmt;
static sqlite3_stmt *PeerGetStmt;
static sqlite3_stmt *PeerInsertStmt;
static sqlite3_stmt *PeerUpdateStmt;
static sqlite3_stmt *PeerHistoryInsertStmt;

static int autocrypt_db_create(const char *db_path)
{
  if (sqlite3_open_v2(db_path, &AutocryptDB,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
  {
    mutt_error(_("Unable to open autocrypt database %s"), db_path);
    return -1;
  }
  return mutt_autocrypt_schema_init();
}

int mutt_autocrypt_db_init(int can_create)
{
  int rv = -1;
  struct Buffer *db_path = NULL;
  struct stat sb;

  if (AutocryptDB)
    return 0;

  if (!C_Autocrypt || !C_AutocryptDir)
    return -1;

  db_path = mutt_buffer_pool_get();
  mutt_buffer_concat_path(db_path, C_AutocryptDir, "autocrypt.db");

  if (stat(mutt_b2s(db_path), &sb))
  {
    if (!can_create)
      goto cleanup;
    if (autocrypt_db_create(mutt_b2s(db_path)))
      goto cleanup;
    /* Don't abort the whole init process because account creation failed */
    mutt_autocrypt_account_init();
  }
  else
  {
    if (sqlite3_open_v2(mutt_b2s(db_path), &AutocryptDB, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
      mutt_error(_("Unable to open autocrypt database %s"), mutt_b2s(db_path));
      goto cleanup;
    }

    if (mutt_autocrypt_schema_update())
      goto cleanup;
  }

  rv = 0;

cleanup:
  mutt_buffer_pool_release(&db_path);
  return rv;
}

void mutt_autocrypt_db_close(void)
{
  if (!AutocryptDB)
    return;

  sqlite3_finalize(AccountGetStmt);
  AccountGetStmt = NULL;
  sqlite3_finalize(AccountInsertStmt);
  AccountInsertStmt = NULL;

  sqlite3_finalize(PeerGetStmt);
  PeerGetStmt = NULL;
  sqlite3_finalize(PeerInsertStmt);
  PeerInsertStmt = NULL;
  sqlite3_finalize(PeerUpdateStmt);
  PeerUpdateStmt = NULL;

  sqlite3_finalize(PeerHistoryInsertStmt);
  PeerHistoryInsertStmt = NULL;

  sqlite3_close_v2(AutocryptDB);
  AutocryptDB = NULL;
}

/* The autocrypt spec says email addresses should be
 * normalized to lower case and stored in idna form.
 *
 * In order to avoid visible changes to addresses in the index,
 * we make a copy of the address before lowercasing it.
 *
 * The return value must be freed.
 */
static char *normalize_email_addr(struct Address *addr)
{
  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  struct Address *norm_addr = mutt_addr_copy(addr);
  mutt_addrlist_append(&al, norm_addr);

  mutt_addrlist_to_local(&al);
  mutt_str_strlower(norm_addr->mailbox);
  mutt_addrlist_to_intl(&al, NULL);

  char *email = mutt_str_strdup(TAILQ_FIRST(&al)->mailbox);
  mutt_addrlist_clear(&al);

  return email;
}

/* Helper that converts to char * and mutt_str_strdups the result */
static char *strdup_column_text(sqlite3_stmt *stmt, int index)
{
  const char *val = (const char *) sqlite3_column_text(stmt, index);
  return mutt_str_strdup(val);
}

struct AutocryptAccount *mutt_autocrypt_db_account_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AutocryptAccount));
}

void mutt_autocrypt_db_account_free(struct AutocryptAccount **account)
{
  if (!account || !*account)
    return;
  FREE(&(*account)->email_addr);
  FREE(&(*account)->keyid);
  FREE(&(*account)->keydata);
  FREE(account);
}

int mutt_autocrypt_db_account_get(struct Address *addr, struct AutocryptAccount **account)
{
  int rv = -1, result;
  char *email = NULL;

  email = normalize_email_addr(addr);
  *account = NULL;

  if (!AccountGetStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "SELECT "
                           "email_addr, "
                           "keyid, "
                           "keydata, "
                           "prefer_encrypt, "
                           "enabled "
                           "FROM account "
                           "WHERE email_addr = ?",
                           -1, SQLITE_PREPARE_PERSISTENT, &AccountGetStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_text(AccountGetStmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  result = sqlite3_step(AccountGetStmt);
  if (result != SQLITE_ROW)
  {
    if (result == SQLITE_DONE)
      rv = 0;
    goto cleanup;
  }

  *account = mutt_autocrypt_db_account_new();
  (*account)->email_addr = strdup_column_text(AccountGetStmt, 0);
  (*account)->keyid = strdup_column_text(AccountGetStmt, 1);
  (*account)->keydata = strdup_column_text(AccountGetStmt, 2);
  (*account)->prefer_encrypt = sqlite3_column_int(AccountGetStmt, 3);
  (*account)->enabled = sqlite3_column_int(AccountGetStmt, 4);

  rv = 1;

cleanup:
  FREE(&email);
  sqlite3_reset(AccountGetStmt);
  return rv;
}

int mutt_autocrypt_db_account_insert(struct Address *addr, const char *keyid,
                                     const char *keydata, int prefer_encrypt)
{
  int rv = -1;
  char *email = NULL;

  email = normalize_email_addr(addr);

  if (!AccountInsertStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "INSERT INTO account "
                           "(email_addr, "
                           "keyid, "
                           "keydata, "
                           "prefer_encrypt, "
                           "enabled) "
                           "VALUES (?, ?, ?, ?, ?);",
                           -1, SQLITE_PREPARE_PERSISTENT, &AccountInsertStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_text(AccountInsertStmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(AccountInsertStmt, 2, keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(AccountInsertStmt, 3, keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(AccountInsertStmt, 4, prefer_encrypt) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(AccountInsertStmt, 5, 1) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(AccountInsertStmt) != SQLITE_DONE)
    goto cleanup;

  rv = 0;

cleanup:
  FREE(&email);
  sqlite3_reset(AccountInsertStmt);
  return rv;
}

struct AutocryptPeer *mutt_autocrypt_db_peer_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AutocryptPeer));
}

void mutt_autocrypt_db_peer_free(struct AutocryptPeer **peer)
{
  if (!peer || !*peer)
    return;
  FREE(&(*peer)->email_addr);
  FREE(&(*peer)->keyid);
  FREE(&(*peer)->keydata);
  FREE(&(*peer)->gossip_keyid);
  FREE(&(*peer)->gossip_keydata);
  FREE(peer);
}

int mutt_autocrypt_db_peer_get(struct Address *addr, struct AutocryptPeer **peer)
{
  int rv = -1, result;
  char *email = NULL;

  email = normalize_email_addr(addr);
  *peer = NULL;

  if (!PeerGetStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "SELECT "
                           "email_addr, "
                           "last_seen, "
                           "autocrypt_timestamp, "
                           "keyid, "
                           "keydata, "
                           "prefer_encrypt, "
                           "gossip_timestamp, "
                           "gossip_keyid, "
                           "gossip_keydata "
                           "FROM peer "
                           "WHERE email_addr = ?",
                           -1, SQLITE_PREPARE_PERSISTENT, &PeerGetStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_text(PeerGetStmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  result = sqlite3_step(PeerGetStmt);
  if (result != SQLITE_ROW)
  {
    if (result == SQLITE_DONE)
      rv = 0;
    goto cleanup;
  }

  *peer = mutt_autocrypt_db_peer_new();
  (*peer)->email_addr = strdup_column_text(PeerGetStmt, 0);
  (*peer)->last_seen = sqlite3_column_int64(PeerGetStmt, 1);
  (*peer)->autocrypt_timestamp = sqlite3_column_int64(PeerGetStmt, 2);
  (*peer)->keyid = strdup_column_text(PeerGetStmt, 3);
  (*peer)->keydata = strdup_column_text(PeerGetStmt, 4);
  (*peer)->prefer_encrypt = sqlite3_column_int(PeerGetStmt, 5);
  (*peer)->gossip_timestamp = sqlite3_column_int64(PeerGetStmt, 6);
  (*peer)->gossip_keyid = strdup_column_text(PeerGetStmt, 7);
  (*peer)->gossip_keydata = strdup_column_text(PeerGetStmt, 8);

  rv = 1;

cleanup:
  FREE(&email);
  sqlite3_reset(PeerGetStmt);
  return rv;
}

int mutt_autocrypt_db_peer_insert(struct Address *addr, struct AutocryptPeer *peer)
{
  int rv = -1;
  char *email = NULL;

  email = normalize_email_addr(addr);

  if (!PeerInsertStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "INSERT INTO peer "
                           "(email_addr, "
                           "last_seen, "
                           "autocrypt_timestamp, "
                           "keyid, "
                           "keydata, "
                           "prefer_encrypt, "
                           "gossip_timestamp, "
                           "gossip_keyid, "
                           "gossip_keydata) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
                           -1, SQLITE_PREPARE_PERSISTENT, &PeerInsertStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_text(PeerInsertStmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerInsertStmt, 2, peer->last_seen) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerInsertStmt, 3, peer->autocrypt_timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerInsertStmt, 4, peer->keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerInsertStmt, 5, peer->keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(PeerInsertStmt, 6, peer->prefer_encrypt) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerInsertStmt, 7, peer->gossip_timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerInsertStmt, 8, peer->gossip_keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerInsertStmt, 9, peer->gossip_keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(PeerInsertStmt) != SQLITE_DONE)
    goto cleanup;

  rv = 0;

cleanup:
  FREE(&email);
  sqlite3_reset(PeerInsertStmt);
  return rv;
}

int mutt_autocrypt_db_peer_update(struct Address *addr, struct AutocryptPeer *peer)
{
  int rv = -1;
  char *email = NULL;

  email = normalize_email_addr(addr);

  if (!PeerUpdateStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "UPDATE peer SET "
                           "last_seen = ?, "
                           "autocrypt_timestamp = ?, "
                           "keyid = ?, "
                           "keydata = ?, "
                           "prefer_encrypt = ?, "
                           "gossip_timestamp = ?, "
                           "gossip_keyid = ?, "
                           "gossip_keydata = ? "
                           "WHERE email_addr = ?;",
                           -1, SQLITE_PREPARE_PERSISTENT, &PeerUpdateStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_int64(PeerUpdateStmt, 1, peer->last_seen) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerUpdateStmt, 2, peer->autocrypt_timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerUpdateStmt, 3, peer->keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerUpdateStmt, 4, peer->keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(PeerUpdateStmt, 5, peer->prefer_encrypt) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerUpdateStmt, 6, peer->gossip_timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerUpdateStmt, 7, peer->gossip_keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerUpdateStmt, 8, peer->gossip_keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerUpdateStmt, 9, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(PeerUpdateStmt) != SQLITE_DONE)
    goto cleanup;

  rv = 0;

cleanup:
  FREE(&email);
  sqlite3_reset(PeerUpdateStmt);
  return rv;
}

struct AutocryptPeerHistory *mutt_autocrypt_db_peer_history_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AutocryptPeerHistory));
}

void mutt_autocrypt_db_peer_history_free(struct AutocryptPeerHistory **peerhist)
{
  if (!peerhist || !*peerhist)
    return;
  FREE(&(*peerhist)->peer_email_addr);
  FREE(&(*peerhist)->email_msgid);
  FREE(&(*peerhist)->keydata);
  FREE(peerhist);
}

int mutt_autocrypt_db_peer_history_insert(struct Address *addr,
                                          struct AutocryptPeerHistory *peerhist)
{
  int rv = -1;
  char *email = NULL;

  email = normalize_email_addr(addr);

  if (!PeerHistoryInsertStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "INSERT INTO peer_history "
                           "(peer_email_addr, "
                           "email_msgid, "
                           "timestamp, "
                           "keydata) "
                           "VALUES (?, ?, ?, ?);",
                           -1, SQLITE_PREPARE_PERSISTENT,
                           &PeerHistoryInsertStmt, NULL) != SQLITE_OK)
      goto cleanup;
  }

  if (sqlite3_bind_text(PeerHistoryInsertStmt, 1, email, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerHistoryInsertStmt, 2, peerhist->email_msgid, -1,
                        SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int64(PeerHistoryInsertStmt, 3, peerhist->timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerHistoryInsertStmt, 4, peerhist->keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(PeerHistoryInsertStmt) != SQLITE_DONE)
    goto cleanup;

  rv = 0;

cleanup:
  FREE(&email);
  sqlite3_reset(PeerHistoryInsertStmt);
  return rv;
}
