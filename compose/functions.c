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
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "index/lib.h"
#include "opcodes.h"

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");

/**
 * op_compose_attach_file - attach files to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_attach_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_attach_key - attach a PGP public key - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_attach_key(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_attach_message - attach messages to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_attach_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_bcc - edit the BCC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_bcc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_cc - edit the CC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_cc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_description - edit attachment description - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_description(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_encoding - edit attachment transfer-encoding - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_encoding(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_fcc - enter a file to save a copy of this message in - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_fcc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_file - edit the file to be attached - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_from - edit the from field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_from(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_headers - edit the message with headers - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_headers(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_language - edit the 'Content-Language' of the attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_language(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_message - edit the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_mime - edit attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_mime(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_reply_to - edit the Reply-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_reply_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_subject - edit the subject of this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_subject(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_to - edit the TO list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_get_attachment - get a temporary copy of an attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_get_attachment(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_group_alts - group tagged attachments as 'multipart/alternative' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_group_alts(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_group_lingual - group tagged attachments as 'multipart/multilingual' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_group_lingual(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_ispell - run ispell on the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_ispell(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_move_down - move an attachment down in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_move_down(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_move_up - move an attachment up in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_move_up(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_new_mime - compose new attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_new_mime(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_pgp_menu - show PGP options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_pgp_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_postpone_message - save this message to send later - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_postpone_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_rename_attachment - send attachment with a different name - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_rename_attachment(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_rename_file - rename/move an attached file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_rename_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_send_message - send the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_send_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_smime_menu - show S/MIME options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_smime_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_disposition - toggle disposition between inline/attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_disposition(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_recode - toggle recoding of this attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_recode(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_unlink - toggle whether to delete file after sending it - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_toggle_unlink(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_update_encoding - update an attachment's encoding info - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_update_encoding(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_write_message - write the message to a folder - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_write_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_delete - delete the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_delete(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_display_headers - display message and toggle header weeding - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_display_headers(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_edit_type - edit attachment content type - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_edit_type(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_exit - exit this menu - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_exit(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_filter - filter attachment through a shell command - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_filter(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_forget_passphrase(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_print - print the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_print(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_save - save message/attachment to a mailbox/file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_save(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

// -----------------------------------------------------------------------------

#ifdef USE_AUTOCRYPT
/**
 * op_compose_autocrypt_menu - show autocrypt compose menu options - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_autocrypt_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}
#endif

#ifdef USE_NNTP
/**
 * op_compose_edit_followup_to - edit the Followup-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_followup_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_newsgroups - edit the newsgroups list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_newsgroups(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_x_comment_to - edit the X-Comment-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_x_comment_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}
#endif

#ifdef MIXMASTER
/**
 * op_compose_mix - send the message through a mixmaster remailer chain - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_mix(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
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
