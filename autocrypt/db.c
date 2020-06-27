/**
 * @file
 * Autocrypt database handling
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
 * @page autocrypt_db Autocrypt database handling
 *
 * Autocrypt database handling
 */

#include "config.h"
#include <stddef.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "mutt_globals.h"
#include "autocrypt/lib.h"

/* Prepared statements */
static sqlite3_stmt *AccountGetStmt;
static sqlite3_stmt *AccountInsertStmt;
static sqlite3_stmt *AccountUpdateStmt;
static sqlite3_stmt *AccountDeleteStmt;
static sqlite3_stmt *PeerGetStmt;
static sqlite3_stmt *PeerInsertStmt;
static sqlite3_stmt *PeerUpdateStmt;
static sqlite3_stmt *PeerHistoryInsertStmt;
static sqlite3_stmt *GossipHistoryInsertStmt;

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
    /* L10N: s is the path to the database.
       For some reason sqlite3 failed to open that database file.  */
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

  if (!C_Autocrypt || !C_AutocryptDir)
    return -1;

  struct Buffer *db_path = mutt_buffer_pool_get();
  mutt_buffer_concat_path(db_path, C_AutocryptDir, "autocrypt.db");

  struct stat sb;
  if (stat(mutt_b2s(db_path), &sb) == 0)
  {
    if (sqlite3_open_v2(mutt_b2s(db_path), &AutocryptDB, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
      /* L10N: Error message if autocrypt couldn't open the SQLite database
         for some reason.  The %s is the full path of the database file.  */
      mutt_error(_("Unable to open autocrypt database %s"), mutt_b2s(db_path));
      goto cleanup;
    }

    if (mutt_autocrypt_schema_update())
      goto cleanup;
  }
  else
  {
    if (!can_create)
      goto cleanup;
    if (autocrypt_db_create(mutt_b2s(db_path)))
      goto cleanup;
    /* Don't abort the whole init process because account creation failed */
    mutt_autocrypt_account_init(true);
    mutt_autocrypt_scan_mailboxes();
  }

  rc = 0;

cleanup:
  mutt_buffer_pool_release(&db_path);
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
  mutt_str_lower(a->mailbox);
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
    mutt_str_lower(np->mailbox);
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

  struct Address *norm_addr = mutt_addr_new();
  norm_addr->mailbox = mutt_str_dup(addr->mailbox);
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
  return mutt_mem_calloc(1, sizeof(struct AutocryptAccount));
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

  if (sqlite3_bind_text(AccountGetStmt, 1, norm_addr->mailbox, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

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

  if (sqlite3_bind_text(AccountInsertStmt, 1, norm_addr->mailbox, -1, SQLITE_STATIC) != SQLITE_OK)
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
 * @param[out] accounts     List of accounts
 * @param[out] num_accounts Number of accounts
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_db_account_get_all(struct AutocryptAccount ***accounts, int *num_accounts)
{
  int rc = -1, result;
  sqlite3_stmt *stmt = NULL;
  struct AutocryptAccount **results = NULL;
  int results_len = 0, results_count = 0;

  *accounts = NULL;
  *num_accounts = 0;

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

  while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    if (results_count == results_len)
    {
      results_len += 5;
      mutt_mem_realloc(&results, results_len * sizeof(struct AutocryptAccount *));
    }

    struct AutocryptAccount *account = mutt_autocrypt_db_account_new();
    results[results_count++] = account;

    account->email_addr = strdup_column_text(stmt, 0);
    account->keyid = strdup_column_text(stmt, 1);
    account->keydata = strdup_column_text(stmt, 2);
    account->prefer_encrypt = sqlite3_column_int(stmt, 3);
    account->enabled = sqlite3_column_int(stmt, 4);
  }

  if (result == SQLITE_DONE)
  {
    *accounts = results;
    rc = *num_accounts = results_count;
  }
  else
  {
    while (results_count > 0)
      mutt_autocrypt_db_account_free(&results[--results_count]);
    FREE(&results);
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
  return mutt_mem_calloc(1, sizeof(struct AutocryptPeer));
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

  if (sqlite3_bind_text(PeerGetStmt, 1, norm_addr->mailbox, -1, SQLITE_STATIC) != SQLITE_OK)
    goto cleanup;

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

  if (sqlite3_bind_text(PeerInsertStmt, 1, norm_addr->mailbox, -1, SQLITE_STATIC) != SQLITE_OK)
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
  return mutt_mem_calloc(1, sizeof(struct AutocryptPeerHistory));
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

  if (sqlite3_bind_text(PeerHistoryInsertStmt, 1, norm_addr->mailbox, -1, SQLITE_STATIC) != SQLITE_OK)
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
  return mutt_mem_calloc(1, sizeof(struct AutocryptGossipHistory));
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

  if (sqlite3_bind_text(GossipHistoryInsertStmt, 1, norm_addr->mailbox, -1,
                        SQLITE_STATIC) != SQLITE_OK)
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
