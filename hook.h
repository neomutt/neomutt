/**
 * @file
 * Parse and execute user-defined hooks
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

#ifndef MUTT_HOOK_H
#define MUTT_HOOK_H

#include <stdbool.h>
#include <stdio.h>
#include "mutt_commands.h"

struct Address;
struct Buffer;
struct Email;
struct ListHead;
struct Mailbox;

/* These Config Variables are only used in hook.c */
extern char *DefaultHook;
extern bool  ForceName;
extern bool  SaveName;

/* types for mutt_parse_hook() */
#define MUTT_FOLDER_HOOK   (1 << 0)  ///< folder-hook: when entering a mailbox
#define MUTT_MBOX_HOOK     (1 << 1)  ///< mbox-hook: move messages after reading them
#define MUTT_SEND_HOOK     (1 << 2)  ///< send-hook: when composing a new email
#define MUTT_FCC_HOOK      (1 << 3)  ///< fcc-hook: to save outgoing email
#define MUTT_SAVE_HOOK     (1 << 4)  ///< save-hook: set a default folder when saving an email
#define MUTT_CHARSET_HOOK  (1 << 5)  ///< charset-hook: create a charset alias for malformed emails
#define MUTT_ICONV_HOOK    (1 << 6)  ///< iconv-hook: create a system charset alias
#define MUTT_MESSAGE_HOOK  (1 << 7)  ///< message-hook: run before displaying a message
#define MUTT_CRYPT_HOOK    (1 << 8)  ///< crypt-hook: automatically select a PGP/SMIME key
#define MUTT_ACCOUNT_HOOK  (1 << 9)  ///< account-hook: when changing between accounts
#define MUTT_REPLY_HOOK    (1 << 10) ///< reply-hook: when replying to an email
#define MUTT_SEND2_HOOK    (1 << 11) ///< send2-hook: when changing fields in the compose menu
#ifdef USE_COMPRESSED
#define MUTT_OPEN_HOOK     (1 << 12) ///< open-hook: to read a compressed mailbox
#define MUTT_APPEND_HOOK   (1 << 13) ///< append-hook: append to a compressed mailbox
#define MUTT_CLOSE_HOOK    (1 << 14) ///< close-hook: write to a compressed mailbox
#endif
#define MUTT_TIMEOUT_HOOK  (1 << 15) ///< timeout-hook: run a command periodically
#define MUTT_STARTUP_HOOK  (1 << 16) ///< startup-hook: run when starting NeoMutt
#define MUTT_SHUTDOWN_HOOK (1 << 17) ///< shutdown-hook: run when leaving NeoMutt
#define MUTT_GLOBAL_HOOK   (1 << 18) ///< Hooks which don't take a regex

void  mutt_account_hook(const char *url);
void  mutt_crypt_hook(struct ListHead *list, struct Address *addr);
void  mutt_default_save(char *path, size_t pathlen, struct Email *e);
void  mutt_delete_hooks(int type);
char *mutt_find_hook(int type, const char *pat);
void  mutt_folder_hook(const char *path, const char *desc);
void  mutt_message_hook(struct Mailbox *m, struct Email *e, int type);
enum CommandResult mutt_parse_hook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
enum CommandResult mutt_parse_unhook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
void  mutt_select_fcc(char *path, size_t pathlen, struct Email *e);
void  mutt_startup_shutdown_hook(int type);
void  mutt_timeout_hook(void);

#endif /* MUTT_HOOK_H */
