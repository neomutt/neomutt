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
 * @page complete String auto-completion routines
 *
 * String auto-completion routines
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/mutt.h"
#include "core/lib.h"
#include "globals.h"
#include "muttlib.h"
#include "options.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif

/**
 * mutt_complete - Attempt to complete a partial pathname
 * @param buf    Buffer containing pathname
 * @param buflen Length of buffer
 * @retval 0 if ok
 * @retval -1 if no matches
 *
 * Given a partial pathname, fill in as much of the rest of the path as is
 * unique.
 */
int mutt_complete(char *buf, size_t buflen)
{
  char *p = NULL;
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  int i, init = 0;
  size_t len;
  char dirpart[PATH_MAX], exp_dirpart[PATH_MAX];
  char filepart[PATH_MAX];
#ifdef USE_IMAP
  char imap_path[PATH_MAX];
#endif

  mutt_debug(LL_DEBUG2, "completing %s\n", buf);

#ifdef USE_NNTP
  if (OptNews)
    return nntp_complete(buf, buflen);
#endif

#ifdef USE_IMAP
  /* we can use '/' as a delimiter, imap_complete rewrites it */
  if ((*buf == '=') || (*buf == '+') || (*buf == '!'))
  {
    if (*buf == '!')
      p = NONULL(C_Spoolfile);
    else
      p = NONULL(C_Folder);

    mutt_path_concat(imap_path, p, buf + 1, sizeof(imap_path));
  }
  else
    mutt_str_strfcpy(imap_path, buf, sizeof(imap_path));

  if (imap_path_probe(imap_path, NULL) == MUTT_IMAP)
    return imap_complete(buf, buflen, imap_path);
#endif

  if ((*buf == '=') || (*buf == '+') || (*buf == '!'))
  {
    dirpart[0] = *buf;
    dirpart[1] = '\0';
    if (*buf == '!')
      mutt_str_strfcpy(exp_dirpart, C_Spoolfile, sizeof(exp_dirpart));
    else
      mutt_str_strfcpy(exp_dirpart, C_Folder, sizeof(exp_dirpart));
    p = strrchr(buf, '/');
    if (p)
    {
      char tmp[PATH_MAX];
      if (!mutt_path_concatn(tmp, sizeof(tmp), exp_dirpart, strlen(exp_dirpart),
                             buf + 1, (size_t)(p - buf - 1)))
      {
        return -1;
      }
      mutt_str_strfcpy(exp_dirpart, tmp, sizeof(exp_dirpart));
      mutt_str_substr_copy(buf, p + 1, dirpart, sizeof(dirpart));
      mutt_str_strfcpy(filepart, p + 1, sizeof(filepart));
    }
    else
      mutt_str_strfcpy(filepart, buf + 1, sizeof(filepart));
    dirp = opendir(exp_dirpart);
  }
  else
  {
    p = strrchr(buf, '/');
    if (p)
    {
      if (p == buf) /* absolute path */
      {
        p = buf + 1;
        mutt_str_strfcpy(dirpart, "/", sizeof(dirpart));
        exp_dirpart[0] = '\0';
        mutt_str_strfcpy(filepart, p, sizeof(filepart));
        dirp = opendir(dirpart);
      }
      else
      {
        mutt_str_substr_copy(buf, p, dirpart, sizeof(dirpart));
        mutt_str_strfcpy(filepart, p + 1, sizeof(filepart));
        mutt_str_strfcpy(exp_dirpart, dirpart, sizeof(exp_dirpart));
        mutt_expand_path(exp_dirpart, sizeof(exp_dirpart));
        dirp = opendir(exp_dirpart);
      }
    }
    else
    {
      /* no directory name, so assume current directory. */
      dirpart[0] = '\0';
      mutt_str_strfcpy(filepart, buf, sizeof(filepart));
      dirp = opendir(".");
    }
  }

  if (!dirp)
  {
    mutt_debug(LL_DEBUG1, "%s: %s (errno %d)\n", exp_dirpart, strerror(errno), errno);
    return -1;
  }

  /* special case to handle when there is no filepart yet.  find the first
   * file/directory which is not "." or ".." */
  len = mutt_str_strlen(filepart);
  if (len == 0)
  {
    while ((de = readdir(dirp)))
    {
      if ((mutt_str_strcmp(".", de->d_name) != 0) &&
          (mutt_str_strcmp("..", de->d_name) != 0))
      {
        mutt_str_strfcpy(filepart, de->d_name, sizeof(filepart));
        init++;
        break;
      }
    }
  }

  while ((de = readdir(dirp)))
  {
    if (mutt_str_strncmp(de->d_name, filepart, len) == 0)
    {
      if (init)
      {
        for (i = 0; filepart[i] && de->d_name[i]; i++)
        {
          if (filepart[i] != de->d_name[i])
            break;
        }
        filepart[i] = '\0';
      }
      else
      {
        char tmp[PATH_MAX];
        struct stat st;

        mutt_str_strfcpy(filepart, de->d_name, sizeof(filepart));

        /* check to see if it is a directory */
        if (dirpart[0] != '\0')
        {
          mutt_str_strfcpy(tmp, exp_dirpart, sizeof(tmp));
          mutt_str_strfcpy(tmp + strlen(tmp), "/", sizeof(tmp) - strlen(tmp));
        }
        else
          tmp[0] = '\0';
        mutt_str_strfcpy(tmp + strlen(tmp), filepart, sizeof(tmp) - strlen(tmp));
        if ((stat(tmp, &st) != -1) && (st.st_mode & S_IFDIR))
        {
          mutt_str_strfcpy(filepart + strlen(filepart), "/",
                           sizeof(filepart) - strlen(filepart));
        }
        init = 1;
      }
    }
  }
  closedir(dirp);

  if (dirpart[0] != '\0')
  {
    mutt_str_strfcpy(buf, dirpart, buflen);
    if ((mutt_str_strcmp("/", dirpart) != 0) && (dirpart[0] != '=') && (dirpart[0] != '+'))
      mutt_str_strfcpy(buf + strlen(buf), "/", buflen - strlen(buf));
    mutt_str_strfcpy(buf + strlen(buf), filepart, buflen - strlen(buf));
  }
  else
    mutt_str_strfcpy(buf, filepart, buflen);

  return init ? 0 : -1;
}
