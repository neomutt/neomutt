/**
 * @file
 * Email Aliases
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

/**
 * @page alias ALIAS: Email Aliases
 *
 * Email Aliases
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | alias/alias.c       | @subpage alias_alias       |
 * | alias/commands.c    | @subpage alias_commands    |
 * | alias/config.c      | @subpage alias_config      |
 * | alias/dlgalias.c    | @subpage alias_dlgalias    |
 * | alias/dlgquery.c    | @subpage alias_dlgquery    |
 * | alias/gui.c         | @subpage alias_gui         |
 * | alias/reverse.c     | @subpage alias_reverse     |
 */

#ifndef MUTT_ALIAS_LIB_H
#define MUTT_ALIAS_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt_commands.h"

struct Address;
struct AddressList;
struct Buffer;
struct ConfigSet;
struct Envelope;

extern char *C_AliasFile;

/* These Config Variables are only used in dlgalias.c */
extern char *C_AliasFormat;
extern short C_SortAlias;

/* These Config Variables are only used in dlgquery.c */
extern char *C_QueryCommand;
extern char *C_QueryFormat;

extern struct HashTable *ReverseAliases;

void alias_init    (void);
void alias_shutdown(void);

void                alias_create           (struct AddressList *al);
struct AddressList *alias_lookup           (const char *name);

bool                mutt_addr_is_user      (const struct Address *addr);
void                mutt_expand_aliases_env(struct Envelope *env);
void                mutt_expand_aliases    (struct AddressList *al);
struct AddressList *mutt_get_address       (struct Envelope *env, const char **prefix);

enum CommandResult parse_alias  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unalias(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

int  alias_complete(char *buf, size_t buflen);

int  query_complete(char *buf, size_t buflen);
void query_index   (void);

struct Address *alias_reverse_lookup(const struct Address *addr);

bool config_init_alias(struct ConfigSet *cs);

#endif /* MUTT_ALIAS_LIB_H */
