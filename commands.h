/**
 * @file
 * Functions to parse commands in a config file
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMMANDS_H
#define MUTT_COMMANDS_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "config/lib.h"
#include "core/lib.h"

struct Buffer;
struct GroupList;

/* parameter to parse_mailboxes */
#define MUTT_NAMED   (1 << 0)

enum CommandResult parse_mailboxes       (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_my_hdr          (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_subjectrx_list  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_subscribe_to    (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unalternates    (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unmailboxes     (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unsubjectrx_list(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unsubscribe_from(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

enum CommandResult parse_rc_line_cwd(const char *line, char *cwd, struct Buffer *err);
char *mutt_get_sourced_cwd(void);
bool mailbox_add_simple(const char *mailbox, struct Buffer *err);

int parse_grouplist(struct GroupList *gl, struct Buffer *buf, struct Buffer *s, struct Buffer *err);
void source_stack_cleanup(void);
int source_rc(const char *rcfile_path, struct Buffer *err);

enum CommandResult set_dump(ConfigDumpFlags flags, struct Buffer *err);

#endif /* MUTT_COMMANDS_H */
