/**
 * @file
 * Sidebar Expando definitions
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
 * @page sidebar_expando Sidebar Expando definitions
 *
 * Sidebar Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "expando.h"
#include "expando/lib.h"
#include "index/lib.h"

/**
 * add_indent - Generate the needed indentation
 * @param buf    Output buffer
 * @param buflen Size of output buffer
 * @param sbe    Sidebar entry
 * @retval num Bytes written
 */
static size_t add_indent(char *buf, size_t buflen, const struct SbEntry *sbe)
{
  size_t res = 0;
  const char *const c_sidebar_indent_string = cs_subset_string(NeoMutt->sub, "sidebar_indent_string");
  for (int i = 0; i < sbe->depth; i++)
  {
    res += mutt_str_copy(buf + res, c_sidebar_indent_string, buflen - res);
  }
  return res;
}

/**
 * sidebar_bang - Sidebar: Flagged flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void sidebar_bang(const struct ExpandoNode *node, void *data,
                         MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  if (m->msg_flagged == 0)
  {
    buf_strcpy(buf, "");
  }
  else if (m->msg_flagged == 1)
  {
    buf_strcpy(buf, "!");
  }
  else if (m->msg_flagged == 2)
  {
    buf_strcpy(buf, "!!");
  }
  else
  {
    buf_printf(buf, "%d!", m->msg_flagged);
  }
}

/**
 * sidebar_a_num - Sidebar: Alert for new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->notify_user;
}

/**
 * sidebar_B - Sidebar: Name of the mailbox - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void sidebar_B(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;

  char tmp[256] = { 0 };

  const size_t ilen = sizeof(tmp);
  const size_t off = add_indent(tmp, ilen, sbe);
  snprintf(tmp + off, ilen - off, "%s", sbe->box);

  buf_strcpy(buf, tmp);
}

/**
 * sidebar_d_num - Sidebar: Number of deleted messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_d_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct IndexSharedData *shared = sdata->shared;
  const struct Mailbox *m = sbe->mailbox;
  const struct Mailbox *m_cur = shared->mailbox;

  const bool c = m_cur && mutt_str_equal(m_cur->realpath, m->realpath);

  return c ? m_cur->msg_deleted : 0;
}

/**
 * sidebar_D - Sidebar: Descriptive name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void sidebar_D(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;

  char tmp[256] = { 0 };

  const size_t ilen = sizeof(tmp);
  const size_t off = add_indent(tmp, ilen, sbe);

  if (sbe->mailbox->name)
  {
    snprintf(tmp + off, ilen - off, "%s", sbe->mailbox->name);
  }
  else
  {
    snprintf(tmp + off, ilen - off, "%s", sbe->box);
  }

  buf_strcpy(buf, tmp);
}

/**
 * sidebar_F_num - Sidebar: Number of flagged messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_F_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_flagged;
}

/**
 * sidebar_L_num - Sidebar: Number of limited messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_L_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct IndexSharedData *shared = sdata->shared;
  const struct Mailbox *m = sbe->mailbox;
  const struct Mailbox *m_cur = shared->mailbox;

  const bool c = m_cur && mutt_str_equal(m_cur->realpath, m->realpath);

  return c ? m_cur->vcount : m->msg_count;
}

/**
 * sidebar_n_num - Sidebar: New mail flag - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->has_new;
}

/**
 * sidebar_n - Sidebar: New mail flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void sidebar_n(const struct ExpandoNode *node, void *data,
                      MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  // NOTE(g0mb4): use $flag_chars?
  const char *s = m->has_new ? "N" : " ";
  buf_strcpy(buf, s);
}

/**
 * sidebar_N_num - Sidebar: Number of unread messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_N_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread;
}

/**
 * sidebar_o_num - Sidebar: Number of old messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_o_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread - m->msg_new;
}

/**
 * sidebar_p_num - Sidebar: Poll for new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->poll_new_mail;
}

/**
 * sidebar_r_num - Sidebar: Number of read messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_r_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count - m->msg_unread;
}

/**
 * sidebar_S_num - Sidebar: number of messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_S_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count;
}

/**
 * sidebar_t_num - Sidebar: Number of tagged messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct IndexSharedData *shared = sdata->shared;
  const struct Mailbox *m = sbe->mailbox;
  const struct Mailbox *m_cur = shared->mailbox;

  const bool c = m_cur && mutt_str_equal(m_cur->realpath, m->realpath);

  return c ? m_cur->msg_tagged : 0;
}

/**
 * sidebar_Z_num - Sidebar: Number of new messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long sidebar_Z_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_new;
}

/**
 * SidebarRenderData - Callbacks for Sidebar Expandos
 *
 * @sa SidebarFormatDef, ExpandoDataSidebar
 */
const struct ExpandoRenderData SidebarRenderData[] = {
  // clang-format off
  { ED_SIDEBAR, ED_SID_FLAGGED,       sidebar_bang, NULL },
  { ED_SIDEBAR, ED_SID_NOTIFY,        NULL,         sidebar_a_num },
  { ED_SIDEBAR, ED_SID_NAME,          sidebar_B,    NULL },
  { ED_SIDEBAR, ED_SID_DELETED_COUNT, NULL,         sidebar_d_num },
  { ED_SIDEBAR, ED_SID_DESCRIPTION,   sidebar_D,    NULL },
  { ED_SIDEBAR, ED_SID_FLAGGED_COUNT, NULL,         sidebar_F_num },
  { ED_SIDEBAR, ED_SID_LIMITED_COUNT, NULL,         sidebar_L_num },
  { ED_SIDEBAR, ED_SID_NEW_MAIL,      sidebar_n,    sidebar_n_num },
  { ED_SIDEBAR, ED_SID_UNREAD_COUNT,  NULL,         sidebar_N_num },
  { ED_SIDEBAR, ED_SID_OLD_COUNT,     NULL,         sidebar_o_num },
  { ED_SIDEBAR, ED_SID_POLL,          NULL,         sidebar_p_num },
  { ED_SIDEBAR, ED_SID_READ_COUNT,    NULL,         sidebar_r_num },
  { ED_SIDEBAR, ED_SID_MESSAGE_COUNT, NULL,         sidebar_S_num },
  { ED_SIDEBAR, ED_SID_TAGGED_COUNT,  NULL,         sidebar_t_num },
  { ED_SIDEBAR, ED_SID_UNSEEN_COUNT,  NULL,         sidebar_Z_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
