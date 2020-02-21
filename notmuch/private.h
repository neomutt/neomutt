/**
 * @file
 * Notmuch private types
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

#ifndef MUTT_NOTMUCH_PRIVATE_H
#define MUTT_NOTMUCH_PRIVATE_H

#include <notmuch.h>
#include <stdbool.h>
#include <time.h>
#include "core/lib.h"
#include "progress.h"

struct Path;
struct stat;

/* The definition in <notmuch.h> is broken */
#define LIBNOTMUCH_CHECK_VERSION(major, minor, micro)                             \
  (LIBNOTMUCH_MAJOR_VERSION > (major) ||                                          \
   (LIBNOTMUCH_MAJOR_VERSION == (major) && LIBNOTMUCH_MINOR_VERSION > (minor)) || \
   (LIBNOTMUCH_MAJOR_VERSION == (major) &&                                        \
    LIBNOTMUCH_MINOR_VERSION == (minor) && LIBNOTMUCH_MICRO_VERSION >= (micro)))

extern const char NmUrlProtocol[];
extern const int NmUrlProtocolLen;

/**
 * struct NmAccountData - Notmuch-specific Account data - @extends Account
 */
struct NmAccountData
{
  notmuch_database_t *db;
  bool longrun : 1;    ///< A long-lived action is in progress
  bool trans : 1;      ///< Atomic transaction in progress
};

/**
 * enum NmQueryType - Notmuch Query Types
 *
 * Read whole-thread or matching messages only?
 */
enum NmQueryType
{
  NM_QUERY_TYPE_MESGS = 1, ///< Default: Messages only
  NM_QUERY_TYPE_THREADS,   ///< Whole threads
};

/**
 * struct NmMboxData - Notmuch-specific Mailbox data - @extends Mailbox
 */
struct NmMboxData
{
  struct Url *db_url;  ///< Parsed view url of the Notmuch database
  char *db_query;      ///< Previous query
  int db_limit;        ///< Maximum number of results to return
  enum NmQueryType query_type; ///< Messages or Threads

  struct Progress progress; ///< A progress bar
  int oldmsgcount;
  int ignmsgcount; ///< Ignored messages

  bool noprogress : 1;     ///< Don't show the progress bar
  bool progress_ready : 1; ///< A progress bar has been initialised
};

/**
 * struct NmEmailData - Notmuch-specific Email data - @extends Email
 */
struct NmEmailData
{
  char *folder; ///< Location of the Email
  char *oldpath;
  char *virtual_id;       ///< Unique Notmuch Id
  enum MailboxType type;  ///< Type of Mailbox the Email is in
};

notmuch_database_t *nm_db_do_open     (const char *filename, bool writable, bool verbose);
void                nm_db_free        (notmuch_database_t *db);
const char *        nm_db_get_filename(struct Mailbox *m);
int                 nm_db_get_mtime   (struct Mailbox *m, time_t *mtime);
notmuch_database_t *nm_db_get         (struct Mailbox *m, bool writable);
bool                nm_db_is_longrun  (struct Mailbox *m);
int                 nm_db_release     (struct Mailbox *m);
int                 nm_db_trans_begin (struct Mailbox *m);
int                 nm_db_trans_end   (struct Mailbox *m);

void                  nm_adata_free(void **ptr);
struct NmAccountData *nm_adata_get (struct Mailbox *m);
struct NmAccountData *nm_adata_new (void);
void                  nm_edata_free(void **ptr);
struct NmEmailData *  nm_edata_new (void);
void                  nm_mdata_free(void **ptr);
struct NmMboxData *   nm_mdata_get (struct Mailbox *m);
struct NmMboxData *   nm_mdata_new (const char *url);

#endif /* MUTT_NOTMUCH_PRIVATE_H */
