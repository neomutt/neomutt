/**
 * @file
 * Email Aliases
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_alias Alias
 *
 * Email Aliases (Address Book)
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | alias/alias.c       | @subpage alias_alias       |
 * | alias/array.c       | @subpage alias_array       |
 * | alias/commands.c    | @subpage alias_commands    |
 * | alias/complete.c    | @subpage alias_complete    |
 * | alias/config.c      | @subpage alias_config      |
 * | alias/dlg_alias.c   | @subpage alias_dlg_alias   |
 * | alias/dlg_query.c   | @subpage alias_dlg_query   |
 * | alias/expando.c     | @subpage alias_expando     |
 * | alias/functions.c   | @subpage alias_functions   |
 * | alias/gui.c         | @subpage alias_gui         |
 * | alias/module.c      | @subpage alias_module      |
 * | alias/reverse.c     | @subpage alias_reverse     |
 * | alias/sort.c        | @subpage alias_sort        |
 */

#ifndef MUTT_ALIAS_LIB_H
#define MUTT_ALIAS_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include "core/lib.h"
#include "expando.h" // IWYU pragma: keep

struct Address;
struct AddressList;
struct Alias;
struct Buffer;
struct ConfigSubset;
struct Envelope;
struct TagList;

extern const struct CompleteOps CompleteAliasOps;

void                alias_create           (struct AddressList *al, const struct ConfigSubset *sub);
struct AddressList *alias_lookup           (const char *name);

bool                mutt_addr_is_user      (const struct Address *addr);
void                mutt_expand_aliases_env(struct Envelope *env);
void                mutt_expand_aliases    (struct AddressList *al);
struct AddressList *mutt_get_address       (struct Envelope *env, const char **prefix);

enum CommandResult parse_alias  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unalias(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

void alias_tags_to_buffer(struct TagList *tl, struct Buffer *buf);
void parse_alias_comments(struct Alias *alias, const char *com);
void parse_alias_tags    (const char *tags, struct TagList *tl);

int  alias_complete(struct Buffer *buf, struct ConfigSubset *sub);
void alias_dialog  (struct Mailbox *m, struct ConfigSubset *sub);

int  query_complete(struct Buffer *buf, struct ConfigSubset *sub);
void query_index   (struct Mailbox *m, struct ConfigSubset *sub);

struct Address *alias_reverse_lookup(const struct Address *addr);

#endif /* MUTT_ALIAS_LIB_H */
