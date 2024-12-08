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
#include "domain.h"
#include "dump.h"
#include "globals.h"
#include "module_data.h"
#include "notify2.h"
#include "parse_color.h"
#include "regex4.h"
#include "simple2.h"

/**
 * ColorDefs - Mapping of colour names to their IDs
 */
const struct ColorDefinition ColorDefs[] = {
  // clang-format off
  { "attachment",               CD_PAGER,   MT_COLOR_ATTACHMENT,               CRF_SIMPLE },
  { "attach_headers",           CD_PAGER,   MT_COLOR_ATTACH_HEADERS,           CRF_REGEX },
  { "body",                     CD_PAGER,   MT_COLOR_BODY,                     CRF_REGEX },
  { "bold",                     CD_CORE,    MT_COLOR_BOLD,                     CRF_SIMPLE },
  { "compose_header",           CD_COMPOSE, MT_COLOR_COMPOSE_HEADER,           CRF_SIMPLE },
  { "compose_security_both",    CD_COMPOSE, MT_COLOR_COMPOSE_SECURITY_BOTH,    CRF_SIMPLE },
  { "compose_security_encrypt", CD_COMPOSE, MT_COLOR_COMPOSE_SECURITY_ENCRYPT, CRF_SIMPLE },
  { "compose_security_none",    CD_COMPOSE, MT_COLOR_COMPOSE_SECURITY_NONE,    CRF_SIMPLE },
  { "compose_security_sign",    CD_COMPOSE, MT_COLOR_COMPOSE_SECURITY_SIGN,    CRF_SIMPLE },
  { "error",                    CD_CORE,    MT_COLOR_ERROR,                    CRF_SIMPLE },
  { "hdrdefault",               CD_PAGER,   MT_COLOR_HDRDEFAULT,               CRF_SIMPLE },
  { "header",                   CD_PAGER,   MT_COLOR_HEADER,                   CRF_REGEX },
  { "index",                    CD_INDEX,   MT_COLOR_INDEX,                    CRF_REGEX },
  { "index_author",             CD_INDEX,   MT_COLOR_INDEX_AUTHOR,             CRF_REGEX },
  { "index_collapsed",          CD_INDEX,   MT_COLOR_INDEX_COLLAPSED,          CRF_REGEX },
  { "index_date",               CD_INDEX,   MT_COLOR_INDEX_DATE,               CRF_REGEX },
  { "index_flags",              CD_INDEX,   MT_COLOR_INDEX_FLAGS,              CRF_REGEX },
  { "index_label",              CD_INDEX,   MT_COLOR_INDEX_LABEL,              CRF_REGEX },
  { "index_number",             CD_INDEX,   MT_COLOR_INDEX_NUMBER,             CRF_REGEX },
  { "index_size",               CD_INDEX,   MT_COLOR_INDEX_SIZE,               CRF_REGEX },
  { "index_subject",            CD_INDEX,   MT_COLOR_INDEX_SUBJECT,            CRF_REGEX },
  { "index_tag",                CD_INDEX,   MT_COLOR_INDEX_TAG,                CRF_REGEX },
  { "index_tags",               CD_INDEX,   MT_COLOR_INDEX_TAGS,               CRF_REGEX },
  { "indicator",                CD_CORE,    MT_COLOR_INDICATOR,                CRF_SIMPLE },
  { "italic",                   CD_CORE,    MT_COLOR_ITALIC,                   CRF_SIMPLE },
  { "markers",                  CD_PAGER,   MT_COLOR_MARKERS,                  CRF_SIMPLE },
  { "message",                  CD_CORE,    MT_COLOR_MESSAGE,                  CRF_SIMPLE },
  { "normal",                   CD_CORE,    MT_COLOR_NORMAL,                   CRF_SIMPLE },
  { "options",                  CD_CORE,    MT_COLOR_OPTIONS,                  CRF_SIMPLE },
  { "progress",                 CD_CORE,    MT_COLOR_PROGRESS,                 CRF_SIMPLE },
  { "prompt",                   CD_CORE,    MT_COLOR_PROMPT,                   CRF_SIMPLE },
  { "quoted0",                  CD_QUOTED,  MT_COLOR_QUOTED0,                  CRF_SIMPLE },
  { "quoted1",                  CD_QUOTED,  MT_COLOR_QUOTED1,                  CRF_SIMPLE },
  { "quoted2",                  CD_QUOTED,  MT_COLOR_QUOTED2,                  CRF_SIMPLE },
  { "quoted3",                  CD_QUOTED,  MT_COLOR_QUOTED3,                  CRF_SIMPLE },
  { "quoted4",                  CD_QUOTED,  MT_COLOR_QUOTED4,                  CRF_SIMPLE },
  { "quoted5",                  CD_QUOTED,  MT_COLOR_QUOTED5,                  CRF_SIMPLE },
  { "quoted6",                  CD_QUOTED,  MT_COLOR_QUOTED6,                  CRF_SIMPLE },
  { "quoted7",                  CD_QUOTED,  MT_COLOR_QUOTED7,                  CRF_SIMPLE },
  { "quoted8",                  CD_QUOTED,  MT_COLOR_QUOTED8,                  CRF_SIMPLE },
  { "quoted9",                  CD_QUOTED,  MT_COLOR_QUOTED9,                  CRF_SIMPLE },
  { "search",                   CD_PAGER,   MT_COLOR_SEARCH,                   CRF_SIMPLE },
  { "sidebar_background",       CD_SIDEBAR, MT_COLOR_SIDEBAR_BACKGROUND,       CRF_SIMPLE },
  { "sidebar_divider",          CD_SIDEBAR, MT_COLOR_SIDEBAR_DIVIDER,          CRF_SIMPLE },
  { "sidebar_flagged",          CD_SIDEBAR, MT_COLOR_SIDEBAR_FLAGGED,          CRF_SIMPLE },
  { "sidebar_highlight",        CD_SIDEBAR, MT_COLOR_SIDEBAR_HIGHLIGHT,        CRF_SIMPLE },
  { "sidebar_indicator",        CD_SIDEBAR, MT_COLOR_SIDEBAR_INDICATOR,        CRF_SIMPLE },
  { "sidebar_new",              CD_SIDEBAR, MT_COLOR_SIDEBAR_NEW,              CRF_SIMPLE },
  { "sidebar_ordinary",         CD_SIDEBAR, MT_COLOR_SIDEBAR_ORDINARY,         CRF_SIMPLE },
  { "sidebar_spool_file",       CD_SIDEBAR, MT_COLOR_SIDEBAR_SPOOL_FILE,       CRF_SIMPLE },
  { "sidebar_unread",           CD_SIDEBAR, MT_COLOR_SIDEBAR_UNREAD,           CRF_SIMPLE },
  { "signature",                CD_PAGER,   MT_COLOR_SIGNATURE,                CRF_SIMPLE },
  { "status",                   CD_CORE,    MT_COLOR_STATUS,                   CRF_REGEX | CRF_BACK_REF },
  { "stripe_even",              CD_CORE,    MT_COLOR_STRIPE_EVEN,              CRF_SIMPLE },
  { "stripe_odd",               CD_CORE,    MT_COLOR_STRIPE_ODD,               CRF_SIMPLE },
  { "tilde",                    CD_PAGER,   MT_COLOR_TILDE,                    CRF_SIMPLE },
  { "tree",                     CD_CORE,    MT_COLOR_TREE,                     CRF_SIMPLE },
  { "underline",                CD_CORE,    MT_COLOR_UNDERLINE,                CRF_SIMPLE },
  { "warning",                  CD_CORE,    MT_COLOR_WARNING,                  CRF_SIMPLE },

  // Deprecated
  { "quoted",                   CD_QUOTED,  MT_COLOR_QUOTED0,                  CRF_SYNONYM },
  { "sidebar_spoolfile",        CD_SIDEBAR, MT_COLOR_SIDEBAR_SPOOL_FILE,       CRF_SYNONYM },
  { NULL, 0 },
  // clang-format on
};

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

  parse_extract_token(token, line, TOKEN_NONE);
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
    parse_extract_token(suffix, line, TOKEN_NONE);
    buf_fix_dptr(token);
    buf_add_printf(token, "_%s", buf_string(suffix));
    buf_pool_release(&suffix);
  }

  int rc = color_get_cid(buf_string(token));
  if (rc == -1)
  {
    buf_printf(err, _("%s: no such object"), buf_string(token));
    buf_pool_release(&token);
    return MUTT_CMD_WARNING;
  }
  else
  {
    color_debug(LL_DEBUG5, "object: %s\n", buf_string(token));
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
enum CommandResult parse_uncolor_command(const struct Command *cmd, struct Buffer *line,
                                         const struct ParseContext *pc,
                                         struct ParseError *pe)
{
  struct Buffer *err = pe->message;

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
    parse_extract_token(token, line, TOKEN_NONE);
    if (mutt_str_equal(buf_string(token), "*"))
    {
      struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
      colors_reset(mod_data);
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }
  }

  enum ColorId cid = MT_COLOR_NONE;
  color_debug(LL_DEBUG5, "uncolor: %s\n", buf_string(token));
  rc = parse_object(cmd, line, &cid, err);
  if (rc != MUTT_CMD_SUCCESS)
    goto done;

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
    parse_extract_token(token, line, TOKEN_NONE);
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
      parse_extract_token(token, line, TOKEN_NONE);
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
    parse_extract_token(token, line, TOKEN_NONE);

    if (MoreArgs(line))
    {
      struct Buffer *tmp = buf_pool_get();
      parse_extract_token(tmp, line, TOKEN_NONE);
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
    color_get_name(cid, token);
    color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf_string(token));
    struct ColorModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_COLOR);
    struct EventColor ev_c = { cid, NULL };
    notify_send(mod_data->colors_notify, NT_COLOR, NT_COLOR_SET, &ev_c);
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
                                 const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

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
      parse_extract_token(token, line, TOKEN_NONE);
    }
    goto done;
  }

  color_debug(LL_DEBUG5, "parse: %s\n", buf_string(token));
  rc = parse_uncolor_command(cmd, line, pc, pe);
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
                                const struct ParseContext *pc, struct ParseError *pe)
{
  // Quietly discard the command
  struct Buffer *token = buf_pool_get();
  while (MoreArgs(line))
  {
    parse_extract_token(token, line, TOKEN_NONE);
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
                               const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  // No GUI, or no colours, so quietly discard the command
  if (!OptGui || (COLORS == 0))
  {
    while (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NONE);
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
                              const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  // No GUI, or colours available, so quietly discard the command
  if (!OptGui || (COLORS != 0))
  {
    while (MoreArgs(line))
    {
      parse_extract_token(token, line, TOKEN_NONE);
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

/**
 * ColorCommands - Colour Commands
 */
const struct Command ColorCommands[] = {
  // clang-format off
  { "color", CMD_COLOR, parse_color,
        N_("Define colors for the user interface"),
        N_("color <object> [ <attribute> ... ] <fg> <bg> [ <regex> [ <num> ]]"),
        "configuration.html#color" },
  { "mono", CMD_MONO, parse_mono,
        N_("Deprecated: Use `color` instead"),
        N_("mono <object> <attribute> [ <pattern> | <regex> ]"),
        "configuration.html#color-mono" },
  { "uncolor", CMD_UNCOLOR, parse_uncolor,
        N_("Remove a `color` definition"),
        N_("uncolor <object> { * | <pattern> ... }"),
        "configuration.html#color" },
  { "unmono", CMD_UNMONO, parse_unmono,
        N_("Deprecated: Use `uncolor` instead"),
        N_("unmono <object> { * | <pattern> ... }"),
        "configuration.html#color-mono" },

  { NULL, CMD_NONE, NULL, NULL, NULL, NULL, CRF_NONE },
  // clang-format on
};
