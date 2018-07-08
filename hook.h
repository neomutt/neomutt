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

struct Address;
struct Buffer;
struct Context;
struct Header;
struct ListHead;

/* These Config Variables are only used in hook.c */
extern char *DefaultHook;
extern bool  ForceName;
extern bool  SaveName;

void  mutt_account_hook(const char *url);
void  mutt_crypt_hook(struct ListHead *list, struct Address *addr);
void  mutt_default_save(char *path, size_t pathlen, struct Header *hdr);
void  mutt_delete_hooks(int type);
char *mutt_find_hook(int type, const char *pat);
void  mutt_folder_hook(const char *path);
void  mutt_message_hook(struct Context *ctx, struct Header *hdr, int type);
int   mutt_parse_hook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int   mutt_parse_unhook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
void  mutt_select_fcc(char *path, size_t pathlen, struct Header *hdr);
void  mutt_startup_shutdown_hook(int type);
void  mutt_timeout_hook(void);

#endif /* MUTT_HOOK_H */
