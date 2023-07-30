/**
 * @file
 * String auto-completion routines
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
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
 * @page complete_complete String auto-completion routines
 *
 * String auto-completion routines
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "globals.h"
#include "muttlib.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif

struct CompletionData;

/**
 * mutt_complete - Attempt to complete a partial pathname
 * @param cd  Completion Data
 * @param buf Buffer containing pathname
 * @retval  0 Ok
 * @retval -1 No matches
 *
 * Given a partial pathname, fill in as much of the rest of the path as is
 * unique.
 */
int mutt_complete(struct CompletionData *cd, struct Buffer *buf)
{
  const char *p = NULL;
  DIR *dir = NULL;
  struct dirent *de = NULL;
  int init = 0;
  size_t len;
  struct Buffer *dirpart = NULL;
  struct Buffer *exp_dirpart = NULL;
  struct Buffer *filepart = NULL;
  struct Buffer *tmp = NULL;
#ifdef USE_IMAP
  struct Buffer *imap_path = NULL;
  int rc;
#endif

  mutt_debug(LL_DEBUG2, "completing %s\n", buf_string(buf));

#ifdef USE_NNTP
  if (OptNews)
    return nntp_complete(buf);
#endif

  const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
#ifdef USE_IMAP
  imap_path = buf_pool_get();
  /* we can use '/' as a delimiter, imap_complete rewrites it */
  char ch = buf_at(buf, 0);
  if ((ch == '=') || (ch == '+') || (ch == '!'))
  {
    if (ch == '!')
      p = NONULL(c_spool_file);
    else
      p = NONULL(c_folder);

    buf_concat_path(imap_path, p, buf_string(buf) + 1);
  }
  else
  {
    buf_copy(imap_path, buf);
  }

  if (imap_path_probe(buf_string(imap_path), NULL) == MUTT_IMAP)
  {
    rc = imap_complete(buf, buf_string(imap_path));
    buf_pool_release(&imap_path);
    return rc;
  }

  buf_pool_release(&imap_path);
#endif

  dirpart = buf_pool_get();
  exp_dirpart = buf_pool_get();
  filepart = buf_pool_get();
  tmp = buf_pool_get();

  ch = buf_at(buf, 0);
  if ((ch == '=') || (ch == '+') || (ch == '!'))
  {
    buf_addch(dirpart, ch);
    if (ch == '!')
      buf_strcpy(exp_dirpart, NONULL(c_spool_file));
    else
      buf_strcpy(exp_dirpart, NONULL(c_folder));
    p = strrchr(buf_string(buf), '/');
    if (p)
    {
      buf_concatn_path(tmp, buf_string(exp_dirpart), buf_len(exp_dirpart),
                       buf_string(buf) + 1, (size_t) (p - buf_string(buf) - 1));
      buf_copy(exp_dirpart, tmp);
      buf_substrcpy(dirpart, buf_string(buf), p + 1);
      buf_strcpy(filepart, p + 1);
    }
    else
    {
      buf_strcpy(filepart, buf_string(buf) + 1);
    }
    dir = mutt_file_opendir(buf_string(exp_dirpart), MUTT_OPENDIR_NONE);
  }
  else
  {
    p = strrchr(buf_string(buf), '/');
    if (p)
    {
      if (p == buf_string(buf)) /* absolute path */
      {
        p = buf_string(buf) + 1;
        buf_strcpy(dirpart, "/");
        buf_strcpy(filepart, p);
        dir = mutt_file_opendir(buf_string(dirpart), MUTT_OPENDIR_NONE);
      }
      else
      {
        buf_substrcpy(dirpart, buf_string(buf), p);
        buf_strcpy(filepart, p + 1);
        buf_copy(exp_dirpart, dirpart);
        buf_expand_path(exp_dirpart);
        dir = mutt_file_opendir(buf_string(exp_dirpart), MUTT_OPENDIR_NONE);
      }
    }
    else
    {
      /* no directory name, so assume current directory. */
      buf_strcpy(filepart, buf_string(buf));
      dir = mutt_file_opendir(".", MUTT_OPENDIR_NONE);
    }
  }

  if (!dir)
  {
    mutt_debug(LL_DEBUG1, "%s: %s (errno %d)\n", buf_string(exp_dirpart),
               strerror(errno), errno);
    goto cleanup;
  }

  /* special case to handle when there is no filepart yet.  find the first
   * file/directory which is not "." or ".." */
  len = buf_len(filepart);
  if (len == 0)
  {
    while ((de = readdir(dir)))
    {
      if (!mutt_str_equal(".", de->d_name) && !mutt_str_equal("..", de->d_name))
      {
        buf_strcpy(filepart, de->d_name);
        init++;
        break;
      }
    }
  }

  while ((de = readdir(dir)))
  {
    if (mutt_strn_equal(de->d_name, buf_string(filepart), len))
    {
      if (init)
      {
        char *cp = filepart->data;

        for (int i = 0; (*cp != '\0') && (de->d_name[i] != '\0'); i++, cp++)
        {
          if (*cp != de->d_name[i])
            break;
        }
        *cp = '\0';
        buf_fix_dptr(filepart);
      }
      else
      {
        struct stat st = { 0 };

        buf_strcpy(filepart, de->d_name);

        /* check to see if it is a directory */
        if (buf_is_empty(dirpart))
        {
          buf_reset(tmp);
        }
        else
        {
          buf_copy(tmp, exp_dirpart);
          buf_addch(tmp, '/');
        }
        buf_addstr(tmp, buf_string(filepart));
        if ((stat(buf_string(tmp), &st) != -1) && (st.st_mode & S_IFDIR))
          buf_addch(filepart, '/');
        init = 1;
      }
    }
  }
  closedir(dir);

  if (buf_is_empty(dirpart))
  {
    buf_copy(buf, filepart);
  }
  else
  {
    buf_copy(buf, dirpart);
    if (!mutt_str_equal("/", buf_string(dirpart)) &&
        (buf_string(dirpart)[0] != '=') && (buf_string(dirpart)[0] != '+'))
    {
      buf_addstr(buf, "/");
    }
    buf_addstr(buf, buf_string(filepart));
  }

cleanup:
  buf_pool_release(&dirpart);
  buf_pool_release(&exp_dirpart);
  buf_pool_release(&filepart);
  buf_pool_release(&tmp);

  return init ? 0 : -1;
}
