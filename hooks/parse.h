/**
 * @file
 * Parse user-defined Hooks
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

#ifndef MUTT_HOOKS_PARSE_H
#define MUTT_HOOKS_PARSE_H

#include "config.h"
#include "core/lib.h"

struct Buffer;
struct ParseContext;
struct ParseError;

extern struct HookList Hooks;
extern struct HashTable *IdxFmtHooks;
extern enum CommandId CurrentHookId;

enum CommandResult parse_charset_hook (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_compress_hook(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_crypt_hook   (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_folder_hook  (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_global_hook  (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_index_hook   (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_mailbox_hook (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_mbox_hook    (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_pattern_hook (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_regex_hook   (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_unhook       (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);

#endif /* MUTT_HOOKS_PARSE_H */
