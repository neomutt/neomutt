/**
 * @file
 * Representation of an email
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "mutt/lib.h"
#include "tags.h"
#include "ncrypt/lib.h"

/**
 * struct Email - The envelope/body of an email
 */
struct Email
{
  SecurityFlags security;      ///< bit 0-10: flags, bit 11,12: application, bit 13: traditional pgp
                               ///< See: ncrypt/lib.h pgplib.h, smime.h

  bool mime            : 1;    ///< Has a MIME-Version header?
  bool flagged         : 1;    ///< Marked important?
  bool tagged          : 1;    ///< Email is tagged
  bool deleted         : 1;    ///< Email is deleted
  bool purge           : 1;    ///< Skip trash folder when deleting
  bool quasi_deleted   : 1;    ///< Deleted from neomutt, but not modified on disk
  bool changed         : 1;    ///< Email has been edited
  bool attach_del      : 1;    ///< Has an attachment marked for deletion
  bool old             : 1;    ///< Email is seen, but unread
  bool read            : 1;    ///< Email is read
  bool expired         : 1;    ///< Already expired?
  bool superseded      : 1;    ///< Got superseded?
  bool replied         : 1;    ///< Email has been replied to
  bool subject_changed : 1;    ///< Used for threading
  bool threaded        : 1;    ///< Used for threading
  bool display_subject : 1;    ///< Used for threading
  bool recip_valid     : 1;    ///< Is_recipient is valid
  bool active          : 1;    ///< Message is not to be removed
  bool trash           : 1;    ///< Message is marked as trashed on disk (used by the maildir_trash option)

  // timezone of the sender of this message
  unsigned int zhours   : 5;   ///< Hours away from UTC
  unsigned int zminutes : 6;   ///< Minutes away from UTC
  bool zoccident        : 1;   ///< True, if west of UTC, False if east

  bool searched : 1;           ///< Email has been searched
  bool matched  : 1;           ///< Search matches this Email

  bool attach_valid : 1;       ///< true when the attachment count is valid

  // the following are used to support collapsing threads
  bool collapsed : 1;          ///< Is this message part of a collapsed thread?
  bool limited   : 1;          ///< Is this message in a limited view?
  size_t num_hidden;           ///< Number of hidden messages in this view
                               ///< (only valid for the root header, when collapsed is set)

  short recipient;             ///< User_is_recipient()'s return value, cached

  int pair;                    ///< Color-pair to use when displaying in the index

  time_t date_sent;            ///< Time when the message was sent (UTC)
  time_t received;             ///< Time when the message was placed in the mailbox
  LOFF_T offset;               ///< Where in the stream does this message begin?
  int lines;                   ///< How many lines in the body of this message?
  int index;                   ///< The absolute (unsorted) message number
  int msgno;                   ///< Number displayed to the user
  int vnum;                    ///< Virtual message number
  int score;                   ///< Message score
  struct Envelope *env;        ///< Envelope information
  struct Body *content;        ///< List of MIME parts
  char *path;                  ///< Path of Email (for local Mailboxes)

  char *tree;                  ///< Character string to print thread tree
  struct MuttThread *thread;   ///< Thread of Emails

  short attach_total;          ///< Number of qualifying attachments in message, if attach_valid

#ifdef MIXMASTER
  struct ListHead chain;       ///< Mixmaster chain
#endif

  struct TagList tags;         ///< For drivers that support server tagging

  char *maildir_flags;         ///< Unknown maildir flags

  void *edata;                    ///< Driver-specific data
  void (*edata_free)(void **ptr); ///< Driver-specific data free function
  struct Notify *notify;          ///< Notifications handler
};

/**
 * struct EmailNode - List of Emails
 */
struct EmailNode
{
  struct Email *email;             ///< Email in the list
  STAILQ_ENTRY(EmailNode) entries; ///< Linked list
};
STAILQ_HEAD(EmailList, EmailNode);

/**
 * enum NotifyEmail - Types of Email Event
 *
 * Observers of #NT_EMAIL will be passed an #EventEmail.
 */
enum NotifyEmail
{
  NT_EMAIL_ADD = 1, ///< A new Email has just been created
  NT_EMAIL_REMOVE,  ///< An Email is about to be destroyed
};

/**
 * struct EventEmail - An Event that happened to an Email
 */
struct EventEmail
{
  int num_emails;
  struct Email **emails;
};

bool          email_cmp_strict(const struct Email *e1, const struct Email *e2);
void          email_free      (struct Email **ptr);
struct Email *email_new       (void);
size_t        email_size      (const struct Email *e);

int  emaillist_add_email(struct EmailList *el, struct Email *e);
void emaillist_clear    (struct EmailList *el);

#endif /* MUTT_EMAIL_EMAIL_H */
