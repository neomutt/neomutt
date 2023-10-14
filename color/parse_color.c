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
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "parse_color.h"
#include "parse/lib.h"
#include "attr.h"
#include "color.h"
#include "curses2.h"
#include "debug.h"
#include "globals.h"

color_t color_xterm256_to_24bit(const color_t color);
void modify_color_by_prefix(enum ColorPrefix prefix, bool is_fg, color_t *col, int *attrs);

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
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attributes, e.g. A_UNDERLINE
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 */
enum CommandResult parse_color_namedcolor(const char *s, color_t *col, int *attrs,
                                          bool is_fg, struct Buffer *err)
{
  enum ColorPrefix prefix = COLOR_PREFIX_NONE;
  s += parse_color_prefix(s, &prefix);

  // COLOR_DEFAULT (-1) interferes with mutt_map_get_value()
  if (mutt_str_equal(s, "default"))
  {
    *col = COLOR_DEFAULT;
  }
  else if ((*col = mutt_map_get_value(s, ColorNames)) == -1)
  {
    return MUTT_CMD_WARNING;
  }

  const char *name = mutt_map_get_name(*col, ColorNames);
  if (name)
    color_debug(LL_DEBUG5, "color: %s\n", name);

  modify_color_by_prefix(prefix, is_fg, col, attrs);

#ifdef NEOMUTT_DIRECT_COLORS
  /* If we are running in direct color mode, we must convert the color
   * number 0-15 to an RGB value.
   * The first 16 colours of the xterm palette correspond to the terminal
   * colours. Note that this replace the colour with a predefined RGB value
   * and not the RGB value the terminal configured to use.
   *
   * Note that some colors are "special" e.g. "default" and do not fall in
   * the range from 0 to 15.  These must not be converted.
   */
  const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
  if (c_color_directcolor && (*col < 16))
  {
    *col = color_xterm256_to_24bit(*col);
  }
#endif
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_colornnn - Parse a colorNNN, e.g. "color123".
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attributes, e.g. A_UNDERLINE
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 * @retval #MUTT_CMD_ERROR   Error, colour could not be parsed
 *
 * On #MUTT_CMD_ERROR, an error message will be written to err.
 */
enum CommandResult parse_color_colornnn(const char *s, color_t *col, int *attrs,
                                        bool is_fg, struct Buffer *err)
{
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
  *col = strtoul(s, &eptr, 10);
  /* There are only 256 xterm colors.  Do not confuse with COLORS which is
   * the number of colours the terminal supports (usually one of 16, 256,
   * 16777216 (=24bit)). */
  if ((*s == '\0') || (*eptr != '\0') || (*col >= 256) || ((*col >= COLORS) && !OptNoCurses))
  {
    buf_printf(err, _("%s: color not supported by term"), s);
    return MUTT_CMD_ERROR;
  }

  modify_color_by_prefix(prefix, is_fg, col, attrs);

#ifdef NEOMUTT_DIRECT_COLORS
  const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
  if (c_color_directcolor)
  {
    /* If we are running in direct color mode, we must convert the xterm
     * color numbers 0-255 to an RGB value. */
    *col = color_xterm256_to_24bit(*col);
    /* FIXME: The color values 0 to 7 (both inclusive) are still occupied by
     * the default terminal colours.  As a workaround we round them up to
     * #000008 which is the blackest black we can produce. */
    if (*col < 8)
      *col = 8;
  }
#endif
  color_debug(LL_DEBUG5, "colorNNN %d\n", *col);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_rrggbb - Parse an RGB colour, e.g. "#12FE45"
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attributes, e.g. A_UNDERLINE
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Colour parsed successfully
 * @retval #MUTT_CMD_WARNING Unknown colour, try other parsers
 * @retval #MUTT_CMD_ERROR   Error, colour could not be parsed
 *
 * On #MUTT_CMD_ERROR, an error message will be written to err.
 */
enum CommandResult parse_color_rrggbb(const char *s, color_t *col, int *attrs,
                                      bool is_fg, struct Buffer *err)
{
  /* parse #RRGGBB colours */
  if (s[0] != '#')
    return MUTT_CMD_WARNING;

#ifndef NEOMUTT_DIRECT_COLORS
  buf_printf(err, _("Direct colors support not compiled in: %s"), s);
  return MUTT_CMD_ERROR;
#endif
  const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
  if (!c_color_directcolor)
  {
    buf_printf(err, _("Direct colors support disabled: %s"), s);
    return MUTT_CMD_ERROR;
  }
  s++;
  char *eptr = NULL;
  *col = strtoul(s, &eptr, 16);
  if ((*s == '\0') || (*eptr != '\0') || ((*col >= COLORS) && !OptNoCurses))
  {
    buf_printf(err, _("%s: color not supported by term"), s);
    return MUTT_CMD_ERROR;
  }
  /* FIXME: The color values 0 to 7 (both inclusive) are still occupied by
   * the default terminal colours.  As a workaround we round them up to
   * #000008 which is the blackest black we can produce. */
  if (*col < 8)
    *col = 8;

  color_debug(LL_DEBUG5, "#RRGGBB: %d\n", *col);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_name - Parse a colour name
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attributes, e.g. A_UNDERLINE
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Parse a colour name, such as "red", "brightgreen", "color123", "#12FE45"
 */
enum CommandResult parse_color_name(const char *s, color_t *col, int *attrs,
                                    bool is_fg, struct Buffer *err)
{
  mutt_debug(LL_DEBUG5, "Parsing color name: %s\n", s);

  /* Try the different colour syntaxes.  A return value of MUTT_CMD_WARNING
   * means, we should try the next syntax. */
  enum CommandResult cr;

  /* #RRGGBB */
  cr = parse_color_rrggbb(s, col, attrs, is_fg, err);
  if (cr != MUTT_CMD_WARNING)
    return cr;

  /* color123 */
  cr = parse_color_colornnn(s, col, attrs, is_fg, err);
  if (cr != MUTT_CMD_WARNING)
    return cr;

  /* named color, e.g. "brightred" */
  cr = parse_color_namedcolor(s, col, attrs, is_fg, err);
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
enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s, color_t *fg,
                                    color_t *bg, int *attrs, struct Buffer *err)
{
  while (true)
  {
    if (!MoreArgsF(s, TOKEN_COMMENT))
    {
      buf_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    parse_extract_token(buf, s, TOKEN_COMMENT);

    if (mutt_istr_equal("bold", buf->data))
    {
      *attrs |= A_BOLD;
      color_debug(LL_DEBUG5, "bold\n");
    }
    else if (mutt_istr_equal("italic", buf->data))
    {
      *attrs |= A_ITALIC;
      color_debug(LL_DEBUG5, "italic\n");
    }
    else if (mutt_istr_equal("none", buf->data))
    {
      *attrs = A_NORMAL; // Use '=' to clear other bits
      color_debug(LL_DEBUG5, "none\n");
    }
    else if (mutt_istr_equal("normal", buf->data))
    {
      *attrs = A_NORMAL; // Use '=' to clear other bits
      color_debug(LL_DEBUG5, "normal\n");
    }
    else if (mutt_istr_equal("reverse", buf->data))
    {
      *attrs |= A_REVERSE;
      color_debug(LL_DEBUG5, "reverse\n");
    }
    else if (mutt_istr_equal("standout", buf->data))
    {
      *attrs |= A_STANDOUT;
      color_debug(LL_DEBUG5, "standout\n");
    }
    else if (mutt_istr_equal("underline", buf->data))
    {
      *attrs |= A_UNDERLINE;
      color_debug(LL_DEBUG5, "underline\n");
    }
    else
    {
      enum CommandResult rc = parse_color_name(buf->data, fg, attrs, true, err);
      if (rc != MUTT_CMD_SUCCESS)
        return rc;
      break;
    }
  }

  if (!MoreArgsF(s, TOKEN_COMMENT))
  {
    buf_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_COMMENT);

  return parse_color_name(buf->data, bg, attrs, false, err);
}
