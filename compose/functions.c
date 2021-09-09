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
#include "index/lib.h"

struct ComposeSharedData;

/**
 * op_bounce_message - remail a message to another user - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_bounce_message(struct ComposeSharedData *shard, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_attach_file - attach files to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_attach_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_attach_key - attach a PGP public key - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_attach_key(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_attach_message - attach messages to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_attach_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_bcc - edit the BCC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_bcc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_cc - edit the CC list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_cc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_description - edit attachment description - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_description(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_encoding - edit attachment transfer-encoding - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_encoding(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_fcc - enter a file to save a copy of this message in - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_fcc(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_file - edit the file to be attached - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_from - edit the from field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_from(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_headers - edit the message with headers - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_headers(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_language - edit the 'Content-Language' of the attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_language(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_message - edit the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_mime - edit attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_mime(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_reply_to - edit the Reply-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_reply_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_subject - edit the subject of this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_subject(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_to - edit the TO list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_get_attachment - get a temporary copy of an attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_get_attachment(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_group_alts - group tagged attachments as 'multipart/alternative' - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_group_alts(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_group_lingual - group tagged attachments as 'multipart/multilingual' - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_group_lingual(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_ispell - run ispell on the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_ispell(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_move_down - move an attachment down in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_move_down(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_move_up - move an attachment up in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_move_up(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_new_mime - compose new attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_new_mime(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_pgp_menu - show PGP options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_pgp_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_postpone_message - save this message to send later - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_postpone_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_rename_attachment - send attachment with a different name - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_rename_attachment(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_rename_file - rename/move an attached file - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_rename_file(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_send_message - send the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_send_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_smime_menu - show S/MIME options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_smime_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_disposition - toggle disposition between inline/attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_toggle_disposition(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_recode - toggle recoding of this attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_toggle_recode(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_toggle_unlink - toggle whether to delete file after sending it - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_toggle_unlink(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_update_encoding - update an attachment's encoding info - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_update_encoding(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_write_message - write the message to a folder - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_write_message(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_delete - delete the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_delete(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_display_headers - display message and toggle header weeding - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_display_headers(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_edit_type - edit attachment content type - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_edit_type(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_exit - exit this menu - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_exit(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_filter - filter attachment through a shell command - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_filter(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_forget_passphrase(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_print - print the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_print(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_save - save message/attachment to a mailbox/file - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_save(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

// -----------------------------------------------------------------------------

#ifdef USE_AUTOCRYPT
/**
 * op_compose_autocrypt_menu - show autocrypt compose menu options - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_autocrypt_menu(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}
#endif

#ifdef USE_NNTP
/**
 * op_compose_edit_followup_to - edit the Followup-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_followup_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_newsgroups - edit the newsgroups list - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_newsgroups(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}

/**
 * op_compose_edit_x_comment_to - edit the X-Comment-To field - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_edit_x_comment_to(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}
#endif

#ifdef MIXMASTER
/**
 * op_compose_mix - send the message through a mixmaster remailer chain - Implements ::compose_function_t - @ingroup compose_function_api
 */
int op_compose_mix(struct ComposeSharedData *shared, int op)
{
  return IR_CONTINUE;
}
#endif
