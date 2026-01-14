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
#include <stddef.h>
#include "core/lib.h"

struct Buffer;
struct Hook;

/**
 * struct HookParseError - Error information from parsing a hook command
 */
struct HookParseError
{
  const char *message;  ///< Error message (may be NULL if no error)
  size_t position;      ///< Position in the original line where the error occurred
};

/**
 * struct FolderHookData - Parsed data from a folder-hook command line
 *
 * This structure holds the parsed components of a folder-hook command.
 * The caller is responsible for freeing the allocated strings.
 */
struct FolderHookData
{
  char *regex;    ///< The regex pattern (caller must free)
  char *command;  ///< The command to execute (caller must free)
  bool pat_not;   ///< true if the pattern is negated (starts with '!')
  bool use_regex; ///< true if regex mode is enabled (false if -noregex was used)
};

void folder_hook_data_free(struct FolderHookData *data);

extern struct HookList Hooks;
extern struct HashTable *IdxFmtHooks;
extern enum CommandId CurrentHookId;

enum CommandResult parse_hook_charset (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_compress(const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_crypt   (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_folder  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_global  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_index   (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_mailbox (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_mbox    (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_pattern (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_hook_regex   (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unhook       (const struct Command *cmd, struct Buffer *line, struct Buffer *err);

bool parse_folder_hook_line(const char *line, struct FolderHookData *data,
                            struct HookParseError *error);

#endif /* MUTT_HOOKS_PARSE_H */
