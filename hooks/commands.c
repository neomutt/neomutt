/**
 * @file
 * Hook Commands
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page hooks_commands Hook Commands
 *
 * Hook Commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "commands.h"
#include "dump.h"
#include "parse.h"

/**
 * HooksCommands - Hook Commands
 */
const struct Command HooksCommands[] = {
  // clang-format off
  { "account-hook", CMD_ACCOUNT_HOOK, parse_hook_regex, CMD_NO_DATA,
        N_("Run a command when switching to a matching account"),
        N_("account-hook <regex> <command>"),
        "optionalfeatures.html#account-hook" },
  { "charset-hook", CMD_CHARSET_HOOK, parse_hook_charset, CMD_NO_DATA,
        N_("Define charset alias for languages"),
        N_("charset-hook <alias> <charset>"),
        "configuration.html#charset-hook" },
  { "crypt-hook", CMD_CRYPT_HOOK, parse_hook_crypt, CMD_NO_DATA,
        N_("Specify which keyid to use for recipients matching regex"),
        N_("crypt-hook <regex> <keyid>"),
        "configuration.html#crypt-hook" },
  { "fcc-hook", CMD_FCC_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Pattern rule to set the save location for outgoing mail"),
        N_("fcc-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "fcc-save-hook", CMD_FCC_SAVE_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Equivalent to both 'fcc-hook' and 'save-hook'"),
        N_("fcc-save-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "folder-hook", CMD_FOLDER_HOOK, parse_hook_folder, CMD_NO_DATA,
        N_("Run a command upon entering a folder matching regex"),
        N_("folder-hook [ -noregex ] <regex> <command>"),
        "configuration.html#folder-hook" },
  { "iconv-hook", CMD_ICONV_HOOK, parse_hook_charset, CMD_NO_DATA,
        N_("Define a system-specific alias for a character set"),
        N_("iconv-hook <charset> <local-charset>"),
        "configuration.html#charset-hook" },
  { "index-format-hook", CMD_INDEX_FORMAT_HOOK, parse_hook_index, CMD_NO_DATA,
        N_("Create dynamic index format strings"),
        N_("index-format-hook <name> [ ! ]<pattern> <format-string>"),
        "configuration.html#index-format-hook" },
  { "mbox-hook", CMD_MBOX_HOOK, parse_hook_mbox, CMD_NO_DATA,
        N_("On leaving a mailbox, move read messages matching a regex"),
        N_("mbox-hook [ -noregex ] <regex> <mailbox>"),
        "configuration.html#mbox-hook" },
  { "message-hook", CMD_MESSAGE_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when viewing a message matching patterns"),
        N_("message-hook <pattern> <command>"),
        "configuration.html#message-hook" },
  { "reply-hook", CMD_REPLY_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when replying to messages matching a pattern"),
        N_("reply-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "save-hook", CMD_SAVE_HOOK, parse_hook_mailbox, CMD_NO_DATA,
        N_("Set default save folder for messages"),
        N_("save-hook <pattern> <mailbox>"),
        "configuration.html#default-save-mailbox" },
  { "send-hook", CMD_SEND_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run a command when sending a message, new or reply, matching a pattern"),
        N_("send-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "send2-hook", CMD_SEND2_HOOK, parse_hook_pattern, CMD_NO_DATA,
        N_("Run command whenever a composed message is edited"),
        N_("send2-hook <pattern> <command>"),
        "configuration.html#send-hook" },
  { "shutdown-hook", CMD_SHUTDOWN_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command before NeoMutt exits"),
        N_("shutdown-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "startup-hook", CMD_STARTUP_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command when NeoMutt starts up"),
        N_("startup-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "timeout-hook", CMD_TIMEOUT_HOOK, parse_hook_global, CMD_NO_DATA,
        N_("Run a command after a specified timeout or idle period"),
        N_("timeout-hook <command>"),
        "optionalfeatures.html#global-hooks" },
  { "unhook", CMD_UNHOOK, parse_unhook, CMD_NO_DATA,
        N_("Remove hooks of a given type"),
        N_("unhook { * | <hook-type> }"),
        "configuration.html#unhook" },

  { "hooks", CMD_HOOKS, parse_hooks, CMD_NO_DATA,
        N_("Show a list of all the hooks"),
        N_("hooks"),
        "configuration.html#hooks" },

  // Deprecated
  { "pgp-hook", CMD_NONE, NULL, IP "crypt-hook", NULL, NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
