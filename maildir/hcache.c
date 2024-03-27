/**
 * @file
 * Maildir Header Cache
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page maildir_hcache Maildir Header Cache
 *
 * Maildir Header Cache
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "hcache/lib.h"
#include "progress/lib.h"
#include "edata.h"
#include "globals.h"

struct Progress;

/**
 * maildir_hcache_key - Get the header cache key for an Email
 * @param e Email
 * @retval str Header cache key string
 */
const char *maildir_hcache_key(struct Email *e)
{
  return e->path + 4;
}

/**
 * maildir_hcache_keylen - Calculate the length of the Maildir path
 * @param fn File name
 * @retval num Length in bytes
 *
 * @note This length excludes the flags, which will vary
 */
size_t maildir_hcache_keylen(const char *fn)
{
  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();
  const char *p = strrchr(fn, c_maildir_field_delimiter);
  return p ? (size_t) (p - fn) : mutt_str_len(fn);
}

/**
 * maildir_hcache_close - Close the Header Cache
 * @param ptr Header Cache
 */
void maildir_hcache_close(struct HeaderCache **ptr)
{
  hcache_close(ptr);
}

/**
 * maildir_hcache_delete - Delete Emails from the Header Cache
 * @param hc        Header Cache
 * @param ea        Emails to delete
 * @param mbox_path Path to Mailbox
 * @param progress  Progress Bar
 * @retval enum #MxOpenReturns
 *
 * @note May be interruped by Ctrl-C (SIGINT)
 */
enum MxOpenReturns maildir_hcache_delete(struct HeaderCache *hc, struct EmailArray *ea,
                                         const char *mbox_path, struct Progress *progress)
{
  progress_set_message(progress, _("Deleting cache %s..."), mbox_path);

  int count = 0;
  struct Email **ep = NULL;
  enum MxOpenReturns rc = MX_OPEN_ABORT;

  mutt_sig_allow_interrupt(true);
  ARRAY_FOREACH(ep, ea)
  {
    if (SigInt)
    {
      SigInt = false; // LCOV_EXCL_LINE
      goto done;      // LCOV_EXCL_LINE
    }

    struct Email *e = *ep;
    struct MaildirEmailData *edata = maildir_edata_get(e);

    hcache_delete_email(hc, e->path + edata->uid_start, edata->uid_length);
    progress_update(progress, count++, -1);
  }
  rc = MX_OPEN_OK;

done:
  mutt_sig_allow_interrupt(false);
  return rc;
}

/**
 * maildir_hcache_open - Open the Header Cache
 * @param m Mailbox
 */
struct HeaderCache *maildir_hcache_open(struct Mailbox *m)
{
  if (!m)
    return NULL;

  const char *const c_header_cache = cs_subset_path(NeoMutt->sub, "header_cache");

  return hcache_open(c_header_cache, mailbox_path(m), NULL);
}

/**
 * maildir_hcache_read - Read Emails from the Header Cache
 * @param[in]     hc        Header Cache
 * @param[in]     mbox_path Path to Mailbox
 * @param[in,out] fa        Filenames to look up
 * @param[in,out] ea        Emails found in the Header Cache
 * @param[in]     progress  Progress Bar
 * @retval enum #MxOpenReturns
 *
 * For each filename in @a fa, try to find a matching Email in the Header Cache.
 * The Emails are stored in @a ea.
 *
 * @note May be interruped by Ctrl-C (SIGINT)
 */
enum MxOpenReturns maildir_hcache_read(struct HeaderCache *hc, const char *mbox_path,
                                       struct FilenameArray *fa,
                                       struct EmailArray *ea, struct Progress *progress)
{
  if (!hc || ARRAY_EMPTY(fa))
    return MX_OPEN_OK;

  enum MxOpenReturns rc = MX_OPEN_ABORT;
  progress_set_size(progress, ARRAY_SIZE(fa));
  progress_set_message(progress, _("Reading cache %s..."), mbox_path);

  struct Buffer *path_file = buf_pool_get();
  const bool c_maildir_header_cache_verify = cs_subset_bool(NeoMutt->sub, "maildir_header_cache_verify");
  struct Filename *fnp = NULL;

  mutt_sig_allow_interrupt(true);
  ARRAY_FOREACH(fnp, fa)
  {
    if (SigInt)
    {
      SigInt = false; // LCOV_EXCL_LINE
      goto done;      // LCOV_EXCL_LINE
    }

    struct stat st_lastchanged = { 0 };

    struct HCacheEntry hce = hcache_fetch_email(hc, fnp->sub_name + fnp->uid_start,
                                                fnp->uid_length, 0);
    if (!hce.email) // not in cache
      continue;

    if (c_maildir_header_cache_verify)
    {
      buf_printf(path_file, "%s/%s", mbox_path, fnp->sub_name);
      int rc_stat = stat(buf_string(path_file), &st_lastchanged);
      if ((rc_stat != 0) || (st_lastchanged.st_mtime > hce.uidvalidity))
      {
        FREE(&hce.email); // cache out-of-date
        continue;
      }
    }

    struct MaildirEmailData *edata = maildir_edata_new();
    edata->uid_start = fnp->uid_start;
    edata->uid_length = fnp->uid_length;

    hce.email->edata = edata;
    hce.email->edata_free = maildir_edata_free;
    hce.email->old = fnp->is_cur;
    hce.email->path = fnp->sub_name; // Transfer string
    fnp->sub_name = NULL;
    ARRAY_ADD(ea, hce.email); // Transfer Email
    hce.email = NULL;
    progress_update(progress, ARRAY_SIZE(ea), -1);
  }
  rc = MX_OPEN_OK;

done:
  mutt_sig_allow_interrupt(false);
  buf_pool_release(&path_file);
  return rc;
}

/**
 * maildir_hcache_store - Save Emails to the Header Cache
 * @param hc        Header Cache
 * @param ea        Emails to save
 * @param skip      Number Emails to skip in @a ea
 * @param mbox_path Path to Mailbox
 * @param progress  Progress Bar
 * @retval enum #MxOpenReturns
 *
 * @note May be interruped by Ctrl-C (SIGINT)
 */
enum MxOpenReturns maildir_hcache_store(struct HeaderCache *hc, struct EmailArray *ea,
                                        size_t skip, const char *mbox_path,
                                        struct Progress *progress)
{
  if (ARRAY_SIZE(ea) == skip)
    return MX_OPEN_OK;

  progress_set_message(progress, _("Saving cache %s..."), mbox_path);

  int count = 0;
  struct Email **ep = NULL;
  enum MxOpenReturns rc = MX_OPEN_ABORT;

  mutt_sig_allow_interrupt(true);
  ARRAY_FOREACH_FROM(ep, ea, skip)
  {
    if (SigInt)
    {
      SigInt = false; // LCOV_EXCL_LINE
      goto done;      // LCOV_EXCL_LINE
    }

    struct Email *e = *ep;
    struct MaildirEmailData *edata = maildir_edata_get(e);

    hcache_store_email(hc, e->path + edata->uid_start, edata->uid_length, e, 0);
    progress_update(progress, count++, -1);
  }
  rc = MX_OPEN_OK;

done:
  mutt_sig_allow_interrupt(false);
  return rc;
}
