/**
 * @file
 * GUI display a user-configurable status line
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Austin Ray <austin@austinray.io>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021 Eric Blake <eblake@redhat.com>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page neo_status GUI display a user-configurable status line
 *
 * GUI display a user-configurable status line
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "status.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "postpone/lib.h"
#include "globals.h"
#include "mutt_mailbox.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mview.h"

void status_f(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf);

const struct ExpandoRenderData StatusRenderData[];

/**
 * get_sort_str - Get the sort method as a string
 * @param buf    Buffer for the sort string
 * @param buflen Length of the buffer
 * @param method Sort method, see #SortType
 * @retval ptr Buffer pointer
 */
static char *get_sort_str(char *buf, size_t buflen, enum SortType method)
{
  snprintf(buf, buflen, "%s%s%s", (method & SORT_REVERSE) ? "reverse-" : "",
           (method & SORT_LAST) ? "last-" : "",
           mutt_map_get_name(method & SORT_MASK, SortMethods));
  return buf;
}

/**
 * struct MenuStatusLineData - Data for creating a Menu line
 */
struct MenuStatusLineData
{
  struct IndexSharedData *shared; ///< Data shared between Index, Pager and Sidebar
  struct Menu *menu;              ///< Current Menu
};

/**
 * status_r - Status: Modified/read-only flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_r(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MbTable *c_status_chars = cs_subset_mbtable(NeoMutt->sub, "status_chars");
  if (!c_status_chars || !c_status_chars->len)
    return;

  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  size_t i = 0;

  if (m)
  {
    i = shared->attach_msg ? 3 :
                             ((m->readonly || m->dontwrite) ? 2 :
                              (m->changed ||
                               /* deleted doesn't necessarily mean changed in IMAP */
                               (m->type != MUTT_IMAP && m->msg_deleted)) ?
                                                              1 :
                                                              0);
  }

  if (i >= c_status_chars->len)
    buf_addstr(buf, c_status_chars->chars[0]);
  else
    buf_addstr(buf, c_status_chars->chars[i]);
}

/**
 * status_D - Status: Description of the mailbox - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_D(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  // If there's a descriptive name, use it. Otherwise, use %f
  if (m && m->name)
  {
    const char *s = m->name;
    buf_strcpy(buf, s);
    return;
  }

  status_f(node, data, flags, max_cols, buf);
}

/**
 * status_f - Status: pathname of the mailbox - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_f(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  char tmp[128] = { 0 };

  if (m && m->compress_info && (m->realpath[0] != '\0'))
  {
    mutt_str_copy(tmp, m->realpath, sizeof(tmp));
    mutt_pretty_mailbox(tmp, sizeof(tmp));
  }
  else if (m && (m->type == MUTT_NOTMUCH) && m->name)
  {
    mutt_str_copy(tmp, m->name, sizeof(tmp));
  }
  else if (m && !buf_is_empty(&m->pathbuf))
  {
    mutt_str_copy(tmp, mailbox_path(m), sizeof(tmp));
    mutt_pretty_mailbox(tmp, sizeof(tmp));
  }
  else
  {
    mutt_str_copy(tmp, _("(no mailbox)"), sizeof(tmp));
  }

  buf_strcpy(buf, tmp);
}

/**
 * status_M_num - Status: Number of messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_M_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct MailboxView *mv = shared->mailbox_view;

  return mv ? mv->vcount : 0;
}

/**
 * status_m_num - Status: Number of messages in the mailbox - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_m_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_count : 0;
}

/**
 * status_n_num - Status: Number of new messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_new : 0;
}

/**
 * status_o_num - Status: Number of old messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_o_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? (m->msg_unread - m->msg_new) : 0;
}

/**
 * status_d_num - Status: Number of deleted messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_d_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_deleted : 0;
}

/**
 * status_F_num - Status: Number of flagged messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_F_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_flagged : 0;
}

/**
 * status_t_num - Status: Number of tagged messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_tagged : 0;
}

/**
 * status_p_num - Status: Number of postponed messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  struct Mailbox *m = shared->mailbox;

  return mutt_num_postponed(m, false);
}

/**
 * status_b_num - Status: Number of mailboxes with new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_b_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  struct Mailbox *m = shared->mailbox;

  return mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_NO_FLAGS);
}

/**
 * status_l_num - Status: Size of the current mailbox - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_l_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  return m->size;
}

/**
 * status_l - Status: Size of the current mailbox - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_l(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  char tmp[128] = { 0 };

  const off_t num = m ? m->size : 0;
  mutt_str_pretty_size(tmp, sizeof(tmp), num);
  buf_strcpy(buf, tmp);
}

/**
 * status_T - Status: Current threading mode - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_T(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const enum UseThreads c_use_threads = mutt_thread_style();
  const char *s = get_use_threads_str(c_use_threads);
  buf_strcpy(buf, s);
}

/**
 * status_s - Status: Sorting mode - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_s(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  char tmp[128] = { 0 };

  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  const char *s = get_sort_str(tmp, sizeof(tmp), c_sort);
  buf_strcpy(buf, s);
}

/**
 * status_S - Status: Aux sorting method - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_S(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  char tmp[128] = { 0 };

  const enum SortType c_sort_aux = cs_subset_sort(NeoMutt->sub, "sort_aux");
  const char *s = get_sort_str(tmp, sizeof(tmp), c_sort_aux);
  buf_strcpy(buf, s);
}

/**
 * status_P_num - Status: Percentage through index - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_P_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct Menu *menu = msld->menu;
  if (!menu)
    return 0;

  if (menu->top + menu->page_len >= menu->max)
    return 100;

  return (100 * (menu->top + menu->page_len)) / menu->max;
}

/**
 * status_P - Status: Percentage through index - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_P(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct Menu *menu = msld->menu;
  if (!menu)
    return;

  char tmp[128] = { 0 };

  char *cp = NULL;
  if (menu->top + menu->page_len >= menu->max)
  {
    cp = menu->top ?
             /* L10N: Status bar message: the end of the list emails is visible in the index */
             _("end") :
             /* L10N: Status bar message: all the emails are visible in the index */
             _("all");
  }
  else
  {
    int count = (100 * (menu->top + menu->page_len)) / menu->max;
    /* L10N: Status bar, percentage of way through index.
           `%d` is the number, `%%` is the percent symbol.
           They may be reordered, or space inserted, if you wish. */
    snprintf(tmp, sizeof(tmp), _("%d%%"), count);
    cp = tmp;
  }

  buf_strcpy(buf, cp);
}

/**
 * status_h - Status: Local hostname - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_h(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const char *s = ShortHostname;
  buf_strcpy(buf, s);
}

/**
 * status_L_num - Status: Size of the messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_L_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct MailboxView *mailbox_view = shared->mailbox_view;
  if (!mailbox_view)
    return 0;

  return mailbox_view->vsize;
}

/**
 * status_L - Status: Size of the messages - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_L(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct MailboxView *mailbox_view = shared->mailbox_view;

  char tmp[128] = { 0 };

  const off_t num = mailbox_view ? mailbox_view->vsize : 0;
  mutt_str_pretty_size(tmp, sizeof(tmp), num);
  buf_strcpy(buf, tmp);
}

/**
 * status_R_num - Status: Number of read messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_R_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? (m->msg_count - m->msg_unread) : 0;
}

/**
 * status_u_num - Status: Number of unread messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long status_u_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct Mailbox *m = shared->mailbox;

  return m ? m->msg_unread : 0;
}

/**
 * status_v - Status: Version string - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_v(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const char *s = mutt_make_version();
  buf_strcpy(buf, s);
}

/**
 * status_V - Status: Active limit pattern - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void status_V(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct MenuStatusLineData *msld = data;
  const struct IndexSharedData *shared = msld->shared;
  const struct MailboxView *mailbox_view = shared->mailbox_view;

  const char *s = mview_has_limit(mailbox_view) ? mailbox_view->pattern : "";
  buf_strcpy(buf, s);
}

/**
 * menu_status_line - Create the status line
 * @param[out] buf      Buffer in which to save string
 * @param[in]  shared   Shared Index data
 * @param[in]  menu     Current menu
 * @param[in]  max_cols Maximum number of columns to use (-1 means unlimited)
 * @param[in]  exp      Expando
 *
 * @sa status_format_str()
 */
void menu_status_line(struct Buffer *buf, struct IndexSharedData *shared,
                      struct Menu *menu, int max_cols, const struct Expando *exp)
{
  struct MenuStatusLineData data = { shared, menu };

  expando_filter(exp, StatusRenderData, &data, MUTT_FORMAT_NO_FLAGS, max_cols, buf);
}

/**
 * StatusRenderData - Callbacks for Status Expandos
 *
 * @sa StatusFormatDef, ExpandoDataGlobal, ExpandoDataIndex, ExpandoDataMenu
 */
const struct ExpandoRenderData StatusRenderData[] = {
  // clang-format off
  { ED_INDEX,  ED_IND_UNREAD_MAILBOXES,NULL,     status_b_num },
  { ED_INDEX,  ED_IND_DELETED_COUNT,   NULL,     status_d_num },
  { ED_INDEX,  ED_IND_DESCRIPTION,     status_D, NULL },
  { ED_INDEX,  ED_IND_FLAGGED_COUNT,   NULL,     status_F_num },
  { ED_INDEX,  ED_IND_MAILBOX_PATH,    status_f, NULL },
  { ED_GLOBAL, ED_GLO_HOSTNAME,        status_h, NULL },
  { ED_INDEX,  ED_IND_LIMIT_SIZE,      status_L, status_L_num },
  { ED_INDEX,  ED_IND_MAILBOX_SIZE,    status_l, status_l_num },
  { ED_INDEX,  ED_IND_LIMIT_COUNT,     NULL,     status_M_num },
  { ED_INDEX,  ED_IND_MESSAGE_COUNT,   NULL,     status_m_num },
  { ED_INDEX,  ED_IND_NEW_COUNT,       NULL,     status_n_num },
  { ED_INDEX,  ED_IND_OLD_COUNT,       NULL,     status_o_num },
  { ED_MENU,   ED_MEN_PERCENTAGE,      status_P, status_P_num },
  { ED_INDEX,  ED_IND_POSTPONED_COUNT, NULL,     status_p_num },
  { ED_INDEX,  ED_IND_READ_COUNT,      NULL,     status_R_num },
  { ED_INDEX,  ED_IND_READONLY,        status_r, NULL },
  { ED_GLOBAL, ED_GLO_SORT,            status_s, NULL },
  { ED_GLOBAL, ED_GLO_SORT_AUX,        status_S, NULL },
  { ED_INDEX,  ED_IND_TAGGED_COUNT,    NULL,     status_t_num },
  { ED_GLOBAL, ED_GLO_USE_THREADS,     status_T, NULL },
  { ED_INDEX,  ED_IND_UNREAD_COUNT,    NULL,     status_u_num },
  { ED_INDEX,  ED_IND_LIMIT_PATTERN,   status_V, NULL },
  { ED_GLOBAL, ED_GLO_VERSION,         status_v, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
