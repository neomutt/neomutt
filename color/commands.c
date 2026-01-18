/**
 * @file
 * Parse colour commands
 *
 * @authors
 * Copyright (C) 2021-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page color_commands Parse colour commands
 *
 * Parse NeoMutt 'color', 'uncolor', 'mono' and 'unmono' commands.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "commands.h"
#include "parse/lib.h"
#include "attr.h"
#include "color.h"
#include "debug.h"
#include "dump.h"
#include "globals.h"
#include "notify2.h"
#include "parse_color.h"
#include "regex4.h"
#include "simple2.h"

/**
 * ColorFields - Mapping of colour names to their IDs
 */
const struct Mapping ColorFields[] = {
  // clang-format off
  { "attachment",                MT_COLOR_ATTACHMENT },
  { "attach_headers",            MT_COLOR_ATTACH_HEADERS },
  { "body",                      MT_COLOR_BODY },
  { "bold",                      MT_COLOR_BOLD },
  { "compose_header",            MT_COLOR_COMPOSE_HEADER },
  { "compose_security_both",     MT_COLOR_COMPOSE_SECURITY_BOTH },
  { "compose_security_encrypt",  MT_COLOR_COMPOSE_SECURITY_ENCRYPT },
  { "compose_security_none",     MT_COLOR_COMPOSE_SECURITY_NONE },
  { "compose_security_sign",     MT_COLOR_COMPOSE_SECURITY_SIGN },
  { "error",                     MT_COLOR_ERROR },
  { "hdrdefault",                MT_COLOR_HDRDEFAULT },
  { "header",                    MT_COLOR_HEADER },
  { "index",                     MT_COLOR_INDEX },
  { "index_author",              MT_COLOR_INDEX_AUTHOR },
  { "index_collapsed",           MT_COLOR_INDEX_COLLAPSED },
  { "index_date",                MT_COLOR_INDEX_DATE },
  { "index_flags",               MT_COLOR_INDEX_FLAGS },
  { "index_label",               MT_COLOR_INDEX_LABEL },
  { "index_number",              MT_COLOR_INDEX_NUMBER },
  { "index_size",                MT_COLOR_INDEX_SIZE },
  { "index_subject",             MT_COLOR_INDEX_SUBJECT },
  { "index_tag",                 MT_COLOR_INDEX_TAG },
  { "index_tags",                MT_COLOR_INDEX_TAGS },
  { "indicator",                 MT_COLOR_INDICATOR },
  { "italic",                    MT_COLOR_ITALIC },
  { "markers",                   MT_COLOR_MARKERS },
  { "message",                   MT_COLOR_MESSAGE },
  { "normal",                    MT_COLOR_NORMAL },
  { "options",                   MT_COLOR_OPTIONS },
  { "progress",                  MT_COLOR_PROGRESS },
  { "prompt",                    MT_COLOR_PROMPT },
  { "quoted0",                   MT_COLOR_QUOTED0 },
  { "quoted1",                   MT_COLOR_QUOTED1 },
  { "quoted2",                   MT_COLOR_QUOTED2 },
  { "quoted3",                   MT_COLOR_QUOTED3 },
  { "quoted4",                   MT_COLOR_QUOTED4 },
  { "quoted5",                   MT_COLOR_QUOTED5 },
  { "quoted6",                   MT_COLOR_QUOTED6 },
  { "quoted7",                   MT_COLOR_QUOTED7 },
  { "quoted8",                   MT_COLOR_QUOTED8 },
  { "quoted9",                   MT_COLOR_QUOTED9 },
  { "search",                    MT_COLOR_SEARCH },
  { "sidebar_background",        MT_COLOR_SIDEBAR_BACKGROUND },
  { "sidebar_divider",           MT_COLOR_SIDEBAR_DIVIDER },
  { "sidebar_flagged",           MT_COLOR_SIDEBAR_FLAGGED },
  { "sidebar_highlight",         MT_COLOR_SIDEBAR_HIGHLIGHT },
  { "sidebar_indicator",         MT_COLOR_SIDEBAR_INDICATOR },
  { "sidebar_new",               MT_COLOR_SIDEBAR_NEW },
  { "sidebar_ordinary",          MT_COLOR_SIDEBAR_ORDINARY },
  { "sidebar_spool_file",        MT_COLOR_SIDEBAR_SPOOLFILE },
  { "sidebar_unread",            MT_COLOR_SIDEBAR_UNREAD },
  { "signature",                 MT_COLOR_SIGNATURE },
  { "status",                    MT_COLOR_STATUS },
  { "stripe_even",               MT_COLOR_STRIPE_EVEN},
  { "stripe_odd",                MT_COLOR_STRIPE_ODD},
  { "tilde",                     MT_COLOR_TILDE },
  { "tree",                      MT_COLOR_TREE },
  { "underline",                 MT_COLOR_UNDERLINE },
  { "warning",                   MT_COLOR_WARNING },
  // Deprecated
  { "quoted",                    MT_COLOR_QUOTED0 },
  { "sidebar_spoolfile",         MT_COLOR_SIDEBAR_SPOOLFILE },
  { NULL, 0 },
  // clang-format on
};

/**
 * get_colorid_name - Get the name of a Colour ID
 * @param cid Colour, e.g. #MT_COLOR_HEADER
 * @param buf Buffer for result
 */
void get_colorid_name(unsigned int cid, struct Buffer *buf)
{
  const char *name = mutt_map_get_name(cid, ColorFields);
  if (name)
    buf_addstr(buf, name);
  else
    buf_printf(buf, "UNKNOWN %d", cid);
}

/**
 * parse_object - Identify a colour object
 * @param[in]  cmd  Command being parsed
 * @param[in]  line Buffer containing string to be parsed
 * @param[out] cid  Object type, e.g. #MT_COLOR_TILDE
 * @param[out] err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Identify a colour object, e.g. "message", "compose header"
 */
static enum CommandResult parse_object(const struct Command *cmd, struct Buffer *line,
                                       enum ColorId *cid, struct Buffer *err)
{
  if (!MoreArgsF(line, TOKEN_COMMENT))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  color_debug(LL_DEBUG5, "color: %s\n", buf_string(token));

  if (mutt_istr_equal(buf_string(token), "compose"))
  {
    if (!MoreArgs(line))
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      buf_pool_release(&token);
      return MUTT_CMD_WARNING;
    }

    struct Buffer *suffix = buf_pool_get();
    parse_extract_token(suffix, line, TOKEN_NO_FLAGS);
    buf_fix_dptr(token);
    buf_add_printf(token, "_%s", buf_string(suffix));
    buf_pool_release(&suffix);
  }

  int rc = mutt_map_get_value(buf_string(token), ColorFields);
  if (rc == -1)
  {
    buf_printf(err, _("%s: no such object"), buf_string(token));
    buf_pool_release(&token);
    return MUTT_CMD_WARNING;
  }
  else
  {
    color_debug(LL_DEBUG5, "object: %s\n", mutt_map_get_name(rc, ColorFields));
  }

  *cid = rc;
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_uncolor_command - Parse an 'uncolor' command - Implements Command::parse() - @ingroup command_parse
 *
 * Usage:
 * * uncolor OBJECT [ PATTERN | REGEX | * ]
 */
enum CommandResult parse_uncolor_command(const struct Command *cmd,
                                         struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  // Peek at the next token ('*' won't match a colour name)
  if (line->dptr[0] == '*')
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal(buf_string(token), "*"))
    {
      colors_reset();
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }
  }

  unsigned int cid = MT_COLOR_NONE;
  color_debug(LL_DEBUG5, "uncolor: %s\n", buf_string(token));
  rc = parse_object(cmd, line, &cid, err);
  if (rc != MUTT_CMD_SUCCESS)
    goto done;

  if (cid == -1)
  {
    buf_printf(err, _("%s: no such object"), buf_string(token));
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  if ((cid == MT_COLOR_STATUS) && !MoreArgs(line))
  {
    color_debug(LL_DEBUG5, "simple\n");
    simple_color_reset(cid); // default colour for the status bar
    goto done;
  }

  if (!mutt_color_has_pattern(cid))
  {
    color_debug(LL_DEBUG5, "simple\n");
    simple_color_reset(cid);
    goto done;
  }

  if (!MoreArgs(line))
  {
    if (regex_colors_parse_uncolor(cid, NULL))
      rc = MUTT_CMD_SUCCESS;
    else
      rc = MUTT_CMD_ERROR;
    goto done;
  }

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf_string(token)))
    {
      if (regex_colors_parse_uncolor(cid, NULL))
        rc = MUTT_CMD_SUCCESS;
      else
        rc = MUTT_CMD_ERROR;
      goto done;
    }

    regex_colors_parse_uncolor(cid, buf_string(token));

  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_color_command - Parse a 'color' command
 * @param cmd      Command being parsed
 * @param line     Buffer containing string to be parsed
 * @param err      Buffer for error messages
 * @param callback Function to handle command - Implements ::parser_callback_t
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Usage:
 * * color OBJECT [ ATTRS ] FG BG [ PATTERN | REGEX ] [ NUM ]
 * * mono  OBJECT   ATTRS         [ PATTERN | REGEX ] [ NUM ]
 */
static enum CommandResult parse_color_command(const struct Command *cmd,
                                              struct Buffer *line, struct Buffer *err,
                                              parser_callback_t callback)
{
  if (!cmd || !line || !err)
    return MUTT_CMD_ERROR;

  unsigned int match = 0;
  enum ColorId cid = MT_COLOR_NONE;
  enum CommandResult rc = MUTT_CMD_ERROR;
  struct AttrColor *ac = NULL;
  struct Buffer *token = buf_pool_get();

  if (!MoreArgs(line))
  {
    if (StartupComplete)
    {
      color_dump();
      rc = MUTT_CMD_SUCCESS;
    }
    else
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      rc = MUTT_CMD_WARNING;
    }

    goto done;
  }

  rc = parse_object(cmd, line, &cid, err);
  if (rc != MUTT_CMD_SUCCESS)
    goto done;

  ac = attr_color_new();
  rc = callback(cmd, line, ac, err);
  if (rc != MUTT_CMD_SUCCESS)
    goto done;

  //------------------------------------------------------------------
  // Business Logic

  rc = MUTT_CMD_ERROR;
  if ((ac->fg.type == CT_RGB) || (ac->bg.type == CT_RGB))
  {
#ifndef NEOMUTT_DIRECT_COLORS
    buf_printf(err, _("Direct colors support not compiled in: %s"), buf_string(line));
    goto done;
#endif

    const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
    if (!c_color_directcolor)
    {
      buf_printf(err, _("Direct colors support disabled: %s"), buf_string(line));
      goto done;
    }
  }

  if ((ac->fg.color >= COLORS) || (ac->bg.color >= COLORS))
  {
    buf_printf(err, _("%s: color not supported by term"), buf_string(line));
    goto done;
  }

  //------------------------------------------------------------------

  /* extract a regular expression if needed */

  if (mutt_color_has_pattern(cid) && (cid != MT_COLOR_STATUS))
  {
    color_debug(LL_DEBUG5, "regex needed\n");
    if (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
    }
    else
    {
      buf_strcpy(token, ".*");
    }
  }

  if (MoreArgs(line) && (cid != MT_COLOR_STATUS))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto done;
  }

  if (regex_colors_parse_color_list(cid, buf_string(token), ac, &rc, err))
  {
    color_debug(LL_DEBUG5, "regex_colors_parse_color_list done\n");
    goto done;
    // do nothing
  }
  else if ((cid == MT_COLOR_STATUS) && MoreArgs(line))
  {
    color_debug(LL_DEBUG5, "status\n");
    /* 'color status fg bg' can have up to 2 arguments:
     * 0 arguments: sets the default status color (handled below by else part)
     * 1 argument : colorize pattern on match
     * 2 arguments: colorize nth submatch of pattern */
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (MoreArgs(line))
    {
      struct Buffer *tmp = buf_pool_get();
      parse_extract_token(tmp, line, TOKEN_NO_FLAGS);
      if (!mutt_str_atoui_full(buf_string(tmp), &match))
      {
        buf_printf(err, _("%s: invalid number: %s"), cmd->name, buf_string(tmp));
        buf_pool_release(&tmp);
        rc = MUTT_CMD_WARNING;
        goto done;
      }
      buf_pool_release(&tmp);
    }

    if (MoreArgs(line))
    {
      buf_printf(err, _("%s: too many arguments"), cmd->name);
      rc = MUTT_CMD_WARNING;
      goto done;
    }

    rc = regex_colors_parse_status_list(cid, buf_string(token), ac, match, err);
    goto done;
  }
  else // Remaining simple colours
  {
    color_debug(LL_DEBUG5, "simple\n");
    if (simple_color_set(cid, ac))
      rc = MUTT_CMD_SUCCESS;
    else
      rc = MUTT_CMD_ERROR;
  }

  if (rc == MUTT_CMD_SUCCESS)
  {
    get_colorid_name(cid, token);
    color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf_string(token));
    struct EventColor ev_c = { cid, NULL };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);
  }

done:
  attr_color_free(&ac);
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_uncolor - Parse the 'uncolor' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `uncolor <object> { * | <pattern> ... }`
 */
enum CommandResult parse_uncolor(const struct Command *cmd, struct Buffer *line,
                                 struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  if (!OptGui) // No GUI, so quietly discard the command
  {
    while (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
    }
    goto done;
  }

  color_debug(LL_DEBUG5, "parse: %s\n", buf_string(token));
  rc = parse_uncolor_command(cmd, line, err);
  curses_colors_dump(token);

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_unmono - Parse the 'unmono' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unmono <object> { * | <pattern> ... }`
 */
enum CommandResult parse_unmono(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  // Quietly discard the command
  struct Buffer *token = buf_pool_get();
  while (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
  }
  buf_pool_release(&token);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color - Parse the 'color' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `color object [ attribute ...] foreground background`
 * - `color index [ attribute ...] foreground background [ pattern ]`
 * - `color { header | body } [ attribute ...] foreground background regex`
 */
enum CommandResult parse_color(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  // No GUI, or no colours, so quietly discard the command
  if (!OptGui || (COLORS == 0))
  {
    while (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
    }
    goto done;
  }

  color_debug(LL_DEBUG5, "parse: color\n");
  rc = parse_color_command(cmd, line, err, parse_color_pair);
  curses_colors_dump(token);

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_mono - Parse the 'mono' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `mono <object> <attribute> [ <pattern> | <regex> ]`
 */
enum CommandResult parse_mono(const struct Command *cmd, struct Buffer *line,
                              struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  // No GUI, or colours available, so quietly discard the command
  if (!OptGui || (COLORS != 0))
  {
    while (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NO_FLAGS);
    }
    goto done;
  }

  color_debug(LL_DEBUG5, "parse: %s\n", buf_string(token));
  rc = parse_color_command(cmd, line, err, parse_attr_spec);
  curses_colors_dump(token);

done:
  buf_pool_release(&token);
  return rc;
}
