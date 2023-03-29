/**
 * @file
 * Body Caching (local copies of email bodies)
 *
 * @authors
 * Copyright (C) 2006-2007,2009,2017 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2006,2009 Rocco Rutte <pdmef@gmx.net>
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
 * @page bcache_bcache Body Cache functions
 *
 * Body Caching (Local copies of email bodies)
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "mutt_account.h"
#include "muttlib.h"

struct ConnAccount;

/**
 * struct BodyCache - Local cache of email bodies
 */
struct BodyCache
{
  char *path; ///< On-disk path to the file
};

/**
 * bcache_path - Create the cache path for a given account/mailbox
 * @param account Account info
 * @param mailbox Mailbox name
 * @param bcache  Body cache
 * @retval  0 Success
 * @retval -1 Failure
 */
static int bcache_path(struct ConnAccount *account, const char *mailbox, struct BodyCache *bcache)
{
  char host[256] = { 0 };
  struct Url url = { 0 };

  const char *const c_message_cache_dir = cs_subset_path(NeoMutt->sub, "message_cache_dir");
  if (!account || !c_message_cache_dir || !bcache)
    return -1;

  struct stat st = { 0 };
  if (!((stat(c_message_cache_dir, &st) == 0) && S_ISDIR(st.st_mode)))
  {
    mutt_error(_("Cache disabled, $message_cache_dir isn't a directory: %s"), c_message_cache_dir);
    return -1;
  }

  /* make up a Url we can turn into a string */
  mutt_account_tourl(account, &url);
  /* mutt_account_tourl() just sets up some pointers;
   * if this ever changes, we have a memleak here */
  url.path = NULL;
  if (url_tostring(&url, host, sizeof(host), U_PATH) < 0)
  {
    mutt_debug(LL_DEBUG1, "URL to string failed\n");
    return -1;
  }

  struct Buffer *path = buf_pool_get();
  struct Buffer *dst = buf_pool_get();
  mutt_encode_path(path, NONULL(mailbox));

  buf_printf(dst, "%s/%s%s", c_message_cache_dir, host, buf_string(path));
  if (*(dst->dptr - 1) != '/')
    buf_addch(dst, '/');

  mutt_debug(LL_DEBUG3, "path: '%s'\n", buf_string(dst));
  bcache->path = buf_strdup(dst);

  buf_pool_release(&path);
  buf_pool_release(&dst);
  return 0;
}

/**
 * mutt_bcache_move - Change the id of a message in the cache
 * @param bcache Body cache
 * @param id     Per-mailbox unique identifier for the message
 * @param newid  New id for the message
 * @retval  0 Success
 * @retval -1 Error
 */
static int mutt_bcache_move(struct BodyCache *bcache, const char *id, const char *newid)
{
  if (!bcache || !id || (*id == '\0') || !newid || (*newid == '\0'))
    return -1;

  struct Buffer *path = buf_pool_get();
  struct Buffer *newpath = buf_pool_get();

  buf_printf(path, "%s%s", bcache->path, id);
  buf_printf(newpath, "%s%s", bcache->path, newid);

  mutt_debug(LL_DEBUG3, "bcache: mv: '%s' '%s'\n", buf_string(path), buf_string(newpath));

  int rc = rename(buf_string(path), buf_string(newpath));
  buf_pool_release(&path);
  buf_pool_release(&newpath);
  return rc;
}

/**
 * mutt_bcache_open - Open an Email-Body Cache
 * @param account current mailbox' account (required)
 * @param mailbox path to the mailbox of the account (optional)
 * @retval NULL Failure
 *
 * The driver using it is responsible for ensuring that hierarchies are
 * separated by '/' (if it knows of such a concepts like mailboxes or
 * hierarchies)
 */
struct BodyCache *mutt_bcache_open(struct ConnAccount *account, const char *mailbox)
{
  if (!account)
    return NULL;

  struct BodyCache *bcache = mutt_mem_calloc(1, sizeof(struct BodyCache));
  if (bcache_path(account, mailbox, bcache) < 0)
  {
    mutt_bcache_close(&bcache);
    return NULL;
  }

  return bcache;
}

/**
 * mutt_bcache_close - Close an Email-Body Cache
 * @param[out] bcache Body cache
 *
 * Free all memory of bcache and finally FREE() it, too.
 */
void mutt_bcache_close(struct BodyCache **bcache)
{
  if (!bcache || !*bcache)
    return;
  FREE(&(*bcache)->path);
  FREE(bcache);
}

/**
 * mutt_bcache_get - Open a file in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval ptr  Success
 * @retval NULL Failure
 */
FILE *mutt_bcache_get(struct BodyCache *bcache, const char *id)
{
  if (!id || (*id == '\0') || !bcache)
    return NULL;

  struct Buffer *path = buf_pool_get();
  buf_addstr(path, bcache->path);
  buf_addstr(path, id);

  FILE *fp = mutt_file_fopen(buf_string(path), "r");

  mutt_debug(LL_DEBUG3, "bcache: get: '%s': %s\n", buf_string(path), fp ? "yes" : "no");

  buf_pool_release(&path);
  return fp;
}

/**
 * mutt_bcache_put - Create a file in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval ptr  Success
 * @retval NULL Failure
 *
 * The returned FILE* is in a temporary location.
 * Use mutt_bcache_commit to put it into place
 */
FILE *mutt_bcache_put(struct BodyCache *bcache, const char *id)
{
  if (!id || (*id == '\0') || !bcache)
    return NULL;

  struct Buffer *path = buf_pool_get();
  buf_printf(path, "%s%s%s", bcache->path, id, ".tmp");

  struct stat st = { 0 };
  if (stat(bcache->path, &st) == 0)
  {
    if (!S_ISDIR(st.st_mode))
    {
      mutt_error(_("Message cache isn't a directory: %s"), bcache->path);
      return NULL;
    }
  }
  else
  {
    if (mutt_file_mkdir(bcache->path, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
    {
      mutt_error(_("Can't create %s: %s"), bcache->path, strerror(errno));
      return NULL;
    }
  }

  mutt_debug(LL_DEBUG3, "bcache: put: '%s'\n", buf_string(path));

  FILE *fp = mutt_file_fopen(buf_string(path), "w+");
  buf_pool_release(&path);
  return fp;
}

/**
 * mutt_bcache_commit - Move a temporary file into the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bcache_commit(struct BodyCache *bcache, const char *id)
{
  struct Buffer *tmpid = buf_pool_get();
  buf_printf(tmpid, "%s.tmp", id);

  int rc = mutt_bcache_move(bcache, buf_string(tmpid), id);
  buf_pool_release(&tmpid);
  return rc;
}

/**
 * mutt_bcache_del - Delete a file from the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bcache_del(struct BodyCache *bcache, const char *id)
{
  if (!id || (*id == '\0') || !bcache)
    return -1;

  struct Buffer *path = buf_pool_get();
  buf_addstr(path, bcache->path);
  buf_addstr(path, id);

  mutt_debug(LL_DEBUG3, "bcache: del: '%s'\n", buf_string(path));

  int rc = unlink(buf_string(path));
  buf_pool_release(&path);
  return rc;
}

/**
 * mutt_bcache_exists - Check if a file exists in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param id     Per-mailbox unique identifier for the message
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_bcache_exists(struct BodyCache *bcache, const char *id)
{
  if (!id || (*id == '\0') || !bcache)
    return -1;

  struct Buffer *path = buf_pool_get();
  buf_addstr(path, bcache->path);
  buf_addstr(path, id);

  int rc = 0;
  struct stat st = { 0 };
  if (stat(buf_string(path), &st) < 0)
    rc = -1;
  else
    rc = (S_ISREG(st.st_mode) && (st.st_size != 0)) ? 0 : -1;

  mutt_debug(LL_DEBUG3, "bcache: exists: '%s': %s\n", buf_string(path),
             (rc == 0) ? "yes" : "no");

  buf_pool_release(&path);
  return rc;
}

/**
 * mutt_bcache_list - Find matching entries in the Body Cache
 * @param bcache Body Cache from mutt_bcache_open()
 * @param want_id Callback function called for each match
 * @param data    Data to pass to the callback function
 * @retval -1  Failure
 * @retval >=0 count of matching items
 *
 * This more or less "examines" the cache and calls a function with
 * each id it finds if given.
 *
 * The optional callback function gets the id of a message, the very same
 * body cache handle mutt_bcache_list() is called with (to, perhaps,
 * perform further operations on the bcache), and a data cookie which is
 * just passed as-is. If the return value of the callback is non-zero, the
 * listing is aborted and continued otherwise. The callback is optional
 * so that this function can be used to count the items in the cache
 * (see below for return value).
 */
int mutt_bcache_list(struct BodyCache *bcache, bcache_list_t want_id, void *data)
{
  DIR *dir = NULL;
  struct dirent *de = NULL;
  int rc = -1;

  if (!bcache || !(dir = mutt_file_opendir(bcache->path, MUTT_OPENDIR_NONE)))
    goto out;

  rc = 0;

  mutt_debug(LL_DEBUG3, "bcache: list: dir: '%s'\n", bcache->path);

  while ((de = readdir(dir)))
  {
    if (mutt_str_startswith(de->d_name, ".") || mutt_str_startswith(de->d_name, ".."))
    {
      continue;
    }

    mutt_debug(LL_DEBUG3, "bcache: list: dir: '%s', id :'%s'\n", bcache->path, de->d_name);

    if (want_id && (want_id(de->d_name, bcache, data) != 0))
      goto out;

    rc++;
  }

out:
  if (dir)
  {
    if (closedir(dir) < 0)
      rc = -1;
  }
  mutt_debug(LL_DEBUG3, "bcache: list: did %d entries\n", rc);
  return rc;
}
