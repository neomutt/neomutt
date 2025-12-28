/**
 * @file
 * Setup NeoMutt Commands
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "attach/lib.h"
#include "color/lib.h"
#include "parse/lib.h"
#include "alternates.h"
#include "globals.h"
#include "group.h"
#include "ifdef.h"
#include "ignore.h"
#include "mailboxes.h"
#include "my_hdr.h"
#include "parse.h"
#include "score.h"
#include "setenv.h"
#include "source.h"
#include "spam.h"
#include "stailq.h"
#include "subjectrx.h"
#include "tags.h"

/**
 * MuttCommands - General NeoMutt Commands
 */
static const struct Command MuttCommands[] = {
  // clang-format off
  { "alias", CMD_ALIAS, parse_alias, CMD_NO_DATA,
        N_("Define an alias (name to email address)"),
        N_("alias [ -group <name> ... ] <key> <address> [, <address> ... ]"),
        "configuration.html#alias" },
  { "alternates", CMD_ALTERNATES, parse_alternates, CMD_NO_DATA,
        N_("Define a list of alternate email addresses for the user"),
        N_("alternates [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#alternates" },
  { "alternative_order", CMD_ALTERNATIVE_ORDER, parse_stailq, IP &AlternativeOrderList,
        N_("Set preference order for multipart alternatives"),
        N_("alternative_order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#alternative-order" },
  { "attachments", CMD_ATTACHMENTS, parse_attachments, CMD_NO_DATA,
        N_("Set attachment counting rules"),
        N_("attachments { + | - }<disposition> <mime-type> [ <mime-type> ... ] | ?"),
        "mimesupport.html#attachments" },
  { "auto_view", CMD_AUTO_VIEW, parse_stailq, IP &AutoViewList,
        N_("Automatically display specified MIME types inline"),
        N_("auto_view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#auto-view" },
  { "cd", CMD_CD, parse_cd, CMD_NO_DATA,
        N_("Change NeoMutt's current working directory"),
        N_("cd [ <directory> ]"),
        "configuration.html#cd" },
  { "color", CMD_COLOR, parse_color, CMD_NO_DATA,
        N_("Define colors for the user interface"),
        N_("color <object> [ <attribute> ... ] <foreground> <background> [ <regex> [ <num> ]]"),
        "configuration.html#color" },
  { "echo", CMD_ECHO, parse_echo, CMD_NO_DATA,
        N_("Print a message to the status line"),
        N_("echo <message>"),
        "advancedusage.html#echo" },
  { "finish", CMD_FINISH, parse_finish, CMD_NO_DATA,
        N_("Stop reading current config file"),
        N_("finish "),
        "optionalfeatures.html#ifdef" },
  { "group", CMD_GROUP, parse_group, CMD_NO_DATA,
        N_("Add addresses to an address group"),
        N_("group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "hdr_order", CMD_HDR_ORDER, parse_stailq, IP &HeaderOrderList,
        N_("Define custom order of headers displayed"),
        N_("hdr_order <header> [ <header> ... ]"),
        "configuration.html#hdr-order" },
  { "ifdef", CMD_IFDEF, parse_ifdef, CMD_NO_DATA,
        N_("Conditionally include config commands if symbol defined"),
        N_("ifdef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "ifndef", CMD_IFNDEF, parse_ifdef, CMD_NO_DATA,
        N_("Conditionally include if symbol is not defined"),
        N_("ifndef <symbol> '<config-command> [ <args> ... ]'"),
        "optionalfeatures.html#ifdef" },
  { "ignore", CMD_IGNORE, parse_ignore, CMD_NO_DATA,
        N_("Hide specified headers when displaying messages"),
        N_("ignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "lists", CMD_LISTS, parse_lists, CMD_NO_DATA,
        N_("Add address to the list of mailing lists"),
        N_("lists [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#lists" },
  { "mailboxes", CMD_MAILBOXES, parse_mailboxes, CMD_NO_DATA,
        N_("Define a list of mailboxes to watch"),
        N_("mailboxes [[ -label <label> ] | -nolabel ] [[ -notify | -nonotify ] [ -poll | -nopoll ] <mailbox> ] [ ... ]"),
        "configuration.html#mailboxes" },
  { "mailto_allow", CMD_MAILTO_ALLOW, parse_stailq, IP &MailToAllow,
        N_("Permit specific header-fields in mailto URL processing"),
        N_("mailto_allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "mime_lookup", CMD_MIME_LOOKUP, parse_stailq, IP &MimeLookupList,
        N_("Map specified MIME types/subtypes to display handlers"),
        N_("mime_lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#mime-lookup" },
  { "mono", CMD_MONO, parse_mono, CMD_NO_DATA,
        N_("**Deprecated**: Use `color`"),
        N_("mono <object> <attribute> [ <pattern> | <regex> ]"),
        "configuration.html#color-mono" },
  { "my_hdr", CMD_MY_HDR, parse_my_hdr, CMD_NO_DATA,
        N_("Add a custom header to outgoing messages"),
        N_("my_hdr <string>"),
        "configuration.html#my-hdr" },
  { "named-mailboxes", CMD_NAMED_MAILBOXES, parse_mailboxes, CMD_NO_DATA,
        N_("Define a list of labelled mailboxes to watch"),
        N_("named-mailboxes <description> <mailbox> [ <description> <mailbox> ... ]"),
        "configuration.html#mailboxes" },
  { "nospam", CMD_NOSPAM, parse_nospam, CMD_NO_DATA,
        N_("Remove a spam detection rule"),
        N_("nospam { * | <regex> }"),
        "configuration.html#spam" },
  { "reset", CMD_RESET, parse_set, CMD_NO_DATA,
        N_("Reset a config option to its initial value"),
        N_("reset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "score", CMD_SCORE, parse_score, CMD_NO_DATA,
        N_("Set a score value on emails matching a pattern"),
        N_("score <pattern> <value>"),
        "configuration.html#score-command" },
  { "set", CMD_SET, parse_set, CMD_NO_DATA,
        N_("Set a config variable"),
        N_("set { [ no | inv | & ] <variable> [?] | <variable> [=|+=|-=] value } [ ... ]"),
        "configuration.html#set" },
  { "setenv", CMD_SETENV, parse_setenv, CMD_NO_DATA,
        N_("Set an environment variable"),
        N_("setenv { <variable>? | <variable> <value> }"),
        "advancedusage.html#setenv" },
  { "source", CMD_SOURCE, parse_source, CMD_NO_DATA,
        N_("Read and execute commands from a config file"),
        N_("source <filename>"),
        "configuration.html#source" },
  { "spam", CMD_SPAM, parse_spam, CMD_NO_DATA,
        N_("Define rules to parse spam detection headers"),
        N_("spam <regex> <format>"),
        "configuration.html#spam" },
  { "subjectrx", CMD_SUBJECTRX, parse_subjectrx_list, CMD_NO_DATA,
        N_("Apply regex-based rewriting to message subjects"),
        N_("subjectrx <regex> <replacement>"),
        "advancedusage.html#display-munging" },
  { "subscribe", CMD_SUBSCRIBE, parse_subscribe, CMD_NO_DATA,
        N_("Add address to the list of subscribed mailing lists"),
        N_("subscribe [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#lists" },
  { "tag-formats", CMD_TAG_FORMATS, parse_tag_formats, CMD_NO_DATA,
        N_("Define expandos tags"),
        N_("tag-formats <tag> <format-string> { tag format-string ... }"),
        "optionalfeatures.html#custom-tags" },
  { "tag-transforms", CMD_TAG_TRANSFORMS, parse_tag_transforms, CMD_NO_DATA,
        N_("Rules to transform tags into icons"),
        N_("tag-transforms <tag> <transformed-string> { tag transformed-string ... }"),
        "optionalfeatures.html#custom-tags" },
  { "toggle", CMD_TOGGLE, parse_set, CMD_NO_DATA,
        N_("Toggle the value of a boolean/quad config option"),
        N_("toggle <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unalias", CMD_UNALIAS, parse_unalias, CMD_NO_DATA,
        N_("Remove an alias definition"),
        N_("unalias [ -group <name> ... ] { * | <key> ... }"),
        "configuration.html#alias" },
  { "unalternates", CMD_UNALTERNATES, parse_unalternates, CMD_NO_DATA,
        N_("Remove addresses from `alternates` list"),
        N_("unalternates [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#alternates" },
  { "unalternative_order", CMD_UNALTERNATIVE_ORDER, parse_unstailq, IP &AlternativeOrderList,
        N_("Remove MIME types from preference order"),
        N_("unalternative_order { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#alternative-order" },
  { "unattachments", CMD_UNATTACHMENTS, parse_unattachments, CMD_NO_DATA,
        N_("Remove attachment counting rules"),
        N_("unattachments { * | { + | - }<disposition> <mime-type> [ <mime-type> ... ] }"),
        "mimesupport.html#attachments" },
  { "unauto_view", CMD_UNAUTO_VIEW, parse_unstailq, IP &AutoViewList,
        N_("Remove MIME types from `auto_view` list"),
        N_("unauto_view { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#auto-view" },
  { "uncolor", CMD_UNCOLOR, parse_uncolor, CMD_NO_DATA,
        N_("Remove a `color` definition"),
        N_("uncolor <object> { * | <pattern> ... }"),
        "configuration.html#color" },
  { "ungroup", CMD_UNGROUP, parse_group, CMD_NO_DATA,
        N_("Remove addresses from an address `group`"),
        N_("ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "unhdr_order", CMD_UNHDR_ORDER, parse_unstailq, IP &HeaderOrderList,
        N_("Remove header from `hdr_order` list"),
        N_("unhdr_order { * | <header> ... }"),
        "configuration.html#hdr-order" },
  { "unignore", CMD_UNIGNORE, parse_unignore, CMD_NO_DATA,
        N_("Remove a header from the `hdr_order` list"),
        N_("unignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "unlists", CMD_UNLISTS, parse_unlists, CMD_NO_DATA,
        N_("Remove address from the list of mailing lists"),
        N_("unlists [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#lists" },
  { "unmailboxes", CMD_UNMAILBOXES, parse_unmailboxes, CMD_NO_DATA,
        N_("Remove mailboxes from the watch list"),
        N_("unmailboxes { * | <mailbox> ... }"),
        "configuration.html#mailboxes" },
  { "unmailto_allow", CMD_UNMAILTO_ALLOW, parse_unstailq, IP &MailToAllow,
        N_("Disallow header-fields in mailto processing"),
        N_("unmailto_allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "unmime_lookup", CMD_UNMIME_LOOKUP, parse_unstailq, IP &MimeLookupList,
        N_("Remove custom MIME-type handlers"),
        N_("unmime_lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#mime-lookup" },
  { "unmono", CMD_UNMONO, parse_unmono, CMD_NO_DATA,
        N_("**Deprecated**: Use `uncolor`"),
        N_("unmono <object> { * | <pattern> ... }"),
        "configuration.html#color-mono" },
  { "unmy_hdr", CMD_UNMY_HDR, parse_unmy_hdr, CMD_NO_DATA,
        N_("Remove a header previously added with `my_hdr`"),
        N_("unmy_hdr { * | <field> ... }"),
        "configuration.html#my-hdr" },
  { "unscore", CMD_UNSCORE, parse_unscore, CMD_NO_DATA,
        N_("Remove scoring rules for matching patterns"),
        N_("unscore { * | <pattern> ... }"),
        "configuration.html#score-command" },
  { "unset", CMD_UNSET, parse_set, CMD_NO_DATA,
        N_("Reset a config option to false/empty"),
        N_("unset <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unsetenv", CMD_UNSETENV, parse_setenv, CMD_NO_DATA,
        N_("Unset an environment variable"),
        N_("unsetenv <variable>"),
        "advancedusage.html#setenv" },
  { "unsubjectrx", CMD_UNSUBJECTRX, parse_unsubjectrx_list, CMD_NO_DATA,
        N_("Remove subject-rewriting rules"),
        N_("unsubjectrx { * | <regex> }"),
        "advancedusage.html#display-munging" },
  { "unsubscribe", CMD_UNSUBSCRIBE, parse_unsubscribe, CMD_NO_DATA,
        N_("Remove address from the list of subscribed mailing lists"),
        N_("unsubscribe [ -group <name> ... ] { * | <regex> ... }"),
        "configuration.html#lists" },
  { "version", CMD_VERSION, parse_version, CMD_NO_DATA,
        N_("Show NeoMutt version and build information"),
        N_("version "),
        "configuration.html#version" },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};

/**
 * commands_init - Initialize commands array and register default commands
 */
bool commands_init(void)
{
  return commands_register(&NeoMutt->commands, MuttCommands);
}

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
      return cmd;
  }

  return NULL;
}
