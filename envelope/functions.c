/**
 * @file
 * Envelope functions
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
 * @page envelope_functions Envelope functions
 *
 * Envelope functions
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
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
#include "browser/lib.h"
#include "compose/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "hook.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "rfc3676.h"
#include "wdata.h"
#ifdef MIXMASTER
#include "remailer.h"
#endif

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
 * edit_address_list - Let the user edit the address list
 * @param[in]     field Field to edit, e.g. #HDR_FROM
 * @param[in,out] al    AddressList to edit
 * @retval true The address list was changed
 */
static bool edit_address_list(enum HeaderField field, struct AddressList *al)
{
  struct Buffer *old_list = mutt_buffer_pool_get();
  struct Buffer *new_list = mutt_buffer_pool_get();

  /* need to be large for alias expansion */
  mutt_buffer_alloc(old_list, 8192);
  mutt_buffer_alloc(new_list, 8192);

  mutt_addrlist_to_local(al);
  mutt_addrlist_write(al, new_list->data, new_list->dsize, false);
  mutt_buffer_fix_dptr(new_list);
  mutt_buffer_copy(old_list, new_list);
  if (mutt_buffer_get_field(_(Prompts[field]), new_list, MUTT_COMP_ALIAS, false,
                            NULL, NULL, NULL) == 0)
  {
    mutt_addrlist_clear(al);
    mutt_addrlist_parse2(al, mutt_buffer_string(new_list));
    mutt_expand_aliases(al);
  }

  char *err = NULL;
  if (mutt_addrlist_to_intl(al, &err) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    mutt_refresh();
    FREE(&err);
  }

  const bool rc =
      !mutt_str_equal(mutt_buffer_string(new_list), mutt_buffer_string(old_list));
  mutt_buffer_pool_release(&old_list);
  mutt_buffer_pool_release(&new_list);
  return rc;
}

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
    {
      e->security &= ~(SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE);
    }
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

// -----------------------------------------------------------------------------

/**
 * op_envelope_edit_bcc - Edit the BCC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_bcc(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_BCC, &shared->email->env->bcc))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_BCC);
  return IR_SUCCESS;
}

/**
 * op_envelope_edit_cc - Edit the CC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_cc(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_CC, &shared->email->env->cc))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_CC);
  return IR_SUCCESS;
}

/**
 * op_envelope_edit_fcc - Enter a file to save a copy of this message in - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_fcc(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_copy(fname, shared->fcc);

  if (mutt_buffer_get_field(Prompts[HDR_FCC], fname, MUTT_COMP_FILE | MUTT_COMP_CLEAR,
                            false, NULL, NULL, NULL) != 0)
  {
    goto done; // aborted
  }

  if (mutt_str_equal(shared->fcc->data, fname->data))
    goto done; // no change

  mutt_buffer_copy(shared->fcc, fname);
  mutt_buffer_pretty_mailbox(shared->fcc);
  shared->fcc_set = true;
  mutt_env_notify_send(shared->email, NT_ENVELOPE_FCC);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  rc = IR_SUCCESS;

done:
  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * op_envelope_edit_from - Edit the from field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_from(struct ComposeSharedData *shared, int op)
{
  if (!edit_address_list(HDR_FROM, &shared->email->env->from))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_FROM);
  return IR_SUCCESS;
}

/**
 * op_envelope_edit_reply_to - Edit the Reply-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_reply_to(struct ComposeSharedData *shared, int op)
{
  if (!edit_address_list(HDR_REPLYTO, &shared->email->env->reply_to))
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_REPLY_TO);
  return IR_SUCCESS;
}

/**
 * op_envelope_edit_subject - Edit the subject of this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_subject(struct ComposeSharedData *shared, int op)
{
  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, shared->email->env->subject);
  if (mutt_buffer_get_field(Prompts[HDR_SUBJECT], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) != 0)
  {
    goto done; // aborted
  }

  if (mutt_str_equal(shared->email->env->subject, mutt_buffer_string(buf)))
    goto done; // no change

  mutt_str_replace(&shared->email->env->subject, mutt_buffer_string(buf));
  mutt_env_notify_send(shared->email, NT_ENVELOPE_SUBJECT);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  rc = IR_SUCCESS;

done:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_to - Edit the TO list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_to(struct ComposeSharedData *shared, int op)
{
#ifdef USE_NNTP
  if (shared->news)
    return IR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_TO, &shared->email->env->to))
    return IR_NO_ACTION;

  update_crypt_info(shared);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_TO);
  return IR_SUCCESS;
}

/**
 * op_compose_pgp_menu - Show PGP options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_pgp_menu(struct ComposeSharedData *shared, int op)
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
  if (shared->email->security == old_flags)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return IR_SUCCESS;
}

/**
 * op_compose_smime_menu - Show S/MIME options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_smime_menu(struct ComposeSharedData *shared, int op)
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
  if (shared->email->security == old_flags)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return IR_SUCCESS;
}

#ifdef USE_AUTOCRYPT
/**
 * op_compose_autocrypt_menu - Show autocrypt compose menu options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_autocrypt_menu(struct ComposeSharedData *shared, int op)
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
  if (shared->email->security == old_flags)
    return IR_NO_ACTION;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return IR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_envelope_edit_followup_to - Edit the Followup-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_followup_to(struct ComposeSharedData *shared, int op)
{
  if (!shared->news)
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, shared->email->env->followup_to);
  if (mutt_buffer_get_field(Prompts[HDR_FOLLOWUPTO], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->followup_to, mutt_buffer_string(buf));
    mutt_env_notify_send(shared->email, NT_ENVELOPE_FOLLOWUP_TO);
    rc = IR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_newsgroups - Edit the newsgroups list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_newsgroups(struct ComposeSharedData *shared, int op)
{
  if (!shared->news)
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, shared->email->env->newsgroups);
  if (mutt_buffer_get_field(Prompts[HDR_NEWSGROUPS], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->newsgroups, mutt_buffer_string(buf));
    mutt_env_notify_send(shared->email, NT_ENVELOPE_NEWSGROUPS);
    rc = IR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_x_comment_to - Edit the X-Comment-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_envelope_edit_x_comment_to(struct ComposeSharedData *shared, int op)
{
  const bool c_x_comment_to = cs_subset_bool(shared->sub, "x_comment_to");
  if (!(shared->news && c_x_comment_to))
    return IR_NO_ACTION;

  int rc = IR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, shared->email->env->x_comment_to);
  if (mutt_buffer_get_field(Prompts[HDR_XCOMMENTTO], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&shared->email->env->x_comment_to, mutt_buffer_string(buf));
    mutt_env_notify_send(shared->email, NT_ENVELOPE_X_COMMENT_TO);
    rc = IR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}
#endif

#ifdef MIXMASTER
/**
 * op_compose_mix - Send the message through a mixmaster remailer chain - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_mix(struct ComposeSharedData *shared, int op)
{
  dlg_select_mixmaster_chain(&shared->email->chain);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(shared->email, NT_ENVELOPE_X_COMMENT_TO);
  return IR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * EnvelopeFunctions - All the NeoMutt functions that the Envelope supports
 */
struct EnvelopeFunction EnvelopeFunctions[] = {
  // clang-format off
  { 0, NULL },
  // clang-format on
};

/**
 * env_function_dispatcher - Perform an Envelope function
 * @param win Envelope Window
 * @param op  Operation to perform, e.g. OP_ENVELOPE_EDIT_TO
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int env_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return IR_UNKNOWN;

  int rc = IR_UNKNOWN;
  for (size_t i = 0; EnvelopeFunctions[i].op != OP_NULL; i++)
  {
    const struct EnvelopeFunction *fn = &EnvelopeFunctions[i];
    if (fn->op == op)
    {
      struct EnvelopeWindowData *wdata = win->wdata;
      rc = fn->function(wdata, op);
      break;
    }
  }

  if (rc == IR_UNKNOWN) // Not our function
    return rc;

  const char *result = mutt_map_get_name(rc, RetvalNames);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", OpStrings[op][0], op, NONULL(result));

  return rc;
}
