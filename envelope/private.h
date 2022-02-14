/**
 * @file
 * Private Envelope Data
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

#ifndef MUTT_ENVELOPE_PRIVATE_H
#define MUTT_ENVELOPE_PRIVATE_H

#include "config.h"

struct ComposeSharedData;

/**
 * enum HeaderField - Ordered list of headers for the compose screen
 *
 * The position of various fields on the compose screen.
 */
enum HeaderField
{
  HDR_FROM,    ///< "From:" field
  HDR_TO,      ///< "To:" field
  HDR_CC,      ///< "Cc:" field
  HDR_BCC,     ///< "Bcc:" field
  HDR_SUBJECT, ///< "Subject:" field
  HDR_REPLYTO, ///< "Reply-To:" field
  HDR_FCC,     ///< "Fcc:" (save folder) field
#ifdef MIXMASTER
  HDR_MIX, ///< "Mix:" field (Mixmaster chain)
#endif
  HDR_CRYPT,     ///< "Security:" field (encryption/signing info)
  HDR_CRYPTINFO, ///< "Sign as:" field (encryption/signing info)
#ifdef USE_AUTOCRYPT
  HDR_AUTOCRYPT, ///< "Autocrypt:" and "Recommendation:" fields
#endif
#ifdef USE_NNTP
  HDR_NEWSGROUPS, ///< "Newsgroups:" field
  HDR_FOLLOWUPTO, ///< "Followup-To:" field
  HDR_XCOMMENTTO, ///< "X-Comment-To:" field
#endif
  HDR_CUSTOM_HEADERS, ///< "Headers:" field
  HDR_ATTACH_TITLE,   ///< The "-- Attachments" line
};

extern const char *const Prompts[];

int op_compose_autocrypt_menu    (struct ComposeSharedData *shared, int op);
int op_compose_mix               (struct ComposeSharedData *shared, int op);
int op_compose_pgp_menu          (struct ComposeSharedData *shared, int op);
int op_compose_smime_menu        (struct ComposeSharedData *shared, int op);
int op_envelope_edit_bcc         (struct ComposeSharedData *shared, int op);
int op_envelope_edit_cc          (struct ComposeSharedData *shared, int op);
int op_envelope_edit_fcc         (struct ComposeSharedData *shared, int op);
int op_envelope_edit_followup_to (struct ComposeSharedData *shared, int op);
int op_envelope_edit_from        (struct ComposeSharedData *shared, int op);
int op_envelope_edit_newsgroups  (struct ComposeSharedData *shared, int op);
int op_envelope_edit_reply_to    (struct ComposeSharedData *shared, int op);
int op_envelope_edit_subject     (struct ComposeSharedData *shared, int op);
int op_envelope_edit_to          (struct ComposeSharedData *shared, int op);
int op_envelope_edit_x_comment_to(struct ComposeSharedData *shared, int op);

#endif /* MUTT_ENVELOPE_PRIVATE_H */
