/**
 * @file
 * Body Caching - local copies of email bodies
 *
 * @authors
 * Copyright (C) 2006-2007,2009 Brendan Cully <brendan@kublai.com>
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

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lib/lib.h"
#include "bcache.h"
#include "globals.h"
#include "mutt_account.h"
#include "protos.h"
#include "url.h"

/**
 * struct BodyCache - Local cache of email bodies
 */
struct BodyCache
{
  char path[_POSIX_PATH_MAX];
  size_t pathlen;
};

static int bcache_path(struct Account *account, const char *mailbox, char *dst, size_t dstlen)
{
  char host[STRING];
  char path[_POSIX_PATH_MAX];
  struct Url url;
  int len;

  if (!account || !MessageCachedir || !*MessageCachedir || !dst || !dstlen)
    return -1;

  /* make up a Url we can turn into a string */
  memset(&url, 0, sizeof(struct Url));
  mutt_account_tourl(account, &url);
  /*
   * mutt_account_tourl() just sets up some pointers;
   * if this ever changes, we have a memleak here
   */
  url.path = NULL;
  if (url_tostring(&url, host, sizeof(host), U_PATH) < 0)
  {
    mutt_debug(1, "bcache_path: URL to string failed\n");
    return -1;
  }

  mutt_encode_path(path, sizeof(path), NONULL(mailbox));

  int plen = mutt_strlen(path);
  if (plen == 0)
    return -1;

  len = snprintf(dst, dstlen - 1, "%s/%s%s%s", MessageCachedir, host, path,
                 (*path && path[plen - 1] == '/') ? "" : "/");

  mutt_debug(3, "bcache_path: rc: %d, path: '%s'\n", len, dst);

  if (len < 0 || (size_t) len >= dstlen - 1)
    return -1;

  mutt_debug(3, "bcache_path: directory: '%s'\n", dst);

  return 0;
}

static int mutt_bcache_move(struct BodyCache *bcache, const char *id, const char *newid)
{
  char path[_POSIX_PATH_MAX];
  char newpath[_POSIX_PATH_MAX];

  if (!bcache || !id || !*id || !newid || !*newid)
    return -1;

  snprintf(path, sizeof(path), "%s%s", bcache->path, id);
  snprintf(newpath, sizeof(newpath), "%s%s", bcache->path, newid);

  mutt_debug(3, "bcache: mv: '%s' '%s'\n", path, newpath);

  return rename(path, newpath);
}

struct BodyCache *mutt_bcache_open(struct Account *account, const char *mailbox)
{
  struct BodyCache *bcache = NULL;

  if (!account)
    goto bail;

  bcache = safe_calloc(1, sizeof(struct BodyCache));
  if (bcache_path(account, mailbox, bcache->path, sizeof(bcache->path)) < 0)
    goto bail;
  bcache->pathlen = mutt_strlen(bcache->path);

  return bcache;

bail:
  if (bcache)
    FREE(&bcache);
  return NULL;
}

void mutt_bcache_close(struct BodyCache **bcache)
{
  if (!bcache || !*bcache)
    return;
  FREE(bcache);
}

FILE *mutt_bcache_get(struct BodyCache *bcache, const char *id)
{
  char path[_POSIX_PATH_MAX];
  FILE *fp = NULL;

  if (!id || !*id || !bcache)
    return NULL;

  path[0] = '\0';
  safe_strncat(path, sizeof(path), bcache->path, bcache->pathlen);
  safe_strncat(path, sizeof(path), id, mutt_strlen(id));

  fp = safe_fopen(path, "r");

  mutt_debug(3, "bcache: get: '%s': %s\n", path, fp == NULL ? "no" : "yes");

  return fp;
}

FILE *mutt_bcache_put(struct BodyCache *bcache, const char *id)
{
  char path[LONG_STRING];
  struct stat sb;

  if (!id || !*id || !bcache)
    return NULL;

  if (snprintf(path, sizeof(path), "%s%s%s", bcache->path, id, ".tmp") >= sizeof(path))
  {
    mutt_error(_("Path too long: %s%s%s"), bcache->path, id, ".tmp");
    return NULL;
  }

  if (stat(bcache->path, &sb) == 0)
  {
    if (!S_ISDIR(sb.st_mode))
    {
      mutt_error(_("Message cache isn't a directory: %s."), bcache->path);
      return NULL;
    }
  }
  else
  {
    if (mutt_mkdir(bcache->path, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
    {
      mutt_error(_("Can't create %s %s"), bcache->path, strerror(errno));
      return NULL;
    }
  }

  mutt_debug(3, "bcache: put: '%s'\n", path);

  return safe_fopen(path, "w+");
}

int mutt_bcache_commit(struct BodyCache *bcache, const char *id)
{
  char tmpid[_POSIX_PATH_MAX];

  snprintf(tmpid, sizeof(tmpid), "%s.tmp", id);

  return mutt_bcache_move(bcache, tmpid, id);
}

int mutt_bcache_del(struct BodyCache *bcache, const char *id)
{
  char path[_POSIX_PATH_MAX];

  if (!id || !*id || !bcache)
    return -1;

  path[0] = '\0';
  safe_strncat(path, sizeof(path), bcache->path, bcache->pathlen);
  safe_strncat(path, sizeof(path), id, mutt_strlen(id));

  mutt_debug(3, "bcache: del: '%s'\n", path);

  return unlink(path);
}

int mutt_bcache_exists(struct BodyCache *bcache, const char *id)
{
  char path[_POSIX_PATH_MAX];
  struct stat st;
  int rc = 0;

  if (!id || !*id || !bcache)
    return -1;

  path[0] = '\0';
  safe_strncat(path, sizeof(path), bcache->path, bcache->pathlen);
  safe_strncat(path, sizeof(path), id, mutt_strlen(id));

  if (stat(path, &st) < 0)
    rc = -1;
  else
    rc = S_ISREG(st.st_mode) && st.st_size != 0 ? 0 : -1;

  mutt_debug(3, "bcache: exists: '%s': %s\n", path, rc == 0 ? "yes" : "no");

  return rc;
}

int mutt_bcache_list(struct BodyCache *bcache,
                     int (*want_id)(const char *id, struct BodyCache *bcache, void *data),
                     void *data)
{
  DIR *d = NULL;
  struct dirent *de = NULL;
  int rc = -1;

  if (!bcache || !(d = opendir(bcache->path)))
    goto out;

  rc = 0;

  mutt_debug(3, "bcache: list: dir: '%s'\n", bcache->path);

  while ((de = readdir(d)))
  {
    if ((mutt_strncmp(de->d_name, ".", 1) == 0) ||
        (mutt_strncmp(de->d_name, "..", 2) == 0))
      continue;

    mutt_debug(3, "bcache: list: dir: '%s', id :'%s'\n", bcache->path, de->d_name);

    if (want_id && want_id(de->d_name, bcache, data) != 0)
      goto out;

    rc++;
  }

out:
  if (d)
  {
    if (closedir(d) < 0)
      rc = -1;
  }
  mutt_debug(3, "bcache: list: did %d entries\n", rc);
  return rc;
}
