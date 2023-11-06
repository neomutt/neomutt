/**
 * @file
 * Attachment Selection Dialog
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2006 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "attach.h"
#include "attachments.h"
#include "format_flags.h"
#include "functions.h"
#include "hdrline.h"
#include "hook.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mview.h"
#include "private_data.h"
#include "recvattach.h"

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
 * attach_format_str - Format a string for the attachment menu - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%C     | Character set
 * | \%c     | Character set: convert?
 * | \%D     | Deleted flag
 * | \%d     | Description
 * | \%e     | MIME content-transfer-encoding
 * | \%f     | Filename
 * | \%F     | Filename for content-disposition header
 * | \%I     | Content-disposition, either I (inline) or A (attachment)
 * | \%m     | Major MIME type
 * | \%M     | MIME subtype
 * | \%n     | Attachment number
 * | \%Q     | 'Q', if MIME part qualifies for attachment counting
 * | \%s     | Size
 * | \%t     | Tagged flag
 * | \%T     | Tree chars
 * | \%u     | Unlink
 * | \%X     | Number of qualifying MIME parts in this part and its children
 */
const char *attach_format_str(char *buf, size_t buflen, size_t col, int cols, char op,
                              const char *src, const char *prec, const char *if_str,
                              const char *else_str, intptr_t data, MuttFormatFlags flags)
{
  char fmt[128] = { 0 };
  char charset[128] = { 0 };
  struct AttachPtr *aptr = (struct AttachPtr *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
      if (!optional)
      {
        if (mutt_is_text_part(aptr->body) &&
            mutt_body_get_charset(aptr->body, charset, sizeof(charset)))
        {
          mutt_format_s(buf, buflen, prec, charset);
        }
        else
        {
          mutt_format_s(buf, buflen, prec, "");
        }
      }
      else if (!mutt_is_text_part(aptr->body) ||
               !mutt_body_get_charset(aptr->body, charset, sizeof(charset)))
      {
        optional = false;
      }
      break;
    case 'c':
      /* XXX */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt,
                 ((aptr->body->type != TYPE_TEXT) || aptr->body->noconv) ? 'n' : 'c');
      }
      else if ((aptr->body->type != TYPE_TEXT) || aptr->body->noconv)
      {
        optional = false;
      }
      break;
    case 'd':
    {
      const char *const c_message_format = cs_subset_string(NeoMutt->sub, "message_format");
      if (!optional)
      {
        if (aptr->body->description)
        {
          mutt_format_s(buf, buflen, prec, aptr->body->description);
          break;
        }
        if (mutt_is_message_type(aptr->body->type, aptr->body->subtype) &&
            c_message_format && aptr->body->email)
        {
          char s[128] = { 0 };
          mutt_make_string(s, sizeof(s), cols, c_message_format, NULL, -1,
                           aptr->body->email,
                           MUTT_FORMAT_FORCESUBJ | MUTT_FORMAT_ARROWCURSOR, NULL);
          if (*s)
          {
            mutt_format_s(buf, buflen, prec, s);
            break;
          }
        }
        if (!aptr->body->d_filename && !aptr->body->filename)
        {
          mutt_format_s(buf, buflen, prec, "<no description>");
          break;
        }
      }
      else if (aptr->body->description ||
               (mutt_is_message_type(aptr->body->type, aptr->body->subtype) &&
                c_message_format && aptr->body->email))
      {
        break;
      }
    }
      FALLTHROUGH;

    case 'F':
      if (!optional)
      {
        if (aptr->body->d_filename)
        {
          mutt_format_s(buf, buflen, prec, aptr->body->d_filename);
          break;
        }
      }
      else if (!aptr->body->d_filename && !aptr->body->filename)
      {
        optional = false;
        break;
      }
      FALLTHROUGH;

    case 'f':
      if (!optional)
      {
        if (aptr->body->filename && (*aptr->body->filename == '/'))
        {
          struct Buffer *path = buf_pool_get();

          buf_strcpy(path, aptr->body->filename);
          buf_pretty_mailbox(path);
          mutt_format_s(buf, buflen, prec, buf_string(path));
          buf_pool_release(&path);
        }
        else
        {
          mutt_format_s(buf, buflen, prec, NONULL(aptr->body->filename));
        }
      }
      else if (!aptr->body->filename)
      {
        optional = false;
      }
      break;
    case 'D':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->body->deleted ? 'D' : ' ');
      else if (!aptr->body->deleted)
        optional = false;
      break;
    case 'e':
      if (!optional)
        mutt_format_s(buf, buflen, prec, ENCODING(aptr->body->encoding));
      break;
    case 'I':
      if (optional)
        break;

      const char dispchar[] = { 'I', 'A', 'F', '-' };
      char ch;

      if (aptr->body->disposition < sizeof(dispchar))
      {
        ch = dispchar[aptr->body->disposition];
      }
      else
      {
        mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n",
                   aptr->body->disposition);
        ch = '!';
      }
      snprintf(buf, buflen, "%c", ch);
      break;
    case 'm':
      if (!optional)
        mutt_format_s(buf, buflen, prec, TYPE(aptr->body));
      break;
    case 'M':
      if (!optional)
        mutt_format_s(buf, buflen, prec, aptr->body->subtype);
      else if (!aptr->body->subtype)
        optional = false;
      break;
    case 'n':
      if (optional)
        break;

      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, aptr->num + 1);
      break;
    case 'Q':
      if (optional)
      {
        optional = aptr->body->attach_qualifies;
      }
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        mutt_format_s(buf, buflen, fmt, "Q");
      }
      break;
    case 's':
    {
      size_t l = 0;
      if (aptr->body->filename && (flags & MUTT_FORMAT_STAT_FILE))
      {
        l = mutt_file_get_size(aptr->body->filename);
      }
      else
      {
        l = aptr->body->length;
      }

      if (!optional)
      {
        char tmp[128] = { 0 };
        mutt_str_pretty_size(tmp, sizeof(tmp), l);
        mutt_format_s(buf, buflen, prec, tmp);
      }
      else if (l == 0)
      {
        optional = false;
      }

      break;
    }
    case 't':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->body->tagged ? '*' : ' ');
      else if (!aptr->body->tagged)
        optional = false;
      break;
    case 'T':
      if (!optional)
        mutt_format_s_tree(buf, buflen, prec, NONULL(aptr->tree));
      else if (!aptr->tree)
        optional = false;
      break;
    case 'u':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->body->unlink ? '-' : ' ');
      else if (!aptr->body->unlink)
        optional = false;
      break;
    case 'X':
      if (optional)
      {
        optional = ((aptr->body->attach_count + aptr->body->attach_qualifies) != 0);
      }
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, aptr->body->attach_count + aptr->body->attach_qualifies);
      }
      break;
    default:
      *buf = '\0';
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, attach_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, attach_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * attach_make_entry - Format an Attachment for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $attach_format, attach_format_str()
 */
static void attach_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct AttachPrivateData *priv = menu->mdata;
  struct AttachCtx *actx = priv->actx;

  const char *const c_attach_format = cs_subset_string(NeoMutt->sub, "attach_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_attach_format),
                      attach_format_str, (intptr_t) (actx->idx[actx->v2r[line]]),
                      MUTT_FORMAT_ARROWCURSOR);
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
