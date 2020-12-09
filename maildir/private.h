/**
 * @file
 * Maildir/MH private types
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_PRIVATE_H
#define MUTT_MAILDIR_PRIVATE_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include "sequence.h"

struct Account;
struct Buffer;
struct Email;
struct MdEmailArray;
struct Mailbox;
struct MdEmail;
struct Message;
struct Progress;

extern bool  C_CheckNew;
extern bool  C_MaildirCheckCur;
extern bool  C_MaildirHeaderCacheVerify;
extern bool  C_MhPurge;
extern char *C_MhSeqFlagged;
extern char *C_MhSeqReplied;
extern char *C_MhSeqUnseen;

int    maildir_move_to_mailbox(struct Mailbox *m, struct MdEmailArray *mda);
bool   mh_mkstemp             (struct Mailbox *m, FILE **fp, char **tgt);
mode_t mh_umask               (struct Mailbox *m);

#endif /* MUTT_MAILDIR_PRIVATE_H */
