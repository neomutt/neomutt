/**
 * @file
 * Mbox path manipulations
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
 * @page mbox_path Mbox path manipulations
 *
 * Mbox path manipulations
 */

#include "config.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "globals.h"

extern char *HomeDir;

/**
 * mbox_path2_canon - Canonicalise a Mailbox path - Implements MxOps::path2_canon()
 */
int mbox_path2_canon(struct Path *path)
{
  if (!mutt_path_canon2(path->orig, &path->canon))
    return -1;

  path->flags |= MPATH_CANONICAL;
  return 0;
}

/**
 * mbox_path2_compare - Compare two Mailbox paths - Implements MxOps::path2_compare()
 */
int mbox_path2_compare(struct Path *path1, struct Path *path2)
{
  int rc = mutt_str_strcmp(path1->canon, path2->canon);
  if (rc < 0)
    return -1;
  if (rc > 0)
    return 1;
  return 0;
}

/**
 * mbox_path2_parent - Find the parent of a Mailbox path - Implements MxOps::path2_parent()
 * @retval -1 Mbox mailbox doesn't have a parent
 */
int mbox_path2_parent(const struct Path *path, struct Path **parent)
{
  return -1;
}

/**
 * mbox_path2_pretty - Abbreviate a Mailbox path - Implements MxOps::path2_pretty()
 */
int mbox_path2_pretty(struct Path *path, const char *folder)
{
  if (mutt_path2_abbr_folder(path->orig, folder, &path->pretty))
    return 1;

  if (mutt_path2_pretty(path->orig, HomeDir, &path->pretty))
    return 1;

  path->pretty = mutt_str_strdup(path->orig);
  return 0;
}

/**
 * mbox_path2_probe - Does this Mailbox type recognise this path? - Implements MxOps::path2_probe()
 *
 * **Tests**
 * - Path must exist
 * - Path must be a file
 * - File may be empty
 * - File may begin with "From " -- mbox format
 * - File may begin with 4 x Ctrl-A -- mmdf format
 */
int mbox_path2_probe(struct Path *path, const struct stat *st)
{
  if (S_ISDIR(st->st_mode))
    return -1;

  if (st->st_size == 0) // Accept an empty file as a valid mbox
  {
    path->type = MUTT_MBOX;
    return 0;
  }

  FILE *fp = fopen(path->orig, "r");
  if (!fp)
    return -1;

  int ch;
  while ((ch = fgetc(fp)) != EOF)
  {
    /* Some mailbox creation tools erroneously append a blank line to
     * a file before appending a mail message.  This allows neomutt to
     * detect magic for and thus open those files. */
    if ((ch != '\n') && (ch != '\r'))
    {
      ungetc(ch, fp);
      break;
    }
  }

  enum MailboxType magic = MUTT_UNKNOWN;
  char tmp[256];
  if (fgets(tmp, sizeof(tmp), fp))
  {
    if (mutt_str_startswith(tmp, "From ", CASE_MATCH))
      magic = MUTT_MBOX;
    else if (mutt_str_strcmp(tmp, MMDF_SEP) == 0)
      magic = MUTT_MMDF;
  }
  mutt_file_fclose(&fp);

  // Restore the times as the file was not really accessed.
  // Detection of "new mail" depends on those times being set correctly.
#ifdef HAVE_UTIMENSAT
  struct timespec ts[2];
  mutt_file_get_stat_timespec(&ts[0], &st, MUTT_STAT_ATIME);
  mutt_file_get_stat_timespec(&ts[1], &st, MUTT_STAT_MTIME);
  utimensat(0, path, ts, 0);
#else
  struct utimbuf times;
  times.actime = st->st_atime;
  times.modtime = st->st_mtime;
  utime(path->orig, &times);
#endif

  path->type = magic;
  if (path->type > MUTT_UNKNOWN)
    return 0;
  return -1;
}

/**
 * mbox_path2_tidy - Tidy a Mailbox path - Implements MxOps::path2_tidy()
 */
int mbox_path2_tidy(struct Path *path)
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
