/**
 * @file
 * NeoMutt commands API
 *
 * @authors
 * Copyright (C) 2021-2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_COMMAND_H
#define MUTT_CORE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"

struct ParseContext;
struct ParseError;

/**
 * enum CommandResult - Error codes for command_t parse functions
 */
enum CommandResult
{
  MUTT_CMD_ERROR   = -1, ///< Error: Can't help the user
  MUTT_CMD_WARNING = -2, ///< Warning: Help given to the user
  MUTT_CMD_SUCCESS =  0, ///< Success: Command worked
  MUTT_CMD_FINISH  =  1  ///< Finish: Stop processing this file
};

/**
 * typedef CommandFlags - Special characters that end a text string
 */
typedef uint8_t CommandFlags;        ///< Flags, e.g. #CF_NO_FLAGS
#define CF_NO_FLAGS               0  ///< No flags are set
#define CF_SYNONYM         (1 <<  0) ///< Command is a synonym for another command
#define CF_DEPRECATED      (1 <<  1) ///< Command is deprecated

/**
 * enum CommandId - ID of Command
 *
 * All the Commands in NeoMutt.
 */
enum CommandId
{
  CMD_NONE = 0,              ///< No Command
  CMD_ACCOUNT_HOOK,          ///< `:account-hook`
  CMD_ALIAS,                 ///< `:alias`               @sa #CMD_UNALIAS
  CMD_ALTERNATES,            ///< `:alternates`          @sa #CMD_UNALTERNATES
  CMD_ALTERNATIVE_ORDER,     ///< `:alternative-order`   @sa #CMD_UNALTERNATIVE_ORDER, #EmailModuleData.alternative_order
  CMD_APPEND_HOOK,           ///< `:append-hook`         @sa #CMD_CLOSE_HOOK, #CMD_OPEN_HOOK
  CMD_ATTACHMENTS,           ///< `:attachments`         @sa #CMD_UNATTACHMENTS
  CMD_AUTO_VIEW,             ///< `:auto-view`           @sa #CMD_UNAUTO_VIEW, #EmailModuleData.auto_view
  CMD_BIND,                  ///< `:bind`                @sa #CMD_UNBIND
  CMD_CD,                    ///< `:cd`
  CMD_CHARSET_HOOK,          ///< `:charset-hook`
  CMD_CLOSE_HOOK,            ///< `:close-hook`          @sa #CMD_APPEND_HOOK, #CMD_OPEN_HOOK
  CMD_COLOR,                 ///< `:color`               @sa #CMD_UNCOLOR
  CMD_CRYPT_HOOK,            ///< `:crypt-hook`
  CMD_ECHO,                  ///< `:echo`
  CMD_EXEC,                  ///< `:exec`
  CMD_FCC_HOOK,              ///< `:fcc-hook`
  CMD_FCC_SAVE_HOOK,         ///< `:fcc-save-hook`
  CMD_FINISH,                ///< `:finish`              @sa #CMD_IFDEF, #CMD_IFNDEF
  CMD_FOLDER_HOOK,           ///< `:folder-hook`
  CMD_GROUP,                 ///< `:group`               @sa #CMD_UNGROUP
  CMD_HEADER_ORDER,          ///< `:header-order`        @sa #CMD_UNHEADER_ORDER, #EmailModuleData.header_order
  CMD_HOOKS,                 ///< `:hooks`
  CMD_ICONV_HOOK,            ///< `:iconv-hook`
  CMD_IFDEF,                 ///< `:ifdef`               @sa #CMD_FINISH, #CMD_IFNDEF
  CMD_IFNDEF,                ///< `:ifndef`              @sa #CMD_FINISH, #CMD_IFDEF
  CMD_IGNORE,                ///< `:ignore`              @sa #CMD_UNIGNORE
  CMD_INDEX_FORMAT_HOOK,     ///< `:index-format-hook`
  CMD_LISTS,                 ///< `:lists`               @sa #CMD_UNLISTS
  CMD_LUA,                   ///< `:lua`
  CMD_LUA_SOURCE,            ///< `:lua-source`
  CMD_MACRO,                 ///< `:macro`               @sa #CMD_UNMACRO
  CMD_MAILBOXES,             ///< `:mailboxes`           @sa #CMD_NAMED_MAILBOXES, #CMD_UNMAILBOXES
  CMD_MAILTO_ALLOW,          ///< `:mailto-allow`        @sa #CMD_UNMAILTO_ALLOW, #EmailModuleData.mail_to_allow
  CMD_MBOX_HOOK,             ///< `:mbox-hook`
  CMD_MESSAGE_HOOK,          ///< `:message-hook`
  CMD_MIME_LOOKUP,           ///< `:mime-lookup`         @sa #CMD_UNMIME_LOOKUP, #MimeLookupList
  CMD_MONO,                  ///< `:mono`                @sa #CMD_UNMONO
  CMD_MY_HEADER,             ///< `:my-header`           @sa #CMD_UNMY_HEADER
  CMD_NAMED_MAILBOXES,       ///< `:named-mailboxes`     @sa #CMD_MAILBOXES, #CMD_UNMAILBOXES
  CMD_NOSPAM,                ///< `:nospam`              @sa #CMD_SPAM
  CMD_OPEN_HOOK,             ///< `:open-hook`           @sa #CMD_APPEND_HOOK, #CMD_CLOSE_HOOK
  CMD_PUSH,                  ///< `:push`
  CMD_REPLY_HOOK,            ///< `:reply-hook`
  CMD_RESET,                 ///< `:reset`               @sa #CMD_SET, #CMD_TOGGLE, #CMD_UNSET
  CMD_SAVE_HOOK,             ///< `:save-hook`
  CMD_SCORE,                 ///< `:score`               @sa #CMD_UNSCORE
  CMD_SEND2_HOOK,            ///< `:send2-hook`
  CMD_SEND_HOOK,             ///< `:send-hook`
  CMD_SET,                   ///< `:set`                 @sa #CMD_RESET, #CMD_TOGGLE, #CMD_UNSET
  CMD_SETENV,                ///< `:setenv`              @sa #CMD_UNSETENV
  CMD_SHUTDOWN_HOOK,         ///< `:shutdown-hook`
  CMD_SIDEBAR_PIN,           ///< `:sidebar-pin`         @sa #CMD_SIDEBAR_UNPIN
  CMD_SIDEBAR_UNPIN,         ///< `:sidebar-unpin`       @sa #CMD_SIDEBAR_PIN
  CMD_SOURCE,                ///< `:source`
  CMD_SPAM,                  ///< `:spam`                @sa #CMD_NOSPAM
  CMD_STARTUP_HOOK,          ///< `:startup-hook`
  CMD_SUBJECT_REGEX,         ///< `:subject-regex`       @sa #CMD_UNSUBJECT_REGEX
  CMD_SUBSCRIBE,             ///< `:subscribe`           @sa #CMD_UNSUBSCRIBE
  CMD_SUBSCRIBE_TO,          ///< `:subscribe-to`        @sa #CMD_UNSUBSCRIBE_FROM
  CMD_TAG_FORMATS,           ///< `:tag-formats`         @sa #CMD_TAG_TRANSFORMS
  CMD_TAG_TRANSFORMS,        ///< `:tag-transforms`      @sa #CMD_TAG_FORMATS
  CMD_TIMEOUT_HOOK,          ///< `:timeout-hook`
  CMD_TOGGLE,                ///< `:toggle`              @sa #CMD_RESET, #CMD_SET, #CMD_UNSET
  CMD_UNALIAS,               ///< `:unalias`             @sa #CMD_ALIAS
  CMD_UNALTERNATES,          ///< `:unalternates`        @sa #CMD_ALTERNATES
  CMD_UNALTERNATIVE_ORDER,   ///< `:unalternative-order` @sa #CMD_ALTERNATIVE_ORDER, #EmailModuleData.alternative_order
  CMD_UNATTACHMENTS,         ///< `:unattachments`       @sa #CMD_ATTACHMENTS
  CMD_UNAUTO_VIEW,           ///< `:unauto-view`         @sa #CMD_AUTO_VIEW, #EmailModuleData.auto_view
  CMD_UNBIND,                ///< `:unbind`              @sa #CMD_BIND
  CMD_UNCOLOR,               ///< `:uncolor`             @sa #CMD_COLOR
  CMD_UNGROUP,               ///< `:ungroup`             @sa #CMD_GROUP
  CMD_UNHEADER_ORDER,        ///< `:unheader-order`      @sa #CMD_HEADER_ORDER, #EmailModuleData.header_order
  CMD_UNHOOK,                ///< `:unhook`
  CMD_UNIGNORE,              ///< `:unignore`            @sa #CMD_IGNORE
  CMD_UNLISTS,               ///< `:unlists`             @sa #CMD_LISTS
  CMD_UNMACRO,               ///< `:unmacro`             @sa #CMD_MACRO
  CMD_UNMAILBOXES,           ///< `:unmailboxes`         @sa #CMD_MAILBOXES, #CMD_NAMED_MAILBOXES
  CMD_UNMAILTO_ALLOW,        ///< `:unmailto-allow`      @sa #CMD_MAILTO_ALLOW, #EmailModuleData.mail_to_allow
  CMD_UNMIME_LOOKUP,         ///< `:unmime-lookup`       @sa #CMD_MIME_LOOKUP, #MimeLookupList
  CMD_UNMONO,                ///< `:unmono`              @sa #CMD_MONO
  CMD_UNMY_HEADER,           ///< `:unmy-header`         @sa #CMD_MY_HEADER
  CMD_UNSCORE,               ///< `:unscore`             @sa #CMD_SCORE
  CMD_UNSET,                 ///< `:unset`               @sa #CMD_RESET, #CMD_SET, #CMD_TOGGLE
  CMD_UNSETENV,              ///< `:unsetenv`            @sa #CMD_SETENV
  CMD_UNSUBJECT_REGEX,       ///< `:unsubject-regex`     @sa #CMD_SUBJECT_REGEX
  CMD_UNSUBSCRIBE,           ///< `:unsubscribe`         @sa #CMD_SUBSCRIBE
  CMD_UNSUBSCRIBE_FROM,      ///< `:unsubscribe-from`    @sa #CMD_SUBSCRIBE_TO
  CMD_VERSION,               ///< `:version`
};

#define CMD_NO_DATA  0    ///< Convenience symbol

/**
 * @defgroup command_api Command API
 *
 * Observers of #NT_COMMAND will be passed a #Command.
 *
 * A user-callable command
 */
struct Command
{
  const char     *name;    ///< Name of the Command
  enum CommandId  id;      ///< ID of the Command

  /**
   * @defgroup command_parse parse()
   * @ingroup command_api
   *
   * parse - Function to parse a command
   * @param cmd  Command being parsed
   * @param line Buffer containing string to be parsed
   * @param pc   Parse Context
   * @param pe   Parse Errors
   * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
   *
   * @pre cmd  is not NULL
   * @pre line is not NULL
   * @pre pc   is not NULL
   * @pre pe   is not NULL
   */
  enum CommandResult (*parse)(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);

  intptr_t data;        ///< Data or flags to pass to the command

  const char *help;     ///< One-line description of the Command
  const char *proto;    ///< Command prototype
  const char *path;     ///< Help path, relative to the NeoMutt Docs

  CommandFlags flags;   ///< Command flags, e.g. #CF_SYNONYM
};
ARRAY_HEAD(CommandArray, const struct Command *);

const struct Command *commands_get     (struct CommandArray *ca, const char *name);
void                  commands_clear   (struct CommandArray *ca);
bool                  commands_register(struct CommandArray *ca, const struct Command *cmds);

#endif /* MUTT_CORE_COMMAND_H */
