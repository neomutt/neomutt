/**
 * @file
 * User-defined Hooks
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

#ifndef MUTT_HOOKS_HOOK_H
#define MUTT_HOOKS_HOOK_H

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"

/**
 * struct Hook - A list of user hooks
 */
struct Hook
{
  enum CommandId     id;             ///< Hook CommandId, e.g. #CMD_FOLDER_HOOK
  struct Regex       regex;          ///< Regular expression
  char               *command;       ///< Filename, command or pattern to execute
  char               *source_file;   ///< Used for relative-directory source
  struct PatternList *pattern;       ///< Used for fcc,save,send-hook
  struct Expando     *expando;       ///< Used for format hooks
  TAILQ_ENTRY(Hook)   entries;       ///< Linked list
};
TAILQ_HEAD(HookList, Hook);

void         hook_free(struct Hook **ptr);
struct Hook *hook_new (void);

#endif /* MUTT_HOOKS_HOOK_H */
