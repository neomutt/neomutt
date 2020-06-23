/**
 * @file
 * Path manipulation functions
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page path Path manipulation functions
 *
 * Path manipulation functions
 */

#include "config.h"
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "path.h"
#include "buffer.h"
#include "logging.h"
#include "memory.h"
#include "message.h"
#include "string2.h"

/**
 * mutt_path_tidy_slash - Remove unnecessary slashes and dots
 * @param[in,out] buf    Path to modify
 * @param[in]     is_dir Should a trailing / be removed?
 * @retval true Success
 *
 * Collapse repeated '//' and '/./'
 */
bool mutt_path_tidy_slash(char *buf, bool is_dir)
{
  if (!buf)
    return false;

  char *r = buf;
  char *w = buf;

  while (*r != '\0')
  {
    *w++ = *r++;

    if (r[-1] == '/') /* After a '/' ... */
    {
      for (; (r[0] == '/') || (r[0] == '.'); r++)
      {
        if (r[0] == '/') /* skip multiple /s */
          continue;
        if (r[0] == '.')
        {
          if (r[1] == '/') /* skip /./ */
          {
            r++;
            continue;
          }
          else if (r[1] == '\0') /* skip /.$ */
          {
            r[0] = '\0';
          }
          break; /* dot-anything-else isn't special */
        }
      }
    }
  }

  /* Strip a trailing / as long as it's not the only character */
  if (is_dir && (w > (buf + 1)) && (w[-1] == '/'))
    w--;

  *w = '\0';

  return true;
}

/**
 * mutt_path_tidy_dotdot - Remove dot-dot-slash from a path
 * @param[in,out] buf Path to modify
 * @retval true Success
 *
 * Collapse dot-dot patterns, like '/dir/../'
 */
bool mutt_path_tidy_dotdot(char *buf)
{
  if (!buf || (buf[0] != '/'))
    return false;

  char *dd = buf;

  mutt_debug(LL_DEBUG3, "Collapse path: %s\n", buf);
  while ((dd = strstr(dd, "/..")))
  {
    if (dd[3] == '/') /* paths follow dots */
    {
      char *dest = NULL;
      if (dd != buf) /* not at start of string */
      {
        dd[0] = '\0';
        dest = strrchr(buf, '/');
      }
      if (!dest)
        dest = buf;

      memmove(dest, dd + 3, strlen(dd + 3) + 1);
    }
    else if (dd[3] == '\0') /* dots at end of string */
    {
      if (dd == buf) /* at start of string */
      {
        dd[1] = '\0';
      }
      else
      {
        dd[0] = '\0';
        char *s = strrchr(buf, '/');
        if (s == buf)
          s[1] = '\0';
        else if (s)
          s[0] = '\0';
      }
    }
    else
    {
      dd += 3; /* skip over '/..dir/' */
      continue;
    }

    dd = buf; /* restart at the beginning */
  }

  mutt_debug(LL_DEBUG3, "Collapsed to:  %s\n", buf);
  return true;
}

/**
 * mutt_path_tidy - Remove unnecessary parts of a path
 * @param[in,out] buf Path to modify
 * @param[in]     is_dir Is the path a directory?
 * @retval true Success
 *
 * Remove unnecessary dots and slashes from a path
 */
bool mutt_path_tidy(char *buf, bool is_dir)
{
  if (!buf || (buf[0] != '/'))
    return false;

  if (!mutt_path_tidy_slash(buf, is_dir))
    return false;

  return mutt_path_tidy_dotdot(buf);
}

/**
 * mutt_path_pretty - Tidy a filesystem path
 * @param buf     Path to modify
 * @param buflen  Length of the buffer
 * @param homedir Home directory for '~' substitution
 * @param is_dir  Is the path a directory?
 * @retval true Success
 *
 * Tidy a path and replace a home directory with '~'
 */
bool mutt_path_pretty(char *buf, size_t buflen, const char *homedir, bool is_dir)
{
  if (!buf)
    return false;

  mutt_path_tidy(buf, is_dir);

  size_t len = mutt_str_startswith(buf, homedir);
  if (len == 0)
    return false;

  if ((buf[len] != '/') && (buf[len] != '\0'))
    return false;

  buf[0] = '~';
  if (buf[len] == '\0')
  {
    buf[1] = '\0';
    return true;
  }

  mutt_str_copy(buf + 1, buf + len, buflen - len);
  return true;
}

/**
 * mutt_path_tilde - Expand '~' in a path
 * @param buf     Path to modify
 * @param buflen  Length of the buffer
 * @param homedir Home directory for '~' substitution
 * @retval true Success
 *
 * Behaviour:
 * - `~/dir` (~ expanded)
 * - `~realuser/dir` (~realuser expanded)
 * - `~nonuser/dir` (~nonuser not changed)
 */
bool mutt_path_tilde(char *buf, size_t buflen, const char *homedir)
{
  if (!buf || (buf[0] != '~'))
    return false;

  char result[PATH_MAX] = { 0 };
  char *dir = NULL;
  size_t len = 0;

  if ((buf[1] == '/') || (buf[1] == '\0'))
  {
    if (!homedir)
    {
      mutt_debug(LL_DEBUG3, "no homedir\n");
      return false;
    }

    len = mutt_str_copy(result, homedir, sizeof(result));
    dir = buf + 1;
  }
  else
  {
    char user[128];
    dir = strchr(buf + 1, '/');
    if (dir)
      mutt_str_copy(user, buf + 1, MIN(dir - buf, (unsigned) sizeof(user)));
    else
      mutt_str_copy(user, buf + 1, sizeof(user));

    struct passwd *pw = getpwnam(user);
    if (!pw || !pw->pw_dir)
    {
      mutt_debug(LL_DEBUG1, "no such user: %s\n", user);
      return false;
    }

    len = mutt_str_copy(result, pw->pw_dir, sizeof(result));
  }

  size_t dirlen = mutt_str_len(dir);
  if ((len + dirlen) >= buflen)
  {
    mutt_debug(LL_DEBUG3, "result too big for the buffer %ld >= %ld\n", len + dirlen, buflen);
    return false;
  }

  mutt_str_copy(result + len, dir, sizeof(result) - len);
  mutt_str_copy(buf, result, buflen);

  return true;
}

/**
 * mutt_path_canon - Create the canonical version of a path
 * @param buf     Path to modify
 * @param buflen  Length of the buffer
 * @param homedir Home directory for '~' substitution
 * @param is_dir  Is the path a directory?
 * @retval true Success
 *
 * Remove unnecessary dots and slashes from a path and expand '~'.
 */
bool mutt_path_canon(char *buf, size_t buflen, const char *homedir, bool is_dir)
{
  if (!buf)
    return false;

  char result[PATH_MAX] = { 0 };

  if (buf[0] == '~')
  {
    mutt_path_tilde(buf, buflen, homedir);
  }
  else if (buf[0] != '/')
  {
    if (!getcwd(result, sizeof(result)))
    {
      mutt_debug(LL_DEBUG1, "getcwd failed: %s (%d)\n", strerror(errno), errno);
      return false;
    }

    size_t cwdlen = mutt_str_len(result);
    size_t dirlen = mutt_str_len(buf);
    if ((cwdlen + dirlen + 1) >= buflen)
    {
      mutt_debug(LL_DEBUG3, "result too big for the buffer %ld >= %ld\n",
                 cwdlen + dirlen + 1, buflen);
      return false;
    }

    result[cwdlen] = '/';
    mutt_str_copy(result + cwdlen + 1, buf, sizeof(result) - cwdlen - 1);
    mutt_str_copy(buf, result, buflen);
  }

  if (!mutt_path_tidy(buf, is_dir))
    return false;

  return true;
}

/**
 * mutt_path_basename - Find the last component for a pathname
 * @param f String to be examined
 * @retval ptr Part of pathname after last '/' character
 */
const char *mutt_path_basename(const char *f)
{
  if (!f)
    return NULL;

  const char *p = strrchr(f, '/');
  if (p)
    return p + 1;
  return f;
}

/**
 * mutt_path_concat - Join a directory name and a filename
 * @param d     Buffer for the result
 * @param dir   Directory name
 * @param fname File name
 * @param l     Length of buffer
 * @retval ptr Destination buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
char *mutt_path_concat(char *d, const char *dir, const char *fname, size_t l)
{
  if (!d || !dir || !fname)
    return NULL;

  const char *fmt = "%s/%s";

  if ((fname[0] == '\0') || ((dir[0] != '\0') && (dir[strlen(dir) - 1] == '/')))
    fmt = "%s%s";

  snprintf(d, l, fmt, dir, fname);
  return d;
}

/**
 * mutt_path_dirname - Return a path up to, but not including, the final '/'
 * @param  path Path
 * @retval ptr  The directory containing p
 *
 * Unlike the IEEE Std 1003.1-2001 specification of dirname(3), this
 * implementation does not modify its parameter, so callers need not manually
 * copy their paths into a modifiable buffer prior to calling this function.
 *
 * @note The caller must free the returned string
 */
char *mutt_path_dirname(const char *path)
{
  if (!path)
    return NULL;

  char buf[PATH_MAX] = { 0 };
  mutt_str_copy(buf, path, sizeof(buf));
  return mutt_str_dup(dirname(buf));
}

/**
 * mutt_path_to_absolute - Convert relative filepath to an absolute path
 * @param path      Relative path
 * @param reference Absolute path that \a path is relative to
 * @retval true  Success
 * @retval false Failure
 *
 * Use POSIX functions to convert a path to absolute, relatively to another path
 *
 * @note \a path should be at least of PATH_MAX length
 */
bool mutt_path_to_absolute(char *path, const char *reference)
{
  if (!path || !reference)
    return false;

  char abs_path[PATH_MAX];
  int path_len;

  /* if path is already absolute, don't do anything */
  if ((strlen(path) > 1) && (path[0] == '/'))
  {
    return true;
  }

  char *dirpath = mutt_path_dirname(reference);
  mutt_str_copy(abs_path, dirpath, sizeof(abs_path));
  FREE(&dirpath);
  mutt_strn_cat(abs_path, sizeof(abs_path), "/", 1); /* append a / at the end of the path */

  path_len = sizeof(abs_path) - strlen(path);

  mutt_strn_cat(abs_path, sizeof(abs_path), path, (path_len > 0) ? path_len : 0);

  path = realpath(abs_path, path);
  if (!path && (errno != ENOENT))
  {
    mutt_perror(_("Error: converting path to absolute"));
    return false;
  }

  return true;
}

/**
 * mutt_path_realpath - resolve path, unraveling symlinks
 * @param buf Buffer containing path
 * @retval num String length of resolved path
 * @retval 0   Error, buf is not overwritten
 *
 * Resolve and overwrite the path in buf.
 *
 * @note Size of buf should be at least PATH_MAX bytes.
 */
size_t mutt_path_realpath(char *buf)
{
  if (!buf)
    return 0;

  char s[PATH_MAX];

  if (!realpath(buf, s))
    return 0;

  return mutt_str_copy(buf, s, sizeof(s));
}

/**
 * mutt_path_parent - Find the parent of a path
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @retval true  Success
 */
bool mutt_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return false;

  int n = mutt_str_len(buf);
  if (n < 2)
    return false;

  if (buf[n - 1] == '/')
    n--;

  // Find the previous '/'
  for (n--; ((n >= 0) && (buf[n] != '/')); n--)
    ; // do nothing

  if (n == 0) // Always keep at least one '/'
    n++;

  buf[n] = '\0';
  return true;
}

/**
 * mutt_path_abbr_folder - Create a folder abbreviation
 * @param buf    Path to modify
 * @param buflen Length of buffer
 * @param folder Base path for '=' substitution
 * @retval true Path was abbreviated
 *
 * Abbreviate a path using '=' to represent the 'folder'.
 * If the folder path is passed, it won't be abbreviated to just '='
 */
bool mutt_path_abbr_folder(char *buf, size_t buflen, const char *folder)
{
  if (!buf || !folder)
    return false;

  size_t flen = mutt_str_len(folder);
  if (flen < 2)
    return false;

  if (folder[flen - 1] == '/')
    flen--;

  if (!mutt_strn_equal(buf, folder, flen))
    return false;

  if (buf[flen + 1] == '\0') // Don't abbreviate to '=/'
    return false;

  size_t rlen = mutt_str_len(buf + flen + 1);

  buf[0] = '=';
  memmove(buf + 1, buf + flen + 1, rlen + 1);
  return true;
}

/**
 * mutt_path_escape - Escapes single quotes in a path for a command string
 * @param src the path to escape
 * @retval ptr The escaped string
 */
char *mutt_path_escape(const char *src)
{
  if (!src)
    return NULL;

  static char dest[STR_COMMAND];
  char *destp = dest;
  int destsize = 0;

  while (*src && (destsize < sizeof(dest) - 1))
  {
    if (*src != '\'')
    {
      *destp++ = *src++;
      destsize++;
    }
    else
    {
      /* convert ' into '\'' */
      if (destsize + 4 < sizeof(dest))
      {
        *destp++ = *src++;
        *destp++ = '\\';
        *destp++ = '\'';
        *destp++ = '\'';
        destsize += 4;
      }
      else
        break;
    }
  }
  *destp = '\0';

  return dest;
}

/**
 * mutt_path_getcwd - Get the current working directory
 * @param cwd Buffer for the result
 * @retval ptr String of current working directory
 */
const char *mutt_path_getcwd(struct Buffer *cwd)
{
  if (!cwd)
    return NULL;

  mutt_buffer_alloc(cwd, PATH_MAX);
  char *retval = getcwd(cwd->data, cwd->dsize);
  while (!retval && (errno == ERANGE))
  {
    mutt_buffer_alloc(cwd, cwd->dsize + 256);
    retval = getcwd(cwd->data, cwd->dsize);
  }
  if (retval)
    mutt_buffer_fix_dptr(cwd);
  else
    mutt_buffer_reset(cwd);

  return retval;
}
