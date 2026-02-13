/**
 * @file
 * Handle Email commands
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page email_commands Email commands
 *
 * Email commands
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "commands/lib.h"
#include "group.h"
#include "ignore.h"
#include "module_data.h"
#include "score.h"
#include "spam.h"

struct ParseContext;
struct ParseError;

/**
 * parse_list - Parse a list command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `alternative-order   <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `auto-view           <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `header-order        <header> [ <header> ... ]`
 * - `mailto-allow        { * | <header-field> ... }`
 */
enum CommandResult parse_list(const struct Command *cmd, struct Buffer *line,
                              const struct ParseContext *pc, struct ParseError *pe)
{
  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  switch (cmd->id)
  {
    case CMD_ALTERNATIVE_ORDER:
      return parse_stailq_list(cmd, line, &md->alternative_order, pc, pe);

    case CMD_AUTO_VIEW:
      return parse_stailq_list(cmd, line, &md->auto_view, pc, pe);

    case CMD_HEADER_ORDER:
      return parse_stailq_list(cmd, line, &md->header_order, pc, pe);

    case CMD_MAILTO_ALLOW:
      return parse_stailq_list(cmd, line, &md->mail_to_allow, pc, pe);

    default:
      ASSERT(false);
  }

  return MUTT_CMD_ERROR;
}

/**
 * parse_unlist - Parse an unlist command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unalternative-order { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unauto-view         { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unheader-order      { * | <header> ... }`
 * - `unmailto-allow      { * | <header-field> ... }`
 */
enum CommandResult parse_unlist(const struct Command *cmd, struct Buffer *line,
                                const struct ParseContext *pc, struct ParseError *pe)
{
  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  switch (cmd->id)
  {
    case CMD_UNALTERNATIVE_ORDER:
      return parse_unstailq_list(cmd, line, &md->alternative_order, pc, pe);

    case CMD_UNAUTO_VIEW:
      return parse_unstailq_list(cmd, line, &md->auto_view, pc, pe);

    case CMD_UNHEADER_ORDER:
      return parse_unstailq_list(cmd, line, &md->header_order, pc, pe);

    case CMD_UNMAILTO_ALLOW:
      return parse_unstailq_list(cmd, line, &md->mail_to_allow, pc, pe);

    default:
      ASSERT(false);
  }

  return MUTT_CMD_ERROR;
}

/**
 * EmailCommands - Email Commands
 */
const struct Command EmailCommands[] = {
  // clang-format off
  { "alternative-order", CMD_ALTERNATIVE_ORDER, parse_list, CMD_NO_DATA,
        N_("Set preference order for multipart alternatives"),
        N_("alternative-order <mime-type>[/<mime-subtype> ] [ ... ]"),
        "mimesupport.html#alternative-order" },
  { "auto-view", CMD_AUTO_VIEW, parse_list, CMD_NO_DATA,
        N_("Automatically display specified MIME types inline"),
        N_("auto-view <mime-type>[/<mime-subtype> ] [ ... ]"),
        "mimesupport.html#auto-view" },
  { "group", CMD_GROUP, parse_group, CMD_NO_DATA,
        N_("Add addresses to an address group"),
        N_("group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "header-order", CMD_HEADER_ORDER, parse_list, CMD_NO_DATA,
        N_("Define custom order of headers displayed"),
        N_("header-order <header> [ <header> ... ]"),
        "configuration.html#header-order" },
  { "ignore", CMD_IGNORE, parse_ignore, CMD_NO_DATA,
        N_("Hide specified headers when displaying messages"),
        N_("ignore <string> [ <string> ...]"),
        "configuration.html#ignore" },
  { "lists", CMD_LISTS, parse_lists, CMD_NO_DATA,
        N_("Add address to the list of mailing lists"),
        N_("lists [ -group <name> ... ] <regex> [ ... ]"),
        "configuration.html#lists" },
  { "mailto-allow", CMD_MAILTO_ALLOW, parse_list, CMD_NO_DATA,
        N_("Permit specific header-fields in mailto URL processing"),
        N_("mailto-allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "nospam", CMD_NOSPAM, parse_nospam, CMD_NO_DATA,
        N_("Remove a spam detection rule"),
        N_("nospam { * | <regex> }"),
        "configuration.html#spam" },
  { "score", CMD_SCORE, parse_score, CMD_NO_DATA,
        N_("Set a score value on emails matching a pattern"),
        N_("score <pattern> <value>"),
        "configuration.html#score-command" },
  { "spam", CMD_SPAM, parse_spam, CMD_NO_DATA,
        N_("Define rules to parse spam detection headers"),
        N_("spam <regex> [ <format> ]"),
        "configuration.html#spam" },
  { "unalternative-order", CMD_UNALTERNATIVE_ORDER, parse_unlist, CMD_NO_DATA,
        N_("Remove MIME types from preference order"),
        N_("unalternative-order { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#alternative-order" },
  { "unauto-view", CMD_UNAUTO_VIEW, parse_unlist, CMD_NO_DATA,
        N_("Remove MIME types from `auto-view` list"),
        N_("unauto-view { * | [ <mime-type>[/<mime-subtype> ] ... ] }"),
        "mimesupport.html#auto-view" },
  { "ungroup", CMD_UNGROUP, parse_group, CMD_NO_DATA,
        N_("Remove addresses from an address `group`"),
        N_("ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "unheader-order", CMD_UNHEADER_ORDER, parse_unlist, CMD_NO_DATA,
        N_("Remove header from `header-order` list"),
        N_("unheader-order { * | <header> ... }"),
        "configuration.html#header-order" },
  { "unignore", CMD_UNIGNORE, parse_unignore, CMD_NO_DATA,
        N_("Remove a header from the `header-order` list"),
        N_("unignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "unlists", CMD_UNLISTS, parse_unlists, CMD_NO_DATA,
        N_("Remove address from the list of mailing lists"),
        N_("unlists { * | <regex> ... }"),
        "configuration.html#lists" },
  { "unmailto-allow", CMD_UNMAILTO_ALLOW, parse_unlist, CMD_NO_DATA,
        N_("Disallow header-fields in mailto processing"),
        N_("unmailto-allow { * | <header-field> ... }"),
        "configuration.html#mailto-allow" },
  { "unscore", CMD_UNSCORE, parse_unscore, CMD_NO_DATA,
        N_("Remove scoring rules for matching patterns"),
        N_("unscore { * | <pattern> ... }"),
        "configuration.html#score-command" },

  // Deprecated
  { "alternative_order",   CMD_NONE, NULL, CMD_NO_DATA, "alternative-order",   NULL, NULL, CF_SYNONYM },
  { "auto_view",           CMD_NONE, NULL, CMD_NO_DATA, "auto-view",           NULL, NULL, CF_SYNONYM },
  { "hdr_order",           CMD_NONE, NULL, CMD_NO_DATA, "header-order",        NULL, NULL, CF_SYNONYM },
  { "mailto_allow",        CMD_NONE, NULL, CMD_NO_DATA, "mailto-allow",        NULL, NULL, CF_SYNONYM },
  { "unalternative_order", CMD_NONE, NULL, CMD_NO_DATA, "unalternative-order", NULL, NULL, CF_SYNONYM },
  { "unauto_view",         CMD_NONE, NULL, CMD_NO_DATA, "unauto-view",         NULL, NULL, CF_SYNONYM },
  { "unhdr_order",         CMD_NONE, NULL, CMD_NO_DATA, "unheader-order",      NULL, NULL, CF_SYNONYM },
  { "unmailto_allow",      CMD_NONE, NULL, CMD_NO_DATA, "unmailto-allow",      NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
