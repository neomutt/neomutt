/**
 * @file
 * Parse colour commands
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page color_command Parse colour commands
 *
 * Parse NeoMutt 'color', 'uncolor', 'mono' and 'unmono' commands.
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "globals.h"
#ifdef USE_DEBUG_COLOR
#include <stdio.h>
#include "mutt.h"
#include "pager/private_data.h"
#endif

/**
 * ColorFields - Mapping of colour names to their IDs
 */
const struct Mapping ColorFields[] = {
  // clang-format off
  { "attachment",        MT_COLOR_ATTACHMENT },
  { "attach_headers",    MT_COLOR_ATTACH_HEADERS },
  { "body",              MT_COLOR_BODY },
  { "bold",              MT_COLOR_BOLD },
  { "error",             MT_COLOR_ERROR },
  { "hdrdefault",        MT_COLOR_HDRDEFAULT },
  { "header",            MT_COLOR_HEADER },
  { "index",             MT_COLOR_INDEX },
  { "index_author",      MT_COLOR_INDEX_AUTHOR },
  { "index_collapsed",   MT_COLOR_INDEX_COLLAPSED },
  { "index_date",        MT_COLOR_INDEX_DATE },
  { "index_flags",       MT_COLOR_INDEX_FLAGS },
  { "index_label",       MT_COLOR_INDEX_LABEL },
  { "index_number",      MT_COLOR_INDEX_NUMBER },
  { "index_size",        MT_COLOR_INDEX_SIZE },
  { "index_subject",     MT_COLOR_INDEX_SUBJECT },
  { "index_tag",         MT_COLOR_INDEX_TAG },
  { "index_tags",        MT_COLOR_INDEX_TAGS },
  { "indicator",         MT_COLOR_INDICATOR },
  { "italic",            MT_COLOR_ITALIC },
  { "markers",           MT_COLOR_MARKERS },
  { "message",           MT_COLOR_MESSAGE },
  { "normal",            MT_COLOR_NORMAL },
  { "options",           MT_COLOR_OPTIONS },
  { "progress",          MT_COLOR_PROGRESS },
  { "prompt",            MT_COLOR_PROMPT },
  { "quoted",            MT_COLOR_QUOTED },
  { "search",            MT_COLOR_SEARCH },
#ifdef USE_SIDEBAR
  { "sidebar_background", MT_COLOR_SIDEBAR_BACKGROUND },
  { "sidebar_divider",   MT_COLOR_SIDEBAR_DIVIDER },
  { "sidebar_flagged",   MT_COLOR_SIDEBAR_FLAGGED },
  { "sidebar_highlight", MT_COLOR_SIDEBAR_HIGHLIGHT },
  { "sidebar_indicator", MT_COLOR_SIDEBAR_INDICATOR },
  { "sidebar_new",       MT_COLOR_SIDEBAR_NEW },
  { "sidebar_ordinary",  MT_COLOR_SIDEBAR_ORDINARY },
  { "sidebar_spool_file", MT_COLOR_SIDEBAR_SPOOLFILE },
  { "sidebar_spoolfile", MT_COLOR_SIDEBAR_SPOOLFILE }, // This will be deprecated
  { "sidebar_unread",    MT_COLOR_SIDEBAR_UNREAD },
#endif
  { "signature",         MT_COLOR_SIGNATURE },
  { "status",            MT_COLOR_STATUS },
  { "tilde",             MT_COLOR_TILDE },
  { "tree",              MT_COLOR_TREE },
  { "underline",         MT_COLOR_UNDERLINE },
  { "warning",           MT_COLOR_WARNING },
  { NULL, 0 },
  // clang-format on
};

/**
 * ComposeColorFields - Mapping of compose colour names to their IDs
 */
const struct Mapping ComposeColorFields[] = {
  // clang-format off
  { "header",            MT_COLOR_COMPOSE_HEADER },
  { "security_encrypt",  MT_COLOR_COMPOSE_SECURITY_ENCRYPT },
  { "security_sign",     MT_COLOR_COMPOSE_SECURITY_SIGN },
  { "security_both",     MT_COLOR_COMPOSE_SECURITY_BOTH },
  { "security_none",     MT_COLOR_COMPOSE_SECURITY_NONE },
  { NULL, 0 }
  // clang-format on
};

#ifdef NEOMUTT_DIRECT_COLORS
/**
 * color_xterm256_to_24bit - Convert a xterm color to its RGB value
 * @param[in] color xterm color number to be converted
 * @retval num The color's RGB value as number with value 0xRRGGBB
 *
 * There are 256 xterm colors numbered 0 to 255.
 *
 * Caller contract: color must be between 0 and 255.
 *
 * ## Xterm Color Codes
 * 
 * ### Basic and Bright Colors
 * 
 * - 0-7 correspond to the 8 terminal colours
 * - 8-15 are the bright variants of 0-7
 * 
 * |     |           |     |           |     |           |     |           |     |           |     |           |    |           |    |           |
 * | :-- | :-------- | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :- | :-------  | :- | :-------- |
 * |  0  | `#000000` |  1  | `#800000` |  2  | `#008000` |  3  | `#808000` |  4  | `#000080` |  5  | `#800080` |  6 | `#008080` |  7 | `#c0c0c0` |
 * |  8  | `#808080` |  9  | `#ff0000` | 10  | `#00ff00` | 11  | `#ffff00` | 12  | `#0000ff` | 13  | `#ff00ff` | 14 | `#00ffff` | 15 | `#ffffff` |
 * 
 * ### Color palette
 * 
 * |     |           |     |           |     |           |     |           |     |           |     |           |
 * | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  |
 * |  16 | `#000000` |  17 | `#00005f` |  18 | `#000087` |  19 | `#0000af` |  20 | `#0000d7` |  21 | `#0000ff` |
 * |  22 | `#005f00` |  23 | `#005f5f` |  24 | `#005f87` |  25 | `#005faf` |  26 | `#005fd7` |  27 | `#005fff` |
 * |  28 | `#008700` |  29 | `#00875f` |  30 | `#008787` |  31 | `#0087af` |  32 | `#0087d7` |  33 | `#0087ff` |
 * |  34 | `#00af00` |  35 | `#00af5f` |  36 | `#00af87` |  37 | `#00afaf` |  38 | `#00afd7` |  39 | `#00afff` |
 * |  40 | `#00d700` |  41 | `#00d75f` |  42 | `#00d787` |  43 | `#00d7af` |  44 | `#00d7d7` |  45 | `#00d7ff` |
 * |  46 | `#00ff00` |  47 | `#00ff5f` |  48 | `#00ff87` |  49 | `#00ffaf` |  50 | `#00ffd7` |  51 | `#00ffff` |
 * |  52 | `#5f0000` |  53 | `#5f005f` |  54 | `#5f0087` |  55 | `#5f00af` |  56 | `#5f00d7` |  57 | `#5f00ff` |
 * |  58 | `#5f5f00` |  59 | `#5f5f5f` |  60 | `#5f5f87` |  61 | `#5f5faf` |  62 | `#5f5fd7` |  63 | `#5f5fff` |
 * |  64 | `#5f8700` |  65 | `#5f875f` |  66 | `#5f8787` |  67 | `#5f87af` |  68 | `#5f87d7` |  69 | `#5f87ff` |
 * |  70 | `#5faf00` |  71 | `#5faf5f` |  72 | `#5faf87` |  73 | `#5fafaf` |  74 | `#5fafd7` |  75 | `#5fafff` |
 * |  76 | `#5fd700` |  77 | `#5fd75f` |  78 | `#5fd787` |  79 | `#5fd7af` |  80 | `#5fd7d7` |  81 | `#5fd7ff` |
 * |  82 | `#5fff00` |  83 | `#5fff5f` |  84 | `#5fff87` |  85 | `#5fffaf` |  86 | `#5fffd7` |  87 | `#5fffff` |
 * |  88 | `#870000` |  89 | `#87005f` |  90 | `#870087` |  91 | `#8700af` |  92 | `#8700d7` |  93 | `#8700ff` |
 * |  94 | `#875f00` |  95 | `#875f5f` |  96 | `#875f87` |  97 | `#875faf` |  98 | `#875fd7` |  99 | `#875fff` |
 * | 100 | `#878700` | 101 | `#87875f` | 102 | `#878787` | 103 | `#8787af` | 104 | `#8787d7` | 105 | `#8787ff` |
 * | 106 | `#87af00` | 107 | `#87af5f` | 108 | `#87af87` | 109 | `#87afaf` | 110 | `#87afd7` | 111 | `#87afff` |
 * | 112 | `#87d700` | 113 | `#87d75f` | 114 | `#87d787` | 115 | `#87d7af` | 116 | `#87d7d7` | 117 | `#87d7ff` |
 * | 118 | `#87ff00` | 119 | `#87ff5f` | 120 | `#87ff87` | 121 | `#87ffaf` | 122 | `#87ffd7` | 123 | `#87ffff` |
 * | 124 | `#af0000` | 125 | `#af005f` | 126 | `#af0087` | 127 | `#af00af` | 128 | `#af00d7` | 129 | `#af00ff` |
 * | 130 | `#af5f00` | 131 | `#af5f5f` | 132 | `#af5f87` | 133 | `#af5faf` | 134 | `#af5fd7` | 135 | `#af5fff` |
 * | 136 | `#af8700` | 137 | `#af875f` | 138 | `#af8787` | 139 | `#af87af` | 140 | `#af87d7` | 141 | `#af87ff` |
 * | 142 | `#afaf00` | 143 | `#afaf5f` | 144 | `#afaf87` | 145 | `#afafaf` | 146 | `#afafd7` | 147 | `#afafff` |
 * | 148 | `#afd700` | 149 | `#afd75f` | 150 | `#afd787` | 151 | `#afd7af` | 152 | `#afd7d7` | 153 | `#afd7ff` |
 * | 154 | `#afff00` | 155 | `#afff5f` | 156 | `#afff87` | 157 | `#afffaf` | 158 | `#afffd7` | 159 | `#afffff` |
 * | 160 | `#d70000` | 161 | `#d7005f` | 162 | `#d70087` | 163 | `#d700af` | 164 | `#d700d7` | 165 | `#d700ff` |
 * | 166 | `#d75f00` | 167 | `#d75f5f` | 168 | `#d75f87` | 169 | `#d75faf` | 170 | `#d75fd7` | 171 | `#d75fff` |
 * | 172 | `#d78700` | 173 | `#d7875f` | 174 | `#d78787` | 175 | `#d787af` | 176 | `#d787d7` | 177 | `#d787ff` |
 * | 178 | `#d7af00` | 179 | `#d7af5f` | 180 | `#d7af87` | 181 | `#d7afaf` | 182 | `#d7afd7` | 183 | `#d7afff` |
 * | 184 | `#d7d700` | 185 | `#d7d75f` | 186 | `#d7d787` | 187 | `#d7d7af` | 188 | `#d7d7d7` | 189 | `#d7d7ff` |
 * | 190 | `#d7ff00` | 191 | `#d7ff5f` | 192 | `#d7ff87` | 193 | `#d7ffaf` | 194 | `#d7ffd7` | 195 | `#d7ffff` |
 * | 196 | `#ff0000` | 197 | `#ff005f` | 198 | `#ff0087` | 199 | `#ff00af` | 200 | `#ff00d7` | 201 | `#ff00ff` |
 * | 202 | `#ff5f00` | 203 | `#ff5f5f` | 204 | `#ff5f87` | 205 | `#ff5faf` | 206 | `#ff5fd7` | 207 | `#ff5fff` |
 * | 208 | `#ff8700` | 209 | `#ff875f` | 210 | `#ff8787` | 211 | `#ff87af` | 212 | `#ff87d7` | 213 | `#ff87ff` |
 * | 214 | `#ffaf00` | 215 | `#ffaf5f` | 216 | `#ffaf87` | 217 | `#ffafaf` | 218 | `#ffafd7` | 219 | `#ffafff` |
 * | 220 | `#ffd700` | 221 | `#ffd75f` | 222 | `#ffd787` | 223 | `#ffd7af` | 224 | `#ffd7d7` | 225 | `#ffd7ff` |
 * | 226 | `#ffff00` | 227 | `#ffff5f` | 228 | `#ffff87` | 229 | `#ffffaf` | 230 | `#ffffd7` | 231 | `#ffffff` |
 * 
 * ### Grey Scale Ramp
 * 
 * |     |           |     |           |     |           |     |           |     |           |     |           |     |           |     |           |
 * | :-- | :-------- | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------  | :-- | :-------- |
 * | 232 | `#080808` | 233 | `#121212` | 234 | `#1c1c1c` | 235 | `#262626` | 236 | `#303030` | 237 | `#3a3a3a` | 238 | `#444444` | 239 | `#4e4e4e` |
 * | 240 | `#585858` | 241 | `#606060` | 242 | `#666666` | 243 | `#767676` | 244 | `#808080` | 245 | `#8a8a8a` | 246 | `#949494` | 247 | `#9e9e9e` |
 * | 248 | `#a8a8a8` | 249 | `#b2b2b2` | 250 | `#bcbcbc` | 251 | `#c6c6c6` | 252 | `#d0d0d0` | 253 | `#dadada` | 254 | `#e4e4e4` | 255 | `#eeeeee` |
 */
static uint32_t color_xterm256_to_24bit(const uint32_t color)
{
  static const uint32_t basic[] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
    0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00, 0xffff00,
    0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
  };

  assert(color < 256);

  if (color < 16)
  {
    color_debug(LL_DEBUG5, "Converted color 0-15: %d\n", color);
    /* The first 16 colours are the "usual" terminal colours */
    return basic[color];
  }

  if (color < 232)
  {
    /* The Color palette is divided in 6x6x6 colours, i.e. each R, G, B channel
     * has six values:
     *
     *  value: 1     2     3     4     5     6
     *  color: 0x00  0x5f  0x87  0xaf  0xd7  0xff
     *
     * The steps between the values is 0x28 = 40, the EXCEPT for the first one
     * where it is 0x5f = 95.
     *
     * If we express the xterm color number minus 16 to base 6, i.e.
     *
     *    color - 16 = vr * 36 + vg * 6 + vb * 1
     *
     * with vr, vg, vb integers between 0 and 5, then vr, vg, vb is the channel
     * value for red, green, and blue, respectively.
     */

    uint32_t normalised_color = color - 16;
    uint32_t vr = (normalised_color % 216) / 36; /* 216 = 6*6*6 */
    uint32_t vg = (normalised_color % 36) / 6;
    uint32_t vb = (normalised_color % 6) / 1;

    /* First step is wider than the other ones, so add the difference if needed */
    uint32_t r = vr * 0x28 + ((vr > 0) ? (0x5f - 0x40) : 0);
    uint32_t g = vg * 0x28 + ((vg > 0) ? (0x5f - 0x40) : 0);
    uint32_t b = vb * 0x28 + ((vb > 0) ? (0x5f - 0x40) : 0);

    uint32_t rgb = (r << 16) + (g << 8) + (b << 0);
    color_debug(LL_DEBUG5, "Converted xterm color %d to RGB #%x:\n", color, rgb);
    return rgb;
  }

  /* Grey scale starts at 0x08 and adds 0xa = 10 in very step ending in 0xee.
   * There are a total of 6*4 = 24 grey colors in total. */
  uint32_t steps = color - 232;
  uint32_t grey = (steps * 0x0a) + 0x08;
  uint32_t rgb = (grey << 16) + (grey << 8) + (grey << 0);
  color_debug(LL_DEBUG5, "Converted xterm color %d to RGB #%x:\n", color, rgb);
  return rgb;
}
#endif

/**
 * parse_color_name - Parse a colour name
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attributes, e.g. A_UNDERLINE
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Parse a colour name, such as "red", "brightgreen", "color123".
 */
static enum CommandResult parse_color_name(const char *s, uint32_t *col, int *attrs,
                                           bool is_fg, struct Buffer *err)
{
  int clen;

  mutt_debug(LL_DEBUG5, "Parsing color name: %s\n", s);

  /* allow aliases for xterm color resources */
  if ((clen = mutt_istr_startswith(s, "color")))
  {
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
#ifdef NEOMUTT_DIRECT_COLORS
    const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
    if (c_color_directcolor)
    {
      /* If we are running in direct color mode, we must convert the xterm
       * color numbers 0-255 to an RGB value. */
      *col = color_xterm256_to_24bit(*col);
    }
#endif
    color_debug(LL_DEBUG5, "colorNNN %d\n", *col);
    return MUTT_CMD_SUCCESS;
  }

  /* parse #RRGGBB colours */
  if (s[0] == '#')
  {
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
    color_debug(LL_DEBUG5, "#RRGGBB: %d\n", *col);
    return MUTT_CMD_SUCCESS;
  }

  /* A named colour, e.g. 'brightred' */
  /* prefixes bright, alert, light are only allowed for named colours */
  bool is_alert = false;
  bool is_bright = false;
  bool is_light = false;
  if ((clen = mutt_istr_startswith(s, "bright")))
  {
    color_debug(LL_DEBUG5, "bright\n");
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_istr_startswith(s, "alert")))
  {
    color_debug(LL_DEBUG5, "alert\n");
    is_alert = true;
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_istr_startswith(s, "light")))
  {
    color_debug(LL_DEBUG5, "light\n");
    is_light = true;
    s += clen;
  }
  if ((*col = mutt_map_get_value(s, ColorNames)) != -1)
  {
    const char *name = mutt_map_get_name(*col, ColorNames);
    if (name)
      color_debug(LL_DEBUG5, "color: %s\n", name);

    if (is_bright || is_light)
    {
      if (is_alert)
      {
        *attrs |= A_BOLD;
        *attrs |= A_BLINK;
      }
      else if (is_fg)
      {
        if ((COLORS >= 16) && is_light)
        {
          if (*col <= 7)
          {
            /* Advance the color 0-7 by 8 to get the light version */
            *col += 8;
          }
        }
        else
        {
          *attrs |= A_BOLD;
        }
      }
      else
      {
        if (COLORS >= 16)
        {
          if (*col <= 7)
          {
            /* Advance the color 0-7 by 8 to get the light version */
            *col += 8;
          }
        }
      }
    }
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
    if (c_color_directcolor && (0 <= *col) && (*col < 16))
    {
      *col = color_xterm256_to_24bit(*col);
    }
#endif
    return MUTT_CMD_SUCCESS;
  }

  /* sanity check for the future */
  if (is_bright || is_alert || is_light)
  {
    buf_printf(err, _("'bright', 'alert', 'light' are only allowed for named colors: %s"), s);
    return MUTT_CMD_ERROR;
  }

  buf_printf(err, _("%s: no such color"), s);
  return MUTT_CMD_WARNING;
}

/**
 * parse_attr_spec - Parse an attribute description - Implements ::parser_callback_t - @ingroup parser_callback_api
 */
static enum CommandResult parse_attr_spec(struct Buffer *buf, struct Buffer *s,
                                          uint32_t *fg, uint32_t *bg,
                                          int *attrs, struct Buffer *err)
{
  if (fg)
    *fg = COLOR_UNSET;
  if (bg)
    *bg = COLOR_UNSET;

  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "mono");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  if (mutt_istr_equal("bold", buf->data))
    *attrs |= A_BOLD;
  else if (mutt_istr_equal("italic", buf->data))
    *attrs |= A_ITALIC;
  else if (mutt_istr_equal("none", buf->data))
    *attrs = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("normal", buf->data))
    *attrs = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("reverse", buf->data))
    *attrs |= A_REVERSE;
  else if (mutt_istr_equal("standout", buf->data))
    *attrs |= A_STANDOUT;
  else if (mutt_istr_equal("underline", buf->data))
    *attrs |= A_UNDERLINE;
  else
  {
    buf_printf(err, _("%s: no such attribute"), buf->data);
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_pair - Parse a pair of colours - Implements ::parser_callback_t - @ingroup parser_callback_api
 *
 * Parse a pair of colours, e.g. "red default"
 */
static enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s,
                                           uint32_t *fg, uint32_t *bg,
                                           int *attrs, struct Buffer *err)
{
  while (true)
  {
    if (!MoreArgs(s))
    {
      buf_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

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

  if (!MoreArgs(s))
  {
    buf_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  return parse_color_name(buf->data, bg, attrs, false, err);
}

/**
 * get_colorid_name - Get the name of a color id
 * @param cid Colour, e.g. #MT_COLOR_HEADER
 * @param buf Buffer for result
 */
void get_colorid_name(unsigned int cid, struct Buffer *buf)
{
  const char *name = NULL;

  if ((cid >= MT_COLOR_COMPOSE_HEADER) && (cid <= MT_COLOR_COMPOSE_SECURITY_SIGN))
  {
    name = mutt_map_get_name(cid, ComposeColorFields);
    if (name)
    {
      buf_printf(buf, "compose %s", name);
      return;
    }
  }

  name = mutt_map_get_name(cid, ColorFields);
  if (name)
    buf_printf(buf, "%s", name);
  else
    buf_printf(buf, "UNKNOWN %d", cid);
}

/**
 * parse_object - Identify a colour object
 * @param[in]  buf   Temporary Buffer space
 * @param[in]  s     Buffer containing string to be parsed
 * @param[out] cid   Object type, e.g. #MT_COLOR_TILDE
 * @param[out] ql    Quote level, if type #MT_COLOR_QUOTED
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Identify a colour object, e.g. "quoted", "compose header"
 */
static enum CommandResult parse_object(struct Buffer *buf, struct Buffer *s,
                                       enum ColorId *cid, int *ql, struct Buffer *err)
{
  int rc;

  if (mutt_str_startswith(buf->data, "quoted") != 0)
  {
    int val = 0;
    if (buf->data[6] != '\0')
    {
      if (!mutt_str_atoi_full(buf->data + 6, &val) || (val > COLOR_QUOTES_MAX))
      {
        buf_printf(err, _("%s: no such object"), buf->data);
        return MUTT_CMD_WARNING;
      }
    }

    *ql = val;
    *cid = MT_COLOR_QUOTED;
    return MUTT_CMD_SUCCESS;
  }

  if (mutt_istr_equal(buf->data, "compose"))
  {
    if (!MoreArgs(s))
    {
      buf_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

    rc = mutt_map_get_value(buf->data, ComposeColorFields);
    if (rc == -1)
    {
      buf_printf(err, _("%s: no such object"), buf->data);
      return MUTT_CMD_WARNING;
    }

    *cid = rc;
    return MUTT_CMD_SUCCESS;
  }

  rc = mutt_map_get_value(buf->data, ColorFields);
  if (rc == -1)
  {
    buf_printf(err, _("%s: no such object"), buf->data);
    return MUTT_CMD_WARNING;
  }
  else
  {
    color_debug(LL_DEBUG5, "object: %s\n", mutt_map_get_name(rc, ColorFields));
  }

  *cid = rc;
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_uncolor - Parse an 'uncolor' command
 * @param buf     Temporary Buffer space
 * @param s       Buffer containing string to be parsed
 * @param err     Buffer for error messages
 * @param uncolor If true, 'uncolor', else 'unmono'
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Usage:
 * * uncolor index pattern [pattern...]
 * * unmono  index pattern [pattern...]
 */
static enum CommandResult parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                        struct Buffer *err, bool uncolor)
{
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);

  if (mutt_str_equal(buf->data, "*"))
  {
    colors_clear();
    return MUTT_CMD_SUCCESS;
  }

  unsigned int cid = MT_COLOR_NONE;
  int ql = 0;
  color_debug(LL_DEBUG5, "uncolor: %s\n", buf_string(buf));
  enum CommandResult rc = parse_object(buf, s, &cid, &ql, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  if (cid == -1)
  {
    buf_printf(err, _("%s: no such object"), buf->data);
    return MUTT_CMD_ERROR;
  }

  if (cid == MT_COLOR_QUOTED)
  {
    color_debug(LL_DEBUG5, "quoted\n");
    return quoted_colors_parse_uncolor(cid, ql, err);
  }

  if ((cid == MT_COLOR_STATUS) && !MoreArgs(s))
  {
    color_debug(LL_DEBUG5, "simple\n");
    simple_color_reset(cid); // default colour for the status bar
    return MUTT_CMD_SUCCESS;
  }

  if (!mutt_color_has_pattern(cid))
  {
    color_debug(LL_DEBUG5, "simple\n");
    simple_color_reset(cid);
    return MUTT_CMD_SUCCESS;
  }

  if (OptNoCurses)
  {
    do
    {
      color_debug(LL_DEBUG5, "do nothing\n");
      /* just eat the command, but don't do anything real about it */
      parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    } while (MoreArgs(s));

    return MUTT_CMD_SUCCESS;
  }

  bool changes = false;
  if (!MoreArgs(s))
  {
    if (regex_colors_parse_uncolor(cid, NULL, uncolor))
      return MUTT_CMD_SUCCESS;
    else
      return MUTT_CMD_ERROR;
  }

  do
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      if (regex_colors_parse_uncolor(cid, NULL, uncolor))
        return MUTT_CMD_SUCCESS;
      else
        return MUTT_CMD_ERROR;
    }

    changes |= regex_colors_parse_uncolor(cid, buf->data, uncolor);

  } while (MoreArgs(s));

  if (changes)
    regex_colors_dump_all();

  return MUTT_CMD_SUCCESS;
}

#ifdef USE_DEBUG_COLOR
/**
 * color_dump - Parse 'color' command to display colours - Implements ICommand::parse()
 */
static enum CommandResult color_dump(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  if (MoreArgs(s))
    return MUTT_CMD_ERROR;

  FILE *fp_out = NULL;
  char tempfile[PATH_MAX] = { 0 };
  struct Buffer filebuf = buf_make(4096);
  char color_fg[32] = { 0 };
  char color_bg[32] = { 0 };

  mutt_mktemp(tempfile, sizeof(tempfile));
  fp_out = mutt_file_fopen(tempfile, "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), tempfile);
    buf_dealloc(&filebuf);
    return MUTT_CMD_ERROR;
  }

  buf_addstr(&filebuf, "# All Colours\n\n");
  buf_addstr(&filebuf, "# Simple Colours\n");
  for (enum ColorId cid = MT_COLOR_NONE + 1; cid < MT_COLOR_MAX; cid++)
  {
    struct AttrColor *ac = simple_color_get(cid);
    if (!ac)
      continue;

    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    const char *name = mutt_map_get_name(cid, ColorFields);
    if (!name)
      continue;

    const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
    buf_add_printf(&filebuf, "color %-18s %-30s %-8s %-8s # %s\n", name,
                   color_debug_log_attrs_list(ac->attrs),
                   color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                   color_debug_log_name(color_bg, sizeof(color_bg), cc->bg), swatch);
  }

  if (NumQuotedColors > 0)
  {
    buf_addstr(&filebuf, "\n# Quoted Colours\n");
    for (int i = 0; i < NumQuotedColors; i++)
    {
      struct AttrColor *ac = quoted_colors_get(i);
      if (!ac)
        continue;

      struct CursesColor *cc = ac->curses_color;
      if (!cc)
        continue;

      const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
      buf_add_printf(&filebuf, "color quoted%d %-30s %-8s %-8s # %s\n", i,
                     color_debug_log_attrs_list(ac->attrs),
                     color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                     color_debug_log_name(color_bg, sizeof(color_bg), cc->bg), swatch);
    }
  }

  int rl_count = 0;
  for (enum ColorId id = MT_COLOR_NONE; id != MT_COLOR_MAX; ++id)
  {
    if (!mutt_color_has_pattern(id))
    {
      continue;
    }

    struct RegexColorList *rcl = regex_colors_get_list(id);
    if (!STAILQ_EMPTY(rcl))
      rl_count++;
  }

  if (rl_count > 0)
  {
    for (enum ColorId id = MT_COLOR_NONE; id != MT_COLOR_MAX; ++id)
    {
      if (!mutt_color_has_pattern(id))
      {
        continue;
      }

      struct RegexColorList *rcl = regex_colors_get_list(id);
      if (STAILQ_EMPTY(rcl))
        continue;

      const char *name = mutt_map_get_name(id, ColorFields);
      if (!name)
        continue;

      buf_add_printf(&filebuf, "\n# Regex Colour %s\n", name);

      struct RegexColor *rc = NULL;
      STAILQ_FOREACH(rc, rcl, entries)
      {
        struct AttrColor *ac = &rc->attr_color;
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
        buf_add_printf(&filebuf, "color %-14s %-30s %-8s %-8s %-30s # %s\n",
                       name, color_debug_log_attrs_list(ac->attrs),
                       color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                       color_debug_log_name(color_bg, sizeof(color_bg), cc->bg),
                       rc->pattern, swatch);
      }
    }
  }

#ifdef USE_DEBUG_COLOR
  if (!TAILQ_EMPTY(&MergedColors))
  {
    buf_addstr(&filebuf, "\n# Merged Colours\n");
    struct AttrColor *ac = NULL;
    TAILQ_FOREACH(ac, &MergedColors, entries)
    {
      struct CursesColor *cc = ac->curses_color;
      if (!cc)
        continue;

      const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
      buf_add_printf(&filebuf, "# %-30s %-8s %-8s # %s\n",
                     color_debug_log_attrs_list(ac->attrs),
                     color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                     color_debug_log_name(color_bg, sizeof(color_bg), cc->bg), swatch);
    }
  }

  struct MuttWindow *win = window_get_focus();
  if (win && (win->type == WT_CUSTOM) && win->parent && (win->parent->type == WT_PAGER))
  {
    struct PagerPrivateData *priv = win->parent->wdata;
    if (priv && !TAILQ_EMPTY(&priv->ansi_list))
    {
      buf_addstr(&filebuf, "\n# Ansi Colours\n");
      struct AttrColor *ac = NULL;
      TAILQ_FOREACH(ac, &priv->ansi_list, entries)
      {
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        const char *swatch = color_debug_log_color_attrs(cc->fg, cc->bg, ac->attrs);
        buf_add_printf(&filebuf, "# %-30s %-8s %-8s # %s\n",
                       color_debug_log_attrs_list(ac->attrs),
                       color_debug_log_name(color_fg, sizeof(color_fg), cc->fg),
                       color_debug_log_name(color_bg, sizeof(color_bg), cc->bg), swatch);
      }
    }
  }
#endif

  fputs(filebuf.data, fp_out);

  mutt_file_fclose(&fp_out);
  buf_dealloc(&filebuf);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = tempfile;

  pview.banner = "color";
  pview.flags = MUTT_SHOWCOLOR;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  return MUTT_CMD_SUCCESS;
}
#endif

/**
 * parse_color - Parse a 'color' command
 * @param buf      Temporary Buffer space
 * @param s        Buffer containing string to be parsed
 * @param err      Buffer for error messages
 * @param callback Function to handle command - Implements ::parser_callback_t
 * @param dry_run  If true, test the command, but don't apply it
 * @param color    If true "color", else "mono"
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Usage:
 * * color OBJECT [ ATTRS ] FG BG [ REGEX ]
 * * mono  OBJECT   ATTRS         [ REGEX ]
 */
static enum CommandResult parse_color(struct Buffer *buf, struct Buffer *s,
                                      struct Buffer *err, parser_callback_t callback,
                                      bool dry_run, bool color)
{
  int attrs = 0, q_level = 0;
  uint32_t fg = 0, bg = 0, match = 0;
  enum ColorId cid = MT_COLOR_NONE;
  enum CommandResult rc;

  if (!MoreArgs(s))
  {
#ifdef USE_DEBUG_COLOR
    if (StartupComplete)
      return color_dump(buf, s, 0, err);
#endif

    buf_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  color_debug(LL_DEBUG5, "color: %s\n", buf_string(buf));

  rc = parse_object(buf, s, &cid, &q_level, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  rc = callback(buf, s, &fg, &bg, &attrs, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  /* extract a regular expression if needed */

  if (mutt_color_has_pattern(cid) && cid != MT_COLOR_STATUS)
  {
    color_debug(LL_DEBUG5, "regex needed\n");
    if (MoreArgs(s))
    {
      parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    }
    else
    {
      buf_strcpy(buf, ".*");
    }
  }

  if (MoreArgs(s) && (cid != MT_COLOR_STATUS))
  {
    buf_printf(err, _("%s: too many arguments"), color ? "color" : "mono");
    return MUTT_CMD_WARNING;
  }

  if (dry_run)
  {
    color_debug(LL_DEBUG5, "dry_run bailout\n");
    *s->dptr = '\0'; /* fake that we're done parsing */
    return MUTT_CMD_SUCCESS;
  }

  /* The case of the tree object is special, because a non-default fg color of
   * the tree element may be combined dynamically with the default bg color of
   * an index line, not necessarily defined in a rc file.  */
  if (!OptNoCurses &&
      ((fg == COLOR_DEFAULT) || (bg == COLOR_DEFAULT) || (cid == MT_COLOR_TREE)) &&
      (use_default_colors() != OK))
  {
    buf_strcpy(err, _("default colors not supported"));
    return MUTT_CMD_ERROR;
  }

  if (regex_colors_parse_color_list(cid, buf->data, fg, bg, attrs, &rc, err))
  {
    color_debug(LL_DEBUG5, "regex_colors_parse_color_list done\n");
    return rc;
    // do nothing
  }
  else if (quoted_colors_parse_color(cid, fg, bg, attrs, q_level, &rc, err))
  {
    color_debug(LL_DEBUG5, "quoted_colors_parse_color done\n");
    return rc;
    // do nothing
  }
  else if ((cid == MT_COLOR_STATUS) && MoreArgs(s))
  {
    color_debug(LL_DEBUG5, "status\n");
    /* 'color status fg bg' can have up to 2 arguments:
     * 0 arguments: sets the default status color (handled below by else part)
     * 1 argument : colorize pattern on match
     * 2 arguments: colorize nth submatch of pattern */
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

    if (MoreArgs(s))
    {
      struct Buffer tmp = buf_make(0);
      parse_extract_token(&tmp, s, TOKEN_NO_FLAGS);
      if (!mutt_str_atoui_full(tmp.data, &match))
      {
        buf_printf(err, _("%s: invalid number: %s"), color ? "color" : "mono", tmp.data);
        buf_dealloc(&tmp);
        return MUTT_CMD_WARNING;
      }
      buf_dealloc(&tmp);
    }

    if (MoreArgs(s))
    {
      buf_printf(err, _("%s: too many arguments"), color ? "color" : "mono");
      return MUTT_CMD_WARNING;
    }

    rc = regex_colors_parse_status_list(cid, buf->data, fg, bg, attrs, match, err);
    return rc;
  }
  else // Remaining simple colours
  {
    color_debug(LL_DEBUG5, "simple\n");
    if (simple_color_set(cid, fg, bg, attrs))
      rc = MUTT_CMD_SUCCESS;
    else
      rc = MUTT_CMD_ERROR;
  }

  if (rc == MUTT_CMD_SUCCESS)
  {
    get_colorid_name(cid, buf);
    color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf->data);
    struct EventColor ev_c = { cid, NULL };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);
  }

  return rc;
}

/**
 * mutt_parse_uncolor - Parse the 'uncolor' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  if (OptNoCurses)
  {
    *s->dptr = '\0'; /* fake that we're done parsing */
    return MUTT_CMD_SUCCESS;
  }
  color_debug(LL_DEBUG5, "parse: %s\n", buf_string(buf));
  enum CommandResult rc = parse_uncolor(buf, s, err, true);
  // simple_colors_dump(false);
  curses_colors_dump();
  return rc;
}

/**
 * mutt_parse_unmono - Parse the 'unmono' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_unmono(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  *s->dptr = '\0'; /* fake that we're done parsing */
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_color - Parse the 'color' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_color(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  bool dry_run = OptNoCurses;

  color_debug(LL_DEBUG5, "parse: %s\n", buf_string(buf));
  enum CommandResult rc = parse_color(buf, s, err, parse_color_pair, dry_run, true);
  // simple_colors_dump(false);
  curses_colors_dump();
  return rc;
}

/**
 * mutt_parse_mono - Parse the 'mono' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_mono(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  return parse_color(buf, s, err, parse_attr_spec, true, false);
}
