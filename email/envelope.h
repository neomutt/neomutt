/**
 * @file
 * Representation of an email header (envelope)
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019-2021 Pietro Cerutti <gahr@gahr.ch>
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

struct Email;

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
  char *addr;                   ///< Email address
  char *keydata;                ///< PGP Key data
  bool prefer_encrypt : 1;      ///< User prefers encryption
  bool invalid        : 1;      ///< Header is invalid
  struct AutocryptHeader *next; ///< Linked list
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
  struct AddressList x_original_to;    ///< Email's 'X-Original-to'
  char *list_post;                     ///< This stores a mailto URL, or nothing
  char *list_subscribe;                ///< This stores a mailto URL, or nothing
  char *list_unsubscribe;              ///< This stores a mailto URL, or nothing
  char *const subject;                 ///< Email's subject
  char *const real_subj;               ///< Offset of the real subject
  char *disp_subj;                     ///< Display subject (modified copy of subject)
  char *message_id;                    ///< Message ID
  char *supersedes;                    ///< Supersedes header
  char *date;                          ///< Sent date
  char *x_label;                       ///< X-Label
  char *organization;                  ///< Organisation header
  char *newsgroups;                    ///< List of newsgroups
  char *xref;                          ///< List of cross-references
  char *followup_to;                   ///< List of 'followup-to' fields
  char *x_comment_to;                  ///< List of 'X-comment-to' fields
  struct Buffer spam;                  ///< Spam header
  struct ListHead references;          ///< message references (in reverse order)
  struct ListHead in_reply_to;         ///< in-reply-to header content
  struct ListHead userhdrs;            ///< user defined headers
#ifdef USE_AUTOCRYPT
  struct AutocryptHeader *autocrypt;        ///< Autocrypt header
  struct AutocryptHeader *autocrypt_gossip; ///< Autocrypt Gossip header
#endif
  unsigned char changed; ///< Changed fields, e.g. #MUTT_ENV_CHANGED_SUBJECT
};

/**
 * ExpandoDataEnvelope - Expando UIDs for Envelopes
 *
 * @sa ED_ENVELOPE, ExpandoDomain
 */
enum ExpandoDataEnvelope
{
  ED_ENV_CC_ALL = 1,           ///< Envelope.cc
  ED_ENV_FIRST_NAME,           ///< Envelope.from, Envelope.to, Envelope.cc
  ED_ENV_FROM,                 ///< Envelope.from (first)
  ED_ENV_FROM_FULL,            ///< Envelope.from (all)
  ED_ENV_INITIALS,             ///< Envelope.from (first)
  ED_ENV_LIST_ADDRESS,         ///< Envelope.to, Envelope.cc
  ED_ENV_LIST_EMPTY,           ///< Envelope.to, Envelope.cc
  ED_ENV_MESSAGE_ID,           ///< Envelope.message_id
  ED_ENV_NAME,                 ///< Envelope.from (first)
  ED_ENV_NEWSGROUP,            ///< Envelope.newsgroups
  ED_ENV_ORGANIZATION,         ///< Envelope.organization
  ED_ENV_REAL_NAME,            ///< Envelope.to (first)
  ED_ENV_REPLY_TO,             ///< Envelope.reply_to
  ED_ENV_SENDER,               ///< Envelope, make_from()
  ED_ENV_SENDER_PLAIN,         ///< Envelope, make_from()
  ED_ENV_SPAM,                 ///< Envelope.spam
  ED_ENV_SUBJECT,              ///< Envelope.subject, Envelope.disp_subj
  ED_ENV_THREAD_TREE,          ///< Email.tree
  ED_ENV_THREAD_X_LABEL,       ///< Envelope.x_label
  ED_ENV_TO,                   ///< Envelope.to, Envelope.cc (first)
  ED_ENV_TO_ALL,               ///< Envelope.to (all)
  ED_ENV_USERNAME,             ///< Envelope.from
  ED_ENV_USER_NAME,            ///< Envelope.to (first)
  ED_ENV_X_COMMENT_TO,         ///< Envelope.x_comment_to
  ED_ENV_X_LABEL,              ///< Envelope.x_label
};

/**
 * enum NotifyEnvelope - Types of Envelope Event
 *
 * Observers of #NT_ENVELOPE will not be passed any Event data.
 *
 * Notifications that an Envelope field has changed.
 * Envelope doesn't support notifications, so events will be passed to the Email.
 */
enum NotifyEnvelope
{
  NT_ENVELOPE_BCC  = 1,        ///< "Bcc:"          header has changed
  NT_ENVELOPE_CC,              ///< "Cc:"           header has changed
  NT_ENVELOPE_FCC,             ///< "Fcc:"          header has changed
  NT_ENVELOPE_FOLLOWUP_TO,     ///< "Followup-To:"  header has changed
  NT_ENVELOPE_FROM,            ///< "From:"         header has changed
  NT_ENVELOPE_NEWSGROUPS,      ///< "Newsgroups:"   header has changed
  NT_ENVELOPE_REPLY_TO,        ///< "Reply-To:"     header has changed
  NT_ENVELOPE_SUBJECT,         ///< "Subject:"      header has changed
  NT_ENVELOPE_TO,              ///< "To:"           header has changed
  NT_ENVELOPE_X_COMMENT_TO,    ///< "X-Comment-To:" header has changed
};

bool             mutt_env_cmp_strict (const struct Envelope *e1, const struct Envelope *e2);
void             mutt_env_free       (struct Envelope **ptr);
void             mutt_env_merge      (struct Envelope *base, struct Envelope **extra);
struct Envelope *mutt_env_new        (void);
bool             mutt_env_notify_send(struct Email *e, enum NotifyEnvelope type);
void             mutt_env_set_subject(struct Envelope *env, const char *subj);
int              mutt_env_to_intl    (struct Envelope *env, const char **tag, char **err);
void             mutt_env_to_local   (struct Envelope *env);

#ifdef USE_AUTOCRYPT
struct AutocryptHeader *mutt_autocrypthdr_new(void);
void                    mutt_autocrypthdr_free(struct AutocryptHeader **ptr);
#endif

#endif /* MUTT_EMAIL_ENVELOPE_H */
