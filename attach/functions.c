/**
 * @file
 * Attachment functions
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page attach_functions Attachment functions
 *
 * Attachment functions
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attach.h"
#include "globals.h" // IWYU pragma: keep
#include "mutt_attach.h"
#include "opcodes.h"
#include "private_data.h"
#include "recvattach.h"
#include "recvcmd.h"

/// Error message for unavailable functions
static const char *Not_available_in_this_menu = N_("Not available in this menu");
/// Error message for unavailable functions in attach mode
static const char *Function_not_permitted_in_attach_message_mode = N_(
    "Function not permitted in attach-message mode");

/**
 * attach_collapse - Close the tree of the current attachment
 * @param actx Attachment context
 * @param menu Menu listing Attachments
 */
static void attach_collapse(struct AttachCtx *actx, struct Menu *menu)
{
  int rindex, curlevel;

  struct AttachPtr *cur_att = current_attachment(actx, menu);
  cur_att->collapsed = !cur_att->collapsed;
  /* When expanding, expand all the children too */
  if (cur_att->collapsed)
    return;

  curlevel = cur_att->level;
  const int index = menu_get_index(menu);
  rindex = actx->v2r[index] + 1;

  const bool c_digest_collapse = cs_subset_bool(NeoMutt->sub, "digest_collapse");
  while ((rindex < actx->idxlen) && (actx->idx[rindex]->level > curlevel))
  {
    if (c_digest_collapse && (actx->idx[rindex]->body->type == TYPE_MULTIPART) &&
        mutt_istr_equal(actx->idx[rindex]->body->subtype, "digest"))
    {
      actx->idx[rindex]->collapsed = true;
    }
    else
    {
      actx->idx[rindex]->collapsed = false;
    }
    rindex++;
  }
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
    mutt_error(_(Function_not_permitted_in_attach_message_mode));
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
  if (!menu->tag_prefix)
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

  if (!menu->tag_prefix)
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

// -----------------------------------------------------------------------------

/**
 * op_attachment_collapse - toggle display of subparts - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_collapse(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  if (!cur_att->body->parts)
  {
    mutt_error(_("There are no subparts to show"));
    return FR_NO_ACTION;
  }
  attach_collapse(priv->actx, priv->menu);
  mutt_update_recvattach_menu(priv->actx, priv->menu, false);
  return FR_SUCCESS;
}

/**
 * op_attachment_delete - delete the current entry - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_delete(struct AttachPrivateData *priv, int op)
{
  if (check_readonly(priv->mailbox))
    return FR_ERROR;

#ifdef USE_POP
  if (priv->mailbox->type == MUTT_POP)
  {
    mutt_flushinp();
    mutt_error(_("Can't delete attachment from POP server"));
    return FR_ERROR;
  }
#endif

#ifdef USE_NNTP
  if (priv->mailbox->type == MUTT_NNTP)
  {
    mutt_flushinp();
    mutt_error(_("Can't delete attachment from news server"));
    return FR_ERROR;
  }
#endif

  if ((WithCrypto != 0) && (priv->actx->email->security & SEC_ENCRYPT))
  {
    mutt_message(_("Deletion of attachments from encrypted messages is unsupported"));
    return FR_ERROR;
  }
  if ((WithCrypto != 0) && (priv->actx->email->security & (SEC_SIGN | SEC_PARTSIGN)))
  {
    mutt_message(_("Deletion of attachments from signed messages may invalidate the signature"));
  }
  if (!priv->menu->tag_prefix)
  {
    struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
    if (cur_att->parent_type == TYPE_MULTIPART)
    {
      cur_att->body->deleted = true;
      const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
      const int index = menu_get_index(priv->menu) + 1;
      if (c_resolve && (index < priv->menu->max))
      {
        menu_set_index(priv->menu, index);
      }
      else
      {
        menu_queue_redraw(priv->menu, MENU_REDRAW_CURRENT);
      }
    }
    else
    {
      mutt_message(_("Only deletion of multipart attachments is supported"));
    }
  }
  else
  {
    for (int i = 0; i < priv->menu->max; i++)
    {
      if (priv->actx->idx[i]->body->tagged)
      {
        if (priv->actx->idx[i]->parent_type == TYPE_MULTIPART)
        {
          priv->actx->idx[i]->body->deleted = true;
          menu_queue_redraw(priv->menu, MENU_REDRAW_INDEX);
        }
        else
        {
          mutt_message(_("Only deletion of multipart attachments is supported"));
        }
      }
    }
  }

  return FR_SUCCESS;
}

/**
 * op_attachment_edit_type - edit attachment content type - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_edit_type(struct AttachPrivateData *priv, int op)
{
  recvattach_edit_content_type(priv->actx, priv->menu, priv->actx->email);
  menu_queue_redraw(priv->menu, MENU_REDRAW_INDEX);
  return FR_SUCCESS;
}

/**
 * op_attachment_pipe - pipe message/attachment to a shell command - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_pipe(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_pipe_attachment_list(priv->actx, cur_att->fp, priv->menu->tag_prefix,
                            cur_att->body, false);
  return FR_SUCCESS;
}

/**
 * op_attachment_print - print the current entry - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_print(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_print_attachment_list(priv->actx, cur_att->fp, priv->menu->tag_prefix,
                             cur_att->body);
  return FR_SUCCESS;
}

/**
 * op_attachment_save - save message/attachment to a mailbox/file - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_save(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_save_attachment_list(priv->actx, cur_att->fp, priv->menu->tag_prefix,
                            cur_att->body, priv->actx->email, priv->menu);

  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  const int index = menu_get_index(priv->menu) + 1;
  if (!priv->menu->tag_prefix && c_resolve && (index < priv->menu->max))
    menu_set_index(priv->menu, index);
  return FR_SUCCESS;
}

/**
 * op_attachment_undelete - undelete the current entry - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_undelete(struct AttachPrivateData *priv, int op)
{
  if (check_readonly(priv->mailbox))
    return FR_ERROR;
  if (!priv->menu->tag_prefix)
  {
    struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
    cur_att->body->deleted = false;
    const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
    const int index = menu_get_index(priv->menu) + 1;
    if (c_resolve && (index < priv->menu->max))
    {
      menu_set_index(priv->menu, index);
    }
    else
    {
      menu_queue_redraw(priv->menu, MENU_REDRAW_CURRENT);
    }
  }
  else
  {
    for (int i = 0; i < priv->menu->max; i++)
    {
      if (priv->actx->idx[i]->body->tagged)
      {
        priv->actx->idx[i]->body->deleted = false;
        menu_queue_redraw(priv->menu, MENU_REDRAW_INDEX);
      }
    }
  }
  return FR_SUCCESS;
}

/**
 * op_attachment_view - view attachment using mailcap entry if necessary - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_view(struct AttachPrivateData *priv, int op)
{
  priv->op = mutt_attach_display_loop(priv->sub, priv->menu, op,
                                      priv->actx->email, priv->actx, true);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);

  return FR_CONTINUE;
}

/**
 * op_attachment_view_mailcap - force viewing of attachment using mailcap - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_view_mailcap(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_MAILCAP,
                       priv->actx->email, priv->actx, priv->menu->win);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_attachment_view_pager - view attachment in pager using copiousoutput mailcap - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_view_pager(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_PAGER,
                       priv->actx->email, priv->actx, priv->menu->win);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_attachment_view_text - view attachment as text - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_attachment_view_text(struct AttachPrivateData *priv, int op)
{
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_view_attachment(cur_att->fp, cur_att->body, MUTT_VA_AS_TEXT,
                       priv->actx->email, priv->actx, priv->menu->win);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_bounce_message - remail a message to another user - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_bounce_message(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  attach_bounce_message(priv->mailbox, cur_att->fp, priv->actx,
                        priv->menu->tag_prefix ? NULL : cur_att->body);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_check_traditional - check for classic PGP - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_check_traditional(struct AttachPrivateData *priv, int op)
{
  if (((WithCrypto & APPLICATION_PGP) != 0) &&
      recvattach_pgp_check_traditional(priv->actx, priv->menu))
  {
    priv->actx->email->security = crypt_query(NULL);
    menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  }
  return FR_SUCCESS;
}

/**
 * op_compose_to_sender - compose new message to the current message sender - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_compose_to_sender(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_attach_mail_sender(priv->actx, priv->menu->tag_prefix ? NULL : cur_att->body);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_exit - exit this menu - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_exit(struct AttachPrivateData *priv, int op)
{
  priv->actx->email->attach_del = false;
  for (int i = 0; i < priv->actx->idxlen; i++)
  {
    if (priv->actx->idx[i]->body && priv->actx->idx[i]->body->deleted)
    {
      priv->actx->email->attach_del = true;
      break;
    }
  }
  if (priv->actx->email->attach_del)
    priv->actx->email->changed = true;

  mutt_actx_free(&priv->actx);
  return FR_DONE;
}

/**
 * op_extract_keys - extract supported public keys - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_extract_keys(struct AttachPrivateData *priv, int op)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return FR_NO_ACTION;

  recvattach_extract_pgp_keys(priv->actx, priv->menu);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);

  return FR_SUCCESS;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_forget_passphrase(struct AttachPrivateData *priv, int op)
{
  crypt_forget_passphrase();
  return FR_SUCCESS;
}

/**
 * op_forward_message - forward a message with comments - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_forward_message(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_attach_forward(cur_att->fp, priv->actx->email, priv->actx,
                      priv->menu->tag_prefix ? NULL : cur_att->body, SEND_NO_FLAGS);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_list_subscribe - subscribe to a mailing list - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_list_subscribe(struct AttachPrivateData *priv, int op)
{
  if (!check_attach())
    mutt_send_list_subscribe(priv->mailbox, priv->actx->email);
  return FR_SUCCESS;
}

/**
 * op_list_unsubscribe - unsubscribe from a mailing list - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_list_unsubscribe(struct AttachPrivateData *priv, int op)
{
  if (!check_attach())
    mutt_send_list_unsubscribe(priv->mailbox, priv->actx->email);
  return FR_SUCCESS;
}

/**
 * op_reply - reply to a message - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_reply(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;

  SendFlags flags = SEND_REPLY;
  if (op == OP_GROUP_REPLY)
    flags |= SEND_GROUP_REPLY;
  else if (op == OP_GROUP_CHAT_REPLY)
    flags |= SEND_GROUP_CHAT_REPLY;
  else if (op == OP_LIST_REPLY)
    flags |= SEND_LIST_REPLY;

  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_attach_reply(cur_att->fp, priv->mailbox, priv->actx->email, priv->actx,
                    priv->menu->tag_prefix ? NULL : cur_att->body, flags);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

/**
 * op_resend - use the current message as a template for a new one - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_resend(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_attach_resend(cur_att->fp, priv->mailbox, priv->actx,
                     priv->menu->tag_prefix ? NULL : cur_att->body);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_followup - followup to newsgroup - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_followup(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;

  const enum QuadOption c_followup_to_poster = cs_subset_quad(NeoMutt->sub, "followup_to_poster");
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  if (!cur_att->body->email->env->followup_to ||
      !mutt_istr_equal(cur_att->body->email->env->followup_to, "poster") ||
      (query_quadoption(c_followup_to_poster, _("Reply by mail as poster prefers?")) != MUTT_YES))
  {
    mutt_attach_reply(cur_att->fp, priv->mailbox, priv->actx->email, priv->actx,
                      priv->menu->tag_prefix ? NULL : cur_att->body, SEND_NEWS | SEND_REPLY);
    menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
    return FR_SUCCESS;
  }

  return op_reply(priv, op);
}

/**
 * op_forward_to_group - forward to newsgroup - Implements ::attach_function_t - @ingroup attach_function_api
 */
static int op_forward_to_group(struct AttachPrivateData *priv, int op)
{
  if (check_attach())
    return FR_ERROR;
  struct AttachPtr *cur_att = current_attachment(priv->actx, priv->menu);
  mutt_attach_forward(cur_att->fp, priv->actx->email, priv->actx,
                      priv->menu->tag_prefix ? NULL : cur_att->body, SEND_NEWS);
  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  return FR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * AttachFunctions - All the NeoMutt functions that the Attach supports
 */
static const struct AttachFunction AttachFunctions[] = {
  // clang-format off
  { OP_ATTACHMENT_COLLAPSE,         op_attachment_collapse },
  { OP_ATTACHMENT_DELETE,           op_attachment_delete },
  { OP_ATTACHMENT_EDIT_TYPE,        op_attachment_edit_type },
  { OP_PIPE,                        op_attachment_pipe },
  { OP_ATTACHMENT_PRINT,            op_attachment_print },
  { OP_ATTACHMENT_SAVE,             op_attachment_save },
  { OP_ATTACHMENT_UNDELETE,         op_attachment_undelete },
  { OP_ATTACHMENT_VIEW,             op_attachment_view },
  { OP_ATTACHMENT_VIEW_MAILCAP,     op_attachment_view_mailcap },
  { OP_ATTACHMENT_VIEW_PAGER,       op_attachment_view_pager },
  { OP_ATTACHMENT_VIEW_TEXT,        op_attachment_view_text },
  { OP_BOUNCE_MESSAGE,              op_bounce_message },
  { OP_CHECK_TRADITIONAL,           op_check_traditional },
  { OP_COMPOSE_TO_SENDER,           op_compose_to_sender },
  { OP_DISPLAY_HEADERS,             op_attachment_view },
  { OP_EXIT,                        op_exit },
  { OP_EXTRACT_KEYS,                op_extract_keys },
#ifdef USE_NNTP
  { OP_FOLLOWUP,                    op_followup },
#endif
  { OP_FORGET_PASSPHRASE,           op_forget_passphrase },
  { OP_FORWARD_MESSAGE,             op_forward_message },
#ifdef USE_NNTP
  { OP_FORWARD_TO_GROUP,            op_forward_to_group },
#endif
  { OP_GROUP_CHAT_REPLY,            op_reply },
  { OP_GROUP_REPLY,                 op_reply },
  { OP_LIST_REPLY,                  op_reply },
  { OP_LIST_SUBSCRIBE,              op_list_subscribe },
  { OP_LIST_UNSUBSCRIBE,            op_list_unsubscribe },
  { OP_REPLY,                       op_reply },
  { OP_RESEND,                      op_resend },
  { 0, NULL },
  // clang-format on
};

/**
 * attach_function_dispatcher - Perform a Attach function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int attach_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win)
  {
    mutt_error(_(Not_available_in_this_menu));
    return FR_ERROR;
  }

  struct Menu *menu = win->wdata;
  struct AttachPrivateData *priv = menu->mdata;
  if (!priv)
    return FR_ERROR;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; AttachFunctions[i].op != OP_NULL; i++)
  {
    const struct AttachFunction *fn = &AttachFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(priv, op);
      break;
    }
  }

  return rc;
}
