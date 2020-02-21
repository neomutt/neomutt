/**
 * @file
 * Maildir path manipulations
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
 * @page maildir_path Maildir path manipulations
 *
 * Maildir path manipulations
 */

#include "config.h"
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"

extern char *HomeDir;

/**
 * mh_probe - Is this an mh Mailbox?
 * @param path Path to examine
 * @param st   stat buffer
 * @retval  0 Success, mh Mailbox
 * @retval -1 Path not recognised
 *
 * **Tests**
 * - Path must exist
 * - Path must be a directory
 * - Path must have a subdirectory, one of:
 *   - `.mh_sequences`
 *   - `.xmhcache`
 *   - `.mew_cache`
 *   - `.mew-cache`
 *   - `.sylpheed_cache`
 *   - `.overview`
 */
static int mh_probe(const char *path, const struct stat *st)
{
  if (!S_ISDIR(st->st_mode))
    return -1;

  /* `.overview` isn't an mh folder, but it allows NeoMutt to read Usenet news
   * from the spool.  */
  static const char *tests[] = {
    "%s/.mh_sequences", "%s/.xmhcache",       "%s/.mew_cache",
    "%s/.mew-cache",    "%s/.sylpheed_cache", "%s/.overview",
  };

  char tmp[PATH_MAX];
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    snprintf(tmp, sizeof(tmp), tests[i], path);
    if (access(tmp, F_OK) == 0)
      return 0;
  }

  return -1;
}

/**
 * maildir_probe - Is this a maildir Mailbox?
 *
 * **Tests**
 * - Path must exist
 * - Path must be a directory
 * - Path must have a subdirectory `cur`
 *
 * @note `dir/new` and `dir/tmp` aren't checked
 */
static int maildir_probe(const char *path, const struct stat *st)
{
  if (!S_ISDIR(st->st_mode))
    return -1;

  char cur[PATH_MAX];
  snprintf(cur, sizeof(cur), "%s/cur", path);

  struct stat stc;
  if ((stat(cur, &stc) != 0) || !S_ISDIR(stc.st_mode))
    return -1;

  return 0;
}

/**
 * maildir_path2_canon - Canonicalise a Mailbox path - Implements MxOps::path2_canon()
 */
int maildir_path2_canon(struct Path *path)
{
  if (!mutt_path_canon2(path->orig, &path->canon))
    return -1;

  path->flags |= MPATH_CANONICAL;
  return 0;
}

/**
 * maildir_path2_compare - Compare two Mailbox paths - Implements MxOps::path2_compare()
 */
int maildir_path2_compare(struct Path *path1, struct Path *path2)
{
  int rc = mutt_str_strcmp(path1->canon, path2->canon);
  if (rc < 0)
    return -1;
  if (rc > 0)
    return 1;
  return 0;
}

/**
 * maildir_path2_parent - Find the parent of a Mailbox path - Implements MxOps::path2_parent()
 */
int maildir_path2_parent(const struct Path *path, struct Path **parent)
{
  if (path->orig[1] == '\0')
    return -1;

  char *par_dir = mutt_path_dirname(path->orig);

  struct stat st = { 0 };
  stat(par_dir, &st);
  int rc;
  if (path->type == MUTT_MAILDIR)
    rc = maildir_probe(par_dir, &st);
  else
    rc = mh_probe(par_dir, &st);
  if (rc != 0)
  {
    FREE(&par_dir);
    return -1;
  }

  *parent = mutt_path_new();
  (*parent)->orig = par_dir;
  (*parent)->type = path->type;
  (*parent)->flags = MPATH_RESOLVED | MPATH_TIDY;
  return 0;
}

/**
 * maildir_path2_pretty - Abbreviate a Mailbox path - Implements MxOps::path2_pretty()
 */
int maildir_path2_pretty(struct Path *path, const char *folder)
{
  if (mutt_path2_abbr_folder(path->orig, folder, &path->pretty))
    return 1;

  if (mutt_path2_pretty(path->orig, HomeDir, &path->pretty))
    return 1;

  path->pretty = mutt_str_strdup(path->orig);
  return 0;
}

/**
 * maildir_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe()
 */
int maildir_path2_probe(struct Path *path, const struct stat *st)
{
  if (maildir_probe(path->orig, st) != 0)
    return -1;

  path->type = MUTT_MAILDIR;
  return 0;
}

/**
 * maildir_path2_tidy - Tidy a Mailbox path - Implements MxOps::path2_tidy()
 */
int maildir_path2_tidy(struct Path *path)
{
  char *tidy = mutt_path_tidy2(path->orig, true);
  if (!tidy)
    return -1; // LCOV_EXCL_LINE

  FREE(&path->orig);
  path->orig = tidy;
  tidy = NULL;

  path->flags |= MPATH_TIDY;
  return 0;
}

/**
 * mh_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe()
 */
int mh_path2_probe(struct Path *path, const struct stat *st)
{
  if (mh_probe(path->orig, st) != 0)
    return -1;

  path->type = MUTT_MH;
  return 0;
}
