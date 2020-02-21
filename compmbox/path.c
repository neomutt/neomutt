/**
 * @file
 * Compress path manipulations
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
 * @page comp_path Mbox path manipulations
 *
 * Mbox path manipulations
 */

#include "config.h"
#include <string.h>
#include <sys/stat.h>
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "globals.h"

extern char *HomeDir;

/**
 * comp_path2_canon - Canonicalise a Mailbox path - Implements MxOps::path2_canon()
 */
int comp_path2_canon(struct Path *path)
{
  if (!mutt_path_canon2(path->orig, &path->canon))
    return -1;

  path->flags |= MPATH_CANONICAL;
  return 0;
}

/**
 * comp_path2_compare - Compare two Mailbox paths - Implements MxOps::path2_compare()
 */
int comp_path2_compare(struct Path *path1, struct Path *path2)
{
  int rc = mutt_str_strcmp(path1->canon, path2->canon);
  if (rc < 0)
    return -1;
  if (rc > 0)
    return 1;
  return 0;
}

/**
 * comp_path2_parent - Find the parent of a Mailbox path - Implements MxOps::path2_parent()
 * @retval -1 Compressed mailbox doesn't have a parent
 */
int comp_path2_parent(const struct Path *path, struct Path **parent)
{
  return -1;
}

/**
 * comp_path2_pretty - Abbreviate a Mailbox path - Implements MxOps::path2_pretty()
 */
int comp_path2_pretty(struct Path *path, const char *folder)
{
  if (mutt_path2_abbr_folder(path->canon, folder, &path->pretty))
    return 1;

  if (mutt_path2_pretty(path->canon, HomeDir, &path->pretty))
    return 1;

  path->pretty = mutt_str_strdup(path->canon);
  return 0;
}

/**
 * comp_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe()
 *
 * **Tests**
 * - Path must exist
 * - Path must be a file
 * - Path must match an 'open-hook'
 */
int comp_path2_probe(struct Path *path, const struct stat *st)
{
  if (!S_ISREG(st->st_mode))
    return -1;

  if (!mutt_comp_can_read(path->orig))
    return -1;

  path->type = MUTT_COMPRESSED;
  return 0;
}

/**
 * comp_path2_tidy - Tidy a Mailbox path - Implements MxOps::path2_tidy()
 */
int comp_path2_tidy(struct Path *path)
{
  char *tidy = mutt_path_tidy2(path->orig, false);
  if (!tidy)
    return -1; // LCOV_EXCL_LINE

  FREE(&path->orig);
  path->orig = tidy;
  tidy = NULL;

  path->flags |= MPATH_TIDY;
  return 0;
}
