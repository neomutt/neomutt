/**
 * @file
 * XDG Base Directory Specification handling
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

#include "xdg.h"

#include "config.h"
#include <string.h>
#include <unistd.h>
#include "mutt/buffer.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "muttlib.h"

/**
 * The XDG Base Directory Specification environment variable names as strings.
 * Use this to convert the symbolic constant to a string.
 */
const char *const XdgEnvVarNames[] = {
  [XDG_DATA_HOME] = "XDG_DATA_HOME",
  [XDG_CONFIG_HOME] = "XDG_CONFIG_HOME",
  [XDG_STATE_HOME] = "XDG_STATE_HOME",
  /* Not officially named yet
  [XDG_BIN_HOME] = "XDG_BIN_HOME",
*/
  [XDG_DATA_DIRS] = "XDG_DATA_DIRS",
  [XDG_CONFIG_DIRS] = "XDG_CONFIG_DIRS",
  [XDG_CACHE_HOME] = "XDG_CACHE_HOME",
  /* Has no default value
  [XDG_RUNTIME_DIR] = "XDG_RUNTIME_DIR",
*/
};

/**
 * The default values for the XDG environment variables according to the XDG
 * Base Directory Specification [0].
 *
 * [0] https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */
static const char *const XdgDefaults[] = {
  [XDG_DATA_HOME] = "~/.local/share",
  [XDG_CONFIG_HOME] = "~/.config",
  [XDG_STATE_HOME] = "~/.local/state",
  /* Not officially named yet
  [XDG_BIN_HOME] = "~/.local/bin",
*/
  [XDG_DATA_DIRS] = "/usr/local/share/:/usr/share/",
  [XDG_CONFIG_DIRS] = "/etc/xdg",
  [XDG_CACHE_HOME] = "~/.cache",
  // [XDG_RUNTIME_DIR] = "", // No named default path in the spec
};

/**
 * mutt_xdg_get_path - Return the XDG path
 * @param type Type of XDG variable, e.g. #XDG_CONFIG_HOME
 * @param buf  the buffer to save the path to
 * @retval <0  on error
 * @retval  1  otherwise
 * @sa mutt_xdg_get_app_path()
 *
 * Respects the environment variable and falls back to the specification
 * default if not set. The path returned is essentially the value of the
 * environment variable plus fall back handling.
 *
 * Some XDG environment variables are allowed to contain colon separated lists
 * of directories. In this case the buffer contains such a colon separated list
 * and not a single directory.
 *
 * Note: This function does not test whether the path(s) exist.
 */
int mutt_xdg_get_path(enum XdgEnvVar type, struct Buffer *buf)
{
  int ret = 0;
  mutt_buffer_reset(buf);
  const char *xdg_env = mutt_str_getenv(XdgEnvVarNames[type]);
  if (xdg_env)
  {
    // Sanity check: paths given must be absolute, otherwise they should be
    // ignored (see XDG Spec).
    char *tmp_xdg_env = mutt_str_dup(xdg_env);
    char *x = tmp_xdg_env; /* strsep() changes tmp_xdg_env, so free x instead later */
    bool first_path = true;
    char *dir = NULL;
    while ((dir = strsep(&tmp_xdg_env, ":")))
    {
      if (dir[0] == '/')
      {
        if (!first_path)
          if ((ret = mutt_buffer_addch(buf, ':')) < 0)
            break;
        if ((ret = mutt_buffer_addstr(buf, dir)) < 0)
          break;
        first_path = false;
      }
    }
    FREE(&x);
  }
  if (ret < 0)
    return ret; // some error occurred

  if (mutt_buffer_is_empty(buf))
  {
    if ((ret = mutt_buffer_strcpy(buf, XdgDefaults[type])) < 0)
      return ret;
    // Note that only our defaults have a '~' to expand and that the defaults
    // are never lists of directories. Thus, this call really expands all paths
    // (i.e. that one path).
    mutt_buffer_expand_path(buf);
  }
  return 1;
}

/**
 * mutt_xdg_get_app_path - Return the XDG path for this application.
 * @param type Type of XDG variable, e.g. #XDG_CONFIG_HOME
 * @param buf  the buffer to save the path to
 * @retval <0  on error
 * @retval  1  otherwise
 * @sa mutt_xdg_get_path()
 *
 * Respects the environment variable and falls back to the specification
 * default if not set. The path returned is the path for this application not
 * the value of $XDG_CONFIG_HOME, i.e. mutt_xdg_get_path(XDG_CONFIG_HOME, buf)
 * returns something like "/home/foo/.config/neomutt".
 *
 * Some XDG environment variables are allowed to contain colon separated lists
 * of directories. In this case the buffer contains such a colon separated list
 * and not a single directory.
 *
 * Note: This function does not test whether the path(s) exist.
 */
int mutt_xdg_get_app_path(enum XdgEnvVar type, struct Buffer *buf)
{
  int ret = mutt_xdg_get_path(type, buf);
  if (ret < 0)
    return ret;

  char *tmp_dirs = mutt_str_dup(mutt_buffer_string(buf));
  char *x = tmp_dirs; /* strsep() changes tmp_dirs, so free x instead later */
  mutt_buffer_reset(buf);
  bool first_path = true;
  char *dir = NULL;
  while ((dir = strsep(&tmp_dirs, ":")))
  {
    if (!first_path)
      if ((ret = mutt_buffer_addch(buf, ':')) < 0)
        break;

    const int dir_len = strlen(dir);
    const bool slash = dir[dir_len - 1] == '/';
    if ((ret = mutt_buffer_add_printf(buf, slash ? "%s%s" : "%s/%s", dir, PACKAGE)) < 0)
      break;
    first_path = false;
  }
  FREE(&x);

  return ret < 0 ? ret : 1;
}

/**
 * mutt_xdg_get_first_existing_path - Return file in an XDG lookup
 * @param type Type of XDG variable, e.g. #XDG_CONFIG_HOME
 * @param path path relative to the applications XDG directory
 * @param buf  the buffer to save the path to
 * @retval <0  on error
 * @retval 1   if a file was found in the file system
 * @retval 0   if no existing file was found
 *
 * Lookup file fpath relative to the appications XDG directory, e.g.
 *
 *    mutt_xdg_get_first_existing_path(XDG_CONFIG_HOME, "neomuttrc", buf)
 *
 * Some XDG variables are a colon separated list of directories. In this case
 * each directory is tried in order until the first time the file was
 * found. That occurrence is then returned.
 *
 * path is not allowed to be NULL.
 */
int mutt_xdg_get_first_existing_path(enum XdgEnvVar type,
                                     const char *const path, struct Buffer *buf)
{
  int ret = mutt_xdg_get_app_path(type, buf);
  if (ret < 0)
    return ret;
  char *tmp_dirs = mutt_str_dup(mutt_buffer_string(buf));
  char *x = tmp_dirs; /* strsep() changes tmp_dirs, so free x instead later */
  mutt_buffer_reset(buf);
  char *dir = NULL;
  ret = 0;
  while ((dir = strsep(&tmp_dirs, ":")))
  {
    if ((ret = mutt_buffer_printf(buf, "%s/%s", dir, path)) < 0)
      break;

    if (access(mutt_buffer_string(buf), F_OK) == 0)
    {
      ret = 1;
      break;
    }
  }
  FREE(&x);
  return ret;
}
