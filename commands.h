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
#include "config/lib.h"
#include "core/lib.h"

struct Buffer;
struct GroupList;
struct HashTable;

/* parameter to parse_mailboxes */
#define MUTT_NAMED   (1 << 0)

enum CommandResult parse_cd              (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_echo            (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_finish          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_group           (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_ifdef           (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_ignore          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_lists           (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_mailboxes       (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_my_hdr          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_nospam          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_setenv          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_source          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_spam            (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_stailq          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_subjectrx_list  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_subscribe       (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_subscribe_to    (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_tag_formats     (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_tag_transforms  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unalternates    (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unignore        (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unlists         (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unmailboxes     (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unmy_hdr        (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unstailq        (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unsubjectrx_list(const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unsubscribe     (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unsubscribe_from(const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_version         (const struct Command *cmd, struct Buffer *line, struct Buffer *err);

const struct Command *command_find_by_id  (const struct CommandArray *ca, enum CommandId id);
const struct Command *command_find_by_name(const struct CommandArray *ca, const char *name);

enum CommandResult parse_rc_line_cwd(const char *line, char *cwd, struct Buffer *err);
char *mutt_get_sourced_cwd(void);
bool mailbox_add_simple(const char *mailbox, struct Buffer *err);

int parse_grouplist(struct GroupList *gl, struct Buffer *buf, struct Buffer *s, struct Buffer *err, struct HashTable *groups);
void source_stack_cleanup(void);
int source_rc(const char *rcfile_path, struct Buffer *err);

enum CommandResult set_dump(enum GetElemListFlags flags, struct Buffer *err);

#endif /* MUTT_COMMANDS_H */
