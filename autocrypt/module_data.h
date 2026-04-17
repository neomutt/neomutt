/**
 * @file
 * Autocrypt private Module data
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_AUTOCRYPT_MODULE_DATA_H
#define MUTT_AUTOCRYPT_MODULE_DATA_H

#include <sqlite3.h>

/**
 * struct AutocryptModuleData - Autocrypt private Module data
 */
struct AutocryptModuleData
{
  struct Notify         *notify;                      ///< Notifications
  struct MenuDefinition *menu_autocrypt;              ///< Autocrypt menu definition
  char                  *autocrypt_default_key;       ///< Autocrypt default key id (used for postponing messages)
  char                  *autocrypt_sign_as;           ///< Autocrypt Key id to sign as

  sqlite3               *autocrypt_db;                ///< Autocrypt database
  sqlite3_stmt          *account_delete_stmt;         ///< Delete an autocrypt account
  sqlite3_stmt          *account_get_stmt;            ///< Get the matching autocrypt accounts
  sqlite3_stmt          *account_insert_stmt;         ///< Insert a new autocrypt account
  sqlite3_stmt          *account_update_stmt;         ///< Update an autocrypt account
  sqlite3_stmt          *gossip_history_insert_stmt;  ///< Add to the gossip history
  sqlite3_stmt          *peer_get_stmt;               ///< Get the matching peer addresses
  sqlite3_stmt          *peer_history_insert_stmt;    ///< Add to the peer history
  sqlite3_stmt          *peer_insert_stmt;            ///< Insert a new peer address
  sqlite3_stmt          *peer_update_stmt;            ///< Update a peer address
};

#endif /* MUTT_AUTOCRYPT_MODULE_DATA_H */
