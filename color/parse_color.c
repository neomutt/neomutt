/**
 * @file
 * Parse colours
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page color_parse_color Parse colours
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "parse_color.h"
#include "parse/lib.h"
#include "attr.h"
#include "color.h"
#include "debug.h"

/// Mapping between a colour name and an ncurses colour
const struct Mapping ColorNames[] = {
  // clang-format off
  { "black",   COLOR_BLACK },
  { "blue",    COLOR_BLUE },
  { "cyan",    COLOR_CYAN },
  { "green",   COLOR_GREEN },
  { "magenta", COLOR_MAGENTA },
  { "red",     COLOR_RED },
  { "white",   COLOR_WHITE },
  { "yellow",  COLOR_YELLOW },
  { "default", COLOR_DEFAULT },
  { 0, 0 },
  // clang-format on
};

/**
 * AttributeNames - Mapping of attribute names to their IDs
 */
static struct Mapping AttributeNames[] = {
  // clang-format off
  { "bold",      A_BOLD      },
  { "italic",    A_ITALIC    },
  { "none",      A_NORMAL    },
  { "normal",    A_NORMAL    },
  { "reverse",   A_REVERSE   },
  { "standout",  A_STANDOUT  },
  { "underline", A_UNDERLINE },
  { NULL, 0 },
  // clang-format on
};

/**
 * parse_color_prefix - Parse a colour prefix, e.g. "bright"
 * @param[in]  s      String to parse
 * @param[out] prefix parsed prefix, see #ColorPrefix
 * @retval num Length of the matched prefix
 * @retval   0 No prefix matched
 *
 * If prefixes should be parsed, but their value is irrelevant, NULL can be
 * passed as 'prefix'.
 */
int parse_color_prefix(const char *s, enum ColorPrefix *prefix)
{
  if (!s || !prefix)
    return 0;

  int clen = 0;

  if ((clen = mutt_istr_startswith(s, "bright")))
  {
    color_debug(LL_DEBUG5, "bright\n");
    if (prefix)
      *prefix = COLOR_PREFIX_BRIGHT;
  }
  else if ((clen = mutt_istr_startswith(s, "alert")))
  {
    color_debug(LL_DEBUG5, "alert\n");
    if (prefix)
      *prefix = COLOR_PREFIX_ALERT;
  }
  else if ((clen = mutt_istr_startswith(s, "light")))
  {
    color_debug(LL_DEBUG5, "light\n");
    if (prefix)
      *prefix = COLOR_PREFIX_LIGHT;
  }

  return clen;
}

/**
 * parse_color_namedcolor - Parse a named colour, e.g. "brightred"
 * @param[in]  s     String to parse
 * @param[out] elem  Colour element to update
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 */
enum CommandResult parse_color_namedcolor(const char *s, struct ColorElement *elem,
                                          struct Buffer *err)
{
  if (!s || !elem)
    return MUTT_CMD_ERROR;

  // COLOR_DEFAULT (-1) interferes with mutt_map_get_value()
  if (mutt_str_equal(s, "default"))
  {
    elem->color = COLOR_DEFAULT;
    elem->type = CT_SIMPLE;
    elem->prefix = COLOR_PREFIX_NONE;
    return MUTT_CMD_SUCCESS;
  }

  enum ColorPrefix prefix = COLOR_PREFIX_NONE;
  s += parse_color_prefix(s, &prefix);

  int color = mutt_map_get_value(s, ColorNames);
  if (color == -1)
    return MUTT_CMD_WARNING;

  elem->color = color;
  elem->type = CT_SIMPLE;
  elem->prefix = prefix;

  const char *name = mutt_map_get_name(elem->color, ColorNames);
  if (name)
    color_debug(LL_DEBUG5, "color: %s\n", name);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_colornnn - Parse a colorNNN, e.g. "color123".
 * @param[in]  s     String to parse
 * @param[out] elem  Colour element to update
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 * @retval #MUTT_CMD_ERROR   Error, colour could not be parsed
 *
 * On #MUTT_CMD_ERROR, an error message will be written to err.
 */
enum CommandResult parse_color_colornnn(const char *s, struct ColorElement *elem,
                                        struct Buffer *err)
{
  if (!s || !elem)
    return MUTT_CMD_ERROR;

  /* prefixes bright, alert, light are only allowed for named colours and
   * colorNNN for backwards compatibility. */
  enum ColorPrefix prefix = COLOR_PREFIX_NONE;
  s += parse_color_prefix(s, &prefix);

  int clen = 0;
  /* allow aliases for xterm color resources */
  if ((clen = mutt_istr_startswith(s, "color")) == 0)
    return MUTT_CMD_WARNING;

  s += clen;
  char *eptr = NULL;

  unsigned long color = strtoul(s, &eptr, 10);
  /* There are only 256 xterm colors.  Do not confuse with COLORS which is
   * the number of colours the terminal supports (usually one of 16, 256,
   * 16777216 (=24bit)). */
  if ((*s == '\0') || (*eptr != '\0') || (color >= 256))
  {
    buf_printf(err, _("%s: color not supported by term"), s);
    return MUTT_CMD_ERROR;
  }

  elem->color = color;
  elem->type = CT_PALETTE;
  elem->prefix = prefix;

  color_debug(LL_DEBUG5, "colorNNN %d\n", elem->color);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_rrggbb - Parse an RGB colour, e.g. "#12FE45"
 * @param[in]  s     String to parse
 * @param[out] elem  Colour element to update
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 * @retval #MUTT_CMD_ERROR   Error, colour could not be parsed
 *
 * On #MUTT_CMD_ERROR, an error message will be written to err.
 */
enum CommandResult parse_color_rrggbb(const char *s, struct ColorElement *elem,
                                      struct Buffer *err)
{
  if (!s || !elem)
    return MUTT_CMD_ERROR;

  /* parse #RRGGBB colours */
  if (s[0] != '#')
    return MUTT_CMD_WARNING;

  s++;
  char *eptr = NULL;
  unsigned long color = strtoul(s, &eptr, 16);

  if ((*s == '\0') || !eptr || (*eptr != '\0') || ((eptr - s) != 6))
  {
    buf_printf(err, _("%s: color not supported by term"), s);
    return MUTT_CMD_ERROR;
  }

  elem->color = color;
  elem->type = CT_RGB;
  elem->prefix = COLOR_PREFIX_NONE;

  color_debug(LL_DEBUG5, "#RRGGBB: %ld\n", color);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_name - Parse a colour name
 * @param[in]  s     String to parse
 * @param[out] elem  Colour element to update
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Parse a colour name, such as "red", "brightgreen", "color123", "#12FE45"
 */
enum CommandResult parse_color_name(const char *s, struct ColorElement *elem,
                                    struct Buffer *err)
{
  color_debug(LL_DEBUG5, "Parsing color name: %s\n", s);

  /* Try the different colour syntaxes.  A return value of MUTT_CMD_WARNING
   * means, we should try the next syntax. */
  enum CommandResult cr;

  /* #RRGGBB */
  cr = parse_color_rrggbb(s, elem, err);
  if (cr != MUTT_CMD_WARNING)
    return cr;

  /* color123 */
  cr = parse_color_colornnn(s, elem, err);
  if (cr != MUTT_CMD_WARNING)
    return cr;

  /* named color, e.g. "brightred" */
  cr = parse_color_namedcolor(s, elem, err);
  if (cr != MUTT_CMD_WARNING)
    return cr;

  buf_printf(err, _("%s: no such color"), s);
  return MUTT_CMD_WARNING;
}

/**
 * parse_color_pair - Parse a pair of colours - Implements ::parser_callback_t - @ingroup parser_callback_api
 *
 * Parse a pair of colours, e.g. "red default"
 */
enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s,
                                    struct AttrColor *ac, struct Buffer *err)
{
  while (true)
  {
    if (!MoreArgsF(s, TOKEN_COMMENT))
    {
      buf_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    parse_extract_token(buf, s, TOKEN_COMMENT);
    if (buf_is_empty(buf))
      continue;

    int attr = mutt_map_get_value(buf->data, AttributeNames);
    if (attr == -1)
    {
      enum CommandResult rc = parse_color_name(buf->data, &ac->fg, err);
      if (rc != MUTT_CMD_SUCCESS)
        return rc;
      break;
    }

    if (attr == A_NORMAL)
      ac->attrs = attr; // Clear all attributes
    else
      ac->attrs |= attr; // Merge with other attributes
  }

  if (!MoreArgsF(s, TOKEN_COMMENT))
  {
    buf_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_COMMENT);

  return parse_color_name(buf->data, &ac->bg, err);
}

/**
 * parse_attr_spec - Parse an attribute description - Implements ::parser_callback_t - @ingroup parser_callback_api
 */
enum CommandResult parse_attr_spec(struct Buffer *buf, struct Buffer *s,
                                   struct AttrColor *ac, struct Buffer *err)
{
  if (!buf || !s || !ac)
    return MUTT_CMD_ERROR;

  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "mono");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  int attr = mutt_map_get_value(buf->data, AttributeNames);
  if (attr == -1)
  {
    buf_printf(err, _("%s: no such attribute"), buf->data);
    return MUTT_CMD_WARNING;
  }

  if (attr == A_NORMAL)
    ac->attrs = attr; // Clear all attributes
  else
    ac->attrs |= attr; // Merge with other attributes

  return MUTT_CMD_SUCCESS;
}
