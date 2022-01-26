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
 * @page attach_dialog Attachment Selection Dialog
 *
 * The Attachment Selection Dialog lets the user select an email attachment.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                        | Type          | See Also                |
 * | :-------------------------- | :------------ | :---------------------- |
 * | Attachment Selection Dialog | WT_DLG_ATTACH | dlg_select_attachment() |
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
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attachments.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "mutt_attach.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "recvattach.h"
#include "recvcmd.h"

/// Help Bar for the Attachment selection dialog
static const struct Mapping AttachHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Save"),  OP_ATTACHMENT_SAVE },
  { N_("Pipe"),  OP_ATTACHMENT_PIPE },
  { N_("Print"), OP_ATTACHMENT_PRINT },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * attach_collapse - Close the tree of the current attachment
 * @param actx Attachment context
 * @param menu Menu listing Attachments
 */
static void attach_collapse(struct AttachCtx *actx, struct Menu *menu)
{
  int rindex, curlevel;

  struct AttachPtr *cur_att = current_attachment(actx, menu);
  cur_att->body->collapsed = !cur_att->body->collapsed;
  /* When expanding, expand all the children too */
  if (cur_att->body->collapsed)
    return;

  curlevel = cur_att->level;
  const int index = menu_get_index(menu);
  rindex = actx->v2r[index] + 1;

  const bool c_digest_collapse =
      cs_subset_bool(NeoMutt->sub, "digest_collapse");
  while ((rindex < actx->idxlen) && (actx->idx[rindex]->level > curlevel))
  {
    if (c_digest_collapse && (actx->idx[rindex]->body->type == TYPE_MULTIPART) &&
        mutt_istr_equal(actx->idx[rindex]->body->subtype, "digest"))
    {
      actx->idx[rindex]->body->collapsed = true;
    }
    else
    {
      actx->idx[rindex]->body->collapsed = false;
    }
    rindex++;
  }
}

/**
 * attach_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_attach`.
 */
static int attach_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
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
 * |:--------|:--------------------------------------------------------
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
  char fmt[128];
  char charset[128];
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
          mutt_format_s(buf, buflen, prec, "");
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
        optional = false;
      break;
    case 'd':
    {
      const char *const c_message_format =
          cs_subset_string(NeoMutt->sub, "message_format");
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
          char s[128];
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
    /* fallthrough */
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
    /* fallthrough */
    case 'f':
      if (!optional)
      {
        if (aptr->body->filename && (*aptr->body->filename == '/'))
        {
          struct Buffer *path = mutt_buffer_pool_get();

          mutt_buffer_strcpy(path, aptr->body->filename);
          mutt_buffer_pretty_mailbox(path);
          mutt_format_s(buf, buflen, prec, mutt_buffer_string(path));
          mutt_buffer_pool_release(&path);
        }
        else
          mutt_format_s(buf, buflen, prec, NONULL(aptr->body->filename));
      }
      else if (!aptr->body->filename)
        optional = false;
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
        ch = dispchar[aptr->body->disposition];
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
        optional = aptr->body->attach_qualifies;
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
        l = aptr->body->length;

      if (!optional)
      {
        char tmp[128];
        mutt_str_pretty_size(tmp, sizeof(tmp), l);
        mutt_format_s(buf, buflen, prec, tmp);
      }
      else if (l == 0)
        optional = false;

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
        optional = ((aptr->body->attach_count + aptr->body->attach_qualifies) != 0);
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
 * attach_make_entry - Format a menu item for the attachment list - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static void attach_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct AttachCtx *actx = menu->mdata;

  const char *const c_attach_format =
      cs_subset_string(NeoMutt->sub, "attach_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_attach_format),
                      attach_format_str, (intptr_t) (actx->idx[actx->v2r[line]]),
                      MUTT_FORMAT_ARROWCURSOR);
}

/**
 * attach_tag - Tag an attachment - Implements Menu::tag() - @ingroup menu_tag
 */
static int attach_tag(struct Menu *menu, int sel, int act)
{
  struct AttachCtx *actx = menu->mdata;
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
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->notify, attach_config_observer, menu);
  notify_observer_remove(win_menu->notify, attach_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * check_attach - Check if in attach-message mode
 * @retval true Mailbox is readonly
 */
static bool check_attach(void)
{
  if (OptAttachMsg)
  {
    mutt_flushinp();
    mutt_error(_("Function not permitted in attach-message mode"));
    return true;
  }

  return false;
}

/**
 * check_readonly - Check if the Mailbox is readonly
 * @param m Mailbox
 * @retval true Mailbox is readonly
 */
static bool check_readonly(struct Mailbox *m)
{
  if (!m || m->readonly)
  {
    mutt_flushinp();
    mutt_error(_("Mailbox is read-only"));
    return true;
  }

  return false;
}

/**
 * recvattach_extract_pgp_keys - Extract PGP keys from attachments
 * @param actx Attachment context
 * @param menu Menu listing attachments
 */
static void recvattach_extract_pgp_keys(struct AttachCtx *actx, struct Menu *menu)
{
  if (!menu->tagprefix)
  {
    struct AttachPtr *cur_att = current_attachment(actx, menu);
    crypt_pgp_extract_key_from_attachment(cur_att->fp, cur_att->body);
  }
  else
  {
    for (int i = 0; i < actx->idxlen; i++)
    {
      if (actx->idx[i]->body->tagged)
      {
        crypt_pgp_extract_key_from_attachment(actx->idx[i]->fp, actx->idx[i]->body);
      }
    }
  }
}

/**
 * recvattach_pgp_check_traditional - Is the Attachment inline PGP?
 * @param actx Attachment to check
 * @param menu Menu listing Attachments
 * @retval 1 The (tagged) Attachment(s) are inline PGP
 *
 * @note If the menu->tagprefix is set, all the tagged attachments will be checked.
 */
static int recvattach_pgp_check_traditional(struct AttachCtx *actx, struct Menu *menu)
{
  int rc = 0;

  if (!menu->tagprefix)
  {
    struct AttachPtr *cur_att = current_attachment(actx, menu);
    rc = crypt_pgp_check_traditional(cur_att->fp, cur_att->body, true);
  }
  else
  {
    for (int i = 0; i < actx->idxlen; i++)
      if (actx->idx[i]->body->tagged)
        rc = rc || crypt_pgp_check_traditional(actx->idx[i]->fp, actx->idx[i]->body, true);
  }

  return rc;
}

/**
 * dlg_select_attachment - Show the attachments in a Menu
 * @param sub Config Subset
 * @param m   Mailbox
 * @param e   Email
 * @param fp File with the content of the email, or NULL
 */
void dlg_select_attachment(struct ConfigSubset *sub, struct Mailbox *m,
                           struct Email *e, FILE *fp)
{
  if (!m || !e || !fp)
  {
    return;
  }

  int op = OP_NULL;

  /* make sure we have parsed this message */
  mutt_parse_mime_message(e, fp);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  struct MuttWindow *dlg = simple_dialog_new(MENU_ATTACH, WT_DLG_ATTACH, AttachHelp);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = attach_make_entry;
  menu->tag = attach_tag;

  struct MuttWindow *win_menu = menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, attach_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, attach_window_observer, win_menu);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, _("Attachments"));

  struct AttachCtx *actx = mutt_actx_new();
  actx->email = e;
  actx->fp_root = fp;
  mutt_update_recvattach_menu(actx, menu, true);

  while (true)
  {
    if (op == OP_NULL)
      op = menu_loop(menu);
    window_redraw(dlg);
    switch (op)
    {
      case OP_ATTACHMENT_VIEW_MAILCAP:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_MAILCAP, e,
                             actx, menu->win);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_ATTACHMENT_VIEW_TEXT:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_AS_TEXT, e,
                             actx, menu->win);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_ATTACHMENT_VIEW_PAGER:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_PAGER, e, actx, menu->win);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_DISPLAY_HEADERS:
      case OP_ATTACHMENT_VIEW:
        op = mutt_attach_display_loop(sub, menu, op, e, actx, true);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        continue;

      case OP_ATTACHMENT_COLLAPSE:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (!cur_att->body->parts)
        {
          mutt_error(_("There are no subparts to show"));
          break;
        }
        attach_collapse(actx, menu);
        mutt_update_recvattach_menu(actx, menu, false);
        break;
      }

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_EXTRACT_KEYS:
        if (WithCrypto & APPLICATION_PGP)
        {
          recvattach_extract_pgp_keys(actx, menu);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        break;

      case OP_CHECK_TRADITIONAL:
        if (((WithCrypto & APPLICATION_PGP) != 0) &&
            recvattach_pgp_check_traditional(actx, menu))
        {
          e->security = crypt_query(NULL);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        break;

      case OP_ATTACHMENT_PRINT:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_print_attachment_list(actx, cur_att->fp, menu->tagprefix, cur_att->body);
        break;
      }

      case OP_ATTACHMENT_PIPE:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_pipe_attachment_list(actx, cur_att->fp, menu->tagprefix, cur_att->body, false);
        break;
      }

      case OP_ATTACHMENT_SAVE:
      {
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_save_attachment_list(actx, cur_att->fp, menu->tagprefix,
                                  cur_att->body, e, menu);

        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        const int index = menu_get_index(menu) + 1;
        if (!menu->tagprefix && c_resolve && (index < menu->max))
          menu_set_index(menu, index);
        break;
      }

      case OP_ATTACHMENT_DELETE:
        if (check_readonly(m))
          break;

#ifdef USE_POP
        if (m->type == MUTT_POP)
        {
          mutt_flushinp();
          mutt_error(_("Can't delete attachment from POP server"));
          break;
        }
#endif

#ifdef USE_NNTP
        if (m->type == MUTT_NNTP)
        {
          mutt_flushinp();
          mutt_error(_("Can't delete attachment from news server"));
          break;
        }
#endif

        if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT))
        {
          mutt_message(_("Deletion of attachments from encrypted messages is "
                         "unsupported"));
          break;
        }
        if ((WithCrypto != 0) && (e->security & (SEC_SIGN | SEC_PARTSIGN)))
        {
          mutt_message(_("Deletion of attachments from signed messages may "
                         "invalidate the signature"));
        }
        if (!menu->tagprefix)
        {
          struct AttachPtr *cur_att = current_attachment(actx, menu);
          if (cur_att->parent_type == TYPE_MULTIPART)
          {
            cur_att->body->deleted = true;
            const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
            const int index = menu_get_index(menu) + 1;
            if (c_resolve && (index < menu->max))
            {
              menu_set_index(menu, index);
            }
            else
              menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
          }
          else
          {
            mutt_message(
                _("Only deletion of multipart attachments is supported"));
          }
        }
        else
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (actx->idx[i]->body->tagged)
            {
              if (actx->idx[i]->parent_type == TYPE_MULTIPART)
              {
                actx->idx[i]->body->deleted = true;
                menu_queue_redraw(menu, MENU_REDRAW_INDEX);
              }
              else
              {
                mutt_message(
                    _("Only deletion of multipart attachments is supported"));
              }
            }
          }
        }
        break;

      case OP_ATTACHMENT_UNDELETE:
        if (check_readonly(m))
          break;
        if (!menu->tagprefix)
        {
          struct AttachPtr *cur_att = current_attachment(actx, menu);
          cur_att->body->deleted = false;
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
          const int index = menu_get_index(menu) + 1;
          if (c_resolve && (index < menu->max))
          {
            menu_set_index(menu, index);
          }
          else
            menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        }
        else
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (actx->idx[i]->body->tagged)
            {
              actx->idx[i]->body->deleted = false;
              menu_queue_redraw(menu, MENU_REDRAW_INDEX);
            }
          }
        }
        break;

      case OP_RESEND:
      {
        if (check_attach())
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_resend(cur_att->fp, m, actx,
                           menu->tagprefix ? NULL : cur_att->body);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_BOUNCE_MESSAGE:
      {
        if (check_attach())
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_bounce(m, cur_att->fp, actx,
                           menu->tagprefix ? NULL : cur_att->body);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_FORWARD_MESSAGE:
      {
        if (check_attach())
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_forward(cur_att->fp, e, actx,
                            menu->tagprefix ? NULL : cur_att->body, SEND_NO_FLAGS);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

#ifdef USE_NNTP
      case OP_FORWARD_TO_GROUP:
      {
        if (check_attach())
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_forward(cur_att->fp, e, actx,
                            menu->tagprefix ? NULL : cur_att->body, SEND_NEWS);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_FOLLOWUP:
      {
        if (check_attach())
          break;

        const enum QuadOption c_followup_to_poster =
            cs_subset_quad(NeoMutt->sub, "followup_to_poster");
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (!cur_att->body->email->env->followup_to ||
            !mutt_istr_equal(cur_att->body->email->env->followup_to,
                             "poster") ||
            (query_quadoption(c_followup_to_poster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          mutt_attach_reply(cur_att->fp, m, e, actx,
                            menu->tagprefix ? NULL : cur_att->body, SEND_NEWS | SEND_REPLY);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
          break;
        }
      }
#endif
      /* fallthrough */
      case OP_REPLY:
      case OP_GROUP_REPLY:
      case OP_GROUP_CHAT_REPLY:
      case OP_LIST_REPLY:
      {
        if (check_attach())
          break;

        SendFlags flags = SEND_REPLY;
        if (op == OP_GROUP_REPLY)
          flags |= SEND_GROUP_REPLY;
        else if (op == OP_GROUP_CHAT_REPLY)
          flags |= SEND_GROUP_CHAT_REPLY;
        else if (op == OP_LIST_REPLY)
          flags |= SEND_LIST_REPLY;

        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_reply(cur_att->fp, m, e, actx,
                          menu->tagprefix ? NULL : cur_att->body, flags);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_LIST_SUBSCRIBE:
        if (!check_attach())
          mutt_send_list_subscribe(m, e);
        break;

      case OP_LIST_UNSUBSCRIBE:
        if (!check_attach())
          mutt_send_list_unsubscribe(m, e);
        break;

      case OP_COMPOSE_TO_SENDER:
      {
        if (check_attach())
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_attach_mail_sender(actx, menu->tagprefix ? NULL : cur_att->body);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        break;
      }

      case OP_ATTACHMENT_EDIT_TYPE:
        recvattach_edit_content_type(actx, menu, e);
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        break;

      case OP_EXIT:
        e->attach_del = false;
        for (int i = 0; i < actx->idxlen; i++)
        {
          if (actx->idx[i]->body && actx->idx[i]->body->deleted)
          {
            e->attach_del = true;
            break;
          }
        }
        if (e->attach_del)
          e->changed = true;

        mutt_actx_free(&actx);

        simple_dialog_free(&dlg);
        return;
    }

    op = OP_NULL;
  }

  /* not reached */
}
