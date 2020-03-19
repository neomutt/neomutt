/**
 * @file
 * Autocrypt database schema
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
 * @page autocrypt_schema Autocrypt database schema
 *
 * Autocrypt database schema
 */

#include "config.h"
#include <stddef.h>
#include <sqlite3.h>
#include "autocrypt_private.h"
#include "mutt/lib.h"

/**
 * mutt_autocrypt_schema_init - Set up an Autocrypt database
 */
int mutt_autocrypt_schema_init(void)
{
  char *errmsg = NULL;

  const char *schema = "BEGIN TRANSACTION; "

                       "CREATE TABLE account ("
                       "email_addr text primary key not null, "
                       "keyid text, "
                       "keydata text, "
                       "prefer_encrypt int, "
                       "enabled int);"

                       "CREATE TABLE peer ("
                       "email_addr text primary key not null, "
                       "last_seen int, "
                       "autocrypt_timestamp int, "
                       "keyid text, "
                       "keydata text, "
                       "prefer_encrypt int, "
                       "gossip_timestamp int, "
                       "gossip_keyid text, "
                       "gossip_keydata text);"

                       "CREATE TABLE peer_history ("
                       "peer_email_addr text not null, "
                       "email_msgid text, "
                       "timestamp int, "
                       "keydata text);"

                       "CREATE INDEX peer_history_email "
                       "ON peer_history ("
                       "peer_email_addr);"

                       "CREATE TABLE gossip_history ("
                       "peer_email_addr text not null, "
                       "sender_email_addr text, "
                       "email_msgid text, "
                       "timestamp int, "
                       "gossip_keydata text);"

                       "CREATE INDEX gossip_history_email "
                       "ON gossip_history ("
                       "peer_email_addr);"

                       "CREATE TABLE schema ("
                       "version int);"

                       "INSERT into schema (version) values (1);"

                       "COMMIT TRANSACTION";

  if (sqlite3_exec(AutocryptDB, schema, NULL, NULL, &errmsg) != SQLITE_OK)
  {
    mutt_debug(LL_DEBUG1, "mutt_autocrypt_schema_init() returned %s\n", errmsg);
    sqlite3_free(errmsg);
    return -1;
  }
  return 0;
}

/**
 * mutt_autocrypt_schema_update - Update the version number of the Autocrypt database schema
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_autocrypt_schema_update(void)
{
  sqlite3_stmt *stmt = NULL;
  int rc = -1;

  if (sqlite3_prepare_v2(AutocryptDB, "SELECT version FROM schema;", -1, &stmt, NULL) != SQLITE_OK)
    goto cleanup;

  if (sqlite3_step(stmt) != SQLITE_ROW)
    goto cleanup;

  int version = sqlite3_column_int(stmt, 0);

  if (version > 1)
  {
    /* L10N: The autocrypt database keeps track of schema version numbers.
       This error occurs if the version number is too high.
       Presumably because this is an old version of NeoMutt and the
       database was upgraded by a future version.  */
    mutt_error(_("Autocrypt database version is too new"));
    goto cleanup;
  }

  /* TODO: schema version upgrades go here.
   * Bump one by one, each update inside a transaction. */

  rc = 0;

cleanup:
  sqlite3_finalize(stmt);
  return rc;
}
