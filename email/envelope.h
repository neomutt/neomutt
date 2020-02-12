/**
 * @file
 * Representation of an email header (envelope)
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_EMAIL_ENVELOPE_H
#define MUTT_EMAIL_ENVELOPE_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "address/lib.h"

#define MUTT_ENV_CHANGED_IRT     (1 << 0)  ///< In-Reply-To changed to link/break threads
#define MUTT_ENV_CHANGED_REFS    (1 << 1)  ///< References changed to break thread
#define MUTT_ENV_CHANGED_XLABEL  (1 << 2)  ///< X-Label edited
#define MUTT_ENV_CHANGED_SUBJECT (1 << 3)  ///< Protected header update

#ifdef USE_AUTOCRYPT
/**
 * struct AutocryptHeader - Parse Autocrypt header info
 */
struct AutocryptHeader
{
  char *addr;
  char *keydata;
  bool prefer_encrypt : 1;
  bool invalid : 1;
  struct AutocryptHeader *next;
};
#endif

/**
 * struct Envelope - The header of an Email
 */
struct Envelope
{
  struct AddressList return_path;      ///< Return path for the Email
  struct AddressList from;             ///< Email's 'From' list
  struct AddressList to;               ///< Email's 'To' list
  struct AddressList cc;               ///< Email's 'Cc' list
  struct AddressList bcc;              ///< Email's 'Bcc' list
  struct AddressList sender;           ///< Email's sender
  struct AddressList reply_to;         ///< Email's 'reply-to'
  struct AddressList mail_followup_to; ///< Email's 'mail-followup-to'
  struct AddressList x_original_to;    ///< Email's 'X-Orig-to'
  char *list_post;                     ///< This stores a mailto URL, or nothing
  char *subject;                       ///< Email's subject
  char *real_subj;                     ///< Offset of the real subject
  char *disp_subj;                     ///< Display subject (modified copy of subject)
  char *message_id;                    ///< Message ID
  char *supersedes;                    ///< Supersedes header
  char *date;                          ///< Sent date
  char *x_label;                       ///< X-Label
  char *organization;                  ///< Organisation header
#ifdef USE_NNTP
  char *newsgroups;                    ///< List of newsgroups
  char *xref;                          ///< List of cross-references
  char *followup_to;                   ///< List of 'followup-to' fields
  char *x_comment_to;                  ///< List of 'X-comment-to' fields
#endif
  struct Buffer spam;                  ///< Spam header
  struct ListHead references;          ///< message references (in reverse order)
  struct ListHead in_reply_to;         ///< in-reply-to header content
  struct ListHead userhdrs;            ///< user defined headers
#ifdef USE_AUTOCRYPT
  struct AutocryptHeader *autocrypt;
  struct AutocryptHeader *autocrypt_gossip;
#endif
  unsigned char changed;               ///< Changed fields, e.g. #MUTT_ENV_CHANGED_SUBJECT
};

bool             mutt_env_cmp_strict(const struct Envelope *e1, const struct Envelope *e2);
void             mutt_env_free      (struct Envelope **ptr);
void             mutt_env_merge     (struct Envelope *base, struct Envelope **extra);
struct Envelope *mutt_env_new       (void);
int              mutt_env_to_intl   (struct Envelope *env, const char **tag, char **err);
void             mutt_env_to_local  (struct Envelope *e);

#ifdef USE_AUTOCRYPT
struct AutocryptHeader *mutt_autocrypthdr_new(void);
void                    mutt_autocrypthdr_free(struct AutocryptHeader **p);
#endif

#endif /* MUTT_EMAIL_ENVELOPE_H */
