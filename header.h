/**
 * @file
 * Representation of the email's header
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

#ifndef _MUTT_HEADER_H
#define _MUTT_HEADER_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "mutt/mutt.h"
#include "tags.h"

/**
 * struct Header - The header/envelope of an email
 */
struct Header
{
  unsigned int security : 12; /**< bit 0-8: flags, bit 9,10: application.
                                 see: mutt_crypt.h pgplib.h, smime.h */

  bool mime            : 1; /**< has a MIME-Version header? */
  bool flagged         : 1; /**< marked important? */
  bool tagged          : 1;
  bool deleted         : 1;
  bool purge           : 1; /**< skip trash folder when deleting */
  bool quasi_deleted   : 1; /**< deleted from neomutt, but not modified on disk */
  bool changed         : 1;
  bool attach_del      : 1; /**< has an attachment marked for deletion */
  bool old             : 1;
  bool read            : 1;
  bool expired         : 1; /**< already expired? */
  bool superseded      : 1; /**< got superseded? */
  bool replied         : 1;
  bool subject_changed : 1; /**< used for threading */
  bool threaded        : 1; /**< used for threading */
  bool display_subject : 1; /**< used for threading */
  bool recip_valid     : 1; /**< is_recipient is valid */
  bool active          : 1; /**< message is not to be removed */
  bool trash           : 1; /**< message is marked as trashed on disk.
                             * This flag is used by the maildir_trash
                             * option.
                             */
  bool xlabel_changed  : 1; /**< editable - used for syncing */

  /* timezone of the sender of this message */
  unsigned int zhours : 5;
  unsigned int zminutes : 6;
  bool zoccident : 1;

  /* bits used for caching when searching */
  bool searched : 1;
  bool matched : 1;

  /* tells whether the attachment count is valid */
  bool attach_valid : 1;

  /* the following are used to support collapsing threads  */
  bool collapsed : 1; /**< is this message part of a collapsed thread? */
  bool limited : 1;   /**< is this message in a limited view?  */
  size_t num_hidden;  /**< number of hidden messages in this view */

  short recipient;    /**< user_is_recipient()'s return value, cached */

  int pair;           /**< color-pair to use when displaying in the index */

  time_t date_sent;   /**< time when the message was sent (UTC) */
  time_t received;    /**< time when the message was placed in the mailbox */
  LOFF_T offset;      /**< where in the stream does this message begin? */
  int lines;          /**< how many lines in the body of this message? */
  int index;          /**< the absolute (unsorted) message number */
  int msgno;          /**< number displayed to the user */
  int virtual;        /**< virtual message number */
  int score;
  struct Envelope *env;      /**< envelope information */
  struct Body *content;      /**< list of MIME parts */
  char *path;

  char *tree; /**< character string to print thread tree */
  struct MuttThread *thread;

  /* Number of qualifying attachments in message, if attach_valid */
  short attach_total;

#ifdef MIXMASTER
  struct ListHead chain;
#endif

#ifdef USE_POP
  int refno; /**< message number on server */
#endif

  struct TagHead tags; /**< for drivers that support server tagging */

#if defined(USE_POP) || defined(USE_IMAP) || defined(USE_NNTP) || defined(USE_NOTMUCH)
  void *data;                       /**< driver-specific data */
  void (*free_cb)(struct Header *); /**< driver-specific data free function */
#endif

  char *maildir_flags; /**< unknown maildir flags */
};

void           mutt_header_free(struct Header **h);
struct Header *mutt_header_new(void);

int mbox_strict_cmp_headers(const struct Header *h1, const struct Header *h2);

#endif /* _MUTT_HEADER_H */
