/**
 * @file
 * Sidebar Window
 *
 * @authors
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Ashish Panigrahi <ashish.panigrahi@protonmail.com>
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
 * @page sidebar_window Sidebar Window
 *
 * The Sidebar Window is an interactive window that displays a list of
 * mailboxes to the user.
 *
 * ## Windows
 *
 * | Name           | Type       | See Also          |
 * | :------------- | :--------- | :---------------- |
 * | Sidebar Window | WT_SIDEBAR | mutt_window_new() |
 *
 * **Parent**
 * - @ref index_dlg_index
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #SidebarWindowData
 *
 * The Sidebar Window stores its data (#SidebarWindowData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler               |
 * | :-------------------- | :-------------------- |
 * | #NT_ACCOUNT           | sb_account_observer() |
 * | #NT_COLOR             | sb_color_observer()   |
 * | #NT_COMMAND           | sb_command_observer() |
 * | #NT_CONFIG            | sb_config_observer()  |
 * | #NT_INDEX             | sb_index_observer()   |
 * | #NT_MAILBOX           | sb_mailbox_observer() |
 * | #NT_WINDOW            | sb_window_observer()  |
 * | MuttWindow::recalc()  | sb_recalc()           |
 * | MuttWindow::repaint() | sb_repaint()          |
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "expando/lib.h"
#include "index/lib.h"

const struct ExpandoRenderData SidebarRenderData[];

/**
 * struct SidebarData - Data passed to sidebar_format_str()
 */
struct SidebarData
{
  struct SbEntry *entry;          ///< Info about a folder
  struct IndexSharedData *shared; ///< Shared Index Data
};

/**
 * imap_is_prefix - Check if folder matches the beginning of mbox
 * @param folder Folder
 * @param mbox   Mailbox path
 * @retval num Length of the prefix
 */
static int imap_is_prefix(const char *folder, const char *mbox)
{
  int plen = 0;

  struct Url *url_m = url_parse(mbox);
  struct Url *url_f = url_parse(folder);
  if (!url_m || !url_f)
    goto done;

  if (!mutt_istr_equal(url_m->host, url_f->host))
    goto done;

  if (url_m->user && url_f->user && !mutt_istr_equal(url_m->user, url_f->user))
    goto done;

  size_t mlen = mutt_str_len(url_m->path);
  size_t flen = mutt_str_len(url_f->path);
  if (flen > mlen)
    goto done;

  if (!mutt_strn_equal(url_m->path, url_f->path, flen))
    goto done;

  plen = strlen(mbox) - mlen + flen;

done:
  url_free(&url_m);
  url_free(&url_f);

  return plen;
}

/**
 * abbrev_folder - Abbreviate a Mailbox path using a folder
 * @param mbox   Mailbox path to shorten
 * @param folder Folder path to use
 * @param type   Mailbox type
 * @retval ptr Pointer into the mbox param
 */
static const char *abbrev_folder(const char *mbox, const char *folder, enum MailboxType type)
{
  if (!mbox || !folder)
    return NULL;

  if (type == MUTT_IMAP)
  {
    int prefix = imap_is_prefix(folder, mbox);
    if (prefix == 0)
      return NULL;
    return mbox + prefix;
  }

  const char *const c_sidebar_delim_chars = cs_subset_string(NeoMutt->sub, "sidebar_delim_chars");
  if (!c_sidebar_delim_chars)
    return NULL;

  size_t flen = mutt_str_len(folder);
  if (flen == 0)
    return NULL;
  if (strchr(c_sidebar_delim_chars, folder[flen - 1])) // folder ends with a delimiter
    flen--;

  size_t mlen = mutt_str_len(mbox);
  if (mlen < flen)
    return NULL;

  if (!mutt_strn_equal(folder, mbox, flen))
    return NULL;

  // After the match, check that mbox has a delimiter
  if (!strchr(c_sidebar_delim_chars, mbox[flen]))
    return NULL;

  if (mlen > flen)
  {
    return mbox + flen + 1;
  }

  // mbox and folder are equal, use the chunk after the last delimiter
  while (mlen--)
  {
    if (strchr(c_sidebar_delim_chars, mbox[mlen]))
    {
      return mbox + mlen + 1;
    }
  }

  return NULL;
}

/**
 * abbrev_url - Abbreviate a url-style Mailbox path
 * @param mbox Mailbox path to shorten
 * @param type Mailbox type
 * @retval ptr mbox unchanged
 *
 * Use heuristics to shorten a non-local Mailbox path.
 * Strip the host part (or database part for Notmuch).
 *
 * e.g.
 * - `imap://user@host.com/apple/banana` becomes `apple/banana`
 * - `notmuch:///home/user/db?query=hello` becomes `query=hello`
 */
static const char *abbrev_url(const char *mbox, enum MailboxType type)
{
  /* This is large enough to skip `notmuch://`,
   * but not so large that it will go past the host part. */
  const int scheme_len = 10;

  size_t len = mutt_str_len(mbox);
  if ((len < scheme_len) || ((type != MUTT_NNTP) && (type != MUTT_IMAP) &&
                             (type != MUTT_NOTMUCH) && (type != MUTT_POP)))
  {
    return mbox;
  }

  const char split = (type == MUTT_NOTMUCH) ? '?' : '/';

  // Skip over the scheme, e.g. `imaps://`, `notmuch://`
  const char *last = strchr(mbox + scheme_len, split);
  if (last)
    mbox = last + 1;
  return mbox;
}

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
 * calc_color - Calculate the colour of a Sidebar row
 * @param m         Mailbox
 * @param current   true, if this is the current Mailbox
 * @param highlight true, if this Mailbox has the highlight on it
 * @retval enum #ColorId, e.g. #MT_COLOR_SIDEBAR_NEW
 */
static const struct AttrColor *calc_color(const struct Mailbox *m, bool current, bool highlight)
{
  const struct AttrColor *ac = NULL;

  const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
  if (simple_color_is_set(MT_COLOR_SIDEBAR_SPOOLFILE) &&
      mutt_str_equal(mailbox_path(m), c_spool_file))
  {
    ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_SPOOLFILE));
  }

  if (simple_color_is_set(MT_COLOR_SIDEBAR_FLAGGED) && (m->msg_flagged > 0))
  {
    ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_FLAGGED));
  }

  if (simple_color_is_set(MT_COLOR_SIDEBAR_UNREAD) && (m->msg_unread > 0))
  {
    ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_UNREAD));
  }

  if (simple_color_is_set(MT_COLOR_SIDEBAR_NEW) && m->has_new)
  {
    ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_NEW));
  }

  if (!ac && simple_color_is_set(MT_COLOR_SIDEBAR_ORDINARY))
  {
    ac = simple_color_get(MT_COLOR_SIDEBAR_ORDINARY);
  }

  const struct AttrColor *ac_bg = simple_color_get(MT_COLOR_NORMAL);
  ac_bg = merged_color_overlay(ac_bg, simple_color_get(MT_COLOR_SIDEBAR_BACKGROUND));
  ac = merged_color_overlay(ac_bg, ac);

  if (current || highlight)
  {
    int color;
    if (current)
    {
      if (simple_color_is_set(MT_COLOR_SIDEBAR_INDICATOR))
        color = MT_COLOR_SIDEBAR_INDICATOR;
      else
        color = MT_COLOR_INDICATOR;
    }
    else
    {
      color = MT_COLOR_SIDEBAR_HIGHLIGHT;
    }

    ac = merged_color_overlay(ac, simple_color_get(color));
  }

  return ac;
}

/**
 * calc_path_depth - Calculate the depth of a Mailbox path
 * @param[in]  mbox      Mailbox path to examine
 * @param[in]  delims    Delimiter characters
 * @param[out] last_part Last path component
 * @retval num Depth
 */
static int calc_path_depth(const char *mbox, const char *delims, const char **last_part)
{
  if (!mbox || !delims || !last_part)
    return 0;

  int depth = 0;
  const char *match = NULL;
  while ((match = strpbrk(mbox, delims)))
  {
    depth++;
    mbox = match + 1;
  }

  *last_part = mbox;
  return depth;
}

/**
 * sidebar_bang - Sidebar: Flagged flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void sidebar_bang(const struct ExpandoNode *node, void *data,
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
long sidebar_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->notify_user;
}

/**
 * sidebar_B - Sidebar: Name of the mailbox - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void sidebar_B(const struct ExpandoNode *node, void *data,
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
long sidebar_d_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
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
void sidebar_D(const struct ExpandoNode *node, void *data,
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
long sidebar_F_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_flagged;
}

/**
 * sidebar_L_num - Sidebar: Number of limited messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_L_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
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
long sidebar_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->has_new;
}

/**
 * sidebar_n - Sidebar: New mail flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void sidebar_n(const struct ExpandoNode *node, void *data,
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
long sidebar_N_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread;
}

/**
 * sidebar_o_num - Sidebar: Number of old messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_o_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_unread - m->msg_new;
}

/**
 * sidebar_p_num - Sidebar: Poll for new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->poll_new_mail;
}

/**
 * sidebar_r_num - Sidebar: Number of read messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_r_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count - m->msg_unread;
}

/**
 * sidebar_S_num - Sidebar: number of messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_S_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_count;
}

/**
 * sidebar_t_num - Sidebar: Number of tagged messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long sidebar_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
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
long sidebar_Z_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct SidebarData *sdata = data;
  const struct SbEntry *sbe = sdata->entry;
  const struct Mailbox *m = sbe->mailbox;

  return m->msg_new;
}

/**
 * make_sidebar_entry - Turn mailbox data into a sidebar string
 * @param[out] buf     Buffer in which to save string
 * @param[in]  buflen  Buffer length
 * @param[in]  width   Desired width in screen cells
 * @param[in]  sbe     Mailbox object
 * @param[in]  shared  Shared Index Data
 *
 * Take all the relevant mailbox data and the desired screen width and then get
 * expando_render() to do the actual work.
 *
 * @sa $sidebar_format
 */
static void make_sidebar_entry(char *buf, size_t buflen, int width,
                               struct SbEntry *sbe, struct IndexSharedData *shared)
{
  struct SidebarData sdata = { sbe, shared };

  struct Buffer *tmp = buf_pool_get();
  const struct Expando *c_sidebar_format = cs_subset_expando(NeoMutt->sub, "sidebar_format");
  expando_filter(c_sidebar_format, SidebarRenderData, &sdata,
                 MUTT_FORMAT_NO_FLAGS, width, tmp);
  mutt_str_copy(buf, buf_string(tmp), buflen);
  buf_pool_release(&tmp);

  /* Force string to be exactly the right width */
  int w = mutt_strwidth(buf);
  int s = mutt_str_len(buf);
  width = MIN(buflen, width);
  if (w < width)
  {
    /* Pad with spaces */
    memset(buf + s, ' ', width - w);
    buf[s + width - w] = '\0';
  }
  else if (w > width)
  {
    /* Truncate to fit */
    size_t len = mutt_wstr_trunc(buf, buflen, width, NULL);
    buf[len] = '\0';
  }
}

/**
 * update_entries_visibility - Should a SbEntry be displayed in the sidebar?
 * @param wdata Sidebar data
 *
 * For each SbEntry in the entries array, check whether we should display it.
 * This is determined by several criteria.  If the Mailbox:
 * * is the currently open mailbox
 * * is the currently highlighted mailbox
 * * has unread messages
 * * has flagged messages
 * * is pinned
 */
static void update_entries_visibility(struct SidebarWindowData *wdata)
{
  /* Aliases for readability */
  const bool c_sidebar_new_mail_only = cs_subset_bool(NeoMutt->sub, "sidebar_new_mail_only");
  const bool c_sidebar_non_empty_mailbox_only = cs_subset_bool(NeoMutt->sub, "sidebar_non_empty_mailbox_only");
  struct SbEntry *sbe = NULL;

  struct IndexSharedData *shared = wdata->shared;
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    int i = ARRAY_FOREACH_IDX;
    sbe = *sbep;

    sbe->is_hidden = false;

    if (!sbe->mailbox->visible)
    {
      sbe->is_hidden = true;
      continue;
    }

    if (shared->mailbox &&
        mutt_str_equal(sbe->mailbox->realpath, shared->mailbox->realpath))
    {
      /* Spool directories are always visible */
      continue;
    }

    if (mutt_list_find(&SidebarPinned, mailbox_path(sbe->mailbox)) ||
        mutt_list_find(&SidebarPinned, sbe->mailbox->name))
    {
      /* Explicitly asked to be visible */
      continue;
    }

    if (c_sidebar_non_empty_mailbox_only && (i != wdata->opn_index) &&
        (sbe->mailbox->msg_count == 0))
    {
      sbe->is_hidden = true;
    }

    if (c_sidebar_new_mail_only && (i != wdata->opn_index) &&
        (sbe->mailbox->msg_unread == 0) && (sbe->mailbox->msg_flagged == 0) &&
        !sbe->mailbox->has_new)
    {
      sbe->is_hidden = true;
    }
  }
}

/**
 * prepare_sidebar - Prepare the list of SbEntry's for the sidebar display
 * @param wdata     Sidebar data
 * @param page_size Number of lines on a page
 * @retval false No, don't draw the sidebar
 * @retval true  Yes, draw the sidebar
 *
 * Before painting the sidebar, we determine which are visible, sort
 * them and set up our page pointers.
 *
 * This is a lot of work to do each refresh, but there are many things that
 * can change outside of the sidebar that we don't hear about.
 */
static bool prepare_sidebar(struct SidebarWindowData *wdata, int page_size)
{
  if (ARRAY_EMPTY(&wdata->entries) || (page_size <= 0))
    return false;

  struct SbEntry **sbep = NULL;
  const bool c_sidebar_new_mail_only = cs_subset_bool(NeoMutt->sub, "sidebar_new_mail_only");
  const bool c_sidebar_non_empty_mailbox_only = cs_subset_bool(NeoMutt->sub, "sidebar_non_empty_mailbox_only");

  sbep = (wdata->opn_index >= 0) ? ARRAY_GET(&wdata->entries, wdata->opn_index) : NULL;
  const struct SbEntry *opn_entry = sbep ? *sbep : NULL;
  sbep = (wdata->hil_index >= 0) ? ARRAY_GET(&wdata->entries, wdata->hil_index) : NULL;
  const struct SbEntry *hil_entry = sbep ? *sbep : NULL;

  update_entries_visibility(wdata);
  const short c_sidebar_sort_method = cs_subset_sort(NeoMutt->sub, "sidebar_sort_method");
  sb_sort_entries(wdata, c_sidebar_sort_method);

  if (opn_entry || hil_entry)
  {
    ARRAY_FOREACH(sbep, &wdata->entries)
    {
      if ((opn_entry == *sbep) && (*sbep)->mailbox->visible)
        wdata->opn_index = ARRAY_FOREACH_IDX;
      if ((hil_entry == *sbep) && (*sbep)->mailbox->visible)
        wdata->hil_index = ARRAY_FOREACH_IDX;
    }
  }

  if ((wdata->hil_index < 0) || (hil_entry && hil_entry->is_hidden) ||
      (c_sidebar_sort_method != wdata->previous_sort))
  {
    if (wdata->opn_index >= 0)
    {
      wdata->hil_index = wdata->opn_index;
    }
    else
    {
      wdata->hil_index = 0;
      /* Note is_hidden will only be set when `$sidebar_new_mail_only` */
      if ((*ARRAY_GET(&wdata->entries, 0))->is_hidden && !sb_next(wdata))
        wdata->hil_index = -1;
    }
  }

  /* Set the Top and Bottom to frame the wdata->hil_index in groups of page_size */

  /* If `$sidebar_new_mail_only` or `$sidebar_non_empty_mailbox_only` is set,
   * some entries may be hidden so we need to scan for the framing interval */
  if (c_sidebar_new_mail_only || c_sidebar_non_empty_mailbox_only)
  {
    wdata->top_index = -1;
    wdata->bot_index = -1;
    while (wdata->bot_index < wdata->hil_index)
    {
      wdata->top_index = wdata->bot_index + 1;
      int page_entries = 0;
      while (page_entries < page_size)
      {
        wdata->bot_index++;
        if (wdata->bot_index >= ARRAY_SIZE(&wdata->entries))
          break;
        if (!(*ARRAY_GET(&wdata->entries, wdata->bot_index))->is_hidden)
          page_entries++;
      }
    }
  }
  else
  {
    /* Otherwise we can just calculate the interval */
    wdata->top_index = (wdata->hil_index / page_size) * page_size;
    wdata->bot_index = wdata->top_index + page_size - 1;
  }

  if (wdata->bot_index > (ARRAY_SIZE(&wdata->entries) - 1))
    wdata->bot_index = ARRAY_SIZE(&wdata->entries) - 1;

  wdata->previous_sort = c_sidebar_sort_method;

  return (wdata->hil_index >= 0);
}

/**
 * sb_recalc - Recalculate the Sidebar display - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
int sb_recalc(struct MuttWindow *win)
{
  struct SidebarWindowData *wdata = sb_wdata_get(win);
  struct IndexSharedData *shared = wdata->shared;

  if (ARRAY_EMPTY(&wdata->entries))
  {
    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &ml, entries)
    {
      if (np->mailbox->visible)
        sb_add_mailbox(wdata, np->mailbox);
    }
    neomutt_mailboxlist_clear(&ml);
  }

  if (!prepare_sidebar(wdata, win->state.rows))
  {
    win->actions |= WA_REPAINT;
    return 0;
  }

  int num_rows = win->state.rows;
  int num_cols = win->state.cols;

  if (ARRAY_EMPTY(&wdata->entries) || (num_rows <= 0))
    return 0;

  if (wdata->top_index < 0)
    return 0;

  int width = num_cols - wdata->divider_width;
  int row = 0;
  struct Mailbox *m_cur = shared->mailbox;
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM(sbep, &wdata->entries, wdata->top_index)
  {
    if (row >= num_rows)
      break;

    if ((*sbep)->is_hidden)
      continue;

    struct SbEntry *entry = (*sbep);
    struct Mailbox *m = entry->mailbox;

    const int entryidx = ARRAY_FOREACH_IDX;
    entry->color = calc_color(m, (entryidx == wdata->opn_index),
                              (entryidx == wdata->hil_index));

    if (m_cur && (m_cur->realpath[0] != '\0') &&
        mutt_str_equal(m->realpath, m_cur->realpath))
    {
      m->msg_unread = m_cur->msg_unread;
      m->msg_count = m_cur->msg_count;
      m->msg_flagged = m_cur->msg_flagged;
    }

    const char *path = mailbox_path(m);

    const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
    // Try to abbreviate the full path
    const char *abbr = abbrev_folder(path, c_folder, m->type);
    if (!abbr)
      abbr = abbrev_url(path, m->type);
    const char *short_path = abbr ? abbr : path;

    /* Compute the depth */
    const char *last_part = abbr;
    const char *const c_sidebar_delim_chars = cs_subset_string(NeoMutt->sub, "sidebar_delim_chars");
    entry->depth = calc_path_depth(abbr, c_sidebar_delim_chars, &last_part);

    const bool short_path_is_abbr = (short_path == abbr);
    const bool c_sidebar_short_path = cs_subset_bool(NeoMutt->sub, "sidebar_short_path");
    if (c_sidebar_short_path)
    {
      short_path = last_part;
    }

    // Don't indent if we were unable to create an abbreviation.
    // Otherwise, the full path will be indent, and it looks unusual.
    const bool c_sidebar_folder_indent = cs_subset_bool(NeoMutt->sub, "sidebar_folder_indent");
    if (c_sidebar_folder_indent && short_path_is_abbr)
    {
      const short c_sidebar_component_depth = cs_subset_number(NeoMutt->sub, "sidebar_component_depth");
      if (c_sidebar_component_depth > 0)
        entry->depth -= c_sidebar_component_depth;
    }
    else if (!c_sidebar_folder_indent)
    {
      entry->depth = 0;
    }

    mutt_str_copy(entry->box, short_path, sizeof(entry->box));
    make_sidebar_entry(entry->display, sizeof(entry->display), width, entry, shared);
    row++;
  }

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * draw_divider - Draw a line between the sidebar and the rest of neomutt
 * @param wdata    Sidebar data
 * @param win      Window to draw on
 * @param num_rows Height of the Sidebar
 * @param num_cols Width of the Sidebar
 * @retval 0   Empty string
 * @retval num Character occupies n screen columns
 *
 * Draw a divider using characters from the config option "sidebar_divider_char".
 * This can be an ASCII or Unicode character.
 * We calculate these characters' width in screen columns.
 *
 * If the user hasn't set $sidebar_divider_char we pick a character for them,
 * respecting the value of $ascii_chars.
 */
static int draw_divider(struct SidebarWindowData *wdata, struct MuttWindow *win,
                        int num_rows, int num_cols)
{
  if ((num_rows < 1) || (num_cols < 1) || (wdata->divider_width > num_cols) ||
      (wdata->divider_width == 0))
  {
    return 0;
  }

  const int width = wdata->divider_width;
  const char *const c_sidebar_divider_char = cs_subset_string(NeoMutt->sub, "sidebar_divider_char");

  const struct AttrColor *ac = simple_color_get(MT_COLOR_NORMAL);
  ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_BACKGROUND));
  ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_DIVIDER));
  mutt_curses_set_color(ac);

  const bool c_sidebar_on_right = cs_subset_bool(NeoMutt->sub, "sidebar_on_right");
  const int col = c_sidebar_on_right ? 0 : (num_cols - width);

  for (int i = 0; i < num_rows; i++)
  {
    mutt_window_move(win, col, i);

    if (wdata->divider_type == SB_DIV_USER)
      mutt_window_addstr(win, NONULL(c_sidebar_divider_char));
    else
      mutt_window_addch(win, '|');
  }

  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  return width;
}

/**
 * fill_empty_space - Wipe the remaining Sidebar space
 * @param win        Window to draw on
 * @param first_row  Window line to start (0-based)
 * @param num_rows   Number of rows to fill
 * @param div_width  Width in screen characters taken by the divider
 * @param num_cols   Number of columns to fill
 *
 * Write spaces over the area the sidebar isn't using.
 */
static void fill_empty_space(struct MuttWindow *win, int first_row,
                             int num_rows, int div_width, int num_cols)
{
  /* Fill the remaining rows with blank space */
  const struct AttrColor *ac = simple_color_get(MT_COLOR_NORMAL);
  ac = merged_color_overlay(ac, simple_color_get(MT_COLOR_SIDEBAR_BACKGROUND));
  mutt_curses_set_color(ac);

  const bool c_sidebar_on_right = cs_subset_bool(NeoMutt->sub, "sidebar_on_right");
  if (!c_sidebar_on_right)
    div_width = 0;
  for (int r = 0; r < num_rows; r++)
  {
    mutt_window_move(win, div_width, first_row + r);
    mutt_curses_set_color_by_id(MT_COLOR_SIDEBAR_BACKGROUND);

    for (int i = 0; i < num_cols; i++)
      mutt_window_addch(win, ' ');
  }
}

/**
 * sb_repaint - Repaint the Sidebar display - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
int sb_repaint(struct MuttWindow *win)
{
  struct SidebarWindowData *wdata = sb_wdata_get(win);
  const bool c_sidebar_on_right = cs_subset_bool(NeoMutt->sub, "sidebar_on_right");

  int row = 0;
  int num_rows = win->state.rows;
  int num_cols = win->state.cols;

  if (wdata->top_index >= 0)
  {
    int col = 0;
    if (c_sidebar_on_right)
      col = wdata->divider_width;

    struct SbEntry **sbep = NULL;
    ARRAY_FOREACH_FROM(sbep, &wdata->entries, wdata->top_index)
    {
      if (row >= num_rows)
        break;

      if ((*sbep)->is_hidden)
        continue;

      struct SbEntry *entry = (*sbep);
      mutt_window_move(win, col, row);
      mutt_curses_set_color(entry->color);
      mutt_window_printf(win, "%s", entry->display);
      mutt_refresh();
      row++;
    }
  }

  fill_empty_space(win, row, num_rows - row, wdata->divider_width,
                   num_cols - wdata->divider_width);
  draw_divider(wdata, win, num_rows, num_cols);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
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
