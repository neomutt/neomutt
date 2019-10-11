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
#include "ncrypt/lib.h"
#include "tags.h"

/**
 * struct Email - The envelope/body of an email
 */
struct Email
{
  SecurityFlags security;      ///< bit 0-10: flags, bit 11,12: application, bit 13: traditional pgp
                               ///< See: ncrypt/lib.h pgplib.h, smime.h
  bool active           : 1;   ///< Message is not to be removed
  bool expired          : 1;   ///< Already expired?
  bool flagged          : 1;   ///< Marked important?
  bool mime             : 1;   ///< Has a MIME-Version header?
  bool old              : 1;   ///< Email is seen, but unread
  bool read             : 1;   ///< Email is read
  bool replied          : 1;   ///< Email has been replied to
  bool superseded       : 1;   ///< Got superseded?
  bool trash            : 1;   ///< Message is marked as trashed on disk (used by the maildir_trash option)

  unsigned int zhours   : 5;   ///< Sender TZ: Hours away from UTC
  unsigned int zminutes : 6;   ///< Sender TZ: Minutes away from UTC
  bool zoccident        : 1;   ///< Sender TZ: True, if west of UTC, False if east

  time_t date_sent;            ///< Time when the message was sent (UTC)
  time_t received;             ///< Time when the message was placed in the mailbox
  int lines;                   ///< How many lines in the body of this message?

  // ------------------------------------------------------------
  // Questionable

  // Only non-zero for 'mbox' which doesn't use the header cache
  LOFF_T offset;               ///< Where in the stream does this message begin?

  // Saved by Maildir,Pop,Imap but overwritten after reading from cache
  int index;                   ///< The absolute (unsorted) message number

  // Stored as 0 by all backends; always recalculated when opening Mailbox
  int score;                   ///< Message score

  // Never set when writing to cache
  bool attach_del      : 1;    ///< Has an attachment marked for deletion
  bool deleted         : 1;    ///< Email is deleted
  bool purge           : 1;    ///< Skip trash folder when deleting
  bool quasi_deleted   : 1;    ///< Deleted from neomutt, but not modified on disk

  // GUI
  bool display_subject : 1;    ///< Used for threading
  bool subject_changed : 1;    ///< Used for threading

  int msgno;                   ///< Number displayed to the user
  int vnum;                    ///< Virtual message number

  // ------------------------------------------------------------
  // This DATA is cached, but the POINTERS aren't

  struct Envelope *env;        ///< Envelope information
  struct Body *content;        ///< List of MIME parts
  char *maildir_flags;         ///< Unknown maildir flags

  // ------------------------------------------------------------
  // Not cached, mostly View data

  bool attach_valid    : 1;    ///< true when the attachment count is valid
  bool changed         : 1;    ///< Email has been edited
  bool collapsed       : 1;    ///< Is this message part of a collapsed thread?
  bool limited         : 1;    ///< Is this message in a limited view?
  bool matched         : 1;    ///< Search matches this Email
  bool recip_valid     : 1;    ///< Is_recipient is valid
  bool searched        : 1;    ///< Email has been searched
  bool tagged          : 1;    ///< Email is tagged
  bool threaded        : 1;    ///< Used for threading

  short attach_total;          ///< Number of qualifying attachments in message, if attach_valid
  size_t num_hidden;           ///< Number of hidden messages in this view
  short recipient;             ///< User_is_recipient()'s return value, cached
  int pair;                    ///< Color-pair to use when displaying in the index
  char *path;                  ///< Path of Email (for local Mailboxes)
  char *tree;                  ///< Character string to print thread tree
  struct MuttThread *thread;   ///< Thread of Emails
  struct TagList tags;         ///< For drivers that support server tagging

#ifdef MIXMASTER
  struct ListHead chain;       ///< Mixmaster chain
#endif

  void *edata;                 ///< Driver-specific data
  void (*free_edata)(void **); ///< Driver-specific data free function
  struct Notify *notify;       ///< Notifications handler
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
 * struct EventEmail - An Event that happened to an Email
 */
struct EventEmail
{
  int num_emails;
  struct Email **emails;
};

/**
 * enum NotifyEmail - Types of Email Event
 */
enum NotifyEmail
{
  NT_EMAIL_ADD = 1,
  NT_EMAIL_REMOVE,
  NT_EMAIL_NEW,
};

bool          email_cmp_strict(const struct Email *e1, const struct Email *e2);
void          email_free      (struct Email **ptr);
struct Email *email_new       (void);
size_t        email_size      (const struct Email *e);

int  emaillist_add_email(struct EmailList *el, struct Email *e);
void emaillist_clear    (struct EmailList *el);

#endif /* MUTT_EMAIL_EMAIL_H */
