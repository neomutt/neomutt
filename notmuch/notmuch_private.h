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

#include <notmuch.h>
#include "email/lib.h"
#include "progress.h"

#ifndef MUTT_NOTMUCH_NOTMUCH_PRIVATE_H
#define MUTT_NOTMUCH_NOTMUCH_PRIVATE_H

/**
 * struct NmAccountData - Account-specific Notmuch data - @extends Account
 */
struct NmAccountData
{
  int dummy;
};

/**
 * enum NmQueryType - Notmuch Query Types
 *
 * Read whole-thread or matching messages only?
 */
enum NmQueryType
{
  NM_QUERY_TYPE_MESGS = 1, /**< Default: Messages only */
  NM_QUERY_TYPE_THREADS    /**< Whole threads */
};

/**
 * struct NmMboxData - Mailbox-specific Notmuch data - @extends Mailbox
 */
struct NmMboxData
{
  notmuch_database_t *db;

  struct Url db_url;   /**< Parsed view url of the Notmuch database */
  char *db_url_holder; /**< The storage string used by db_url, we keep it
                        *   to be able to free db_url */
  char *db_query;      /**< Previous query */
  int db_limit;        /**< Maximum number of results to return */
  enum NmQueryType query_type; /**< Messages or Threads */

  struct Progress progress; /**< A progress bar */
  int oldmsgcount;
  int ignmsgcount; /**< Ignored messages */

  bool noprogress : 1;     /**< Don't show the progress bar */
  bool longrun : 1;        /**< A long-lived action is in progress */
  bool trans : 1;          /**< Atomic transaction in progress */
  bool progress_ready : 1; /**< A progress bar has been initialised */
};

/**
 * struct NmEmailData - Notmuch data attached to an Email - @extends Email
 */
struct NmEmailData
{
  char *folder; /**< Location of the Email */
  char *oldpath;
  char *virtual_id;       /**< Unique Notmuch Id */
  enum MailboxType magic; /**< Type of Mailbox the Email is in */
};

#endif /* MUTT_NOTMUCH_NOTMUCH_PRIVATE_H */
