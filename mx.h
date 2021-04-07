/**
 * @file
 * API for mailboxes
 *
 * @authors
 * Copyright (C) 1996-2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MX_H
#define MUTT_MX_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "config/lib.h"
#include "core/lib.h"

struct Email;

extern const struct MxOps *mx_ops[];

extern struct EnumDef MboxTypeDef;

typedef uint8_t MsgOpenFlags;      ///< Flags for mx_msg_open_new(), e.g. #MUTT_ADD_FROM
#define MUTT_MSG_NO_FLAGS       0  ///< No flags are set
#define MUTT_ADD_FROM     (1 << 0) ///< add a From_ line
#define MUTT_SET_DRAFT    (1 << 1) ///< set the message draft flag

/* Wrappers for the Mailbox API, see MxOps */
enum MxStatus   mx_mbox_check      (struct Mailbox *m);
enum MxStatus   mx_mbox_check_stats(struct Mailbox *m, uint8_t flags);
enum MxStatus   mx_mbox_close      (struct Mailbox **ptr);
bool            mx_mbox_open       (struct Mailbox *m, OpenMailboxFlags flags);
enum MxStatus   mx_mbox_sync       (struct Mailbox *m);
int             mx_msg_close       (struct Mailbox *m, struct Message **msg);
int             mx_msg_commit      (struct Mailbox *m, struct Message *msg);
struct Message *mx_msg_open_new    (struct Mailbox *m, const struct Email *e, MsgOpenFlags flags);
struct Message *mx_msg_open        (struct Mailbox *m, int msgno);
int             mx_msg_padding_size(struct Mailbox *m);
int             mx_save_hcache     (struct Mailbox *m, struct Email *e);
int             mx_path_canon      (char *buf, size_t buflen, const char *folder, enum MailboxType *type);
int             mx_path_canon2     (struct Mailbox *m, const char *folder);
int             mx_path_parent     (char *buf, size_t buflen);
int             mx_path_pretty     (char *buf, size_t buflen, const char *folder);
enum MailboxType mx_path_probe     (const char *path);
struct Mailbox *mx_path_resolve    (const char *path);
struct Mailbox *mx_resolve         (const char *path_or_name);
int             mx_tags_commit     (struct Mailbox *m, struct Email *e, char *tags);
int             mx_tags_edit       (struct Mailbox *m, const char *tags, char *buf, size_t buflen);
enum MailboxType mx_type           (struct Mailbox *m);

struct Account *mx_ac_find     (struct Mailbox *m);
struct Mailbox *mx_mbox_find   (struct Account *a, const char *path);
struct Mailbox *mx_mbox_find2  (const char *path);
bool            mx_mbox_ac_link(struct Mailbox *m);
bool            mx_ac_add      (struct Account *a, struct Mailbox *m);
int             mx_ac_remove   (struct Mailbox *m);

int                 mx_access           (const char *path, int flags);
void                mx_alloc_memory     (struct Mailbox *m);
int                 mx_path_is_empty    (const char *path);
void                mx_fastclose_mailbox(struct Mailbox **ptr);
const struct MxOps *mx_get_ops          (enum MailboxType type);
bool                mx_tags_is_supported(struct Mailbox *m);

#endif /* MUTT_MX_H */
