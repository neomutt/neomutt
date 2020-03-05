/**
 * @file
 * Mailbox path functions
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
 * @page mx_path Mailbox path functions
 *
 * Mailbox path functions
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include "email/lib.h"
#include "alias/lib.h"
#include "globals.h"
#include "hook.h"
#include "mx.h"

/**
 * path2_tidy - Tidy a Mailbox path
 */
static int path2_tidy(struct Path *path)
{
  // Contract for MxOps::path2_tidy
  if (!path || !path->orig || (path->type <= MUTT_UNKNOWN) || !(path->flags & MPATH_RESOLVED))
    return -1;
  if (path->flags & MPATH_TIDY)
    return 0;
  if (path->canon || (path->flags & MPATH_CANONICAL))
    return -1;

  const struct MxOps *ops = mx_get_ops(path->type);
  if (!ops || !ops->path2_tidy)
    return -1;

  if (ops->path2_tidy(path) != 0)
    return -1;

  return 0;
}

/**
 * path2_resolve - Resolve special strings in a Mailbox Path
 * @param path   Path to resolve
 * @param folder Current $folder
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Find and expand some special strings found at the beginning of a Mailbox Path.
 * The strings must be followed by `/` or NUL.
 *
 * | String   | Expansion              |
 * | -------- | ---------------------- |
 * | `!!`     | Previous Mailbox       |
 * | `!`      | `$spoolfile`           |
 * | `+`      | `$folder`              |
 * | `-`      | Previous Mailbox       |
 * | `<`      | `$record`              |
 * | `=`      | `$folder`              |
 * | `>`      | `$mbox`                |
 * | `^`      | Current Mailbox        |
 * | `@alias` | Full name of `alias`   |
 *
 * @note Paths beginning with `~` will be expanded later by MxOps::path2_tidy()
 */
static int path2_resolve(struct Path *path, const char *folder)
{
  if (!path || !path->orig)
    return -1;
  if (path->flags & MPATH_RESOLVED)
    return 0;

  char buf[PATH_MAX];
  mutt_str_strfcpy(buf, path->orig, sizeof(buf));

  for (size_t i = 0; i < 3; i++)
  {
    if ((buf[0] == '!') && (buf[1] == '!'))
    {
      if (((buf[2] == '/') || (buf[2] == '\0')))
      {
        mutt_str_inline_replace(buf, sizeof(buf), 2, LastFolder->orig);
      }
    }
    else if ((buf[0] == '+') || (buf[0] == '='))
    {
      size_t folder_len = mutt_str_strlen(folder);
      if ((folder_len > 0) && (folder[folder_len - 1] != '/'))
      {
        buf[0] = '/';
        mutt_str_inline_replace(buf, sizeof(buf), 0, folder);
      }
      else
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, folder);
      }
    }
    else if ((buf[1] == '/') || (buf[1] == '\0'))
    {
      if (buf[0] == '!')
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, C_Spoolfile);
      }
      else if (buf[0] == '-')
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, LastFolder->orig);
      }
      else if (buf[0] == '<')
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, C_Record);
      }
      else if (buf[0] == '>')
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, C_Mbox);
      }
      else if (buf[0] == '^')
      {
        mutt_str_inline_replace(buf, sizeof(buf), 1, CurrentFolder->orig);
      }
    }
    else if (buf[0] == '@')
    {
      /* elm compatibility, @ expands alias to user name */
      struct AddressList *al = alias_lookup(buf + 1);
      if (TAILQ_EMPTY(al))
        break;

      struct Email *e = email_new();
      e->env = mutt_env_new();
      mutt_addrlist_copy(&e->env->from, al, false);
      mutt_addrlist_copy(&e->env->to, al, false);
      mutt_default_save(buf, sizeof(buf), e);
      email_free(&e);
      break;
    }
    else
    {
      break;
    }
  }

  mutt_str_replace(&path->orig, buf);
  path->flags |= MPATH_RESOLVED;

  return 0;
}

/**
 * mx_path2_canon - Canonicalise a Mailbox path
 */
int mx_path2_canon(struct Path *path)
{
  if (!path || !path->orig)
    return -1;
  if (path->flags & MPATH_CANONICAL)
    return 0;

  int rc = path2_tidy(path);
  if (rc < 0)
    return rc;

  const struct MxOps *ops = mx_get_ops(path->type);
  if (!ops || !ops->path2_canon)
    return -1;

  if (ops->path2_canon(path) != 0)
    return -1;

  return 0;
}

/**
 * mx_path2_compare - Compare two Mailbox paths - Wrapper for MxOps::path2_compare()
 *
 * The two Paths will be canonicalised, if necessary, before being compared.
 * @sa MxOps::path2_canon()
 */
int mx_path2_compare(struct Path *path1, struct Path *path2)
{
  if (!path1 || !path2)
    return -1;
  if (!(path1->flags & MPATH_RESOLVED) || !(path2->flags & MPATH_RESOLVED))
    return -1;
  if (path1->type != path2->type)
    return -1;
  if (mx_path2_canon(path1) != 0)
    return -1;
  if (mx_path2_canon(path2) != 0)
    return -1;

  const struct MxOps *ops = mx_get_ops(path1->type);
  if (!ops || !ops->path2_compare)
    return -1;

  return ops->path2_compare(path1, path2);
}

/**
 * mx_path2_parent - Find the parent of a Mailbox path
 * @retval -1 Error
 * @retval  0 Success, parent returned
 * @retval  1 Success, path is has no parent
 */
int mx_path2_parent(const struct Path *path, struct Path **parent)
{
  if (!path)
    return -1;
  if (!(path->flags & MPATH_RESOLVED))
    return -1;
  if (path->flags & MPATH_ROOT)
    return 1;

  const struct MxOps *ops = mx_get_ops(path->type);
  if (!ops || !ops->path2_parent)
    return -1;

  return ops->path2_parent(path, parent);
}

/**
 * mx_path2_pretty - Abbreviate a Mailbox path - Wrapper for MxOps::path2_pretty()
 *
 * @note The caller must free the returned string
 */
int mx_path2_pretty(struct Path *path, const char *folder)
{
  // Contract for MxOps::path2_pretty
  if (!path || !path->orig || (path->type <= MUTT_UNKNOWN) ||
      !(path->flags & MPATH_RESOLVED) || !folder)
  {
    return -1;
  }

  if (path->desc)
  {
    path->pretty = mutt_str_strdup(path->desc);
    return 2;
  }

  const struct MxOps *ops = mx_get_ops(path->type);
  if (!ops || !ops->path2_pretty)
    return -1;

  return ops->path2_pretty(path, folder);
}

/**
 * mx_path2_probe - Determine the Mailbox type of a path
 * @param path Path to examine
 * @retval num XXX
 */
int mx_path2_probe(struct Path *path)
{
  // Contract for MxOps::path2_probe
  if (!path || !path->orig || path->canon || (path->type > MUTT_UNKNOWN) ||
      !(path->flags & MPATH_RESOLVED))
  {
    return -1;
  }
  if (path->flags & (MPATH_TIDY | MPATH_CANONICAL))
    return 0;

  int rc = 0;

  // First, search the non-local Mailbox types
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
  {
    if ((*ops)->is_local)
      continue;
    rc = (*ops)->path2_probe(path, NULL);
    if (rc == 0)
      return 0;
  }

  struct stat st = { 0 };
  if (stat(path->orig, &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "unable to stat %s: %s (errno %d)\n", path->orig,
               strerror(errno), errno);
    return -1;
  }

  // Next, search the local Mailbox types
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
  {
    if (!(*ops)->is_local)
      continue;
    rc = (*ops)->path2_probe(path, &st);
    if (rc == 0)
      return 0;
  }

  mutt_debug(LL_DEBUG2, "Can't identify path: %s\n", path->orig);
  return -1;
}

/**
 * mx_path2_resolve - XXX
 */
int mx_path2_resolve(struct Path *path, const char *folder)
{
  int rc = path2_resolve(path, folder);
  if (rc < 0)
    return rc;

  rc = mx_path2_probe(path);
  if (rc < 0)
    return rc;

  rc = path2_tidy(path);
  if (rc < 0)
    return rc;

  return 0;
}
