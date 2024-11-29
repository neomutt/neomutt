/**
 * @file
 * Browse NNTP groups
 *
 * @authors
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page nntp_expando_browser Browse NNTP groups
 *
 * Browse NNTP groups
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "expando_browser.h"
#include "browser/lib.h"
#include "expando/lib.h"
#include "mdata.h"

/**
 * group_index_a_num - NNTP: Alert for new mail - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long group_index_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->ff->notify_user;
}

/**
 * group_index_C_num - NNTP: Index number - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long group_index_C_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->num + 1;
}

/**
 * group_index_d - NNTP: Description - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void group_index_d(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  char tmp[128] = { 0 };

  if (!folder->ff->nd->desc)
    return;

  char *desc = mutt_str_dup(folder->ff->nd->desc);
  const char *const c_newsgroups_charset = cs_subset_string(NeoMutt->sub, "newsgroups_charset");
  if (c_newsgroups_charset)
  {
    mutt_ch_convert_string(&desc, c_newsgroups_charset, cc_charset(), MUTT_ICONV_HOOK_FROM);
  }
  mutt_mb_filter_unprintable(&desc);
  mutt_str_copy(tmp, desc, sizeof(tmp));
  FREE(&desc);

  buf_strcpy(buf, tmp);
}

/**
 * group_index_f - NNTP: Newsgroup name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void group_index_f(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  const char *s = folder->ff->name;
  buf_strcpy(buf, s);
}

/**
 * group_index_M - NNTP: Moderated flag - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void group_index_M(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  const char *s = NULL;
  // NOTE(g0mb4): use $flag_chars?
  if (folder->ff->nd->deleted)
  {
    s = "D";
  }
  else
  {
    s = folder->ff->nd->allowed ? " " : "-";
  }

  buf_strcpy(buf, s);
}

/**
 * group_index_n_num - NNTP: Number of new articles - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long group_index_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  const struct NntpMboxData *nd = folder->ff->nd;

  const bool c_mark_old = cs_subset_bool(NeoMutt->sub, "mark_old");

  if (c_mark_old && (nd->last_cached >= nd->first_message) &&
      (nd->last_cached <= nd->last_message))
  {
    return nd->last_message - nd->last_cached;
  }

  return nd->unread;
}

/**
 * group_index_N - NNTP: New flag - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void group_index_N(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  const char *s = NULL;
  // NOTE(g0mb4): use $flag_chars?
  if (folder->ff->nd->subscribed)
  {
    s = " ";
  }
  else
  {
    s = folder->ff->has_new_mail ? "N" : "u";
  }

  buf_strcpy(buf, s);
}

/**
 * group_index_p_num - NNTP: Poll for new mail - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long group_index_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->ff->poll_new_mail;
}

/**
 * group_index_s_num - NNTP: Number of unread articles - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long group_index_s_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->ff->nd->unread;
}

/**
 * GroupIndexRenderCallbacks - Callbacks for Nntp Browser Expandos
 *
 * @sa GroupIndexFormatDef, ExpandoDataFolder
 */
const struct ExpandoRenderCallback GroupIndexRenderCallbacks[] = {
  // clang-format off
  { ED_FOLDER, ED_FOL_NOTIFY,       NULL,          group_index_a_num },
  { ED_FOLDER, ED_FOL_NUMBER,       NULL,          group_index_C_num },
  { ED_FOLDER, ED_FOL_DESCRIPTION,  group_index_d, NULL },
  { ED_FOLDER, ED_FOL_NEWSGROUP,    group_index_f, NULL },
  { ED_FOLDER, ED_FOL_FLAGS,        group_index_M, NULL },
  { ED_FOLDER, ED_FOL_FLAGS2,       group_index_N, NULL },
  { ED_FOLDER, ED_FOL_NEW_COUNT,    NULL,          group_index_n_num },
  { ED_FOLDER, ED_FOL_POLL,         NULL,          group_index_p_num },
  { ED_FOLDER, ED_FOL_UNREAD_COUNT, NULL,          group_index_s_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
