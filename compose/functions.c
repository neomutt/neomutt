/**
 * @file
 * Compose functions
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page compose_functions Compose functions
 *
 * Compose functions
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "attach/lib.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif
#include "browser/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attach_data.h"
#include "commands.h"
#include "context.h"
#include "env_data.h"
#include "hook.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "rfc3676.h"
#include "shared_data.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/adata.h" // IWYU pragma: keep
#endif
#ifdef USE_POP
#include "pop/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");

/**
 * check_count - Check if there are any attachments
 * @param actx Attachment context
 * @retval true There are attachments
 */
static bool check_count(struct AttachCtx *actx)
{
  if (actx->idxlen == 0)
  {
    mutt_error(_("There are no attachments"));
    return false;
  }

  return true;
}

#ifdef USE_AUTOCRYPT
/**
 * autocrypt_compose_menu - Autocrypt compose settings
 * @param e Email
 * @param sub ConfigSubset
 */
static void autocrypt_compose_menu(struct Email *e, const struct ConfigSubset *sub)
{
  /* L10N: The compose menu autocrypt prompt.
     (e)ncrypt enables encryption via autocrypt.
     (c)lear sets cleartext.
     (a)utomatic defers to the recommendation.  */
  const char *prompt = _("Autocrypt: (e)ncrypt, (c)lear, (a)utomatic?");

  e->security |= APPLICATION_PGP;

  /* L10N: The letter corresponding to the compose menu autocrypt prompt
     (e)ncrypt, (c)lear, (a)utomatic */
  const char *letters = _("eca");

  int choice = mutt_multi_choice(prompt, letters);
  switch (choice)
  {
    case 1:
      e->security |= (SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE);
      e->security &= ~(SEC_ENCRYPT | SEC_SIGN | SEC_OPPENCRYPT | SEC_INLINE);
      break;
    case 2:
      e->security &= ~SEC_AUTOCRYPT;
      e->security |= SEC_AUTOCRYPT_OVERRIDE;
      break;
    case 3:
    {
      e->security &= ~SEC_AUTOCRYPT_OVERRIDE;
      const bool c_crypt_opportunistic_encrypt =
          cs_subset_bool(sub, "crypt_opportunistic_encrypt");
      if (c_crypt_opportunistic_encrypt)
        e->security |= SEC_OPPENCRYPT;
      break;
    }
  }
}
#endif

/**
 * update_crypt_info - Update the crypto info
 * @param shared Shared compose data
 */
void update_crypt_info(struct ComposeSharedData *shared)
{
  struct Email *e = shared->email;

  const bool c_crypt_opportunistic_encrypt =
      cs_subset_bool(shared->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt)
    crypt_opportunistic_encrypt(e);

#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(shared->sub, "autocrypt");
  if (c_autocrypt)
  {
    struct ComposeEnvelopeData *edata = shared->edata;
    edata->autocrypt_rec = mutt_autocrypt_ui_recommendation(e, NULL);

    /* Anything that enables SEC_ENCRYPT or SEC_SIGN, or turns on SMIME
     * overrides autocrypt, be it oppenc or the user having turned on
     * those flags manually. */
    if (e->security & (SEC_ENCRYPT | SEC_SIGN | APPLICATION_SMIME))
      e->security &= ~(SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE);
    else
    {
      if (!(e->security & SEC_AUTOCRYPT_OVERRIDE))
      {
        if (edata->autocrypt_rec == AUTOCRYPT_REC_YES)
        {
          e->security |= (SEC_AUTOCRYPT | APPLICATION_PGP);
          e->security &= ~(SEC_INLINE | APPLICATION_SMIME);
        }
        else
          e->security &= ~SEC_AUTOCRYPT;
      }
    }
  }
#endif
}

/**
 * check_attachments - Check if any attachments have changed or been deleted
 * @param actx Attachment context
 * @param sub  ConfigSubset
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_attachments(struct AttachCtx *actx, struct ConfigSubset *sub)
{
  int rc = -1;
  struct stat st = { 0 };
  struct Buffer *pretty = NULL, *msg = NULL;

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body->type == TYPE_MULTIPART)
      continue;
    if (stat(actx->idx[i]->body->filename, &st) != 0)
    {
      if (!pretty)
        pretty = mutt_buffer_pool_get();
      mutt_buffer_strcpy(pretty, actx->idx[i]->body->filename);
      mutt_buffer_pretty_mailbox(pretty);
      /* L10N: This message is displayed in the compose menu when an attachment
         doesn't stat.  %d is the attachment number and %s is the attachment
         filename.  The filename is located last to avoid a long path hiding
         the error message.  */
      mutt_error(_("Attachment #%d no longer exists: %s"), i + 1,
                 mutt_buffer_string(pretty));
      goto cleanup;
    }

    if (actx->idx[i]->body->stamp < st.st_mtime)
    {
      if (!pretty)
        pretty = mutt_buffer_pool_get();
      mutt_buffer_strcpy(pretty, actx->idx[i]->body->filename);
      mutt_buffer_pretty_mailbox(pretty);

      if (!msg)
        msg = mutt_buffer_pool_get();
      /* L10N: This message is displayed in the compose menu when an attachment
         is modified behind the scenes.  %d is the attachment number and %s is
         the attachment filename.  The filename is located last to avoid a long
         path hiding the prompt question.  */
      mutt_buffer_printf(msg, _("Attachment #%d modified. Update encoding for %s?"),
                         i + 1, mutt_buffer_string(pretty));

      enum QuadOption ans = mutt_yesorno(mutt_buffer_string(msg), MUTT_YES);
      if (ans == MUTT_YES)
        mutt_update_encoding(actx->idx[i]->body, sub);
      else if (ans == MUTT_ABORT)
        goto cleanup;
    }
  }

  rc = 0;

cleanup:
  mutt_buffer_pool_release(&pretty);
  mutt_buffer_pool_release(&msg);
  return rc;
}

/**
 * edit_address_list - Let the user edit the address list
 * @param[in]     field Field to edit, e.g. #HDR_FROM
 * @param[in,out] al    AddressList to edit
 * @retval true The address list was changed
 */
static bool edit_address_list(int field, struct AddressList *al)
{
  char buf[8192] = { 0 }; /* needs to be large for alias expansion */
  char old_list[8192] = { 0 };

  mutt_addrlist_to_local(al);
  mutt_addrlist_write(al, buf, sizeof(buf), false);
  mutt_str_copy(old_list, buf, sizeof(buf));
  if (mutt_get_field(_(Prompts[field]), buf, sizeof(buf), MUTT_COMP_ALIAS,
                     false, NULL, NULL) == 0)
  {
    mutt_addrlist_clear(al);
    mutt_addrlist_parse2(al, buf);
    mutt_expand_aliases(al);
  }

  char *err = NULL;
  if (mutt_addrlist_to_intl(al, &err) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    mutt_refresh();
    FREE(&err);
  }

  return !mutt_str_equal(buf, old_list);
}

/**
 * delete_attachment - Delete an attachment
 * @param actx Attachment context
 * @param x    Index number of attachment
 * @retval  0 Success
 * @retval -1 Error
 */
static int delete_attachment(struct AttachCtx *actx, int x)
{
  if (!actx || (x < 0) || (x >= actx->idxlen))
    return -1;

  struct AttachPtr **idx = actx->idx;
  int rindex = actx->v2r[x];

  if ((rindex == 0) && (actx->idxlen == 1))
  {
    mutt_error(_("You may not delete the only attachment"));
    idx[rindex]->body->tagged = false;
    return -1;
  }

  for (int y = 0; y < actx->idxlen; y++)
  {
    if (idx[y]->body->next == idx[rindex]->body)
    {
      idx[y]->body->next = idx[rindex]->body->next;
      break;
    }
  }

  idx[rindex]->body->next = NULL;
  /* mutt_make_message_attach() creates body->parts, shared by
   * body->email->body.  If we NULL out that, it creates a memory
   * leak because mutt_free_body() frees body->parts, not
   * body->email->body.
   *
   * Other mutt_send_message() message constructors are careful to free
   * any body->parts, removing depth:
   *  - mutt_prepare_template() used by postponed, resent, and draft files
   *  - mutt_copy_body() used by the recvattach menu and $forward_attachments.
   *
   * I believe it is safe to completely remove the "body->parts =
   * NULL" statement.  But for safety, am doing so only for the case
   * it must be avoided: message attachments.
   */
  if (!idx[rindex]->body->email)
    idx[rindex]->body->parts = NULL;
  mutt_body_free(&(idx[rindex]->body));
  FREE(&idx[rindex]->tree);
  FREE(&idx[rindex]);
  for (; rindex < actx->idxlen - 1; rindex++)
    idx[rindex] = idx[rindex + 1];
  idx[actx->idxlen - 1] = NULL;
  actx->idxlen--;

  return 0;
}

/**
 * update_idx - Add a new attachment to the message
 * @param menu Current menu
 * @param actx Attachment context
 * @param ap   Attachment to add
 */
static void update_idx(struct Menu *menu, struct AttachCtx *actx, struct AttachPtr *ap)
{
  ap->level = (actx->idxlen > 0) ? actx->idx[actx->idxlen - 1]->level : 0;
  if (actx->idxlen)
    actx->idx[actx->idxlen - 1]->body->next = ap->body;
  ap->body->aptr = ap;
  mutt_actx_add_attach(actx, ap);
  update_menu(actx, menu, false);
  menu_set_index(menu, actx->vcount - 1);
}

/**
 * compose_attach_swap - Swap two adjacent entries in the attachment list
 * @param[in]  msg   Body of email
 * @param[out] idx   Array of Attachments
 * @param[in]  first Index of first attachment to swap
 */
static void compose_attach_swap(struct Body *msg, struct AttachPtr **idx, short first)
{
  /* Reorder Body pointers.
   * Must traverse msg from top since Body has no previous ptr.  */
  for (struct Body *part = msg; part; part = part->next)
  {
    if (part->next == idx[first]->body)
    {
      idx[first]->body->next = idx[first + 1]->body->next;
      idx[first + 1]->body->next = idx[first]->body;
      part->next = idx[first + 1]->body;
      break;
    }
  }

  /* Reorder index */
  struct AttachPtr *saved = idx[first];
  idx[first] = idx[first + 1];
  idx[first + 1] = saved;

  /* Swap ptr->num */
  int i = idx[first]->num;
  idx[first]->num = idx[first + 1]->num;
  idx[first + 1]->num = i;
}

// -----------------------------------------------------------------------------

/**
 * op_compose_attach_file - Attach files to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_attach_file(struct ComposeSharedData *shared, int op)
{
  char *prompt = _("Attach file");
  int numfiles = 0;
  char **files = NULL;

  struct Buffer *fname = mutt_buffer_pool_get();
  if ((mutt_buffer_enter_fname(prompt, fname, false, NULL, true, &files,
                               &numfiles, MUTT_SEL_MULTI) == -1) ||
      mutt_buffer_is_empty(fname))
  {
    for (int i = 0; i < numfiles; i++)
      FREE(&files[i]);

    FREE(&files);
    mutt_buffer_pool_release(&fname);
    return IR_NO_ACTION;
  }

  bool error = false;
  bool added_attachment = false;
  if (numfiles > 1)
  {
    mutt_message(ngettext("Attaching selected file...",
                          "Attaching selected files...", numfiles));
  }
  for (int i = 0; i < numfiles; i++)
  {
    char *att = files[i];
    if (!att)
      continue;

    struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
    ap->unowned = true;
    ap->body = mutt_make_file_attach(att, shared->sub);
    if (ap->body)
    {
      added_attachment = true;
      update_idx(shared->adata->menu, shared->adata->actx, ap);
    }
    else
    {
      error = true;
      mutt_error(_("Unable to attach %s"), att);
      FREE(&ap);
    }
    FREE(&files[i]);
  }

  FREE(&files);
  mutt_buffer_pool_release(&fname);

  if (!error)
    mutt_clear_error();

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  if (added_attachment)
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_attach_key - Attach a PGP public key - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_attach_key(struct ComposeSharedData *shared, int op)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return IR_NOT_IMPL;
  struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
  ap->body = crypt_pgp_make_key_attachment();
  if (ap->body)
  {
    update_idx(shared->adata->menu, shared->adata->actx, ap);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  }
  else
    FREE(&ap);

  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_attach_message - Attach messages to this message - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_COMPOSE_ATTACH_MESSAGE
 * - OP_COMPOSE_ATTACH_NEWS_MESSAGE
 */
static int op_compose_attach_message(struct ComposeSharedData *shared, int op)
{
  char *prompt = _("Open mailbox to attach message from");

#ifdef USE_NNTP
  OptNews = false;
  if (shared->mailbox && (op == OP_COMPOSE_ATTACH_NEWS_MESSAGE))
  {
    const char *const c_news_server =
        cs_subset_string(shared->sub, "news_server");
    CurrentNewsSrv = nntp_select_server(shared->mailbox, c_news_server, false);
    if (!CurrentNewsSrv)
      return IR_NO_ACTION;

    prompt = _("Open newsgroup to attach message from");
    OptNews = true;
  }
#endif

  struct Buffer *fname = mutt_buffer_pool_get();
  if (shared->mailbox)
  {
#ifdef USE_NNTP
    if ((op == OP_COMPOSE_ATTACH_MESSAGE) ^ (shared->mailbox->type == MUTT_NNTP))
#endif
    {
      mutt_buffer_strcpy(fname, mailbox_path(shared->mailbox));
      mutt_buffer_pretty_mailbox(fname);
    }
  }

  if ((mutt_buffer_enter_fname(prompt, fname, true, shared->mailbox, false,
                               NULL, NULL, MUTT_SEL_NO_FLAGS) == -1) ||
      mutt_buffer_is_empty(fname))
  {
    mutt_buffer_pool_release(&fname);
    return IR_NO_ACTION;
  }

#ifdef USE_NNTP
  if (OptNews)
    nntp_expand_path(fname->data, fname->dsize, &CurrentNewsSrv->conn->account);
  else
#endif
    mutt_buffer_expand_path(fname);
#ifdef USE_IMAP
  if (imap_path_probe(mutt_buffer_string(fname), NULL) != MUTT_IMAP)
#endif
#ifdef USE_POP
    if (pop_path_probe(mutt_buffer_string(fname), NULL) != MUTT_POP)
#endif
#ifdef USE_NNTP
      if (!OptNews && (nntp_path_probe(mutt_buffer_string(fname), NULL) != MUTT_NNTP))
#endif
        if (mx_path_probe(mutt_buffer_string(fname)) != MUTT_NOTMUCH)
        {
          /* check to make sure the file exists and is readable */
          if (access(mutt_buffer_string(fname), R_OK) == -1)
          {
            mutt_perror(mutt_buffer_string(fname));
            mutt_buffer_pool_release(&fname);
            return IR_ERROR;
          }
        }

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);

  struct Mailbox *m_attach = mx_path_resolve(mutt_buffer_string(fname));
  const bool old_readonly = m_attach->readonly;
  if (!mx_mbox_open(m_attach, MUTT_READONLY))
  {
    mutt_error(_("Unable to open mailbox %s"), mutt_buffer_string(fname));
    mx_fastclose_mailbox(m_attach);
    m_attach = NULL;
    mutt_buffer_pool_release(&fname);
    return IR_ERROR;
  }
  mutt_buffer_pool_release(&fname);

  if (m_attach->msg_count == 0)
  {
    mx_mbox_close(m_attach);
    mutt_error(_("No messages in that folder"));
    return IR_NO_ACTION;
  }

  /* `$sort`, `$sort_aux`, `$use_threads` could be changed in mutt_index_menu() */
  const enum SortType old_sort = cs_subset_sort(shared->sub, "sort");
  const enum SortType old_sort_aux = cs_subset_sort(shared->sub, "sort_aux");
  const unsigned char old_use_threads =
      cs_subset_enum(shared->sub, "use_threads");

  OptAttachMsg = true;
  mutt_message(_("Tag the messages you want to attach"));
  struct MuttWindow *dlg_index = index_pager_init();
  dialog_push(dlg_index);
  struct Mailbox *m_attach_new = mutt_index_menu(dlg_index, m_attach);
  dialog_pop();
  mutt_window_free(&dlg_index);
  OptAttachMsg = false;

  if (!shared->mailbox)
  {
    /* Restore old $sort variables */
    cs_subset_str_native_set(shared->sub, "sort", old_sort, NULL);
    cs_subset_str_native_set(shared->sub, "sort_aux", old_sort_aux, NULL);
    cs_subset_str_native_set(shared->sub, "use_threads", old_use_threads, NULL);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
    notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
    return IR_SUCCESS;
  }

  bool added_attachment = false;
  for (int i = 0; i < m_attach_new->msg_count; i++)
  {
    if (!m_attach_new->emails[i])
      break;
    if (!message_is_tagged(m_attach_new->emails[i]))
      continue;

    struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
    ap->body = mutt_make_message_attach(m_attach_new, m_attach_new->emails[i],
                                        true, shared->sub);
    if (ap->body)
    {
      added_attachment = true;
      update_idx(shared->adata->menu, shared->adata->actx, ap);
    }
    else
    {
      mutt_error(_("Unable to attach"));
      FREE(&ap);
    }
  }
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);

  if (m_attach_new == m_attach)
  {
    m_attach->readonly = old_readonly;
  }
  mx_fastclose_mailbox(m_attach_new);

  /* Restore old $sort variables */
  cs_subset_str_native_set(shared->sub, "sort", old_sort, NULL);
  cs_subset_str_native_set(shared->sub, "sort_aux", old_sort_aux, NULL);
  cs_subset_str_native_set(shared->sub, "use_threads", old_use_threads, NULL);
  if (added_attachment)
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_bcc - Edit the BCC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_bcc(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_BCC, &shared->email->env->bcc))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_cc - Edit the CC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_cc(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_CC, &shared->email->env->cc))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_description - Edit attachment description - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_description(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  mutt_buffer_strcpy(buf, cur_att->body->description);

  /* header names should not be translated */
  if (mutt_buffer_get_field("Description: ", buf, MUTT_COMP_NO_FLAGS, false,
                            NULL, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(cur_att->body->description, mutt_buffer_string(buf)))
    {
      mutt_str_replace(&cur_att->body->description, mutt_buffer_string(buf));
      menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = IR_SUCCESS;
    }
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_compose_edit_encoding - Edit attachment transfer-encoding - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_encoding(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  mutt_buffer_strcpy(buf, ENCODING(cur_att->body->encoding));

  if ((mutt_buffer_get_field("Content-Transfer-Encoding: ", buf,
                             MUTT_COMP_NO_FLAGS, false, NULL, NULL, NULL) == 0) &&
      !mutt_buffer_is_empty(buf))
  {
    int enc = mutt_check_encoding(mutt_buffer_string(buf));
    if ((enc != ENC_OTHER) && (enc != ENC_UUENCODED))
    {
      if (enc != cur_att->body->encoding)
      {
        cur_att->body->encoding = enc;
        menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        mutt_clear_error();
        mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
        rc = IR_SUCCESS;
      }
    }
    else
    {
      mutt_error(_("Invalid encoding"));
      rc = IR_ERROR;
    }
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_compose_edit_fcc - Enter a file to save a copy of this message in - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_fcc(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_copy(fname, shared->fcc);
  if (mutt_buffer_get_field(Prompts[HDR_FCC], fname, MUTT_COMP_FILE | MUTT_COMP_CLEAR,
                            false, NULL, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(shared->fcc->data, fname->data))
    {
      mutt_buffer_copy(shared->fcc, fname);
      mutt_buffer_pretty_mailbox(shared->fcc);
      shared->fcc_set = true;
      notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = IR_SUCCESS;
    }
  }
  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * op_compose_edit_file - Edit the file to be attached - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_file(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  const char *const c_editor = cs_subset_string(shared->sub, "editor");
  mutt_edit_file(NONULL(c_editor), cur_att->body->filename);
  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  /* Unconditional hook since editor was invoked */
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_from - Edit the from field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_from(struct ComposeSharedData *shared, int op)
{
  if (!edit_address_list(HDR_FROM, &shared->email->env->from))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_headers - Edit the message with headers - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_headers(struct ComposeSharedData *shared, int op)
{
  mutt_rfc3676_space_unstuff(shared->email);
  const char *tag = NULL;
  char *err = NULL;
  mutt_env_to_local(shared->email->env);
  const char *const c_editor = cs_subset_string(shared->sub, "editor");
  mutt_edit_headers(NONULL(c_editor), shared->email->body->filename,
                    shared->email, shared->fcc);
  if (mutt_env_to_intl(shared->email->env, &tag, &err))
  {
    mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
    FREE(&err);
  }
  update_crypt_info(shared);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);

  mutt_rfc3676_space_stuff(shared->email);
  mutt_update_encoding(shared->email->body, shared->sub);

  /* attachments may have been added */
  if (shared->adata->actx->idxlen &&
      shared->adata->actx->idx[shared->adata->actx->idxlen - 1]->body->next)
  {
    mutt_actx_entries_free(shared->adata->actx);
    update_menu(shared->adata->actx, shared->adata->menu, true);
  }

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  /* Unconditional hook since editor was invoked */
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_language - Edit the 'Content-Language' of the attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_language(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  char buf[PATH_MAX];
  mutt_str_copy(buf, cur_att->body->language, sizeof(buf));
  if (mutt_get_field("Content-Language: ", buf, sizeof(buf), MUTT_COMP_NO_FLAGS,
                     false, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(cur_att->body->language, buf))
    {
      cur_att->body->language = mutt_str_dup(buf);
      menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
      notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = IR_SUCCESS;
    }
    mutt_clear_error();
  }
  else
  {
    mutt_warning(_("Empty 'Content-Language'"));
    rc = IR_ERROR;
  }
  return rc;
}

/**
 * op_compose_edit_message - Edit the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_message(struct ComposeSharedData *shared, int op)
{
  const bool c_edit_headers = cs_subset_bool(shared->sub, "edit_headers");
  if (!c_edit_headers)
  {
    mutt_rfc3676_space_unstuff(shared->email);
    const char *const c_editor = cs_subset_string(shared->sub, "editor");
    mutt_edit_file(c_editor, shared->email->body->filename);
    mutt_rfc3676_space_stuff(shared->email);
    mutt_update_encoding(shared->email->body, shared->sub);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    /* Unconditional hook since editor was invoked */
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
    return IR_SUCCESS;
  }

  return op_compose_edit_headers(shared, op);
}

/**
 * op_compose_edit_mime - Edit attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_mime(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (!mutt_edit_attachment(cur_att->body))
    return IR_NO_ACTION;

  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_reply_to - Edit the Reply-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_reply_to(struct ComposeSharedData *shared, int op)
{
  if (!edit_address_list(HDR_REPLYTO, &shared->email->env->reply_to))
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_edit_subject - Edit the subject of this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_subject(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  char buf[PATH_MAX];
  mutt_str_copy(buf, shared->email->env->subject, sizeof(buf));
  if (mutt_get_field(Prompts[HDR_SUBJECT], buf, sizeof(buf), MUTT_COMP_NO_FLAGS,
                     false, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(shared->email->env->subject, buf))
    {
      mutt_str_replace(&shared->email->env->subject, buf);
      notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = IR_SUCCESS;
    }
  }

  return rc;
}

/**
 * op_compose_edit_to - Edit the TO list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_to(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_TO, &shared->email->env->to))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_get_attachment - Get a temporary copy of an attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_get_attachment(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (shared->adata->menu->tagprefix)
  {
    for (struct Body *top = shared->email->body; top; top = top->next)
    {
      if (top->tagged)
        mutt_get_tmp_attachment(top);
    }
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  }
  else if (mutt_get_tmp_attachment(cur_att->body) == 0)
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);

  /* No send2hook since this doesn't change the message. */
  return IR_SUCCESS;
}

/**
 * op_compose_group_alts - Group tagged attachments as 'multipart/alternative' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_group_alts(struct ComposeSharedData *shared, int op)
{
  static const char *ALTS_TAG = "Alternatives for \"%s\"";
  if (shared->adata->menu->tagged < 2)
  {
    mutt_error(
        _("Grouping 'alternatives' requires at least 2 tagged messages"));
    return IR_ERROR;
  }

  struct Body *group = mutt_body_new();
  group->type = TYPE_MULTIPART;
  group->subtype = mutt_str_dup("alternative");
  group->disposition = DISP_INLINE;

  struct Body *alts = NULL;
  /* group tagged message into a multipart/alternative */
  struct Body *bptr = shared->email->body;
  for (int i = 0; bptr;)
  {
    if (bptr->tagged)
    {
      bptr->tagged = false;
      bptr->disposition = DISP_INLINE;

      /* for first match, set group desc according to match */
      if (!group->description)
      {
        char *p = bptr->description ? bptr->description : bptr->filename;
        if (p)
        {
          group->description = mutt_mem_calloc(1, strlen(p) + strlen(ALTS_TAG) + 1);
          sprintf(group->description, ALTS_TAG, p);
        }
      }

      // append bptr to the alts list, and remove from the shared->email->body list
      if (alts)
      {
        alts->next = bptr;
        bptr = bptr->next;
        alts = alts->next;
        alts->next = NULL;
      }
      else
      {
        group->parts = bptr;
        alts = bptr;
        bptr = bptr->next;
        alts->next = NULL;
      }

      for (int j = i; j < shared->adata->actx->idxlen - 1; j++)
      {
        shared->adata->actx->idx[j] = shared->adata->actx->idx[j + 1];
        shared->adata->actx->idx[j + 1] = NULL; /* for debug reason */
      }
      shared->adata->actx->idxlen--;
    }
    else
    {
      bptr = bptr->next;
      i++;
    }
  }

  group->next = NULL;
  mutt_generate_boundary(&group->parameter);

  /* if no group desc yet, make one up */
  if (!group->description)
    group->description = mutt_str_dup("unknown alternative group");

  struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
  gptr->body = group;
  update_idx(shared->adata->menu, shared->adata->actx, gptr);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  return IR_SUCCESS;
}

/**
 * op_compose_group_lingual - Group tagged attachments as 'multipart/multilingual' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_group_lingual(struct ComposeSharedData *shared, int op)
{
  static const char *LINGUAL_TAG = "Multilingual part for \"%s\"";
  if (shared->adata->menu->tagged < 2)
  {
    mutt_error(
        _("Grouping 'multilingual' requires at least 2 tagged messages"));
    return IR_ERROR;
  }

  /* traverse to see whether all the parts have Content-Language: set */
  int tagged_with_lang_num = 0;
  for (struct Body *b = shared->email->body; b; b = b->next)
    if (b->tagged && b->language && *b->language)
      tagged_with_lang_num++;

  if (shared->adata->menu->tagged != tagged_with_lang_num)
  {
    if (mutt_yesorno(_("Not all parts have 'Content-Language' set, continue?"), MUTT_YES) != MUTT_YES)
    {
      mutt_message(_("Not sending this message"));
      return IR_ERROR;
    }
  }

  struct Body *group = mutt_body_new();
  group->type = TYPE_MULTIPART;
  group->subtype = mutt_str_dup("multilingual");
  group->disposition = DISP_INLINE;

  struct Body *alts = NULL;
  /* group tagged message into a multipart/multilingual */
  struct Body *bptr = shared->email->body;
  for (int i = 0; bptr;)
  {
    if (bptr->tagged)
    {
      bptr->tagged = false;
      bptr->disposition = DISP_INLINE;

      /* for first match, set group desc according to match */
      if (!group->description)
      {
        char *p = bptr->description ? bptr->description : bptr->filename;
        if (p)
        {
          group->description = mutt_mem_calloc(1, strlen(p) + strlen(LINGUAL_TAG) + 1);
          sprintf(group->description, LINGUAL_TAG, p);
        }
      }

      // append bptr to the alts list, and remove from the shared->email->body list
      if (alts)
      {
        alts->next = bptr;
        bptr = bptr->next;
        alts = alts->next;
        alts->next = NULL;
      }
      else
      {
        group->parts = bptr;
        alts = bptr;
        bptr = bptr->next;
        alts->next = NULL;
      }

      for (int j = i; j < shared->adata->actx->idxlen - 1; j++)
      {
        shared->adata->actx->idx[j] = shared->adata->actx->idx[j + 1];
        shared->adata->actx->idx[j + 1] = NULL; /* for debug reason */
      }
      shared->adata->actx->idxlen--;
    }
    else
    {
      bptr = bptr->next;
      i++;
    }
  }

  group->next = NULL;
  mutt_generate_boundary(&group->parameter);

  /* if no group desc yet, make one up */
  if (!group->description)
    group->description = mutt_str_dup("unknown multilingual group");

  struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
  gptr->body = group;
  update_idx(shared->adata->menu, shared->adata->actx, gptr);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  return IR_SUCCESS;
}

/**
 * op_compose_ispell - Run ispell on the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_ispell(struct ComposeSharedData *shared, int op)
{
  endwin();
  const char *const c_ispell = cs_subset_string(shared->sub, "ispell");
  char buf[PATH_MAX];
  snprintf(buf, sizeof(buf), "%s -x %s", NONULL(c_ispell), shared->email->body->filename);
  if (mutt_system(buf) == -1)
  {
    mutt_error(_("Error running \"%s\""), buf);
    return IR_ERROR;
  }

  mutt_update_encoding(shared->email->body, shared->sub);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_move_down - Move an attachment down in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_move_down(struct ComposeSharedData *shared, int op)
{
  int index = menu_get_index(shared->adata->menu);
  if (index < 0)
    return IR_ERROR;

  if (index == (shared->adata->actx->idxlen - 1))
  {
    mutt_error(_("Attachment is already at bottom"));
    return IR_NO_ACTION;
  }
  if (index == 0)
  {
    mutt_error(_("The fundamental part can't be moved"));
    return IR_ERROR;
  }
  compose_attach_swap(shared->email->body, shared->adata->actx->idx, index);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  menu_set_index(shared->adata->menu, index + 1);
  return IR_SUCCESS;
}

/**
 * op_compose_move_up - Move an attachment up in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_move_up(struct ComposeSharedData *shared, int op)
{
  int index = menu_get_index(shared->adata->menu);
  if (index == 0)
  {
    mutt_error(_("Attachment is already at top"));
    return IR_NO_ACTION;
  }
  if (index == 1)
  {
    mutt_error(_("The fundamental part can't be moved"));
    return IR_ERROR;
  }
  compose_attach_swap(shared->email->body, shared->adata->actx->idx, index - 1);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  menu_set_index(shared->adata->menu, index - 1);
  return IR_SUCCESS;
}

/**
 * op_compose_new_mime - Compose new attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_new_mime(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  struct Buffer *fname = mutt_buffer_pool_get();
  struct AttachPtr *ap = NULL;

  if ((mutt_buffer_get_field(_("New file: "), fname, MUTT_COMP_FILE, false,
                             NULL, NULL, NULL) != 0) ||
      mutt_buffer_is_empty(fname))
  {
    mutt_buffer_pool_release(&fname);
    goto done;
  }
  mutt_buffer_expand_path(fname);

  /* Call to lookup_mime_type () ?  maybe later */
  char type[256] = { 0 };
  if ((mutt_get_field("Content-Type: ", type, sizeof(type), MUTT_COMP_NO_FLAGS,
                      false, NULL, NULL) != 0) ||
      (type[0] == '\0'))
  {
    mutt_buffer_pool_release(&fname);
    goto done;
  }

  rc = IR_ERROR;
  char *p = strchr(type, '/');
  if (!p)
  {
    mutt_error(_("Content-Type is of the form base/sub"));
    mutt_buffer_pool_release(&fname);
    goto done;
  }
  *p++ = 0;
  enum ContentType itype = mutt_check_mime_type(type);
  if (itype == TYPE_OTHER)
  {
    mutt_error(_("Unknown Content-Type %s"), type);
    mutt_buffer_pool_release(&fname);
    goto done;
  }
  ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
  /* Touch the file */
  FILE *fp = mutt_file_fopen(mutt_buffer_string(fname), "w");
  if (!fp)
  {
    mutt_error(_("Can't create file %s"), mutt_buffer_string(fname));
    mutt_buffer_pool_release(&fname);
    goto done;
  }
  mutt_file_fclose(&fp);

  ap->body = mutt_make_file_attach(mutt_buffer_string(fname), shared->sub);
  if (!ap->body)
  {
    mutt_error(_("What we have here is a failure to make an attachment"));
    mutt_buffer_pool_release(&fname);
    goto done;
  }
  update_idx(shared->adata->menu, shared->adata->actx, ap);
  ap = NULL; // shared->adata->actx has taken ownership

  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  cur_att->body->type = itype;
  mutt_str_replace(&cur_att->body->subtype, p);
  cur_att->body->unlink = true;
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);

  if (mutt_compose_attachment(cur_att->body))
  {
    mutt_update_encoding(cur_att->body, shared->sub);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  }
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  rc = IR_SUCCESS;

done:
  FREE(&ap);
  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * op_compose_pgp_menu - Show PGP options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_pgp_menu(struct ComposeSharedData *shared, int op)
{
  const SecurityFlags old_flags = shared->email->security;
  if (!(WithCrypto & APPLICATION_PGP))
    return IR_NOT_IMPL;
  if (!crypt_has_module_backend(APPLICATION_PGP))
  {
    mutt_error(_("No PGP backend configured"));
    return IR_ERROR;
  }
  if (((WithCrypto & APPLICATION_SMIME) != 0) && (shared->email->security & APPLICATION_SMIME))
  {
    if (shared->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return IR_NO_ACTION;
      }
      shared->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    shared->email->security &= ~APPLICATION_SMIME;
    shared->email->security |= APPLICATION_PGP;
    update_crypt_info(shared);
  }
  shared->email->security = crypt_pgp_send_menu(shared->email);
  update_crypt_info(shared);
  if (old_flags == shared->email->security)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_postpone_message - Save this message to send later - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_postpone_message(struct ComposeSharedData *shared, int op)
{
  if (check_attachments(shared->adata->actx, shared->sub) != 0)
  {
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    return IR_ERROR;
  }

  shared->rc = 1;
  return IR_DONE;
}

/**
 * op_compose_rename_attachment - Send attachment with a different name - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_rename_attachment(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  char *src = NULL;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (cur_att->body->d_filename)
    src = cur_att->body->d_filename;
  else
    src = cur_att->body->filename;
  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_strcpy(fname, mutt_path_basename(NONULL(src)));
  int ret = mutt_buffer_get_field(_("Send attachment with name: "), fname,
                                  MUTT_COMP_FILE, false, NULL, NULL, NULL);
  if (ret == 0)
  {
    // It's valid to set an empty string here, to erase what was set
    mutt_str_replace(&cur_att->body->d_filename, mutt_buffer_string(fname));
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  }
  mutt_buffer_pool_release(&fname);
  return IR_SUCCESS;
}

/**
 * op_compose_rename_file - Rename/move an attached file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_rename_file(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_strcpy(fname, cur_att->body->filename);
  mutt_buffer_pretty_mailbox(fname);
  if ((mutt_buffer_get_field(_("Rename to: "), fname, MUTT_COMP_FILE, false,
                             NULL, NULL, NULL) == 0) &&
      !mutt_buffer_is_empty(fname))
  {
    struct stat st = { 0 };
    if (stat(cur_att->body->filename, &st) == -1)
    {
      /* L10N: "stat" is a system call. Do "man 2 stat" for more information. */
      mutt_error(_("Can't stat %s: %s"), mutt_buffer_string(fname), strerror(errno));
      mutt_buffer_pool_release(&fname);
      return IR_ERROR;
    }

    mutt_buffer_expand_path(fname);
    if (mutt_file_rename(cur_att->body->filename, mutt_buffer_string(fname)))
    {
      mutt_buffer_pool_release(&fname);
      return IR_ERROR;
    }

    mutt_str_replace(&cur_att->body->filename, mutt_buffer_string(fname));
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);

    if (cur_att->body->stamp >= st.st_mtime)
      mutt_stamp_attachment(cur_att->body);
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  }
  mutt_buffer_pool_release(&fname);
  return IR_SUCCESS;
}

/**
 * op_compose_send_message - Send the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_send_message(struct ComposeSharedData *shared, int op)
{
  /* Note: We don't invoke send2-hook here, since we want to leave
   * users an opportunity to change settings from the ":" prompt.  */
  if (check_attachments(shared->adata->actx, shared->sub) != 0)
  {
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    return IR_NO_ACTION;
  }

#ifdef MIXMASTER
  if (!STAILQ_EMPTY(&shared->email->chain) && (mix_check_message(shared->email) != 0))
    return IR_NO_ACTION;
#endif

  if (!shared->fcc_set && !mutt_buffer_is_empty(shared->fcc))
  {
    const enum QuadOption c_copy = cs_subset_quad(shared->sub, "copy");
    enum QuadOption ans =
        query_quadoption(c_copy, _("Save a copy of this message?"));
    if (ans == MUTT_ABORT)
      return IR_NO_ACTION;
    else if (ans == MUTT_NO)
      mutt_buffer_reset(shared->fcc);
  }

  shared->rc = 0;
  return IR_DONE;
}

/**
 * op_compose_smime_menu - Show S/MIME options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_smime_menu(struct ComposeSharedData *shared, int op)
{
  const SecurityFlags old_flags = shared->email->security;
  if (!(WithCrypto & APPLICATION_SMIME))
    return IR_NOT_IMPL;
  if (!crypt_has_module_backend(APPLICATION_SMIME))
  {
    mutt_error(_("No S/MIME backend configured"));
    return IR_ERROR;
  }

  if (((WithCrypto & APPLICATION_PGP) != 0) && (shared->email->security & APPLICATION_PGP))
  {
    if (shared->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("PGP already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return IR_NO_ACTION;
      }
      shared->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    shared->email->security &= ~APPLICATION_PGP;
    shared->email->security |= APPLICATION_SMIME;
    update_crypt_info(shared);
  }
  shared->email->security = crypt_smime_send_menu(shared->email);
  update_crypt_info(shared);
  if (old_flags == shared->email->security)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_toggle_disposition - Toggle disposition between inline/attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_disposition(struct ComposeSharedData *shared, int op)
{
  /* toggle the content-disposition between inline/attachment */
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  cur_att->body->disposition =
      (cur_att->body->disposition == DISP_INLINE) ? DISP_ATTACH : DISP_INLINE;
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  return IR_SUCCESS;
}

/**
 * op_compose_toggle_recode - Toggle recoding of this attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_recode(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (!mutt_is_text_part(cur_att->body))
  {
    mutt_error(_("Recoding only affects text attachments"));
    return IR_ERROR;
  }
  cur_att->body->noconv = !cur_att->body->noconv;
  if (cur_att->body->noconv)
    mutt_message(_("The current attachment won't be converted"));
  else
    mutt_message(_("The current attachment will be converted"));
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_toggle_unlink - Toggle whether to delete file after sending it - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_unlink(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  cur_att->body->unlink = !cur_att->body->unlink;

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  /* No send2hook since this doesn't change the message. */
  return IR_SUCCESS;
}

/**
 * op_compose_update_encoding - Update an attachment's encoding info - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_update_encoding(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  bool encoding_updated = false;
  if (shared->adata->menu->tagprefix)
  {
    struct Body *top = NULL;
    for (top = shared->email->body; top; top = top->next)
    {
      if (top->tagged)
      {
        encoding_updated = true;
        mutt_update_encoding(top, shared->sub);
      }
    }
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  }
  else
  {
    struct AttachPtr *cur_att =
        current_attachment(shared->adata->actx, shared->adata->menu);
    mutt_update_encoding(cur_att->body, shared->sub);
    encoding_updated = true;
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
    notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  }
  if (encoding_updated)
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_compose_write_message - Write the message to a folder - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_write_message(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  struct Buffer *fname = mutt_buffer_pool_get();
  if (shared->mailbox)
  {
    mutt_buffer_strcpy(fname, mailbox_path(shared->mailbox));
    mutt_buffer_pretty_mailbox(fname);
  }
  if (shared->adata->actx->idxlen)
    shared->email->body = shared->adata->actx->idx[0]->body;
  if ((mutt_buffer_enter_fname(_("Write message to mailbox"), fname, true,
                               shared->mailbox, false, NULL, NULL, MUTT_SEL_NO_FLAGS) != -1) &&
      !mutt_buffer_is_empty(fname))
  {
    mutt_message(_("Writing message to %s ..."), mutt_buffer_string(fname));
    mutt_buffer_expand_path(fname);

    if (shared->email->body->next)
      shared->email->body = mutt_make_multipart(shared->email->body);

    if (mutt_write_fcc(mutt_buffer_string(fname), shared->email, NULL, false,
                       NULL, NULL, shared->sub) == 0)
      mutt_message(_("Message written"));

    shared->email->body = mutt_remove_multipart(shared->email->body);
    rc = IR_SUCCESS;
  }
  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * op_delete - Delete the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_delete(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (cur_att->unowned)
    cur_att->body->unlink = false;
  int index = menu_get_index(shared->adata->menu);
  if (delete_attachment(shared->adata->actx, index) == -1)
    return IR_ERROR;
  update_menu(shared->adata->actx, shared->adata->menu, false);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  index = menu_get_index(shared->adata->menu);
  if (index == 0)
    shared->email->body = shared->adata->actx->idx[0]->body;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_display_headers - Display message and toggle header weeding - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_DISPLAY_HEADERS
 * - OP_VIEW_ATTACH
 */
static int op_display_headers(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  mutt_attach_display_loop(shared->sub, shared->adata->menu, op, shared->email,
                           shared->adata->actx, false);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  /* no send2hook, since this doesn't modify the message */
  return IR_SUCCESS;
}

/**
 * op_edit_type - Edit attachment content type - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_edit_type(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;

  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  if (!mutt_edit_content_type(NULL, cur_att->body, NULL))
    return IR_NO_ACTION;

  /* this may have been a change to text/something */
  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_exit - Exit this menu - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_exit(struct ComposeSharedData *shared, int op)
{
  const enum QuadOption c_postpone = cs_subset_quad(shared->sub, "postpone");
  enum QuadOption ans =
      query_quadoption(c_postpone, _("Save (postpone) draft message?"));
  if (ans == MUTT_NO)
  {
    for (int i = 0; i < shared->adata->actx->idxlen; i++)
      if (shared->adata->actx->idx[i]->unowned)
        shared->adata->actx->idx[i]->body->unlink = false;

    if (!(shared->flags & MUTT_COMPOSE_NOFREEHEADER))
    {
      for (int i = 0; i < shared->adata->actx->idxlen; i++)
      {
        /* avoid freeing other attachments */
        shared->adata->actx->idx[i]->body->next = NULL;
        /* See the comment in delete_attachment() */
        if (!shared->adata->actx->idx[i]->body->email)
          shared->adata->actx->idx[i]->body->parts = NULL;
        mutt_body_free(&shared->adata->actx->idx[i]->body);
      }
    }
    shared->rc = -1;
    return IR_DONE;
  }
  else if (ans == MUTT_ABORT)
    return IR_NO_ACTION;

  return op_compose_postpone_message(shared, op);
}

/**
 * op_filter - Filter attachment through a shell command - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_FILTER
 * - OP_PIPE
 */
static int op_filter(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  mutt_pipe_attachment_list(shared->adata->actx, NULL, shared->adata->menu->tagprefix,
                            cur_att->body, (op == OP_FILTER));
  if (op == OP_FILTER) /* cte might have changed */
    menu_queue_redraw(shared->adata->menu,
                      shared->adata->menu->tagprefix ? MENU_REDRAW_FULL : MENU_REDRAW_CURRENT);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return IR_SUCCESS;
}

/**
 * op_forget_passphrase - Wipe passphrases from memory - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_forget_passphrase(struct ComposeSharedData *shared, int op)
{
  crypt_forget_passphrase();
  return IR_SUCCESS;
}

/**
 * op_print - Print the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_print(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  mutt_print_attachment_list(shared->adata->actx, NULL,
                             shared->adata->menu->tagprefix, cur_att->body);
  /* no send2hook, since this doesn't modify the message */
  return IR_SUCCESS;
}

/**
 * op_save - Save message/attachment to a mailbox/file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_save(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return IR_NO_ACTION;
  struct AttachPtr *cur_att =
      current_attachment(shared->adata->actx, shared->adata->menu);
  mutt_save_attachment_list(shared->adata->actx, NULL, shared->adata->menu->tagprefix,
                            cur_att->body, NULL, shared->adata->menu);
  /* no send2hook, since this doesn't modify the message */
  return IR_SUCCESS;
}

// -----------------------------------------------------------------------------

#ifdef USE_AUTOCRYPT
/**
 * op_compose_autocrypt_menu - Show autocrypt compose menu options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_autocrypt_menu(struct ComposeSharedData *shared, int op)
{
  const SecurityFlags old_flags = shared->email->security;
  const bool c_autocrypt = cs_subset_bool(shared->sub, "autocrypt");
  if (!c_autocrypt)
    return IR_NO_ACTION;

  if ((WithCrypto & APPLICATION_SMIME) && (shared->email->security & APPLICATION_SMIME))
  {
    if (shared->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return IR_NO_ACTION;
      }
      shared->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    shared->email->security &= ~APPLICATION_SMIME;
    shared->email->security |= APPLICATION_PGP;
    update_crypt_info(shared);
  }
  autocrypt_compose_menu(shared->email, shared->sub);
  update_crypt_info(shared);
  if (old_flags == shared->email->security)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}
#endif

#ifdef USE_NNTP
/**
 * op_compose_edit_followup_to - Edit the Followup-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_followup_to(struct ComposeSharedData *shared, int op)
{
  if (!shared->news)
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  char buf[PATH_MAX];
  mutt_str_copy(buf, shared->email->env->followup_to, sizeof(buf));
  if (mutt_get_field(Prompts[HDR_FOLLOWUPTO], buf, sizeof(buf),
                     MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->followup_to, buf);
    notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
    rc = IR_SUCCESS;
  }

  return rc;
}

/**
 * op_compose_edit_newsgroups - Edit the newsgroups list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_newsgroups(struct ComposeSharedData *shared, int op)
{
  if (!shared->news)
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  char buf[PATH_MAX];
  mutt_str_copy(buf, shared->email->env->newsgroups, sizeof(buf));
  if (mutt_get_field(Prompts[HDR_NEWSGROUPS], buf, sizeof(buf),
                     MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->newsgroups, buf);
    notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
    rc = IR_SUCCESS;
  }

  return rc;
}

/**
 * op_compose_edit_x_comment_to - Edit the X-Comment-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_x_comment_to(struct ComposeSharedData *shared, int op)
{
  const bool c_x_comment_to = cs_subset_bool(shared->sub, "x_comment_to");
  if (!(shared->news && c_x_comment_to))
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  char buf[PATH_MAX];
  mutt_str_copy(buf, shared->email->env->x_comment_to, sizeof(buf));
  if (mutt_get_field(Prompts[HDR_XCOMMENTTO], buf, sizeof(buf),
                     MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->x_comment_to, buf);
    notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
    rc = IR_SUCCESS;
  }

  return rc;
}
#endif

#ifdef MIXMASTER
/**
 * op_compose_mix - Send the message through a mixmaster remailer chain - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_mix(struct ComposeSharedData *shared, int op)
{
  dlg_select_mixmaster_chain(&shared->email->chain);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
  return IR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * ComposeFunctions - All the NeoMutt functions that the Compose supports
 */
struct ComposeFunction ComposeFunctions[] = {
  // clang-format off
  { OP_COMPOSE_ATTACH_FILE,         op_compose_attach_file },
  { OP_COMPOSE_ATTACH_KEY,          op_compose_attach_key },
  { OP_COMPOSE_ATTACH_MESSAGE,      op_compose_attach_message },
  { OP_COMPOSE_ATTACH_NEWS_MESSAGE, op_compose_attach_message },
#ifdef USE_AUTOCRYPT
  { OP_COMPOSE_AUTOCRYPT_MENU,      op_compose_autocrypt_menu },
#endif
  { OP_COMPOSE_EDIT_BCC,            op_compose_edit_bcc },
  { OP_COMPOSE_EDIT_CC,             op_compose_edit_cc },
  { OP_COMPOSE_EDIT_DESCRIPTION,    op_compose_edit_description },
  { OP_COMPOSE_EDIT_ENCODING,       op_compose_edit_encoding },
  { OP_COMPOSE_EDIT_FCC,            op_compose_edit_fcc },
  { OP_COMPOSE_EDIT_FILE,           op_compose_edit_file },
#ifdef USE_NNTP
  { OP_COMPOSE_EDIT_FOLLOWUP_TO,    op_compose_edit_followup_to },
#endif
  { OP_COMPOSE_EDIT_FROM,           op_compose_edit_from },
  { OP_COMPOSE_EDIT_HEADERS,        op_compose_edit_headers },
  { OP_COMPOSE_EDIT_LANGUAGE,       op_compose_edit_language },
  { OP_COMPOSE_EDIT_MESSAGE,        op_compose_edit_message },
  { OP_COMPOSE_EDIT_MIME,           op_compose_edit_mime },
#ifdef USE_NNTP
  { OP_COMPOSE_EDIT_NEWSGROUPS,     op_compose_edit_newsgroups },
#endif
  { OP_COMPOSE_EDIT_REPLY_TO,       op_compose_edit_reply_to },
  { OP_COMPOSE_EDIT_SUBJECT,        op_compose_edit_subject },
  { OP_COMPOSE_EDIT_TO,             op_compose_edit_to },
#ifdef USE_NNTP
  { OP_COMPOSE_EDIT_X_COMMENT_TO,   op_compose_edit_x_comment_to },
#endif
  { OP_COMPOSE_GET_ATTACHMENT,      op_compose_get_attachment },
  { OP_COMPOSE_GROUP_ALTS,          op_compose_group_alts },
  { OP_COMPOSE_GROUP_LINGUAL,       op_compose_group_lingual },
  { OP_COMPOSE_ISPELL,              op_compose_ispell },
#ifdef MIXMASTER
  { OP_COMPOSE_MIX,                 op_compose_mix },
#endif
  { OP_COMPOSE_MOVE_DOWN,           op_compose_move_down },
  { OP_COMPOSE_MOVE_UP,             op_compose_move_up },
  { OP_COMPOSE_NEW_MIME,            op_compose_new_mime },
  { OP_COMPOSE_PGP_MENU,            op_compose_pgp_menu },
  { OP_COMPOSE_POSTPONE_MESSAGE,    op_compose_postpone_message },
  { OP_COMPOSE_RENAME_ATTACHMENT,   op_compose_rename_attachment },
  { OP_COMPOSE_RENAME_FILE,         op_compose_rename_file },
  { OP_COMPOSE_SEND_MESSAGE,        op_compose_send_message },
  { OP_COMPOSE_SMIME_MENU,          op_compose_smime_menu },
  { OP_COMPOSE_TOGGLE_DISPOSITION,  op_compose_toggle_disposition },
  { OP_COMPOSE_TOGGLE_RECODE,       op_compose_toggle_recode },
  { OP_COMPOSE_TOGGLE_UNLINK,       op_compose_toggle_unlink },
  { OP_COMPOSE_UPDATE_ENCODING,     op_compose_update_encoding },
  { OP_COMPOSE_WRITE_MESSAGE,       op_compose_write_message },
  { OP_DELETE,                      op_delete },
  { OP_DISPLAY_HEADERS,             op_display_headers },
  { OP_EDIT_TYPE,                   op_edit_type },
  { OP_EXIT,                        op_exit },
  { OP_FILTER,                      op_filter },
  { OP_FORGET_PASSPHRASE,           op_forget_passphrase },
  { OP_PIPE,                        op_filter },
  { OP_PRINT,                       op_print },
  { OP_SAVE,                        op_save },
  { OP_VIEW_ATTACH,                 op_display_headers },
  { 0, NULL },
  // clang-format on
};

/**
 * compose_function_dispatcher - Perform a Compose function
 * @param win_compose Window for Compose
 * @param op          Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int compose_function_dispatcher(struct MuttWindow *win_compose, int op)
{
  if (!win_compose)
  {
    mutt_error(_(Not_available_in_this_menu));
    return IR_ERROR;
  }

  struct MuttWindow *dlg = dialog_find(win_compose);
  if (!dlg || !dlg->wdata)
    return IR_ERROR;

  int rc = IR_UNKNOWN;
  for (size_t i = 0; ComposeFunctions[i].op != OP_NULL; i++)
  {
    const struct ComposeFunction *fn = &ComposeFunctions[i];
    if (fn->op == op)
    {
      struct ComposeSharedData *shared = dlg->wdata;
      rc = fn->function(shared, op);
      break;
    }
  }

  return rc;
}
