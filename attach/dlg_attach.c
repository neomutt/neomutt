/**
 * @file
 * Attachment Selection Dialog
 *
 * @authors
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021-2024 Richard Russon <rich@flatcap.org>
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
 * @page attach_dlg_attach Attachment Selection Dialog
 *
 * The Attachment Selection Dialog lets the user select an email attachment.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                        | Type              | See Also         |
 * | :-------------------------- | :---------------- | :--------------- |
 * | Attachment Selection Dialog | WT_DLG_ATTACHMENT | dlg_attachment() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #AttachCtx
 *
 * The @ref gui_simple holds a Menu.  The Attachment Selection Dialog stores
 * its data (#AttachCtx) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                  |
 * | :---------- | :----------------------- |
 * | #NT_CONFIG  | attach_config_observer() |
 * | #NT_WINDOW  | attach_window_observer() |
 *
 * The Attachment Selection Dialog doesn't have any specific colours, so it
 * doesn't need to support #NT_COLOR.
 *
 * The Attachment Selection Dialog does not implement MuttWindow::recalc() or
 * MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "expando/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "attach.h"
#include "attachments.h"
#include "functions.h"
#include "hdrline.h"
#include "hook.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mview.h"
#include "private_data.h"
#include "recvattach.h"

void attach_F(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf);

/// Help Bar for the Attachment selection dialog
static const struct Mapping AttachmentHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Save"),  OP_ATTACHMENT_SAVE },
  { N_("Pipe"),  OP_PIPE },
  { N_("Print"), OP_ATTACHMENT_PRINT },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * attach_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_attach`.
 */
static int attach_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "attach_format") && !mutt_str_equal(ev_c->name, "message_format"))
    return 0;

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * attach_c - Attachment: Requires conversion flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_c(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = ((aptr->body->type != TYPE_TEXT) || aptr->body->noconv) ? "n" : "c";
  buf_strcpy(buf, s);
}

/**
 * attach_C - Attachment: Charset - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_C(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  char tmp[128] = { 0 };

  if (mutt_is_text_part(aptr->body) && mutt_body_get_charset(aptr->body, tmp, sizeof(tmp)))
  {
    buf_strcpy(buf, tmp);
  }
}

/**
 * attach_d - Attachment: Description - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_d(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const struct Expando *c_message_format = cs_subset_expando(NeoMutt->sub, "message_format");
  if (aptr->body->description)
  {
    const char *s = aptr->body->description;
    buf_strcpy(buf, s);
    return;
  }

  if (mutt_is_message_type(aptr->body->type, aptr->body->subtype) &&
      c_message_format && aptr->body->email)
  {
    mutt_make_string(buf, max_cols, c_message_format, NULL, -1, aptr->body->email,
                     MUTT_FORMAT_FORCESUBJ | MUTT_FORMAT_ARROWCURSOR, NULL);

    return;
  }

  if (!aptr->body->d_filename && !aptr->body->filename)
  {
    const char *s = "<no description>";
    buf_strcpy(buf, s);
    return;
  }

  attach_F(node, data, flags, max_cols, buf);
}

/**
 * attach_D_num - Attachment: Deleted - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_D_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->deleted;
}

/**
 * attach_D - Attachment: Deleted - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_D(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->deleted ? "D" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_e - Attachment: MIME type - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_e(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = ENCODING(aptr->body->encoding);
  buf_strcpy(buf, s);
}

/**
 * attach_f - Attachment: Filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_f(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->filename && (*aptr->body->filename == '/'))
  {
    struct Buffer *path = buf_pool_get();

    buf_strcpy(path, aptr->body->filename);
    buf_pretty_mailbox(path);
    buf_copy(buf, path);
    buf_pool_release(&path);
  }
  else
  {
    const char *s = aptr->body->filename;
    buf_strcpy(buf, s);
  }
}

/**
 * attach_F - Attachment: Filename in header - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_F(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->d_filename)
  {
    const char *s = aptr->body->d_filename;
    buf_strcpy(buf, s);
    return;
  }

  attach_f(node, data, flags, max_cols, buf);
}

/**
 * attach_I - Attachment: Disposition flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_I(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  static const char dispchar[] = { 'I', 'A', 'F', '-' };
  char ch;

  if (aptr->body->disposition < sizeof(dispchar))
  {
    ch = dispchar[aptr->body->disposition];
  }
  else
  {
    mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n", aptr->body->disposition);
    ch = '!';
  }

  buf_printf(buf, "%c", ch);
}

/**
 * attach_m - Attachment: Major MIME type - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_m(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = TYPE(aptr->body);
  buf_strcpy(buf, s);
}

/**
 * attach_M - Attachment: MIME subtype - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_M(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = aptr->body->subtype;
  buf_strcpy(buf, s);
}

/**
 * attach_n_num - Attachment: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;

  return aptr->num + 1;
}

/**
 * attach_Q_num - Attachment: Attachment counting - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_Q_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->attach_qualifies;
}

/**
 * attach_Q - Attachment: Attachment counting - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_Q(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->attach_qualifies ? "Q" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_s_num - Attachment: Size - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_s_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->filename && (flags & MUTT_FORMAT_STAT_FILE))
    return mutt_file_get_size(aptr->body->filename);

  return aptr->body->length;
}

/**
 * attach_s - Attachment: Size - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_s(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  size_t l = 0;
  if (aptr->body->filename && (flags & MUTT_FORMAT_STAT_FILE))
  {
    l = mutt_file_get_size(aptr->body->filename);
  }
  else
  {
    l = aptr->body->length;
  }

  buf_reset(buf);
  mutt_str_pretty_size(buf, l);
}

/**
 * attach_t_num - Attachment: Is Tagged - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->tagged;
}

/**
 * attach_t - Attachment: Is Tagged - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_t(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->tagged ? "*" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_T - Attachment: Tree characters - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_T(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  node_expando_set_color(node, MT_COLOR_TREE);
  node_expando_set_has_tree(node, true);
  const char *s = aptr->tree;
  buf_strcpy(buf, s);
}

/**
 * attach_u_num - Attachment: Unlink flag - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_u_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->unlink;
}

/**
 * attach_u - Attachment: Unlink flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void attach_u(const struct ExpandoNode *node, void *data, MuttFormatFlags flags,
              int max_cols, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->unlink ? "-" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_X_num - Attachment: Number of MIME parts - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long attach_X_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  const struct Body *body = aptr->body;

  return body->attach_count + body->attach_qualifies;
}

/**
 * attach_make_entry - Format an Attachment for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $attach_format
 */
static int attach_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  struct AttachPrivateData *priv = menu->mdata;
  struct AttachCtx *actx = priv->actx;

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  if (c_arrow_cursor)
  {
    const char *const c_arrow_string = cs_subset_string(menu->sub, "arrow_string");
    max_cols -= (mutt_strwidth(c_arrow_string) + 1);
  }

  const struct Expando *c_attach_format = cs_subset_expando(NeoMutt->sub, "attach_format");
  return expando_filter(c_attach_format, AttachRenderData, (actx->idx[actx->v2r[line]]),
                        MUTT_FORMAT_ARROWCURSOR, max_cols, buf);
}

/**
 * attach_tag - Tag an attachment - Implements Menu::tag() - @ingroup menu_tag
 */
static int attach_tag(struct Menu *menu, int sel, int act)
{
  struct AttachPrivateData *priv = menu->mdata;
  struct AttachCtx *actx = priv->actx;

  struct Body *cur = actx->idx[actx->v2r[sel]]->body;
  bool ot = cur->tagged;

  cur->tagged = ((act >= 0) ? act : !cur->tagged);
  return cur->tagged - ot;
}

/**
 * attach_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int attach_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->sub->notify, attach_config_observer, menu);
  notify_observer_remove(win_menu->notify, attach_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_attachment - Show the attachments in a Menu - @ingroup gui_dlg
 * @param sub        Config Subset
 * @param mv         Mailbox view
 * @param e          Email
 * @param fp         File with the content of the email, or NULL
 * @param attach_msg Are we in "attach message" mode?
 *
 * The Select Attachment dialog shows an Email's attachments.
 * They can be viewed using the Pager or Mailcap programs.
 * They can also be saved, printed, deleted, etc.
 */
void dlg_attachment(struct ConfigSubset *sub, struct MailboxView *mv,
                    struct Email *e, FILE *fp, bool attach_msg)
{
  if (!mv || !mv->mailbox || !e || !fp)
    return;

  struct Mailbox *m = mv->mailbox;

  /* make sure we have parsed this message */
  mutt_parse_mime_message(e, fp);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  struct MuttWindow *dlg = simple_dialog_new(MENU_ATTACHMENT, WT_DLG_ATTACHMENT, AttachmentHelp);
  struct Menu *menu = dlg->wdata;
  menu->make_entry = attach_make_entry;
  menu->tag = attach_tag;

  struct AttachCtx *actx = mutt_actx_new();
  actx->email = e;
  actx->fp_root = fp;
  mutt_update_recvattach_menu(actx, menu, true);

  struct AttachPrivateData *priv = attach_private_data_new();
  priv->menu = menu;
  priv->actx = actx;
  priv->sub = sub;
  priv->mailbox = m;
  priv->attach_msg = attach_msg;
  menu->mdata = priv;
  menu->mdata_free = attach_private_data_free;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, attach_config_observer, menu);
  notify_observer_add(menu->win->notify, NT_WINDOW, attach_window_observer, menu->win);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, _("Attachments"));

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int rc = 0;
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_ATTACHMENT, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_ATTACHMENT);
      continue;
    }
    mutt_clear_error();

    rc = attach_function_dispatcher(dlg, op);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);

    if (rc == FR_CONTINUE)
    {
      op = priv->op;
    }

  } while (rc != FR_DONE);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
}

/**
 * AttachRenderData - Callbacks for Attachment Expandos
 *
 * @sa AttachFormatDef, ExpandoDataAttach, ExpandoDataBody, ExpandoDataGlobal
 */
const struct ExpandoRenderData AttachRenderData[] = {
  // clang-format off
  { ED_ATTACH, ED_ATT_CHARSET,          attach_C,     NULL },
  { ED_BODY,   ED_BOD_CHARSET_CONVERT,  attach_c,     NULL },
  { ED_BODY,   ED_BOD_DELETED,          attach_D,     attach_D_num },
  { ED_BODY,   ED_BOD_DESCRIPTION,      attach_d,     NULL },
  { ED_BODY,   ED_BOD_MIME_ENCODING,    attach_e,     NULL },
  { ED_BODY,   ED_BOD_FILE,             attach_f,     NULL },
  { ED_BODY,   ED_BOD_FILE_DISPOSITION, attach_F,     NULL },
  { ED_BODY,   ED_BOD_DISPOSITION,      attach_I,     NULL },
  { ED_BODY,   ED_BOD_MIME_MAJOR,       attach_m,     NULL },
  { ED_BODY,   ED_BOD_MIME_MINOR,       attach_M,     NULL },
  { ED_ATTACH, ED_ATT_NUMBER,           NULL,         attach_n_num },
  { ED_BODY,   ED_BOD_ATTACH_QUALIFIES, attach_Q,     attach_Q_num },
  { ED_BODY,   ED_BOD_FILE_SIZE,        attach_s,     attach_s_num },
  { ED_BODY,   ED_BOD_TAGGED,           attach_t,     attach_t_num },
  { ED_ATTACH, ED_ATT_TREE,             attach_T,     NULL },
  { ED_BODY,   ED_BOD_UNLINK,           attach_u,     attach_u_num },
  { ED_BODY,   ED_BOD_ATTACH_COUNT,     NULL,         attach_X_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
