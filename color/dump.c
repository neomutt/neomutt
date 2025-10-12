/**
 * @file
 * Colour Dump Command
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
 * @page color_dump Colour Dump Command
 *
 * Colour Dump Command
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "dump.h"
#include "pager/lib.h"
#include "attr.h"
#include "color.h"
#include "domain.h"
#include "parse_color.h"
#include "pattern.h"
#include "regex4.h"
#ifdef USE_DEBUG_COLOR
#include "debug.h"
#endif

/**
 * enum ColorInfoColumn - Name the columns
 */
enum ColorInfoColumn
{
  CIN_NAME = 0, ///< Colour name, e.g. 'body'
  CIN_ATTRS,    ///< Attribute, e.g. 'underline'
  CIN_FG,       ///< Foreground colour
  CIN_BG,       ///< Background colour
  CIN_REGEX,    ///< Regex / Pattern
  CIN_SWATCH,   ///< Colour swatch
};

/**
 * struct ColorInfo - Info about one colour
 */
struct ColorInfo
{
  const char *info[6]; ///< Indexed by ColorInfoColumn
  int match;           ///< Regex match number
};
ARRAY_HEAD(ColorInfoArray, struct ColorInfo);

/**
 * color_log_color_attrs - Get a colourful string to represent a colour in the log
 * @param ac     Colour
 * @param swatch Buffer for swatch
 *
 * @note Do not free the returned string
 */
void color_log_color_attrs(struct AttrColor *ac, struct Buffer *swatch)
{
  buf_reset(swatch);

  if (ac->attrs & A_BLINK)
    buf_add_printf(swatch, "\033[5m");
  if (ac->attrs & A_BOLD)
    buf_add_printf(swatch, "\033[1m");
  if (ac->attrs & A_ITALIC)
    buf_add_printf(swatch, "\033[3m");
  if (ac->attrs == A_NORMAL)
    buf_add_printf(swatch, "\033[0m");
  if (ac->attrs & A_REVERSE)
    buf_add_printf(swatch, "\033[7m");
  if (ac->attrs & A_STANDOUT)
    buf_add_printf(swatch, "\033[1m");
  if (ac->attrs & A_UNDERLINE)
    buf_add_printf(swatch, "\033[4m");

  if (ac->fg.color >= 0)
  {
    switch (ac->fg.type)
    {
      case CT_SIMPLE:
      {
        buf_add_printf(swatch, "\033[%dm", 30 + ac->fg.color);
        break;
      }

      case CT_PALETTE:
      {
        buf_add_printf(swatch, "\033[38;5;%dm", ac->fg.color);
        break;
      }

      case CT_RGB:
      {
        int r = (ac->fg.color >> 16) & 0xff;
        int g = (ac->fg.color >> 8) & 0xff;
        int b = (ac->fg.color >> 0) & 0xff;
        buf_add_printf(swatch, "\033[38;2;%d;%d;%dm", r, g, b);
        break;
      }
    }
  }

  if (ac->bg.color >= 0)
  {
    switch (ac->bg.type)
    {
      case CT_SIMPLE:
      {
        buf_add_printf(swatch, "\033[%dm", 40 + ac->bg.color);
        break;
      }

      case CT_PALETTE:
      {
        buf_add_printf(swatch, "\033[48;5;%dm", ac->bg.color);
        break;
      }

      case CT_RGB:
      {
        int r = (ac->bg.color >> 16) & 0xff;
        int g = (ac->bg.color >> 8) & 0xff;
        int b = (ac->bg.color >> 0) & 0xff;
        buf_add_printf(swatch, "\033[48;2;%d;%d;%dm", r, g, b);
        break;
      }
    }
  }

  buf_addstr(swatch, "XXXXXX\033[0m");
}

/**
 * color_log_attrs_list - Get a string to represent some attributes in the log
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
const char *color_log_attrs_list(int attrs)
{
  static char text[64];

  text[0] = '\0';
  int pos = 0;
  // We can ignore the A_NORMAL case
  if (attrs & A_BLINK)
    pos += snprintf(text + pos, sizeof(text) - pos, "blink ");
  if (attrs & A_BOLD)
    pos += snprintf(text + pos, sizeof(text) - pos, "bold ");
  if (attrs & A_ITALIC)
    pos += snprintf(text + pos, sizeof(text) - pos, "italic ");
  if (attrs & A_REVERSE)
    pos += snprintf(text + pos, sizeof(text) - pos, "reverse ");
  if (attrs & A_STANDOUT)
    pos += snprintf(text + pos, sizeof(text) - pos, "standout ");
  if (attrs & A_UNDERLINE)
    pos += snprintf(text + pos, sizeof(text) - pos, "underline ");

  return text;
}

/**
 * color_log_name - Get a string to represent a colour name
 * @param buf    Buffer for the result
 * @param buflen Length of the Buffer
 * @param elem   Colour to use
 * @retval ptr Generated string
 */
const char *color_log_name(char *buf, int buflen, struct ColorElement *elem)
{
  if (elem->color < 0)
    return "default";

  switch (elem->type)
  {
    case CT_SIMPLE:
    {
      const char *prefix = NULL;
      switch (elem->prefix)
      {
        case COLOR_PREFIX_ALERT:
          prefix = "alert";
          break;
        case COLOR_PREFIX_BRIGHT:
          prefix = "bright";
          break;
        case COLOR_PREFIX_LIGHT:
          prefix = "light";
          break;
        default:
          prefix = "";
          break;
      }

      const char *name = mutt_map_get_name(elem->color, ColorNames);
      snprintf(buf, buflen, "%s%s", prefix, name);
      break;
    }

    case CT_PALETTE:
    {
      if (elem->color < 256)
        snprintf(buf, buflen, "color%d", elem->color);
      else
        snprintf(buf, buflen, "BAD:%d", elem->color); // LCOV_EXCL_LINE
      break;
    }

    case CT_RGB:
    {
      int r = (elem->color >> 16) & 0xff;
      int g = (elem->color >> 8) & 0xff;
      int b = (elem->color >> 0) & 0xff;
      snprintf(buf, buflen, "#%02x%02x%02x", r, g, b);
      break;
    }
  }

  return buf;
}

/**
 * color_info_sort - Compare two ColorInfo by their names - Implements ::sort_t - @ingroup sort_api
 */
static int color_info_sort(const void *a, const void *b, void *sdata)
{
  const struct ColorInfo *x = (const struct ColorInfo *) a;
  const struct ColorInfo *y = (const struct ColorInfo *) b;

  return mutt_str_cmp(x->info[CIN_NAME], y->info[CIN_NAME]);
}

/**
 * cia_measure_column - Measure the width of the nth column of a ColorInfoArray
 * @param cia ColorInfoArray
 * @param col Column to measure
 * @retval num Width
 */
static int cia_measure_column(const struct ColorInfoArray *cia, enum ColorInfoColumn col)
{
  if (!cia)
    return 0;

  struct ColorInfo *ci = NULL;
  int width = 0;

  ARRAY_FOREACH(ci, cia)
  {
    width = MAX(width, mutt_str_len(ci->info[col]));
  }

  return width;
}

/**
 * cia_clear - Clear the contents of a ColorInfoArray
 * @param cia Array to clear
 */
static void cia_clear(struct ColorInfoArray *cia)
{
  if (!cia)
    return;

  struct ColorInfo *ci = NULL;
  ARRAY_FOREACH(ci, cia)
  {
    // Don't free `ci->info[CIN_NAME]`
    FREE(&ci->info[CIN_ATTRS]);
    FREE(&ci->info[CIN_FG]);
    FREE(&ci->info[CIN_BG]);
    FREE(&ci->info[CIN_REGEX]);
    FREE(&ci->info[CIN_SWATCH]);
  }

  ARRAY_FREE(cia);
}

/**
 * dump_color - Dump one set of colours
 * @param cia Array of colour info
 * @param buf Buffer for the result
 */
static void dump_color(const struct ColorInfoArray *cia, struct Buffer *buf)
{
  if (ARRAY_EMPTY(cia))
    return;

  int lengths[5] = { 0 };

  for (enum ColorInfoColumn col = CIN_NAME; col <= CIN_SWATCH; col++)
    lengths[col] = cia_measure_column(cia, col);

  ARRAY_SORT(cia, color_info_sort, NULL);

  struct ColorInfo *ci = NULL;
  ARRAY_FOREACH(ci, cia)
  {
    buf_addstr(buf, "color ");
    buf_add_printf(buf, "%*s ", -lengths[CIN_NAME], ci->info[CIN_NAME]);
    if (lengths[CIN_ATTRS] > 0)
      buf_add_printf(buf, "%*s ", -lengths[CIN_ATTRS], NONULL(ci->info[CIN_ATTRS]));
    buf_add_printf(buf, "%*s ", -lengths[CIN_FG], ci->info[CIN_FG]);
    buf_add_printf(buf, "%*s ", -lengths[CIN_BG], ci->info[CIN_BG]);

    if (lengths[CIN_REGEX] > 0)
    {
      buf_add_printf(buf, "%*s ", -lengths[CIN_REGEX], NONULL(ci->info[CIN_REGEX]));

      if (ci->match > 0)
        buf_add_printf(buf, "%d ", ci->match);
      else
        buf_addstr(buf, "  ");
    }

    buf_add_printf(buf, "# %s", ci->info[CIN_SWATCH]);
    buf_addstr(buf, "\n");
  }
  buf_addstr(buf, "\n");
}

/**
 * color_dump - Display all the colours in the Pager
 */
void color_dump(void)
{
  struct Buffer *tempfile = buf_pool_get();

  buf_mktemp(tempfile);
  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp)
  {
    // LCOV_EXCL_START
    // L10N: '%s' is the file name of the temporary file
    mutt_error(_("Could not create temporary file %s"), buf_string(tempfile));
    buf_pool_release(&tempfile);
    return;
    // LCOV_EXCL_STOP
  }

  struct Buffer *buf = buf_pool_get();

  struct ColorInfoArray cia_simple = ARRAY_HEAD_INITIALIZER;
  struct ColorInfoArray cia_regex = ARRAY_HEAD_INITIALIZER;

  struct UserColor *uc = NULL;
  enum ColorDomainId did = 0;
  struct Buffer *swatch = buf_pool_get();
  struct Buffer *pattern = buf_pool_get();

  ARRAY_FOREACH(uc, &UserColors)
  {
    const struct ColorDefinition *cdef = uc->cdef;

    if (did != uc->did)
    {
      if (!ARRAY_EMPTY(&cia_simple) || !ARRAY_EMPTY(&cia_regex))
      {
        buf_add_printf(buf, "# %s colours\n", domain_get_name(did));
        dump_color(&cia_simple, buf);
        dump_color(&cia_regex, buf);

        cia_clear(&cia_simple);
        cia_clear(&cia_regex);
      }
      did = uc->did;
    }

    switch (uc->type)
    {
      case CDT_SIMPLE:
      {
        char name[64];
        struct AttrColor *ac = uc->cdata;
        if (!attr_color_is_set(ac))
          break;

        color_log_color_attrs(ac, swatch);

        struct ColorInfo ci = { 0 };
        ci.info[CIN_NAME] = cdef->name;
        ci.info[CIN_ATTRS] = mutt_str_dup(color_log_attrs_list(ac->attrs));
        ci.info[CIN_FG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->fg));
        ci.info[CIN_BG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->bg));
        // Ignore ci.info[CIN_REGEX]
        ci.info[CIN_SWATCH] = buf_strdup(swatch);

        ARRAY_ADD(&cia_simple, ci);
        break;
      }

      case CDT_PATTERN:
      {
        char name[64];
        struct PatternColorList *pcl = uc->cdata;

        struct PatternColor *pc = NULL;
        STAILQ_FOREACH(pc, pcl, entries)
        {
          struct AttrColor *ac = &pc->attr_color;
          if (!attr_color_is_set(ac))
            continue;

          color_log_color_attrs(ac, swatch);

          buf_reset(pattern);
          pretty_var(pc->pattern, pattern);

          struct ColorInfo ci = { 0 };
          ci.info[CIN_NAME] = cdef->name;
          ci.info[CIN_ATTRS] = mutt_str_dup(color_log_attrs_list(ac->attrs));
          ci.info[CIN_FG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->fg));
          ci.info[CIN_BG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->bg));
          ci.info[CIN_REGEX] = buf_strdup(pattern);
          ci.info[CIN_SWATCH] = buf_strdup(swatch);

          ARRAY_ADD(&cia_regex, ci);
        }

        break;
      }

      case CDT_REGEX:
      {
        char name[64];
        struct RegexColorList *rcl = uc->cdata;

        struct RegexColor *rc = NULL;
        STAILQ_FOREACH(rc, rcl, entries)
        {
          struct AttrColor *ac = &rc->attr_color;
          if (!attr_color_is_set(ac))
            continue;

          color_log_color_attrs(ac, swatch);

          buf_reset(pattern);
          pretty_var(rc->pattern, pattern);

          struct ColorInfo ci = { 0 };
          ci.info[CIN_NAME] = cdef->name;
          ci.info[CIN_ATTRS] = mutt_str_dup(color_log_attrs_list(ac->attrs));
          ci.info[CIN_FG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->fg));
          ci.info[CIN_BG] = mutt_str_dup(color_log_name(name, sizeof(name), &ac->bg));
          ci.info[CIN_REGEX] = buf_strdup(pattern);
          ci.info[CIN_SWATCH] = buf_strdup(swatch);
          ci.match = rc->match;

          ARRAY_ADD(&cia_regex, ci);
        }

        break;
      }
    }
  }

  if (!ARRAY_EMPTY(&cia_simple) || !ARRAY_EMPTY(&cia_regex))
  {
    buf_add_printf(buf, "# %s colours\n", domain_get_name(did));
    dump_color(&cia_simple, buf);
    dump_color(&cia_regex, buf);
  }

  cia_clear(&cia_simple);
  cia_clear(&cia_regex);

  buf_pool_release(&swatch);
  buf_pool_release(&pattern);

#ifdef USE_DEBUG_COLOR
  merged_colors_dump(buf);
  ansi_colors_dump(buf);
  curses_colors_dump(buf);
  log_multiline(LL_DEBUG1, buf_string(buf));
#endif

  mutt_file_save_str(fp, buf_string(buf));
  buf_pool_release(&buf);
  mutt_file_fclose(&fp);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = "color";
  pview.flags = MUTT_SHOWCOLOR;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);
}
