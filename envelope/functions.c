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
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "enter/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "hook.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"
#include "wdata.h"
#ifdef MIXMASTER
#include "mixmaster/lib.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
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
      const bool c_crypt_opportunistic_encrypt = cs_subset_bool(sub, "crypt_opportunistic_encrypt");
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
  mutt_addrlist_write(al, new_list, false);
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

  const bool rc = !mutt_str_equal(mutt_buffer_string(new_list), mutt_buffer_string(old_list));
  mutt_buffer_pool_release(&old_list);
  mutt_buffer_pool_release(&new_list);
  return rc;
}

/**
 * update_crypt_info - Update the crypto info
 * @param wdata Envelope Window data
 */
void update_crypt_info(struct EnvelopeWindowData *wdata)
{
  struct Email *e = wdata->email;

  const bool c_crypt_opportunistic_encrypt = cs_subset_bool(wdata->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt)
    crypt_opportunistic_encrypt(e);

#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(wdata->sub, "autocrypt");
  if (c_autocrypt)
  {
    wdata->autocrypt_rec = mutt_autocrypt_ui_recommendation(e, NULL);

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
        if (wdata->autocrypt_rec == AUTOCRYPT_REC_YES)
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
 * op_envelope_edit_bcc - Edit the BCC list - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_bcc(struct EnvelopeWindowData *wdata, int op)
{
#ifdef USE_NNTP
  if (wdata->is_news)
    return FR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_BCC, &wdata->email->env->bcc))
    return FR_NO_ACTION;

  update_crypt_info(wdata);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_BCC);
  return FR_SUCCESS;
}

/**
 * op_envelope_edit_cc - Edit the CC list - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_cc(struct EnvelopeWindowData *wdata, int op)
{
#ifdef USE_NNTP
  if (wdata->is_news)
    return FR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_CC, &wdata->email->env->cc))
    return FR_NO_ACTION;

  update_crypt_info(wdata);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_CC);
  return FR_SUCCESS;
}

/**
 * op_envelope_edit_fcc - Enter a file to save a copy of this message in - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_fcc(struct EnvelopeWindowData *wdata, int op)
{
  int rc = FR_NO_ACTION;
  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_copy(fname, wdata->fcc);

  if (mutt_buffer_get_field(Prompts[HDR_FCC], fname, MUTT_COMP_FILE | MUTT_COMP_CLEAR,
                            false, NULL, NULL, NULL) != 0)
  {
    goto done; // aborted
  }

  if (mutt_str_equal(wdata->fcc->data, fname->data))
    goto done; // no change

  mutt_buffer_copy(wdata->fcc, fname);
  mutt_buffer_pretty_mailbox(wdata->fcc);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_FCC);
  rc = FR_SUCCESS;

done:
  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * op_envelope_edit_from - Edit the from field - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_from(struct EnvelopeWindowData *wdata, int op)
{
  if (!edit_address_list(HDR_FROM, &wdata->email->env->from))
    return FR_NO_ACTION;

  update_crypt_info(wdata);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_FROM);
  return FR_SUCCESS;
}

/**
 * op_envelope_edit_reply_to - Edit the Reply-To field - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_reply_to(struct EnvelopeWindowData *wdata, int op)
{
  if (!edit_address_list(HDR_REPLYTO, &wdata->email->env->reply_to))
    return FR_NO_ACTION;

  mutt_env_notify_send(wdata->email, NT_ENVELOPE_REPLY_TO);
  return FR_SUCCESS;
}

/**
 * op_envelope_edit_subject - Edit the subject of this message - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_subject(struct EnvelopeWindowData *wdata, int op)
{
  int rc = FR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, wdata->email->env->subject);
  if (mutt_buffer_get_field(Prompts[HDR_SUBJECT], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) != 0)
  {
    goto done; // aborted
  }

  if (mutt_str_equal(wdata->email->env->subject, mutt_buffer_string(buf)))
    goto done; // no change

  mutt_str_replace(&wdata->email->env->subject, mutt_buffer_string(buf));
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_SUBJECT);
  rc = FR_SUCCESS;

done:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_to - Edit the TO list - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_to(struct EnvelopeWindowData *wdata, int op)
{
#ifdef USE_NNTP
  if (wdata->is_news)
    return FR_NO_ACTION;
#endif
  if (!edit_address_list(HDR_TO, &wdata->email->env->to))
    return FR_NO_ACTION;

  update_crypt_info(wdata);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_TO);
  return FR_SUCCESS;
}

/**
 * op_compose_pgp_menu - Show PGP options - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_compose_pgp_menu(struct EnvelopeWindowData *wdata, int op)
{
  const SecurityFlags old_flags = wdata->email->security;
  if (!(WithCrypto & APPLICATION_PGP))
    return FR_NOT_IMPL;
  if (!crypt_has_module_backend(APPLICATION_PGP))
  {
    mutt_error(_("No PGP backend configured"));
    return FR_ERROR;
  }
  if (((WithCrypto & APPLICATION_SMIME) != 0) && (wdata->email->security & APPLICATION_SMIME))
  {
    if (wdata->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return FR_NO_ACTION;
      }
      wdata->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    wdata->email->security &= ~APPLICATION_SMIME;
    wdata->email->security |= APPLICATION_PGP;
    update_crypt_info(wdata);
  }
  wdata->email->security = crypt_pgp_send_menu(wdata->email);
  update_crypt_info(wdata);
  if (wdata->email->security == old_flags)
    return FR_NO_ACTION;

  mutt_message_hook(NULL, wdata->email, MUTT_SEND2_HOOK);
  notify_send(wdata->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return FR_SUCCESS;
}

/**
 * op_compose_smime_menu - Show S/MIME options - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_compose_smime_menu(struct EnvelopeWindowData *wdata, int op)
{
  const SecurityFlags old_flags = wdata->email->security;
  if (!(WithCrypto & APPLICATION_SMIME))
    return FR_NOT_IMPL;
  if (!crypt_has_module_backend(APPLICATION_SMIME))
  {
    mutt_error(_("No S/MIME backend configured"));
    return FR_ERROR;
  }

  if (((WithCrypto & APPLICATION_PGP) != 0) && (wdata->email->security & APPLICATION_PGP))
  {
    if (wdata->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("PGP already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return FR_NO_ACTION;
      }
      wdata->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    wdata->email->security &= ~APPLICATION_PGP;
    wdata->email->security |= APPLICATION_SMIME;
    update_crypt_info(wdata);
  }
  wdata->email->security = crypt_smime_send_menu(wdata->email);
  update_crypt_info(wdata);
  if (wdata->email->security == old_flags)
    return FR_NO_ACTION;

  mutt_message_hook(NULL, wdata->email, MUTT_SEND2_HOOK);
  notify_send(wdata->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return FR_SUCCESS;
}

#ifdef USE_AUTOCRYPT
/**
 * op_compose_autocrypt_menu - Show autocrypt compose menu options - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_compose_autocrypt_menu(struct EnvelopeWindowData *wdata, int op)
{
  const SecurityFlags old_flags = wdata->email->security;
  const bool c_autocrypt = cs_subset_bool(wdata->sub, "autocrypt");
  if (!c_autocrypt)
    return FR_NO_ACTION;

  if ((WithCrypto & APPLICATION_SMIME) && (wdata->email->security & APPLICATION_SMIME))
  {
    if (wdata->email->security & (SEC_ENCRYPT | SEC_SIGN))
    {
      if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
      {
        mutt_clear_error();
        return FR_NO_ACTION;
      }
      wdata->email->security &= ~(SEC_ENCRYPT | SEC_SIGN);
    }
    wdata->email->security &= ~APPLICATION_SMIME;
    wdata->email->security |= APPLICATION_PGP;
    update_crypt_info(wdata);
  }
  autocrypt_compose_menu(wdata->email, wdata->sub);
  update_crypt_info(wdata);
  if (wdata->email->security == old_flags)
    return FR_NO_ACTION;

  mutt_message_hook(NULL, wdata->email, MUTT_SEND2_HOOK);
  notify_send(wdata->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);
  return FR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_envelope_edit_followup_to - Edit the Followup-To field - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_followup_to(struct EnvelopeWindowData *wdata, int op)
{
  if (!wdata->is_news)
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, wdata->email->env->followup_to);
  if (mutt_buffer_get_field(Prompts[HDR_FOLLOWUPTO], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&wdata->email->env->followup_to, mutt_buffer_string(buf));
    mutt_env_notify_send(wdata->email, NT_ENVELOPE_FOLLOWUP_TO);
    rc = FR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_newsgroups - Edit the newsgroups list - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_newsgroups(struct EnvelopeWindowData *wdata, int op)
{
  if (!wdata->is_news)
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, wdata->email->env->newsgroups);
  if (mutt_buffer_get_field(Prompts[HDR_NEWSGROUPS], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&wdata->email->env->newsgroups, mutt_buffer_string(buf));
    mutt_env_notify_send(wdata->email, NT_ENVELOPE_NEWSGROUPS);
    rc = FR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_envelope_edit_x_comment_to - Edit the X-Comment-To field - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_envelope_edit_x_comment_to(struct EnvelopeWindowData *wdata, int op)
{
  const bool c_x_comment_to = cs_subset_bool(wdata->sub, "x_comment_to");
  if (!(wdata->is_news && c_x_comment_to))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, wdata->email->env->x_comment_to);
  if (mutt_buffer_get_field(Prompts[HDR_XCOMMENTTO], buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&wdata->email->env->x_comment_to, mutt_buffer_string(buf));
    mutt_env_notify_send(wdata->email, NT_ENVELOPE_X_COMMENT_TO);
    rc = FR_SUCCESS;
  }

  mutt_buffer_pool_release(&buf);
  return rc;
}
#endif

#ifdef MIXMASTER
/**
 * op_compose_mix - Send the message through a mixmaster remailer chain - Implements ::envelope_function_t - @ingroup envelope_function_api
 */
static int op_compose_mix(struct EnvelopeWindowData *wdata, int op)
{
  dlg_mixmaster(&wdata->email->chain);
  mutt_message_hook(NULL, wdata->email, MUTT_SEND2_HOOK);
  mutt_env_notify_send(wdata->email, NT_ENVELOPE_MIXMASTER);
  return FR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * EnvelopeFunctions - All the NeoMutt functions that the Envelope supports
 */
struct EnvelopeFunction EnvelopeFunctions[] = {
// clang-format off
#ifdef USE_AUTOCRYPT
  { OP_COMPOSE_AUTOCRYPT_MENU,            op_compose_autocrypt_menu },
#endif
#ifdef MIXMASTER
  { OP_COMPOSE_MIX,                       op_compose_mix },
#endif
  { OP_COMPOSE_PGP_MENU,                  op_compose_pgp_menu },
  { OP_COMPOSE_SMIME_MENU,                op_compose_smime_menu },
  { OP_ENVELOPE_EDIT_BCC,                 op_envelope_edit_bcc },
  { OP_ENVELOPE_EDIT_CC,                  op_envelope_edit_cc },
  { OP_ENVELOPE_EDIT_FCC,                 op_envelope_edit_fcc },
#ifdef USE_NNTP
  { OP_ENVELOPE_EDIT_FOLLOWUP_TO,         op_envelope_edit_followup_to },
#endif
  { OP_ENVELOPE_EDIT_FROM,                op_envelope_edit_from },
#ifdef USE_NNTP
  { OP_ENVELOPE_EDIT_NEWSGROUPS,          op_envelope_edit_newsgroups },
#endif
  { OP_ENVELOPE_EDIT_REPLY_TO,            op_envelope_edit_reply_to },
  { OP_ENVELOPE_EDIT_SUBJECT,             op_envelope_edit_subject },
  { OP_ENVELOPE_EDIT_TO,                  op_envelope_edit_to },
#ifdef USE_NNTP
  { OP_ENVELOPE_EDIT_X_COMMENT_TO,        op_envelope_edit_x_comment_to },
#endif
  { 0, NULL },
  // clang-format on
};

/**
 * env_function_dispatcher - Perform an Envelope function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int env_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  int rc = FR_UNKNOWN;
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

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
