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
 * @param[out] buf   Output buffer
 * @param[in]  depth Depth of indent
 */
static void add_indent(struct Buffer *buf, int depth)
{
  const char *const c_sidebar_indent_string = cs_subset_string(NeoMutt->sub, "sidebar_indent_string");
  for (int i = 0; i < depth; i++)
  {
    buf_addstr(buf, c_sidebar_indent_string);
  }
}

/**
 * sidebar_deleted_count_num - Sidebar: Number of deleted messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_deleted_count_num(const struct ExpandoNode *node,
                                      void *data, MuttFormatFlags flags)
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
 * sidebar_description - Sidebar: Descriptive name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void sidebar_description(const struct ExpandoNode *node, void *data,
                                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;

  add_indent(buf, sbe->depth);

  if (sbe->mailbox->name)
    buf_addstr(buf, sbe->mailbox->name);
  else
    buf_addstr(buf, sbe->box);
}

/**
 * sidebar_flagged - Sidebar: Flagged flags - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void sidebar_flagged(const struct ExpandoNode *node, void *data,
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
 * sidebar_flagged_count_num - Sidebar: Number of flagged messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_flagged_count_num(const struct ExpandoNode *node,
                                      void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_flagged;
}

/**
 * sidebar_limited_count_num - Sidebar: Number of limited messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_limited_count_num(const struct ExpandoNode *node,
                                      void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct IndexSharedData *shared = sdata->shared;
  const struct Mailbox *m = sbe->mailbox;
  const struct MailboxView *mv_cur = shared->mailbox_view;

  const bool c = mv_cur && (mv_cur->mailbox == m);

  return c ? mv_cur->vcount : m->msg_count;
}

/**
 * sidebar_message_count_num - Sidebar: number of messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_message_count_num(const struct ExpandoNode *node,
                                      void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count;
}

/**
 * sidebar_name - Sidebar: Name of the mailbox - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void sidebar_name(const struct ExpandoNode *node, void *data,
                         MuttFormatFlags flags, struct Buffer *buf)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;

  add_indent(buf, sbe->depth);
  buf_addstr(buf, sbe->box);
}

/**
 * sidebar_new_mail - Sidebar: New mail flag - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void sidebar_new_mail(const struct ExpandoNode *node, void *data,
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
 * sidebar_new_mail_num - Sidebar: New mail flag - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_new_mail_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->has_new;
}

/**
 * sidebar_notify_num - Sidebar: Alert for new mail - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_notify_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->notify_user;
}

/**
 * sidebar_old_count_num - Sidebar: Number of old messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_old_count_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread - m->msg_new;
}

/**
 * sidebar_poll_num - Sidebar: Poll for new mail - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_poll_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->poll_new_mail;
}

/**
 * sidebar_read_count_num - Sidebar: Number of read messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_read_count_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count - m->msg_unread;
}

/**
 * sidebar_tagged_count_num - Sidebar: Number of tagged messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_tagged_count_num(const struct ExpandoNode *node, void *data,
                                     MuttFormatFlags flags)
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
 * sidebar_unread_count_num - Sidebar: Number of unread messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_unread_count_num(const struct ExpandoNode *node, void *data,
                                     MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread;
}

/**
 * sidebar_unseen_count_num - Sidebar: Number of new messages - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long sidebar_unseen_count_num(const struct ExpandoNode *node, void *data,
                                     MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_new;
}

/**
 * SidebarRenderCallbacks - Callbacks for Sidebar Expandos
 *
 * @sa SidebarFormatDef, ExpandoDataSidebar
 */
const struct ExpandoRenderCallback SidebarRenderCallbacks[] = {
  // clang-format off
  { ED_SIDEBAR, ED_SID_DELETED_COUNT, NULL,                sidebar_deleted_count_num },
  { ED_SIDEBAR, ED_SID_DESCRIPTION,   sidebar_description, NULL },
  { ED_SIDEBAR, ED_SID_FLAGGED,       sidebar_flagged,     NULL },
  { ED_SIDEBAR, ED_SID_FLAGGED_COUNT, NULL,                sidebar_flagged_count_num },
  { ED_SIDEBAR, ED_SID_LIMITED_COUNT, NULL,                sidebar_limited_count_num },
  { ED_SIDEBAR, ED_SID_MESSAGE_COUNT, NULL,                sidebar_message_count_num },
  { ED_SIDEBAR, ED_SID_NAME,          sidebar_name,        NULL },
  { ED_SIDEBAR, ED_SID_NEW_MAIL,      sidebar_new_mail,    sidebar_new_mail_num },
  { ED_SIDEBAR, ED_SID_NOTIFY,        NULL,                sidebar_notify_num },
  { ED_SIDEBAR, ED_SID_OLD_COUNT,     NULL,                sidebar_old_count_num },
  { ED_SIDEBAR, ED_SID_POLL,          NULL,                sidebar_poll_num },
  { ED_SIDEBAR, ED_SID_READ_COUNT,    NULL,                sidebar_read_count_num },
  { ED_SIDEBAR, ED_SID_TAGGED_COUNT,  NULL,                sidebar_tagged_count_num },
  { ED_SIDEBAR, ED_SID_UNREAD_COUNT,  NULL,                sidebar_unread_count_num },
  { ED_SIDEBAR, ED_SID_UNSEEN_COUNT,  NULL,                sidebar_unseen_count_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
