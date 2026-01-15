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
#include "my_header.h"
#include "parse.h"
#include "score.h"
#include "setenv.h"
#include "source.h"
#include "spam.h"
#include "stailq.h"
#include "subjectrx.h"
#include "tags.h"

/**
 * CommandsCommands - General NeoMutt Commands
 */
const struct Command CommandsCommands[] = {
  // clang-format off
  { "alias", CMD_ALIAS, parse_alias, CMD_NO_DATA,
        N_("Define an alias (name to email address)"),
        N_("alias [ -group <name> ... ] <key> <address> [, <address> ... ]"),
        "configuration.html#alias" },
  { "alternates", CMD_ALTERNATES, parse_alternates, CMD_NO_DATA,
        N_("Define a list of alternate email addresses for the user"),
        N_("alternates [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#alternates" },
  { "alternative-order", CMD_ALTERNATIVE_ORDER, parse_stailq, IP &AlternativeOrderList,
        N_("Set preference order for multipart alternatives"),
        N_("alternative-order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#alternative-order" },
  { "attachments", CMD_ATTACHMENTS, parse_attachments, CMD_NO_DATA,
        N_("Set attachment counting rules"),
        N_("attachments { + | - }<disposition> <mime-type> [ <mime-type> ... ] | ?"),
        "mimesupport.html#attachments" },
  { "auto-view", CMD_AUTO_VIEW, parse_stailq, IP &AutoViewList,
        N_("Automatically display specified MIME types inline"),
        N_("auto-view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
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
        N_("finish"),
        "optionalfeatures.html#ifdef" },
  { "group", CMD_GROUP, parse_group, CMD_NO_DATA,
        N_("Add addresses to an address group"),
        N_("group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "header-order", CMD_HEADER_ORDER, parse_stailq, IP &HeaderOrderList,
        N_("Define custom order of headers displayed"),
        N_("header-order <header> [ <header> ... ]"),
        "configuration.html#header-order" },
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
  { "mailto-allow", CMD_MAILTO_ALLOW, parse_stailq, IP &MailToAllow,
        N_("Permit specific header-fields in mailto URL processing"),
        N_("mailto-allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "mime-lookup", CMD_MIME_LOOKUP, parse_stailq, IP &MimeLookupList,
        N_("Map specified MIME types/subtypes to display handlers"),
        N_("mime-lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]"),
        "mimesupport.html#mime-lookup" },
  { "mono", CMD_MONO, parse_mono, CMD_NO_DATA,
        N_("Deprecated: Use 'color'"),
        N_("mono <object> <attribute> [ <pattern> | <regex> ]"),
        "configuration.html#color-mono" },
  { "my-header", CMD_MY_HEADER, parse_my_header, CMD_NO_DATA,
        N_("Add a custom header to outgoing messages"),
        N_("my-header <string>"),
        "configuration.html#my-header" },
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
        N_("set { [ no | inv | & ] <variable> [?] | <variable> [=|+=|-=] <value> } [ ... ]"),
        "configuration.html#set" },
  { "setenv", CMD_SETENV, parse_setenv, CMD_NO_DATA,
        N_("Set an environment variable"),
        N_("setenv [ { <variable>? | <variable> <value> } ]"),
        "advancedusage.html#setenv" },
  { "source", CMD_SOURCE, parse_source, CMD_NO_DATA,
        N_("Read and execute commands from a config file"),
        N_("source <filename> [ <filename> ... ]"),
        "configuration.html#source" },
  { "spam", CMD_SPAM, parse_spam, CMD_NO_DATA,
        N_("Define rules to parse spam detection headers"),
        N_("spam <regex> [ <format> ]"),
        "configuration.html#spam" },
  { "subject-regex", CMD_SUBJECT_REGEX, parse_subjectrx_list, CMD_NO_DATA,
        N_("Apply regex-based rewriting to message subjects"),
        N_("subject-regex <regex> <replacement>"),
        "advancedusage.html#display-munging" },
  { "subscribe", CMD_SUBSCRIBE, parse_subscribe, CMD_NO_DATA,
        N_("Add address to the list of subscribed mailing lists"),
        N_("subscribe [ -group <name> ... ] <regex> [ <regex> ... ]"),
        "configuration.html#lists" },
  { "tag-formats", CMD_TAG_FORMATS, parse_tag_formats, CMD_NO_DATA,
        N_("Define expandos tags"),
        N_("tag-formats <tag> <format-string> { <tag> <format-string> ... }"),
        "optionalfeatures.html#custom-tags" },
  { "tag-transforms", CMD_TAG_TRANSFORMS, parse_tag_transforms, CMD_NO_DATA,
        N_("Rules to transform tags into icons"),
        N_("tag-transforms <tag> <transformed-string> { <tag> <transformed-string> ... }"),
        "optionalfeatures.html#custom-tags" },
  { "toggle", CMD_TOGGLE, parse_set, CMD_NO_DATA,
        N_("Toggle the value of a boolean/quad config option"),
        N_("toggle <variable> [ <variable> ... ]"),
        "configuration.html#set" },
  { "unalias", CMD_UNALIAS, parse_unalias, CMD_NO_DATA,
        N_("Remove an alias definition"),
        N_("unalias { * | <key> ... }"),
        "configuration.html#alias" },
  { "unalternates", CMD_UNALTERNATES, parse_unalternates, CMD_NO_DATA,
        N_("Remove addresses from 'alternates' list"),
        N_("unalternates { * | <regex> ... }"),
        "configuration.html#alternates" },
  { "unalternative-order", CMD_UNALTERNATIVE_ORDER, parse_unstailq, IP &AlternativeOrderList,
        N_("Remove MIME types from preference order"),
        N_("unalternative-order { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#alternative-order" },
  { "unattachments", CMD_UNATTACHMENTS, parse_unattachments, CMD_NO_DATA,
        N_("Remove attachment counting rules"),
        N_("unattachments { * | { + | - }<disposition> <mime-type> [ <mime-type> ... ] }"),
        "mimesupport.html#attachments" },
  { "unauto-view", CMD_UNAUTO_VIEW, parse_unstailq, IP &AutoViewList,
        N_("Remove MIME types from 'auto-view' list"),
        N_("unauto-view { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#auto-view" },
  { "uncolor", CMD_UNCOLOR, parse_uncolor, CMD_NO_DATA,
        N_("Remove a 'color' definition"),
        N_("uncolor <object> { * | <pattern> ... }"),
        "configuration.html#color" },
  { "ungroup", CMD_UNGROUP, parse_group, CMD_NO_DATA,
        N_("Remove addresses from an address 'group'"),
        N_("ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "unheader-order", CMD_UNHEADER_ORDER, parse_unstailq, IP &HeaderOrderList,
        N_("Remove header from 'header-order' list"),
        N_("unheader-order { * | <header> ... }"),
        "configuration.html#header-order" },
  { "unignore", CMD_UNIGNORE, parse_unignore, CMD_NO_DATA,
        N_("Remove a header from the 'header-order' list"),
        N_("unignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "unlists", CMD_UNLISTS, parse_unlists, CMD_NO_DATA,
        N_("Remove address from the list of mailing lists"),
        N_("unlists { * | <regex> ... }"),
        "configuration.html#lists" },
  { "unmailboxes", CMD_UNMAILBOXES, parse_unmailboxes, CMD_NO_DATA,
        N_("Remove mailboxes from the watch list"),
        N_("unmailboxes { * | <mailbox> ... }"),
        "configuration.html#mailboxes" },
  { "unmailto-allow", CMD_UNMAILTO_ALLOW, parse_unstailq, IP &MailToAllow,
        N_("Disallow header-fields in mailto processing"),
        N_("unmailto-allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "unmime-lookup", CMD_UNMIME_LOOKUP, parse_unstailq, IP &MimeLookupList,
        N_("Remove custom MIME-type handlers"),
        N_("unmime-lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#mime-lookup" },
  { "unmono", CMD_UNMONO, parse_unmono, CMD_NO_DATA,
        N_("Deprecated: Use 'uncolor'"),
        N_("unmono <object> { * | <pattern> ... }"),
        "configuration.html#color-mono" },
  { "unmy-header", CMD_UNMY_HEADER, parse_unmy_header, CMD_NO_DATA,
        N_("Remove a header previously added with 'my-header'"),
        N_("unmy-header { * | <field> ... }"),
        "configuration.html#my-header" },
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
  { "unsubject-regex", CMD_UNSUBJECT_REGEX, parse_unsubjectrx_list, CMD_NO_DATA,
        N_("Remove subject-rewriting rules"),
        N_("unsubject-regex { * | <regex> }"),
        "advancedusage.html#display-munging" },
  { "unsubscribe", CMD_UNSUBSCRIBE, parse_unsubscribe, CMD_NO_DATA,
        N_("Remove address from the list of subscribed mailing lists"),
        N_("unsubscribe { * | <regex> ... }"),
        "configuration.html#lists" },
  { "version", CMD_VERSION, parse_version, CMD_NO_DATA,
        N_("Show NeoMutt version and build information"),
        N_("version"),
        "configuration.html#version" },

  // Deprecated
  { "alternative_order",   CMD_NONE, NULL, IP "alternative-order",   NULL, NULL, NULL, CF_SYNONYM },
  { "auto_view",           CMD_NONE, NULL, IP "auto-view",           NULL, NULL, NULL, CF_SYNONYM },
  { "hdr_order",           CMD_NONE, NULL, IP "header-order",        NULL, NULL, NULL, CF_SYNONYM },
  { "mailto_allow",        CMD_NONE, NULL, IP "mailto-allow",        NULL, NULL, NULL, CF_SYNONYM },
  { "mime_lookup",         CMD_NONE, NULL, IP "mime-lookup",         NULL, NULL, NULL, CF_SYNONYM },
  { "my_hdr",              CMD_NONE, NULL, IP "my-header",           NULL, NULL, NULL, CF_SYNONYM },
  { "subjectrx",           CMD_NONE, NULL, IP "subject-regex",       NULL, NULL, NULL, CF_SYNONYM },
  { "unalternative_order", CMD_NONE, NULL, IP "unalternative-order", NULL, NULL, NULL, CF_SYNONYM },
  { "unauto_view",         CMD_NONE, NULL, IP "unauto-view",         NULL, NULL, NULL, CF_SYNONYM },
  { "unhdr_order",         CMD_NONE, NULL, IP "unheader-order",      NULL, NULL, NULL, CF_SYNONYM },
  { "unmailto_allow",      CMD_NONE, NULL, IP "unmailto-allow",      NULL, NULL, NULL, CF_SYNONYM },
  { "unmime_lookup",       CMD_NONE, NULL, IP "unmime-lookup",       NULL, NULL, NULL, CF_SYNONYM },
  { "unmy_hdr",            CMD_NONE, NULL, IP "unmy-header",         NULL, NULL, NULL, CF_SYNONYM },
  { "unsubjectrx",         CMD_NONE, NULL, IP "unsubject-regex",     NULL, NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
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
      if ((cmd->flags & CF_SYNONYM) && cmd->data)
      {
        const char *real_name = (const char *) cmd->data;
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
