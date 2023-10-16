/**
 * @file
 * Colour and attributes
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
 * @page color_attr Colour and attributes
 *
 * The colour and attributes of a graphical object are represented by an
 * AttrColor.
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "attr.h"
#include "color.h"
#include "curses2.h"
#include "debug.h"

/**
 * attr_color_clear - Free the contents of an AttrColor
 * @param ac AttrColor to empty
 *
 * @note The AttrColor object isn't freed
 */
void attr_color_clear(struct AttrColor *ac)
{
  if (!ac)
    return;

  if (ac->curses_color)
    color_debug(LL_DEBUG5, "clear %p\n", (void *) ac);
  curses_color_free(&ac->curses_color);

  memset(&ac->fg, 0, sizeof(ac->fg));
  memset(&ac->bg, 0, sizeof(ac->bg));

  ac->fg.color = COLOR_DEFAULT;
  ac->bg.color = COLOR_DEFAULT;
  ac->attrs = A_NORMAL;
}

/**
 * attr_color_free - Free an AttrColor
 * @param ptr AttrColor to free
 */
void attr_color_free(struct AttrColor **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AttrColor *ac = *ptr;
  if (ac->ref_count > 1)
  {
    ac->ref_count--;
    *ptr = NULL;
    return;
  }

  attr_color_clear(ac);
  FREE(ptr);
}

/**
 * attr_color_new - Create a new AttrColor
 * @retval ptr New AttrColor
 */
struct AttrColor *attr_color_new(void)
{
  struct AttrColor *ac = mutt_mem_calloc(1, sizeof(*ac));

  ac->fg.color = COLOR_DEFAULT;
  ac->fg.type = CT_SIMPLE;
  ac->fg.prefix = COLOR_PREFIX_NONE;

  ac->bg.color = COLOR_DEFAULT;
  ac->bg.type = CT_SIMPLE;
  ac->bg.prefix = COLOR_PREFIX_NONE;

  ac->attrs = A_NORMAL;

  ac->ref_count = 1;

  return ac;
}

/**
 * attr_color_list_clear - Free the contents of an AttrColorList
 * @param acl List to clear
 *
 * Free each of the AttrColors in a list.
 *
 * @note The list object isn't freed, only emptied
 */
void attr_color_list_clear(struct AttrColorList *acl)
{
  if (!acl)
    return;

  struct AttrColor *ac = NULL;
  struct AttrColor *tmp = NULL;
  TAILQ_FOREACH_SAFE(ac, acl, entries, tmp)
  {
    TAILQ_REMOVE(acl, ac, entries);
    attr_color_free(&ac);
  }
}

/**
 * attr_color_list_find - Find an AttrColor in a list
 * @param acl   List to search
 * @param fg    Foreground colour
 * @param bg    Background colour
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Matching AttrColor
 */
struct AttrColor *attr_color_list_find(struct AttrColorList *acl, color_t fg,
                                       color_t bg, int attrs)
{
  if (!acl)
    return NULL;

  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, acl, entries)
  {
    if (ac->attrs != attrs)
      continue;

    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    if ((cc->fg == fg) && (cc->bg == bg))
      return ac;
  }
  return NULL;
}

/**
 * attr_color_copy - Copy a colour
 * @param ac Colour to copy
 * @retval obj Copy of the colour
 */
struct AttrColor attr_color_copy(const struct AttrColor *ac)
{
  struct AttrColor copy = { 0 };
  if (ac)
    copy = *ac;

  return copy;
}

/**
 * attr_color_is_set - Is the object coloured?
 * @param ac Colour to check
 * @retval true Yes, a 'color' command has been used on this object
 */
bool attr_color_is_set(const struct AttrColor *ac)
{
  if (!ac)
    return false;

  return ((ac->attrs != A_NORMAL) || ac->curses_color);
}

/**
 * attr_color_match - Do the colours match?
 * @param ac1 First colour
 * @param ac2 Second colour
 * @retval true The colours and attributes match
 */
bool attr_color_match(struct AttrColor *ac1, struct AttrColor *ac2)
{
  if ((!ac1) ^ (!ac2)) // One is set, but not the other
    return false;

  if (!ac1) // Two empty colours match
    return true;

  return ((ac1->curses_color == ac2->curses_color) && (ac1->attrs == ac2->attrs));
}

/**
 * modify_color_by_prefix - Modify a colour/attributes based on a prefix, e.g. "bright"
 * @param[in]     prefix prefix to apply
 * @param[in]     is_fg  true if a foreground colour should be modified
 * @param[in,out] col    colour to modify
 * @param[in,out] attrs  attributes to modify
 */
void modify_color_by_prefix(enum ColorPrefix prefix, bool is_fg, color_t *col, int *attrs)
{
  if (prefix == COLOR_PREFIX_NONE)
    return; // nothing to do here

  if (prefix == COLOR_PREFIX_ALERT)
  {
    *attrs |= A_BOLD;
    *attrs |= A_BLINK;
  }
  else if (is_fg)
  {
    if ((COLORS >= 16) && (prefix == COLOR_PREFIX_LIGHT))
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
 * | 240 | `#585858` | 241 | `#626262` | 242 | `#6c6c6c` | 243 | `#767676` | 244 | `#808080` | 245 | `#8a8a8a` | 246 | `#949494` | 247 | `#9e9e9e` |
 * | 248 | `#a8a8a8` | 249 | `#b2b2b2` | 250 | `#bcbcbc` | 251 | `#c6c6c6` | 252 | `#d0d0d0` | 253 | `#dadada` | 254 | `#e4e4e4` | 255 | `#eeeeee` |
 */
color_t color_xterm256_to_24bit(const color_t color)
{
  static const color_t basic[] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
    0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00, 0xffff00,
    0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
  };

  assert(color < 256);

  if (color < 0)
    return color;

  const bool c_color_directcolor = cs_subset_bool(NeoMutt->sub, "color_directcolor");
  if (!c_color_directcolor)
  {
    return color;
  }

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
     *    color - 16 = (vr * 36) + (vg * 6) + (vb * 1)
     *
     * with vr, vg, vb integers between 0 and 5, then vr, vg, vb is the channel
     * value for red, green, and blue, respectively.
     */

    color_t normalised_color = color - 16;
    color_t vr = (normalised_color % 216) / 36; /* 216 = 6*6*6 */
    color_t vg = (normalised_color % 36) / 6;
    color_t vb = (normalised_color % 6) / 1;

    /* First step is wider than the other ones, so add the difference if needed */
    color_t r = (vr * 0x28) + ((vr > 0) ? (0x5f - 0x28) : 0);
    color_t g = (vg * 0x28) + ((vg > 0) ? (0x5f - 0x28) : 0);
    color_t b = (vb * 0x28) + ((vb > 0) ? (0x5f - 0x28) : 0);

    color_t rgb = (r << 16) + (g << 8) + (b << 0);
    color_debug(LL_DEBUG5, "Converted xterm color %d to RGB #%x:\n", color, rgb);
    return rgb;
  }

  /* Grey scale starts at 0x08 and adds 0xa = 10 in very step ending in 0xee.
   * There are a total of 6*4 = 24 grey colors in total. */
  color_t steps = color - 232;
  color_t grey = (steps * 0x0a) + 0x08;
  color_t rgb = (grey << 16) + (grey << 8) + (grey << 0);
  color_debug(LL_DEBUG5, "Converted xterm color %d to RGB #%x:\n", color, rgb);
  return rgb;
}
#endif

/**
 * attr_color_overwrite - Update an AttrColor in-place
 * @param ac_old AttrColor to overwrite
 * @param ac_new AttrColor to copy
 */
void attr_color_overwrite(struct AttrColor *ac_old, struct AttrColor *ac_new)
{
  if (!ac_old || !ac_new)
    return;

  color_t fg = ac_new->fg.color;
  color_t bg = ac_new->bg.color;
  int attrs = ac_new->attrs;

  modify_color_by_prefix(ac_new->fg.prefix, true, &fg, &attrs);
  modify_color_by_prefix(ac_new->bg.prefix, false, &bg, &attrs);

#ifdef NEOMUTT_DIRECT_COLORS
  if ((ac_new->fg.type == CT_SIMPLE) || (ac_new->fg.type == CT_PALETTE))
    fg = color_xterm256_to_24bit(fg);
  else if (fg < 8)
    fg = 8;
  if ((ac_new->bg.type == CT_SIMPLE) || (ac_new->bg.type == CT_PALETTE))
    bg = color_xterm256_to_24bit(bg);
  else if (bg < 8)
    bg = 8;
#endif

  struct CursesColor *cc = curses_color_new(fg, bg);
  curses_color_free(&ac_old->curses_color);
  ac_old->fg = ac_new->fg;
  ac_old->bg = ac_new->bg;
  ac_old->attrs = attrs;
  ac_old->curses_color = cc;
}
