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
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "hcache/lib.h"
#include "edata.h"
#include "mailbox.h"

/**
 * maildir_hcache_key - Get the header cache key for an Email
 * @param e Email
 * @retval str Header cache key string
 */
static const char *maildir_hcache_key(struct Email *e)
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
static size_t maildir_hcache_keylen(const char *fn)
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
 * maildir_hcache_delete - Delete an Email from the Header Cache
 * @param hc Header Cache
 * @param e  Email to delete
 * @retval  0 Success
 * @retval -1 Error
 */
int maildir_hcache_delete(struct HeaderCache *hc, struct Email *e)
{
  if (!hc || !e)
    return 0;

  const char *key = maildir_hcache_key(e);
  size_t keylen = maildir_hcache_keylen(key);

  return hcache_delete_email(hc, key, keylen);
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
 * maildir_hcache_read - Read an Email from the Header Cache
 * @param[in] hc Header Cache
 * @param[in] e  Email to find
 * @param[in] fn Filename
 * @retval ptr Email from Header Cache
 */
struct Email *maildir_hcache_read(struct HeaderCache *hc, struct Email *e, const char *fn)
{
  if (!hc || !e)
    return NULL;

  struct stat st_lastchanged = { 0 };
  int rc = 0;

  const char *key = maildir_hcache_key(e);
  size_t keylen = maildir_hcache_keylen(key);

  struct HCacheEntry hce = hcache_fetch_email(hc, key, keylen, 0);
  if (!hce.email)
    return NULL;

  const bool c_maildir_header_cache_verify = cs_subset_bool(NeoMutt->sub, "maildir_header_cache_verify");
  if (c_maildir_header_cache_verify)
    rc = stat(fn, &st_lastchanged);

  if ((rc == 0) && (st_lastchanged.st_mtime <= hce.uidvalidity))
  {
    hce.email->edata = maildir_edata_new();
    hce.email->edata_free = maildir_edata_free;
    hce.email->old = e->old;
    hce.email->path = mutt_str_dup(e->path);
    maildir_parse_flags(hce.email, fn);
  }
  else
  {
    email_free(&hce.email);
  }

  return hce.email;
}

/**
 * maildir_hcache_store - Save an Email to the Header Cache
 * @param hc        Header Cache
 * @param e         Email to save
 * @retval  0 Success
 * @retval -1 Error
 */
int maildir_hcache_store(struct HeaderCache *hc, struct Email *e)
{
  if (!hc || !e)
    return 0;

  const char *key = maildir_hcache_key(e);
  size_t keylen = maildir_hcache_keylen(key);

  return hcache_store_email(hc, key, keylen, e, 0);
}
