/**
 * @file
 * Setup NeoMutt Commands
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
 * @page commands_commands Setup NeoMutt Commands
 *
 * Setup NeoMutt Commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "color/lib.h"
#include "parse/lib.h"
#include "ifdef.h"
#include "mailboxes.h"
#include "parse.h"
#include "setenv.h"
#include "source.h"
#include "tags.h"

/**
 * CommandsCommands - General NeoMutt Commands
 */
const struct Command CommandsCommands[] = {
  // clang-format off
  { "cd", CMD_CD, parse_cd,
        N_("Change NeoMutt's current working directory"),
        N_("cd [ <directory> ]"),
        "configuration.html#cd" },
  { "color", CMD_COLOR, parse_color,
        N_("Define colors for the user interface"),
        N_("color <object> [ <attribute> ... ] <fg> <bg> [ <regex> [ <num> ]]"),
        "configuration.html#color" },
  { "echo", CMD_ECHO, parse_echo,
        N_("Print a message to the status line"),
        N_("echo <message>"),
        "advancedusage.html#echo" },
  { "finish", CMD_FINISH, parse_finish,
        N_("Stop reading current config file"),
        N_("finish"),
        "optionalfeatures.html#ifdef" },
  { "ifdef", CMD_IFDEF, parse_ifdef,
        N_("Conditionally include config commands if symbol defined"),
        N_("ifdef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "ifndef", CMD_IFNDEF, parse_ifdef,
        N_("Conditionally include if symbol is not defined"),
        N_("ifndef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "mailboxes", CMD_MAILBOXES, parse_mailboxes,
        N_("Define a list of mailboxes to watch"),
        N_("mailboxes [ -label <label> ] [ -notify ] [ -poll ] <mailbox> [ ... ]"),
        "configuration.html#mailboxes" },
  { "mono", CMD_MONO, parse_mono,
        N_("Deprecated: Use `color` instead"),
        N_("mono <object> <attribute> [ <pattern> | <regex> ]"),
        "configuration.html#color-mono" },
  { "named-mailboxes", CMD_NAMED_MAILBOXES, parse_mailboxes,
        N_("Define a list of labelled mailboxes to watch"),
        N_("named-mailboxes [ -notify ] [ -poll ] <label> <mailbox> [ ... ]"),
        "configuration.html#mailboxes" },
  { "reset", CMD_RESET, parse_set,
        N_("Reset a config option to its initial value"),
        N_("reset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "set", CMD_SET, parse_set,
        N_("Set a config variable"),
        N_("set [ no | inv | & ] <variable> [?] | <variable> [=|+=|-=] <value> [...]"),
        "configuration.html#set" },
  { "setenv", CMD_SETENV, parse_setenv,
        N_("Set an environment variable"),
        N_("setenv { <variable>? | <variable>=<value> }"),
        "advancedusage.html#setenv" },
  { "source", CMD_SOURCE, parse_source,
        N_("Read and execute commands from a config file"),
        N_("source <filename> [ <filename> ... ]"),
        "configuration.html#source" },
  { "subscribe", CMD_SUBSCRIBE, parse_subscribe,
        N_("Add address to the list of subscribed mailing lists"),
        N_("subscribe [ -group <name> ... ] <regex> [ ... ]"),
        "configuration.html#lists" },
  { "tag-formats", CMD_TAG_FORMATS, parse_tag_formats,
        N_("Define expandos tags"),
        N_("tag-formats <tag> <format-string> [ ... ] }"),
        "optionalfeatures.html#custom-tags" },
  { "tag-transforms", CMD_TAG_TRANSFORMS, parse_tag_transforms,
        N_("Rules to transform tags into icons"),
        N_("tag-transforms <tag> <transformed-string> [ ... ]"),
        "optionalfeatures.html#custom-tags" },
  { "toggle", CMD_TOGGLE, parse_set,
        N_("Toggle the value of a boolean/quad config option"),
        N_("toggle <variable> [ ... ]"),
        "configuration.html#set" },
  { "uncolor", CMD_UNCOLOR, parse_uncolor,
        N_("Remove a `color` definition"),
        N_("uncolor <object> { * | <pattern> ... }"),
        "configuration.html#color" },
  { "unmailboxes", CMD_UNMAILBOXES, parse_unmailboxes,
        N_("Remove mailboxes from the watch list"),
        N_("unmailboxes { * | <mailbox> ... }"),
        "configuration.html#mailboxes" },
  { "unmono", CMD_UNMONO, parse_unmono,
        N_("Deprecated: Use `uncolor` instead"),
        N_("unmono <object> { * | <pattern> ... }"),
        "configuration.html#color-mono" },
  { "unset", CMD_UNSET, parse_set,
        N_("Reset a config option to false/empty"),
        N_("unset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unsetenv", CMD_UNSETENV, parse_setenv,
        N_("Unset an environment variable"),
        N_("unsetenv <variable>"),
        "advancedusage.html#setenv" },
  { "unsubscribe", CMD_UNSUBSCRIBE, parse_unsubscribe,
        N_("Remove address from the list of subscribed mailing lists"),
        N_("unsubscribe { * | <regex> ... }"),
        "configuration.html#lists" },
  { "version", CMD_VERSION, parse_version,
        N_("Show NeoMutt version and build information"),
        N_("version"),
        "advancedusage.html#version" },

  { NULL, CMD_NONE, NULL, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * command_find_by_id - Find a NeoMutt Command by its CommandId
 * @param ca Array of Commands
 * @param id ID to find
 * @retval ptr Matching Command
 */
const struct Command *command_find_by_id(const struct CommandArray *ca, enum CommandId id)
{
  if (!ca || (id == CMD_NONE))
    return NULL;

  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, ca)
  {
    const struct Command *cmd = *cp;

    if (cmd->id == id)
      return cmd;
  }

  return NULL;
}

/**
 * command_find_by_name - Find a NeoMutt Command by its name
 * @param ca   Array of Commands
 * @param name Name to find
 * @retval ptr Matching Command
 *
 * @note If the name matches a Command Synonym, the real Command is returned.
 */
const struct Command *command_find_by_name(const struct CommandArray *ca, const char *name)
{
  if (!ca || !name)
    return NULL;

  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, ca)
  {
    const struct Command *cmd = *cp;

    if (mutt_str_equal(cmd->name, name))
    {
      // If this is a synonym, look up the real Command
      if ((cmd->flags & CF_SYNONYM) && cmd->help)
      {
        const char *real_name = cmd->help;
        // Search for the real command (non-recursive, single pass)
        const struct Command **rp = NULL;
        ARRAY_FOREACH(rp, ca)
        {
          const struct Command *real_cmd = *rp;
          if (mutt_str_equal(real_cmd->name, real_name) && !(real_cmd->flags & CF_SYNONYM))
            return real_cmd;
        }
        return NULL; // Real command not found
      }
      return cmd;
    }
  }

  return NULL;
}
