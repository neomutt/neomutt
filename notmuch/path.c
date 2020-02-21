/**
 * @file
 * Notmuch path manipulations
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page nm_path Notmuch path manipulations
 *
 * Notmuch path manipulations
 */

#include "config.h"
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "private.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"

extern char *HomeDir;

const char NmUrlProtocol[] = "notmuch://";
const int NmUrlProtocolLen = sizeof(NmUrlProtocol) - 1;

/**
 * qsort_urlquery_cb - Sort two UrlQuery pointers
 * @param a First UrlQuery
 * @param b Second UrlQuery
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int qsort_urlquery_cb(const void *a, const void *b)
{
  const struct UrlQuery *uq1 = *(struct UrlQuery const *const *) a;
  const struct UrlQuery *uq2 = *(struct UrlQuery const *const *) b;

  int rc = mutt_str_strcmp(uq1->name, uq2->name);
  if (rc != 0)
    return rc;

  return mutt_str_strcmp(uq1->value, uq2->value);
}

/**
 * nm_path2_canon - Canonicalise a Mailbox path - Implements MxOps::path2_canon()
 */
int nm_path2_canon(struct Path *path)
{
  int rc = -1;

  struct Buffer buf = mutt_buffer_make(256);
  struct Url *url = url_parse(path->orig);
  if (!url)
    goto done;

  if (url->scheme != U_NOTMUCH)
    goto done;

  // follow symlinks in db path
  char real[PATH_MAX] = { 0 };
  if (!realpath(url->path, real))
    goto done;
  url->path = real;

  size_t count = 0;
  struct UrlQuery *np = NULL;
  STAILQ_FOREACH(np, &url->query_strings, entries)
  {
    count++;
  }

  struct UrlQuery **all_query = mutt_mem_calloc(count, sizeof(struct UrlQuery *));
  struct UrlQuery *tmp = NULL;
  count = 0;
  STAILQ_FOREACH_SAFE(np, &url->query_strings, entries, tmp)
  {
    STAILQ_REMOVE_HEAD(&url->query_strings, entries);
    all_query[count++] = np;
  }

  qsort(all_query, count, sizeof(struct UrlQuery *), qsort_urlquery_cb);

  for (size_t i = 0; i < count; i++)
  {
    STAILQ_INSERT_TAIL(&url->query_strings, all_query[i], entries);
  }
  FREE(&all_query);

  url->host = "/";
  if (url_tobuffer(url, &buf, U_PATH) != 0)
    goto done; // LCOV_EXCL_LINE

  mutt_str_replace(&path->canon, mutt_b2s(&buf));
  path->flags |= MPATH_CANONICAL;
  rc = 0;

done:
  url_free(&url);
  mutt_buffer_dealloc(&buf);
  return rc;
}

/**
 * nm_path2_compare - Compare two Mailbox paths - Implements MxOps::path2_compare()
 *
 * **Tests**
 * - path must match, or may be absent from one, or absent from both
 * - query_strings must match in number, order, name and value
 */
int nm_path2_compare(struct Path *path1, struct Path *path2)
{
  struct Url *url1 = url_parse(path1->canon);
  struct Url *url2 = url_parse(path2->canon);

  int rc = url1->scheme - url2->scheme;
  if (rc != 0)
    goto done;

  if (url1->path && url2->path)
  {
    rc = mutt_str_strcmp(url1->path, url2->path);
    if (rc != 0)
      goto done;
  }

  const char *q1 = strchr(path1->canon, '?');
  const char *q2 = strchr(path2->canon, '?');

  rc = mutt_str_strcmp(q1, q2);

done:
  url_free(&url1);
  url_free(&url2);
  if (rc < 0)
    return -1;
  if (rc > 0)
    return 1;
  return 0;
}

/**
 * nm_path2_parent - Find the parent of a Mailbox path - Implements MxOps::path2_parent()
 * @retval -1 Notmuch mailbox doesn't have a parent
 */
int nm_path2_parent(const struct Path *path, struct Path **parent)
{
  return -1;
}

/**
 * nm_path2_pretty - Abbreviate a Mailbox path - Implements MxOps::path2_pretty()
 */
int nm_path2_pretty(struct Path *path, const char *folder)
{
  int rc = 0;
  struct Url *url1 = url_parse(path->orig);
  struct Url *url2 = url_parse(folder);
  struct Buffer buf = mutt_buffer_make(256);

  if (url1->scheme != url2->scheme)
    goto done;

  if (mutt_str_strcmp(url1->path, url2->path) != 0)
    goto done;

  url1->path = "//";
  if (url_tobuffer(url1, &buf, 0) != 0)
    goto done; // LCOV_EXCL_LINE

  mutt_str_replace(&path->pretty, mutt_b2s(&buf));
  rc = 1;

done:
  url_free(&url1);
  url_free(&url2);
  mutt_buffer_dealloc(&buf);
  return rc;
}

/**
 * nm_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe()
 *
 * **Tests**
 * - Path must begin "notmuch://"
 * - Database path must exist
 * - Database path must be a directory
 * - Database path must contain a subdirectory `.notmuch`
 *
 * @note The case of the URL scheme is ignored
 */
int nm_path2_probe(struct Path *path, const struct stat *st)
{
  int rc = -1;
  struct Url *url = url_parse(path->orig);
  if (!url)
    return -1;

  if (url->scheme != U_NOTMUCH)
    goto done;

  // We stat the dir because NeoMutt can't parse the database path itself.
  struct stat std = { 0 };
  if ((stat(url->path, &std) != 0) || !S_ISDIR(std.st_mode))
    goto done;

  char buf[PATH_MAX];
  snprintf(buf, sizeof(buf), "%s/.notmuch", url->path);
  memset(&std, 0, sizeof(std));
  if ((stat(buf, &std) != 0) || !S_ISDIR(std.st_mode))
    goto done;

  path->type = MUTT_NOTMUCH;
  rc = 0;

done:
  url_free(&url);
  return rc;
}

/**
 * nm_path2_tidy - Tidy a Mailbox path - Implements MxOps::path2_tidy()
 *
 * **Changes**
 * - Lowercase the URL scheme
 * - Tidy the database path
 */
int nm_path2_tidy(struct Path *path)
{
  int rc = -1;
  struct Buffer *buf = NULL;
  char *tidy = NULL;

  struct Url *url = url_parse(path->orig);
  if (!url)
    return -1;

  if (url->scheme != U_NOTMUCH)
    goto done;

  buf = mutt_buffer_pool_get();
  tidy = mutt_path_tidy2(url->path, true);

  url->host = "/";
  url->path = tidy;
  if (url_tobuffer(url, buf, U_PATH) < 0)
    goto done; // LCOV_EXCL_LINE

  mutt_str_replace(&path->orig, mutt_b2s(buf));
  path->flags |= MPATH_TIDY;
  rc = 0;

done:
  url_free(&url);
  FREE(&tidy);
  mutt_buffer_pool_release(&buf);
  return rc;
}
