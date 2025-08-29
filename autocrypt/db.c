/**
 * @file
 * Autocrypt database handling
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Anna Figueiredo Gomes <navi@vlhl.dev>
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
 * @page autocrypt_db Autocrypt database handling
 *
 * Autocrypt database handling
 */

#include "config.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"

// Prepared SQL statements
static sqlite3_stmt *AccountGetStmt = NULL; ///< Get the matching autocrypt accounts
static sqlite3_stmt *AccountInsertStmt = NULL; ///< Insert a new autocrypt account
static sqlite3_stmt *AccountUpdateStmt = NULL; ///< Update an autocrypt account
static sqlite3_stmt *AccountDeleteStmt = NULL; ///< Delete an autocrypt account
static sqlite3_stmt *PeerGetStmt = NULL;    ///< Get the matching peer addresses
static sqlite3_stmt *PeerInsertStmt = NULL; ///< Insert a new peer address
static sqlite3_stmt *PeerUpdateStmt = NULL; ///< Update a peer address
static sqlite3_stmt *PeerHistoryInsertStmt = NULL; ///< Add to the peer history
static sqlite3_stmt *GossipHistoryInsertStmt = NULL; ///< Add to the gossip history

/// Handle to the open Autocrypt database
sqlite3 *AutocryptDB = NULL;

/**
 * autocrypt_db_create - Create an Autocrypt SQLite database
 * @param db_path Path to database file
 * @retval  0 Success
 * @retval -1 Error
 */
static int autocrypt_db_create(const char *db_path)
{
  if (sqlite3_open_v2(db_path, &AutocryptDB,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
  {
    /* L10N: autocrypt couldn't open the SQLite database.
             The %s is the full path of the database file.  */
    mutt_error(_("Unable to open autocrypt database %s"), db_path);
    return -1;
  }
  return mutt_autocrypt_schema_init();
}

/**
 * mutt_autocrypt_db_init - Initialise the Autocrypt SQLite database
 * @param can_create If true, the directory may be created
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_init(bool can_create)
{
  int rc = -1;

  if (AutocryptDB)
    return 0;

  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  const char *const c_autocrypt_dir = cs_subset_path(NeoMutt->sub, "autocrypt_dir");
  if (!c_autocrypt || !c_autocrypt_dir)
    return -1;

  struct Buffer *db_path = buf_pool_get();
  buf_concat_path(db_path, c_autocrypt_dir, "autocrypt.db");

  struct stat st = { 0 };
  if (stat(buf_string(db_path), &st) == 0)
  {
    if (sqlite3_open_v2(buf_string(db_path), &AutocryptDB, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
      /* L10N: autocrypt couldn't open the SQLite database.
               The %s is the full path of the database file.  */
      mutt_error(_("Unable to open autocrypt database %s"), buf_string(db_path));
      goto cleanup;
    }

    if (mutt_autocrypt_schema_update())
      goto cleanup;
  }
  else
  {
    if (!can_create)
      goto cleanup;
    if (autocrypt_db_create(buf_string(db_path)))
      goto cleanup;
    /* Don't abort the whole init process because account creation failed */
    mutt_autocrypt_account_init(true);
    mutt_autocrypt_scan_mailboxes();
  }

  rc = 0;

cleanup:
  buf_pool_release(&db_path);
  return rc;
}

/**
 * mutt_autocrypt_db_close - Close the Autocrypt SQLite database connection
 */
void mutt_autocrypt_db_close(void)
{
  if (!AutocryptDB)
    return;

  sqlite3_finalize(AccountGetStmt);
  AccountGetStmt = NULL;
  sqlite3_finalize(AccountInsertStmt);
  AccountInsertStmt = NULL;
  sqlite3_finalize(AccountUpdateStmt);
  AccountUpdateStmt = NULL;
  sqlite3_finalize(AccountDeleteStmt);
  AccountDeleteStmt = NULL;

  sqlite3_finalize(PeerGetStmt);
  PeerGetStmt = NULL;
  sqlite3_finalize(PeerInsertStmt);
  PeerInsertStmt = NULL;
  sqlite3_finalize(PeerUpdateStmt);
  PeerUpdateStmt = NULL;

  sqlite3_finalize(PeerHistoryInsertStmt);
  PeerHistoryInsertStmt = NULL;

  sqlite3_finalize(GossipHistoryInsertStmt);
  GossipHistoryInsertStmt = NULL;

  sqlite3_close_v2(AutocryptDB);
  AutocryptDB = NULL;
}

/**
 * mutt_autocrypt_db_normalize_addr - Normalise an Email Address
 * @param a Address to normalise
 */
void mutt_autocrypt_db_normalize_addr(struct Address *a)
{
  mutt_addr_to_local(a);
  buf_lower(a->mailbox);
  mutt_addr_to_intl(a);
}

/**
 * mutt_autocrypt_db_normalize_addrlist - Normalise a list of Email Addresses
 * @param al List of Addresses to normalise
 */
void mutt_autocrypt_db_normalize_addrlist(struct AddressList *al)
{
  mutt_addrlist_to_local(al);

  struct Address *np = NULL;
  TAILQ_FOREACH(np, al, entries)
  {
    buf_lower(np->mailbox);
  }

  mutt_addrlist_to_intl(al, NULL);
}

/**
 * copy_normalize_addr - Copy a normalised Email Address
 * @param addr Address to normalise and copy
 * @retval ptr Copy of the Address
 *
 * The autocrypt spec says email addresses should be
 * normalized to lower case and stored in idna form.
 *
 * In order to avoid visible changes to addresses in the index,
 * we make a copy of the address before lowercasing it.
 *
 * @note The return value must be freed
 */
static struct Address *copy_normalize_addr(struct Address *addr)
{
  /* NOTE: the db functions expect a single address, so in
   * this function we copy only the address passed in.
   *
   * The normalize_addrlist above is extended to work on a list
   * because of requirements in autocrypt.c */

  struct Address *norm_addr = mutt_addr_create(NULL, buf_string(addr->mailbox));
  norm_addr->is_intl = addr->is_intl;
  norm_addr->intl_checked = addr->intl_checked;

  mutt_autocrypt_db_normalize_addr(norm_addr);
  return norm_addr;
}

/**
 * strdup_column_text - Copy a string from the database
 * @param stmt  SQLite database statement
 * @param index Database row
 * @retval ptr Copy of string
 */
static char *strdup_column_text(sqlite3_stmt *stmt, int index)
{
  const char *val = (const char *) sqlite3_column_text(stmt, index);
  return mutt_str_dup(val);
}

/**
 * mutt_autocrypt_db_account_new - Create a new AutocryptAccount
 * @retval ptr New AutocryptAccount
 */
struct AutocryptAccount *mutt_autocrypt_db_account_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AutocryptAccount);
}

/**
 * mutt_autocrypt_db_account_free - Free an AutocryptAccount
 * @param ptr Account to free
 */
void mutt_autocrypt_db_account_free(struct AutocryptAccount **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AutocryptAccount *ac = *ptr;
  FREE(&ac->email_addr);
  FREE(&ac->keyid);
  FREE(&ac->keydata);
  FREE(ptr);
}

/**
 * mutt_autocrypt_db_account_get - Get Autocrypt Account data from the database
 * @param[in]  addr    Email Address to lookup
 * @param[out] account Matched account
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_get(struct Address *addr, struct AutocryptAccount **account)
{
  int rc = -1;

  struct Address *norm_addr = copy_normalize_addr(addr);
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
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(AccountGetStmt, 1, buf_string(norm_addr->mailbox), -1,
                        SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }

  int result = sqlite3_step(AccountGetStmt);
  if (result != SQLITE_ROW)
  {
    if (result == SQLITE_DONE)
      rc = 0;
    goto cleanup;
  }

  *account = mutt_autocrypt_db_account_new();
  (*account)->email_addr = strdup_column_text(AccountGetStmt, 0);
  (*account)->keyid = strdup_column_text(AccountGetStmt, 1);
  (*account)->keydata = strdup_column_text(AccountGetStmt, 2);
  (*account)->prefer_encrypt = sqlite3_column_int(AccountGetStmt, 3);
  (*account)->enabled = sqlite3_column_int(AccountGetStmt, 4);

  rc = 1;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(AccountGetStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_account_insert - Insert an Account into the Autocrypt database
 * @param addr           Email Address for the account
 * @param keyid          Autocrypt KeyID
 * @param keydata        Autocrypt key data
 * @param prefer_encrypt Whether the account prefers encryption
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_insert(struct Address *addr, const char *keyid,
                                     const char *keydata, bool prefer_encrypt)
{
  int rc = -1;

  struct Address *norm_addr = copy_normalize_addr(addr);

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
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(AccountInsertStmt, 1, buf_string(norm_addr->mailbox),
                        -1, SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }
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

  rc = 0;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(AccountInsertStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_account_update - Update Account info in the Autocrypt database
 * @param acct Autocrypt Account data
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_update(struct AutocryptAccount *acct)
{
  int rc = -1;

  if (!AccountUpdateStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "UPDATE account SET "
                           "keyid = ?, "
                           "keydata = ?, "
                           "prefer_encrypt = ?, "
                           "enabled = ? "
                           "WHERE email_addr = ?;",
                           -1, SQLITE_PREPARE_PERSISTENT, &AccountUpdateStmt, NULL) != SQLITE_OK)
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(AccountUpdateStmt, 1, acct->keyid, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(AccountUpdateStmt, 2, acct->keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(AccountUpdateStmt, 3, acct->prefer_encrypt) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_int(AccountUpdateStmt, 4, acct->enabled) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(AccountUpdateStmt, 5, acct->email_addr, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(AccountUpdateStmt) != SQLITE_DONE)
    goto cleanup;

  rc = 0;

cleanup:
  sqlite3_reset(AccountUpdateStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_account_delete - Delete an Account from the Autocrypt database
 * @param acct Account to delete
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_delete(struct AutocryptAccount *acct)
{
  int rc = -1;

  if (!AccountDeleteStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "DELETE from account "
                           "WHERE email_addr = ?;",
                           -1, SQLITE_PREPARE_PERSISTENT, &AccountDeleteStmt, NULL) != SQLITE_OK)
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(AccountDeleteStmt, 1, acct->email_addr, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(AccountDeleteStmt) != SQLITE_DONE)
    goto cleanup;

  rc = 0;

cleanup:
  sqlite3_reset(AccountDeleteStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_account_get_all - Get all accounts from an Autocrypt database
 * @param aaa Account array
 * @retval  n Success, number of Accounts
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_get_all(struct AutocryptAccountArray *aaa)
{
  if (!aaa)
    return -1;

  int rc = -1;
  sqlite3_stmt *stmt = NULL;

  /* Note, speed is not of the essence for the account management screen,
   * so we don't bother with a persistent prepared statement */
  if (sqlite3_prepare_v2(AutocryptDB,
                         "SELECT "
                         "email_addr, "
                         "keyid, "
                         "keydata, "
                         "prefer_encrypt, "
                         "enabled "
                         "FROM account "
                         "ORDER BY email_addr",
                         -1, &stmt, NULL) != SQLITE_OK)
  {
    goto cleanup;
  }

  int result = SQLITE_ERROR;
  while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    struct AutocryptAccount *ac = mutt_autocrypt_db_account_new();

    ac->email_addr = strdup_column_text(stmt, 0);
    ac->keyid = strdup_column_text(stmt, 1);
    ac->keydata = strdup_column_text(stmt, 2);
    ac->prefer_encrypt = sqlite3_column_int(stmt, 3);
    ac->enabled = sqlite3_column_int(stmt, 4);

    ARRAY_ADD(aaa, ac);
  }

  if (result == SQLITE_DONE)
  {
    rc = ARRAY_SIZE(aaa);
  }
  else
  {
    struct AutocryptAccount **pac = NULL;
    ARRAY_FOREACH(pac, aaa)
    {
      mutt_autocrypt_db_account_free(pac);
    }
    ARRAY_FREE(aaa);
  }

cleanup:
  sqlite3_finalize(stmt);
  return rc;
}

/**
 * mutt_autocrypt_db_peer_new - Create a new AutocryptPeer
 * @retval ptr New AutocryptPeer
 */
struct AutocryptPeer *mutt_autocrypt_db_peer_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AutocryptPeer);
}

/**
 * mutt_autocrypt_db_peer_free - Free an AutocryptPeer
 * @param ptr AutocryptPeer to free
 */
void mutt_autocrypt_db_peer_free(struct AutocryptPeer **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AutocryptPeer *peer = *ptr;
  FREE(&peer->email_addr);
  FREE(&peer->keyid);
  FREE(&peer->keydata);
  FREE(&peer->gossip_keyid);
  FREE(&peer->gossip_keydata);
  FREE(ptr);
}

/**
 * mutt_autocrypt_db_peer_get - Get peer info from the Autocrypt database
 * @param[in]  addr Email Address to look up
 * @param[out] peer Matching Autocrypt Peer
 * @retval  0 Success, no matches
 * @retval  1 Success, a match
 * @retval -1 Error
 */
int mutt_autocrypt_db_peer_get(struct Address *addr, struct AutocryptPeer **peer)
{
  int rc = -1;

  struct Address *norm_addr = copy_normalize_addr(addr);
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
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(PeerGetStmt, 1, buf_string(norm_addr->mailbox), -1,
                        SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }

  int result = sqlite3_step(PeerGetStmt);
  if (result != SQLITE_ROW)
  {
    if (result == SQLITE_DONE)
      rc = 0;
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

  rc = 1;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(PeerGetStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_peer_insert - Insert a peer into the Autocrypt database
 * @param addr Email Address
 * @param peer AutocryptPeer to insert
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_peer_insert(struct Address *addr, struct AutocryptPeer *peer)
{
  int rc = -1;
  struct Address *norm_addr = NULL;

  norm_addr = copy_normalize_addr(addr);

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
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(PeerInsertStmt, 1, buf_string(norm_addr->mailbox), -1,
                        SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }
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

  rc = 0;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(PeerInsertStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_peer_update - Update the peer info in an Autocrypt database
 * @param peer AutocryptPeer to update
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_peer_update(struct AutocryptPeer *peer)
{
  int rc = -1;

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
    {
      goto cleanup;
    }
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
  if (sqlite3_bind_text(PeerUpdateStmt, 9, peer->email_addr, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(PeerUpdateStmt) != SQLITE_DONE)
    goto cleanup;

  rc = 0;

cleanup:
  sqlite3_reset(PeerUpdateStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_peer_history_new - Create a new AutocryptPeerHistory
 * @retval ptr New AutocryptPeerHistory
 */
struct AutocryptPeerHistory *mutt_autocrypt_db_peer_history_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AutocryptPeerHistory);
}

/**
 * mutt_autocrypt_db_peer_history_free - Free an AutocryptPeerHistory
 * @param ptr AutocryptPeerHistory to free
 */
void mutt_autocrypt_db_peer_history_free(struct AutocryptPeerHistory **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AutocryptPeerHistory *ph = *ptr;
  FREE(&ph->peer_email_addr);
  FREE(&ph->email_msgid);
  FREE(&ph->keydata);
  FREE(ptr);
}

/**
 * mutt_autocrypt_db_peer_history_insert - Insert peer history into the Autocrypt database
 * @param addr     Email Address
 * @param peerhist Peer history to insert
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_peer_history_insert(struct Address *addr,
                                          struct AutocryptPeerHistory *peerhist)
{
  int rc = -1;

  struct Address *norm_addr = copy_normalize_addr(addr);

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
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(PeerHistoryInsertStmt, 1, buf_string(norm_addr->mailbox),
                        -1, SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }
  if (sqlite3_bind_text(PeerHistoryInsertStmt, 2, peerhist->email_msgid, -1,
                        SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }
  if (sqlite3_bind_int64(PeerHistoryInsertStmt, 3, peerhist->timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(PeerHistoryInsertStmt, 4, peerhist->keydata, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(PeerHistoryInsertStmt) != SQLITE_DONE)
    goto cleanup;

  rc = 0;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(PeerHistoryInsertStmt);
  return rc;
}

/**
 * mutt_autocrypt_db_gossip_history_new - Create a new AutocryptGossipHistory
 * @retval ptr New AutocryptGossipHistory
 */
struct AutocryptGossipHistory *mutt_autocrypt_db_gossip_history_new(void)
{
  return MUTT_MEM_CALLOC(1, struct AutocryptGossipHistory);
}

/**
 * mutt_autocrypt_db_gossip_history_free - Free an AutocryptGossipHistory
 * @param ptr AutocryptGossipHistory to free
 */
void mutt_autocrypt_db_gossip_history_free(struct AutocryptGossipHistory **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AutocryptGossipHistory *gh = *ptr;
  FREE(&gh->peer_email_addr);
  FREE(&gh->sender_email_addr);
  FREE(&gh->email_msgid);
  FREE(&gh->gossip_keydata);
  FREE(ptr);
}

/**
 * mutt_autocrypt_db_gossip_history_insert - Insert a gossip history into the Autocrypt database
 * @param addr        Email Address
 * @param gossip_hist Gossip history to insert
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_gossip_history_insert(struct Address *addr,
                                            struct AutocryptGossipHistory *gossip_hist)
{
  int rc = -1;

  struct Address *norm_addr = copy_normalize_addr(addr);

  if (!GossipHistoryInsertStmt)
  {
    if (sqlite3_prepare_v3(AutocryptDB,
                           "INSERT INTO gossip_history "
                           "(peer_email_addr, "
                           "sender_email_addr, "
                           "email_msgid, "
                           "timestamp, "
                           "gossip_keydata) "
                           "VALUES (?, ?, ?, ?, ?);",
                           -1, SQLITE_PREPARE_PERSISTENT,
                           &GossipHistoryInsertStmt, NULL) != SQLITE_OK)
    {
      goto cleanup;
    }
  }

  if (sqlite3_bind_text(GossipHistoryInsertStmt, 1, buf_string(norm_addr->mailbox),
                        -1, SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }
  if (sqlite3_bind_text(GossipHistoryInsertStmt, 2, gossip_hist->sender_email_addr,
                        -1, SQLITE_STATIC) != SQLITE_OK)
  {
    if (sqlite3_bind_text(GossipHistoryInsertStmt, 3, gossip_hist->email_msgid,
                          -1, SQLITE_STATIC) != SQLITE_OK)
    {
      goto cleanup;
    }
  }
  if (sqlite3_bind_int64(GossipHistoryInsertStmt, 4, gossip_hist->timestamp) != SQLITE_OK)
    goto cleanup;
  if (sqlite3_bind_text(GossipHistoryInsertStmt, 5, gossip_hist->gossip_keydata,
                        -1, SQLITE_STATIC) != SQLITE_OK)
  {
    goto cleanup;
  }

  if (sqlite3_step(GossipHistoryInsertStmt) != SQLITE_DONE)
    goto cleanup;

  rc = 0;

cleanup:
  mutt_addr_free(&norm_addr);
  sqlite3_reset(GossipHistoryInsertStmt);
  return rc;
}
