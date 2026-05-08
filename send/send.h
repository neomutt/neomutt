/**
 * @file
 * Prepare and send an email
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2024 Alejandro Colomar <alx@kernel.org>
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

#ifndef MUTT_SEND_SEND_H
#define MUTT_SEND_SEND_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct AddressList;
struct Body;
struct ConfigSubset;
struct Email;
struct EmailArray;
struct Envelope;
struct Mailbox;

/**
 * enum SendFlag - Flags for mutt_send_message(), e.g. #SEND_REPLY
 */
enum SendFlag
{
  SEND_NONE             =        0,  ///< No flags are set
  SEND_REPLY            = 1U <<  0,  ///< Reply to sender
  SEND_GROUP_REPLY      = 1U <<  1,  ///< Reply to all
  SEND_LIST_REPLY       = 1U <<  2,  ///< Reply to mailing list
  SEND_FORWARD          = 1U <<  3,  ///< Forward email
  SEND_POSTPONED        = 1U <<  4,  ///< Recall a postponed email
  SEND_BATCH            = 1U <<  5,  ///< Send email in batch mode (without user interaction)
  SEND_KEY              = 1U <<  6,  ///< Mail a PGP public key
  SEND_RESEND           = 1U <<  7,  ///< Reply using the current email as a template
  SEND_POSTPONED_FCC    = 1U <<  8,  ///< Used by mutt_get_postponed() to signal that the Mutt-Fcc header field was present
  SEND_NO_FREE_HEADER   = 1U <<  9,  ///< Used by the -E flag
  SEND_DRAFT_FILE       = 1U << 10,  ///< Used by the -H flag
  SEND_TO_SENDER        = 1U << 11,  ///< Compose new email to sender
  SEND_GROUP_CHAT_REPLY = 1U << 12,  ///< Reply to all recipients preserving To/Cc
  SEND_NEWS             = 1U << 13,  ///< Reply to a news article
  SEND_REVIEW_TO        = 1U << 14,  ///< Allow the user to edit the To field
  SEND_CONSUMED_STDIN   = 1U << 15,  ///< stdin has been read; don't read it twice
  SEND_CLI_CRYPTO       = 1U << 16,  ///< Enable message security in modes that by default don't enable it
};
typedef uint32_t SendFlags;

void            mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *env_cur, struct ConfigSubset *sub);
struct Address *mutt_default_from(struct ConfigSubset *sub);
int             mutt_edit_address(struct AddressList *al, const char *field, bool expand_aliases);
void            mutt_encode_descriptions(struct Body *b, bool recurse, struct ConfigSubset *sub);
int             mutt_fetch_recips(struct Envelope *out, struct Envelope *in, SendFlags flags, struct ConfigSubset *sub);
void            mutt_fix_reply_recipients(struct Envelope *env, struct ConfigSubset *sub);
void            mutt_forward_intro(struct Email *e, FILE *fp, struct ConfigSubset *sub);
void            mutt_forward_trailer(struct Email *e, FILE *fp, struct ConfigSubset *sub);
void            mutt_make_attribution_intro(struct Email *e, FILE *fp_out, struct ConfigSubset *sub);
void            mutt_make_attribution_trailer(struct Email *e, FILE *fp_out, struct ConfigSubset *sub);
void            mutt_make_forward_subject(struct Envelope *env, struct Email *e, struct ConfigSubset *sub);
void            mutt_make_misc_reply_headers(struct Envelope *env, struct Envelope *env_cur, struct ConfigSubset *sub);
int             mutt_resend_message(FILE *fp, struct Mailbox *m, struct Email *e_cur, struct ConfigSubset *sub);
int             mutt_send_message(SendFlags flags, struct Email *e_templ, const char *tempfile, struct Mailbox *m, struct EmailArray *ea, struct ConfigSubset *sub);
void            mutt_set_followup_to(struct Envelope *env, struct ConfigSubset *sub);
bool            mutt_send_list_subscribe(struct Mailbox *m, struct Email *e);
bool            mutt_send_list_unsubscribe(struct Mailbox *m, struct Email *e);

#endif /* MUTT_SEND_SEND_H */
