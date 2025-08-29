/**
 * @file
 * Representation of an email
 *
 * @authors
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_EMAIL_EMAIL_H
#define MUTT_EMAIL_EMAIL_H

#include "config.h"
#include <stdbool.h>
#include <time.h>
#include "mutt/lib.h"
#include "ncrypt/lib.h"
#include "tags.h"

/**
 * struct Email - The envelope/body of an email
 */
struct Email
{
  // ---------------------------------------------------------------------------
  // Data that gets stored in the Header Cache

  SecurityFlags security;      ///< bit 0-10: flags, bit 11,12: application, bit 13: traditional pgp
                               ///< See: ncrypt/lib.h pgplib.h, smime.h

  bool expired    : 1;         ///< Already expired?
  bool flagged    : 1;         ///< Marked important?
  bool mime       : 1;         ///< Has a MIME-Version header?
  bool old        : 1;         ///< Email is seen, but unread
  bool read       : 1;         ///< Email is read
  bool replied    : 1;         ///< Email has been replied to
  bool superseded : 1;         ///< Got superseded?
  bool trash      : 1;         ///< Message is marked as trashed on disk (used by the maildir_trash option)

  // Timezone of the sender of this message
  unsigned int zhours   : 5;   ///< Hours away from UTC
  unsigned int zminutes : 6;   ///< Minutes away from UTC
  bool zoccident        : 1;   ///< True, if west of UTC, False if east

  time_t date_sent;            ///< Time when the message was sent (UTC)
  time_t received;             ///< Time when the message was placed in the mailbox
  int lines;                   ///< How many lines in the body of this message?

  // ---------------------------------------------------------------------------
  // Management data - Runtime info and glue to hold the objects together

  size_t sequence;             ///< Sequence number assigned on creation
  struct Envelope *env;        ///< Envelope information
  struct Body *body;           ///< List of MIME parts
  char *path;                  ///< Path of Email (for local Mailboxes)
  LOFF_T offset;               ///< Where in the stream does this message begin?
  struct TagList tags;         ///< For drivers that support server tagging
  struct Notify *notify;       ///< Notifications: #NotifyEmail, #EventEmail
  void *edata;                 ///< Driver-specific data

  bool active          : 1;    ///< Message is not to be removed
  bool changed         : 1;    ///< Email has been edited
  bool deleted         : 1;    ///< Email is deleted
  bool purge           : 1;    ///< Skip trash folder when deleting

  /**
   * @defgroup email_edata_free Email Private Data API
   *
   * edata_free - Free the private data attached to the Email
   * @param ptr Private data to be freed
   *
   * @pre ptr  is not NULL
   * @pre *ptr is not NULL
   */
  void (*edata_free)(void **ptr);

#ifdef USE_NOTMUCH
  void *nm_edata;              ///< Notmuch private data
#endif

  // ---------------------------------------------------------------------------
  // View data - Used by the GUI

  bool attach_del      : 1;    ///< Has an attachment marked for deletion
  bool attach_valid    : 1;    ///< true when the attachment count is valid
  bool display_subject : 1;    ///< Used for threading
  bool matched         : 1;    ///< Search matches this Email
  bool quasi_deleted   : 1;    ///< Deleted from neomutt, but not modified on disk
  bool recip_valid     : 1;    ///< Is_recipient is valid
  bool searched        : 1;    ///< Email has been searched
  bool subject_changed : 1;    ///< Used for threading
  bool tagged          : 1;    ///< Email is tagged
  bool threaded        : 1;    ///< Used for threading

  int index;                   ///< The absolute (unsorted) message number
  int msgno;                   ///< Number displayed to the user
  const struct AttrColor *attr_color; ///< Color-pair to use when displaying in the index
  int score;                   ///< Message score
  int vnum;                    ///< Virtual message number
  short attach_total;          ///< Number of qualifying attachments in message, if attach_valid
  short recipient;             ///< User_is_recipient()'s return value, cached

  // The following are used to support collapsing threads
  struct MuttThread *thread;   ///< Thread of Emails
  bool collapsed     : 1;      ///< Is this message part of a collapsed thread?
  bool visible       : 1;      ///< Is this message part of the view?
  bool limit_visited : 1;      ///< Has the limit pattern been applied to this message?
  size_t num_hidden;           ///< Number of hidden messages in this view
                               ///< (only valid when collapsed is set)
  char *tree;                  ///< Character string to print thread tree
};
ARRAY_HEAD(EmailArray, struct Email *);

/**
 * ExpandoDataEmail - Expando UIDs for Emails
 *
 * @sa ED_EMAIL, ExpandoDomain
 */
enum ExpandoDataEmail
{
  ED_EMA_ATTACHMENT_COUNT = 1, ///< Email, mutt_count_body_parts()
  ED_EMA_BODY_CHARACTERS,      ///< Body.length
  ED_EMA_COMBINED_FLAGS,       ///< Email.read, Email.old, thread_is_new(), ...
  ED_EMA_CRYPTO_FLAGS,         ///< Email.security, #SecurityFlags
  ED_EMA_DATE_FORMAT,          ///< Email.date_sent
  ED_EMA_DATE_FORMAT_LOCAL,    ///< Email.date_sent
  ED_EMA_FLAG_CHARS,           ///< Email.deleted, Email.attach_del, ...
  ED_EMA_FROM_LIST,            ///< Envelope.to, Envelope.cc
  ED_EMA_INDEX_HOOK,           ///< Mailbox, Email, mutt_idxfmt_hook()
  ED_EMA_LINES,                ///< Email.lines
  ED_EMA_LIST_OR_SAVE_FOLDER,  ///< Envelope.to, Envelope.cc, check_for_mailing_list()
  ED_EMA_MESSAGE_FLAGS,        ///< Email.tagged, Email.flagged
  ED_EMA_NUMBER,               ///< Email.msgno
  ED_EMA_SCORE,                ///< Email.score
  ED_EMA_SIZE,                 ///< Body.length
  ED_EMA_STATUS_FLAGS,         ///< Email.deleted, Email.attach_del, ...
  ED_EMA_STRF,                 ///< Email.date_sent, Email.zhours, Email.zminutes, Email.zoccident
  ED_EMA_STRF_LOCAL,           ///< Email.date_sent
  ED_EMA_STRF_RECV_LOCAL,      ///< Email.received
  ED_EMA_TAGS,                 ///< Email.tags
  ED_EMA_TAGS_TRANSFORMED,     ///< Email.tags, driver_tags_get_transformed()
  ED_EMA_THREAD_COUNT,         ///< Email, mutt_messages_in_thread()
  ED_EMA_THREAD_HIDDEN_COUNT,  ///< Email.collapsed, Email.num_hidden, ...
  ED_EMA_THREAD_NUMBER,        ///< Email, mutt_messages_in_thread()
  ED_EMA_THREAD_TAGS,          ///< Email.tags
  ED_EMA_TO_CHARS,             ///< Email, User_is_recipient()
};

/**
 * struct EmailNode - List of Emails
 */
struct EmailNode
{
  struct Email *email;             ///< Email in the list
  STAILQ_ENTRY(EmailNode) entries; ///< Linked list
};

/**
 * enum NotifyEmail - Types of Email Event
 *
 * Observers of #NT_EMAIL will be passed an #EventEmail.
 *
 * @note Delete notifications are sent **before** the object is deleted.
 * @note Other notifications are sent **after** the event.
 */
enum NotifyEmail
{
  NT_EMAIL_ADD = 1,         ///< Email has been added
  NT_EMAIL_DELETE,          ///< Email is about to be deleted
  NT_EMAIL_DELETE_ALL,      ///< All the Emails are about to be deleted
  NT_EMAIL_CHANGE,          ///< Email has changed
  NT_EMAIL_CHANGE_ENVELOPE, ///< Email's Envelope has changed
  NT_EMAIL_CHANGE_ATTACH,   ///< Email's Attachments have changed
  NT_EMAIL_CHANGE_SECURITY, ///< Email's security settings have changed
};

/**
 * struct EventEmail - An Event that happened to an Email
 */
struct EventEmail
{
  int num_emails;        ///< Number of Emails the event applies to
  struct Email **emails; ///< Emails affected by the event
};

/**
 * enum NotifyHeader - Types of Header Event
 *
 * Observers of #NT_HEADER will be passed an #EventHeader.
 */
enum NotifyHeader
{
  NT_HEADER_ADD = 1, ///< Header has been added
  NT_HEADER_DELETE,  ///< Header has been removed
  NT_HEADER_CHANGE,  ///< An existing header has been changed
};

/**
 * struct EventHeader - An event that happened to a header
 */
struct EventHeader
{
  char *header; ///< The contents of the header
};

bool          email_cmp_strict(const struct Email *e1, const struct Email *e2);
void          email_free      (struct Email **ptr);
struct Email *email_new       (void);
size_t        email_size      (const struct Email *e);

struct ListNode *header_add   (struct ListHead *hdrlist, const char *header);
struct ListNode *header_find  (const struct ListHead *hdrlist, const char *header);
void             header_free  (struct ListHead *hdrlist, struct ListNode *target);
struct ListNode *header_set   (struct ListHead *hdrlist, const char *header);
struct ListNode *header_update(struct ListNode *hdrnode, const char *header);

#endif /* MUTT_EMAIL_EMAIL_H */
