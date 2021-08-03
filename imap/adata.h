/**
 * @file
 * Imap-specific Account data
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

#ifndef MUTT_IMAP_ADATA_H
#define MUTT_IMAP_ADATA_H

#include <stdbool.h>
#include <time.h>
#include "private.h" // IWYU pragma: keep
#include "mutt/lib.h"

struct Account;
struct Mailbox;

/**
 * struct ImapAccountData - IMAP-specific Account data - @extends Account
 *
 * This data is specific to a Connection to an IMAP server
 */
struct ImapAccountData
{
  struct Connection *conn; ///< Connection to IMAP server
  bool recovering;
  bool closing;         ///< If true, we are waiting for CLOSE completion
  unsigned char state;  ///< ImapState, e.g. #IMAP_AUTHENTICATED
  unsigned char status; ///< ImapFlags, e.g. #IMAP_FATAL
  /* let me explain capstr: SASL needs the capability string (not bits).
   * we have 3 options:
   *   1. rerun CAPABILITY inside SASL function.
   *   2. build appropriate CAPABILITY string by reverse-engineering from bits.
   *   3. keep a copy until after authentication.
   * I've chosen (3) for now. (2) might not be too bad, but it involves
   * tracking all possible capabilities. bah. (1) I don't like because
   * it's just no fun to get the same information twice */
  char *capstr;              ///< Capability string from the server
  ImapCapFlags capabilities; ///< Capability flags
  unsigned char seqid;       ///< tag sequence prefix
  unsigned int seqno;        ///< tag sequence number, e.g. '{seqid}0001'
  time_t lastread;           ///< last time we read a command for the server
  char *buf;
  size_t blen;

  bool unicode; ///< If true, we can send UTF-8, and the server will use UTF8 rather than mUTF7
  bool qresync; ///< true, if QRESYNC is successfully ENABLE'd

  // if set, the response parser will store results for complicated commands here
  struct ImapList *cmdresult;

  /* command queue */
  struct ImapCommand *cmds; ///< Queue of commands for the server
  int cmdslots;             ///< Size of the command queue
  int nextcmd;              ///< Next command to be sent
  int lastcmd;              ///< Last command in the queue
  struct Buffer cmdbuf;

  char delim;                   ///< Path delimiter
  struct Mailbox *mailbox;      ///< Current selected mailbox
  struct Mailbox *prev_mailbox; ///< Previously selected mailbox
  struct Account *account;      ///< Parent Account
};

void                    imap_adata_free(void **ptr);
struct ImapAccountData *imap_adata_get (struct Mailbox *m);
struct ImapAccountData *imap_adata_new (struct Account *a);

#endif /* MUTT_IMAP_ADATA_H */
