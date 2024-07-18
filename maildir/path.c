/**
 * @file
 * Maildir Path handling
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
 * @page maildir_path Maildir Path handling
 *
 * Maildir Path handling
 */

#include "config.h"
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "path.h"
#include "globals.h"

// Mailbox API -----------------------------------------------------------------

/**
 * maildir_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
int maildir_path_canon(struct Buffer *path)
{
  mutt_path_canon(path, HomeDir, true);
  return 0;
}

/**
 * maildir_path_is_empty - Is the mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int maildir_path_is_empty(struct Buffer *path)
{
  DIR *dir = NULL;
  struct dirent *de = NULL;
  int rc = 1; /* assume empty until we find a message */
  char realpath[PATH_MAX] = { 0 };
  int iter = 0;

  /* Strategy here is to look for any file not beginning with a period */

  do
  {
    /* we do "cur" on the first iteration since it's more likely that we'll
     * find old messages without having to scan both subdirs */
    snprintf(realpath, sizeof(realpath), "%s/%s", buf_string(path),
             (iter == 0) ? "cur" : "new");
    dir = mutt_file_opendir(realpath, MUTT_OPENDIR_CREATE);
    if (!dir)
      return -1;
    while ((de = readdir(dir)))
    {
      if (*de->d_name != '.')
      {
        rc = 0;
        break;
      }
    }
    closedir(dir);
    iter++;
  } while (rc && iter < 2);

  return rc;
}

/**
 * maildir_path_probe - Is this a Maildir Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
enum MailboxType maildir_path_probe(const char *path, const struct stat *st)
{
  if (!st || !S_ISDIR(st->st_mode))
    return MUTT_UNKNOWN;

  static const char *subs[] = { "cur", "new" };
  char sub[PATH_MAX] = { 0 };
  struct stat st_sub = { 0 };
  for (size_t i = 0; i < mutt_array_size(subs); i++)
  {
    snprintf(sub, sizeof(sub), "%s/%s", path, subs[i]);
    if ((stat(sub, &st_sub) == 0) && S_ISDIR(st_sub.st_mode))
      return MUTT_MAILDIR;
  }

  return MUTT_UNKNOWN;
}
