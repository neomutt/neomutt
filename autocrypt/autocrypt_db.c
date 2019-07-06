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
#include "mutt.h"
#include "autocrypt.h"
#include "globals.h"

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
    if (!can_create || autocrypt_db_create(mutt_b2s(db_path)))
      goto cleanup;
  }
  else
  {
    if (sqlite3_open_v2(mutt_b2s(db_path), &AutocryptDB, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
    {
      mutt_error(_("Unable to open autocrypt database %s"), mutt_b2s(db_path));
      goto cleanup;
    }
  }

  if (mutt_autocrypt_schema_update())
    goto cleanup;

  rv = 0;

cleanup:
  mutt_buffer_pool_release(&db_path);
  return rv;
}

void mutt_autocrypt_db_close(void)
{
  if (!AutocryptDB)
    return;

  /* TODO:
   * call sqlite3_finalize () for each prepared statement
   */

  sqlite3_close_v2(AutocryptDB);
  AutocryptDB = NULL;
}
