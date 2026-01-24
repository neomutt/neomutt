/**
 * @file
 * Execute user-defined Hooks
 *
 * @authors
 * Copyright (C) 2018-2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_HOOKS_EXEC_H
#define MUTT_HOOKS_EXEC_H

#include "config.h"
#include "core/lib.h"

struct Address;
struct Buffer;
struct Email;
struct ListHead;

void                  exec_account_hook            (const char *url);
void                  exec_folder_hook             (const char *path, const char *desc);
void                  exec_message_hook            (struct Mailbox *m, struct Email *e, enum CommandId id);
void                  exec_startup_shutdown_hook   (enum CommandId id);
void                  exec_timeout_hook            (void);

void                  mutt_crypt_hook              (struct ListHead *list, struct Address *addr);
void                  mutt_default_save            (struct Buffer *path, struct Email *e);
void                  mutt_delete_hooks            (enum CommandId id);
char *                mutt_find_hook               (enum CommandId id, const char *pat);
const struct Expando *mutt_idxfmt_hook             (const char *name, struct Mailbox *m, struct Email *e);
void                  mutt_select_fcc              (struct Buffer *path, struct Email *e);

#endif /* MUTT_HOOKS_EXEC_H */
