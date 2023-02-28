/**
 * @file
 * Notmuch database handling
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page nm_db Notmuch database handling
 *
 * Notmuch database handling
 */

#include "config.h"
#include <limits.h>
#include <notmuch.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "adata.h"
#include "mdata.h"
#include "mutt_logging.h"

/**
 * nm_db_get_filename - Get the filename of the Notmuch database
 * @param m Mailbox
 * @retval ptr Filename
 *
 * @note The return value is a pointer into the `$nm_default_url` global variable.
 *       If that variable changes, the result will be invalid.
 *       It must not be freed.
 */
const char *nm_db_get_filename(struct Mailbox *m)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  const char *db_filename = NULL;

  const char *const c_nm_default_url = cs_subset_string(NeoMutt->sub, "nm_default_url");
  if (mdata && mdata->db_url && mdata->db_url->path)
    db_filename = mdata->db_url->path;
  else
    db_filename = c_nm_default_url;

  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  if (!db_filename && !c_folder)
    return NULL;

  if (!db_filename)
    db_filename = c_folder;

  if (nm_path_probe(db_filename, NULL) == MUTT_NOTMUCH)
    db_filename += NmUrlProtocolLen;

  mutt_debug(LL_DEBUG2, "nm: db filename '%s'\n", db_filename);
  return db_filename;
}

/**
 * get_nm_config_file - Gets the configuration file
 * @retval ptr Config file path. Empty string if no config.
 * @retval NULL Config file path set to `auto`.
 */
static const char *get_nm_config_file(void)
{
  const char *config_to_use = NULL;
  const char *c_nm_config_file = cs_subset_path(NeoMutt->sub, "nm_config_file");

  // Workaround the configuration system mapping "" to NULL.
  if (c_nm_config_file == NULL)
  {
    config_to_use = "";
  }
  else if (!mutt_strn_equal(c_nm_config_file, "auto", 4))
  {
    config_to_use = c_nm_config_file;
  }

  return config_to_use;
}

/**
 * nm_db_do_open - Open a Notmuch database
 * @param filename Database filename
 * @param writable Read/write?
 * @param verbose  Show errors on failure?
 * @retval ptr Notmuch database
 */
notmuch_database_t *nm_db_do_open(const char *filename, bool writable, bool verbose)
{
  notmuch_database_t *db = NULL;
  int ct = 0;
  notmuch_status_t st = NOTMUCH_STATUS_SUCCESS;
  char *msg = NULL;

  const short c_nm_open_timeout = cs_subset_number(NeoMutt->sub, "nm_open_timeout");
  mutt_debug(LL_DEBUG1, "nm: db open '%s' %s (timeout %d)\n", filename,
             writable ? "[WRITE]" : "[READ]", c_nm_open_timeout);

  const notmuch_database_mode_t mode = writable ? NOTMUCH_DATABASE_MODE_READ_WRITE :
                                                  NOTMUCH_DATABASE_MODE_READ_ONLY;

  do
  {
#if LIBNOTMUCH_CHECK_VERSION(5, 4, 0)
    // notmuch 0.32-0.32.2 didn't bump libnotmuch version to 5.4.
    const char *config_file = get_nm_config_file();
    const char *const c_nm_config_profile = cs_subset_string(NeoMutt->sub, "nm_config_profile");

    st = notmuch_database_open_with_config(filename, mode, config_file,
                                           c_nm_config_profile, &db, &msg);

    // Attempt opening database without configuration file. Don't if the user specified no config.
    if (st == NOTMUCH_STATUS_NO_CONFIG && !mutt_str_equal(config_file, ""))
    {
      mutt_debug(LL_DEBUG1, "nm: Could not find notmuch configuration file: %s\n", config_file);
      mutt_debug(LL_DEBUG1, "nm: Attempting to open notmuch db without configuration file.\n");

      FREE(&msg);

      st = notmuch_database_open_with_config(filename, mode, "", NULL, &db, &msg);
    }
#elif LIBNOTMUCH_CHECK_VERSION(4, 2, 0)
    st = notmuch_database_open_verbose(filename, mode, &db, &msg);
#elif defined(NOTMUCH_API_3)
    st = notmuch_database_open(filename, mode, &db);
#else
    db = notmuch_database_open(filename, mode);
#endif
    if ((st == NOTMUCH_STATUS_FILE_ERROR) || db || !c_nm_open_timeout ||
        ((ct / 2) > c_nm_open_timeout))
    {
      break;
    }

    if (verbose && ct && ((ct % 2) == 0))
      mutt_error(_("Waiting for notmuch DB... (%d sec)"), ct / 2);
    mutt_date_sleep_ms(500); /* Half a second */
    ct++;
  } while (true);

  if (st != NOTMUCH_STATUS_SUCCESS)
  {
    db = NULL;
  }

  if (verbose)
  {
    if (!db)
    {
      if (msg)
      {
        mutt_error(msg);
      }
      else
      {
        mutt_error(_("Can't open notmuch database: %s: %s"), filename,
                   st ? notmuch_status_to_string(st) : _("unknown reason"));
      }
    }
    else if (ct > 1)
    {
      mutt_clear_error();
    }
  }

  FREE(&msg);

  return db;
}

/**
 * nm_db_get - Get the Notmuch database
 * @param m        Mailbox
 * @param writable Read/write?
 * @retval ptr Notmuch database
 */
notmuch_database_t *nm_db_get(struct Mailbox *m, bool writable)
{
  struct NmAccountData *adata = nm_adata_get(m);

  if (!adata)
    return NULL;

  // Use an existing open db if we have one.
  if (adata->db)
    return adata->db;

  const char *db_filename = nm_db_get_filename(m);
  if (db_filename)
    adata->db = nm_db_do_open(db_filename, writable, true);

  return adata->db;
}

/**
 * nm_db_release - Close the Notmuch database
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int nm_db_release(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);
  if (!adata || !adata->db || nm_db_is_longrun(m))
    return -1;

  mutt_debug(LL_DEBUG1, "nm: db close\n");
  nm_db_free(adata->db);
  adata->db = NULL;
  adata->longrun = false;
  return 0;
}

/**
 * nm_db_free - Decoupled way to close a Notmuch database
 * @param db Notmuch database
 */
void nm_db_free(notmuch_database_t *db)
{
#ifdef NOTMUCH_API_3
  notmuch_database_destroy(db);
#else
  notmuch_database_close(db);
#endif
}

/**
 * nm_db_trans_begin - Start a Notmuch database transaction
 * @param m Mailbox
 * @retval <0 error
 * @retval 1  new transaction started
 * @retval 0  already within transaction
 */
int nm_db_trans_begin(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);
  if (!adata || !adata->db)
    return -1;

  if (adata->trans)
    return 0;

  mutt_debug(LL_DEBUG2, "nm: db trans start\n");
  if (notmuch_database_begin_atomic(adata->db))
    return -1;
  adata->trans = true;
  return 1;
}

/**
 * nm_db_trans_end - End a database transaction
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int nm_db_trans_end(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);
  if (!adata || !adata->db)
    return -1;

  if (!adata->trans)
    return 0;

  mutt_debug(LL_DEBUG2, "nm: db trans end\n");
  adata->trans = false;
  if (notmuch_database_end_atomic(adata->db))
    return -1;

  return 0;
}

/**
 * nm_db_get_mtime - Get the database modification time
 * @param[in]  m     Mailbox
 * @param[out] mtime Save the modification time
 * @retval  0 Success (result in mtime)
 * @retval -1 Error
 *
 * Get the "mtime" (modification time) of the database file.
 * This is the time of the last update.
 */
int nm_db_get_mtime(struct Mailbox *m, time_t *mtime)
{
  if (!m || !mtime)
    return -1;

  struct stat st = { 0 };
  char path[PATH_MAX] = { 0 };
  const char *db_filename = nm_db_get_filename(m);

  mutt_debug(LL_DEBUG2, "nm: checking database mtime '%s'\n", db_filename);

  // See if the path we were given has a Xapian directory.
  // After notmuch 0.32, a .notmuch folder isn't guaranteed.
  snprintf(path, sizeof(path), "%s/xapian", db_filename);
  if (stat(path, &st) == 0)
  {
    *mtime = st.st_mtime;
    return 0;
  }

  // Otherwise, check for a .notmuch directory.
  snprintf(path, sizeof(path), "%s/.notmuch/xapian", db_filename);

  if (stat(path, &st) != 0)
    return -1;

  *mtime = st.st_mtime;
  return 0;
}

/**
 * nm_db_is_longrun - Is Notmuch in the middle of a long-running transaction
 * @param m Mailbox
 * @retval true Notmuch is in the middle of a long-running transaction
 */
bool nm_db_is_longrun(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);
  if (!adata)
    return false;

  return adata->longrun;
}

/**
 * nm_db_longrun_init - Start a long transaction
 * @param m        Mailbox
 * @param writable Read/write?
 */
void nm_db_longrun_init(struct Mailbox *m, bool writable)
{
  struct NmAccountData *adata = nm_adata_get(m);

  if (!(adata && nm_db_get(m, writable)))
    return;

  adata->longrun = true;
  mutt_debug(LL_DEBUG2, "nm: long run initialized\n");
}

/**
 * nm_db_longrun_done - Finish a long transaction
 * @param m Mailbox
 */
void nm_db_longrun_done(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);

  if (adata)
  {
    adata->longrun = false; /* to force nm_db_release() released DB */
    if (nm_db_release(m) == 0)
      mutt_debug(LL_DEBUG2, "nm: long run deinitialized\n");
    else
      adata->longrun = true;
  }
}

/**
 * nm_db_debug_check - Check if the database is open
 * @param m Mailbox
 */
void nm_db_debug_check(struct Mailbox *m)
{
  struct NmAccountData *adata = nm_adata_get(m);
  if (!adata || !adata->db)
    return;

  mutt_debug(LL_DEBUG1, "nm: ERROR: db is open, closing\n");
  nm_db_release(m);
}
