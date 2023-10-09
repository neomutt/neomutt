/**
 * @file
 * Colour Debugging
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
 * @page color_debug Colour Debugging
 *
 * Lots of debugging of the colour code, conditional on './configure --debug-color'
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "debug.h"
#include "pager/lib.h"
#include "attr.h"
#include "color.h"
#include "curses2.h"
#include "pager/private_data.h" // IWYU pragma: keep
#include "parse_color.h"
#include "quoted.h"
#include "regex4.h"
#include "simple2.h"

extern struct AttrColorList MergedColors;
extern struct CursesColorList CursesColors;
extern int NumCursesColors;

extern struct RegexColorList AttachList;
extern struct RegexColorList BodyList;
extern struct RegexColorList HeaderList;
extern struct RegexColorList IndexAuthorList;
extern struct RegexColorList IndexCollapsedList;
extern struct RegexColorList IndexDateList;
extern struct RegexColorList IndexFlagsList;
extern struct RegexColorList IndexLabelList;
extern struct RegexColorList IndexList;
extern struct RegexColorList IndexNumberList;
extern struct RegexColorList IndexSizeList;
extern struct RegexColorList IndexSubjectList;
extern struct RegexColorList IndexTagList;
extern struct RegexColorList IndexTagsList;
extern struct RegexColorList StatusList;

/**
 * color_debug_log_color_attrs - Get a colourful string to represent a colour in the log
 * @param ac     Colour
 * @param swatch Buffer for swatch
 *
 * @note Do not free the returned string
 */
void color_debug_log_color_attrs(struct AttrColor *ac, struct Buffer *swatch)
{
  buf_reset(swatch);

  if (ac->attrs & A_BLINK)
    buf_add_printf(swatch, "\033[5m");
  if (ac->attrs & A_BOLD)
    buf_add_printf(swatch, "\033[1m");
  if (ac->attrs & A_ITALIC)
    buf_add_printf(swatch, "\033[3m");
  if (ac->attrs & A_NORMAL)
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
 * color_debug_log_color - Get a colourful string to represent a colour in the log
 * @param fg Foreground colour
 * @param bg Background colour
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
const char *color_debug_log_color(color_t fg, color_t bg)
{
  static char text[64];
  size_t pos = 0;
  // snprintf(text, sizeof(text), "\033[38;5;%dm\033[48;5;%dmXXXXXX\033[0m", fg, bg);

  if (fg != -1)
  {
    int r = (fg >> 16) & 0xff;
    int g = (fg >> 8) & 0xff;
    int b = (fg >> 0) & 0xff;

    pos += snprintf(text + pos, sizeof(text) - pos, "\033[38;2;%d;%d;%dm", r, g, b);
  }

  if (bg != -1)
  {
    int r = (bg >> 16) & 0xff;
    int g = (bg >> 8) & 0xff;
    int b = (bg >> 0) & 0xff;

    pos += snprintf(text + pos, sizeof(text) - pos, "\033[48;2;%d;%d;%dm", r, g, b);
  }

  pos += snprintf(text + pos, sizeof(text) - pos, "XXXXXX\033[0m");

  return text;
}

/**
 * color_debug_log_attrcolor - XXX
 *
 * @note Do not free the returned string
 */
void color_debug_log_attrcolor(struct AttrColor *ac, struct Buffer *buf)
{
  if (ac->fg.color != -1)
  {
    switch (ac->fg.type)
    {
      case CT_SIMPLE:
      {
        buf_add_printf(buf, "\033[%dm", 30 + ac->fg.color);
        break;
      }

      case CT_PALETTE:
      {
        buf_add_printf(buf, "\033[38;5;%dm", ac->fg.color);
        break;
      }

      case CT_RGB:
      {
        const int r = (ac->fg.color >> 16) & 0xff;
        const int g = (ac->fg.color >> 8) & 0xff;
        const int b = (ac->fg.color >> 0) & 0xff;

        buf_add_printf(buf, "\033[38;2;%d;%d;%dm", r, g, b);
        break;
      }
    }
  }

  if (ac->bg.color != -1)
  {
    switch (ac->bg.type)
    {
      case CT_SIMPLE:
      {
        buf_add_printf(buf, "\033[%dm", 40 + ac->bg.color);
        break;
      }

      case CT_PALETTE:
      {
        buf_add_printf(buf, "\033[48;5;%dm", ac->bg.color);
        break;
      }

      case CT_RGB:
      {
        const int r = (ac->bg.color >> 16) & 0xff;
        const int g = (ac->bg.color >> 8) & 0xff;
        const int b = (ac->bg.color >> 0) & 0xff;

        buf_add_printf(buf, "\033[48;2;%d;%d;%dm", r, g, b);
        break;
      }
    }
  }

  buf_addstr(buf, "XXXXXX\033[0m");
}

/**
 * color_debug_log_attrs - Get a string to represent some attributes in the log
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
const char *color_debug_log_attrs(int attrs)
{
  static char text[64];
  struct Mapping attr_names[] = {
    { "\033[5mBLI\033[0m", A_BLINK },
    { "\033[1mBLD\033[0m", A_BOLD },
    { "\033[0mNOR\033[0m", A_NORMAL },
    { "\033[7mREV\033[0m", A_REVERSE },
    { "\033[1mSTD\033[0m", A_STANDOUT },
    { "\033[4mUND\033[0m", A_UNDERLINE },
    { NULL, 0 },
  };

  int offset = 0;
  text[0] = '\0';
  for (int i = 0; attr_names[i].name; i++)
  {
    if (attrs & attr_names[i].value)
    {
      offset += snprintf(text + offset, sizeof(text) - offset, "%s ",
                         attr_names[i].name);
    }
  }
  return text;
}

/**
 * color_debug_log_attrs_list - Get a string to represent some attributes in the log
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
const char *color_debug_log_attrs_list(int attrs)
{
  static char text[64];

  text[0] = '\0';
  int pos = 0;
  if (attrs & A_BLINK)
    pos += snprintf(text + pos, sizeof(text) - pos, "blink ");
  if (attrs & A_BOLD)
    pos += snprintf(text + pos, sizeof(text) - pos, "bold ");
  if (attrs & A_ITALIC)
    pos += snprintf(text + pos, sizeof(text) - pos, "italic ");
  if (attrs & A_NORMAL)
    pos += snprintf(text + pos, sizeof(text) - pos, "normal ");
  if (attrs & A_REVERSE)
    pos += snprintf(text + pos, sizeof(text) - pos, "reverse ");
  if (attrs & A_STANDOUT)
    pos += snprintf(text + pos, sizeof(text) - pos, "standout ");
  if (attrs & A_UNDERLINE)
    pos += snprintf(text + pos, sizeof(text) - pos, "underline ");

  return text;
}

/**
 * color_debug_log_name - Get a string to represent a colour name
 * @param buf    Buffer for the result
 * @param buflen Length of the Buffer
 * @param elem   Colour to use
 * @retval ptr Generated string
 */
const char *color_debug_log_name(char *buf, int buflen, struct ColorElement *elem)
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
        snprintf(buf, buflen, "BAD:%d", elem->color);
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
 * attr_color_dump - Dump an Attr Colour to the log
 * @param ac     AttrColor to dump
 * @param prefix prefix for the log block
 */
void attr_color_dump(struct AttrColor *ac, const char *prefix)
{
  if (!ac)
    return;

  int index = ac->curses_color ? ac->curses_color->index : -1;

  color_t fg = COLOR_DEFAULT;
  color_t bg = COLOR_DEFAULT;
  struct CursesColor *cc = ac->curses_color;
  if (cc)
  {
    fg = cc->fg;
    bg = cc->bg;
  }
  const char *color = color_debug_log_color(fg, bg);
  const char *attrs = color_debug_log_attrs(ac->attrs);
  color_debug(LL_DEBUG5, "%s| %5d | %s | 0x%08x | %s\n", NONULL(prefix), index,
              color, ac->attrs, attrs);
}

/**
 * attr_color_list_dump - Dump all the Attr Colours to the log
 * @param acl   List of Attr colours
 * @param title Title for the log block
 */
void attr_color_list_dump(struct AttrColorList *acl, const char *title)
{
  if (!acl)
    return;

  int count = 0;
  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, acl, entries)
  {
    count++;
  }

  color_debug(LL_DEBUG5, "\033[1;32m%s:\033[0m (%d)\n", title, count);
  if (count == 0)
    return;

  color_debug(LL_DEBUG5, "    | Index | Colour | Attrs      | Attrs\n");

  TAILQ_FOREACH(ac, acl, entries)
  {
    attr_color_dump(ac, "    ");
  }
}

/**
 * curses_color_dump - Log one Curses colour
 * @param cc     CursesColor to log
 * @param prefix Prefix for the log line
 */
void curses_color_dump(struct CursesColor *cc, const char *prefix)
{
  if (!cc)
    return;

  char fg[16] = "-";
  char bg[16] = "-";

  if (cc->fg != -1)
    snprintf(fg, sizeof(fg), "#%06x", cc->fg);
  if (cc->bg != -1)
    snprintf(bg, sizeof(bg), "#%06x", cc->bg);

  const char *color = color_debug_log_color(cc->fg, cc->bg);
  color_debug(LL_DEBUG5, "%s| %5d | %-7s %-7s | %s | %2d |\n", NONULL(prefix),
              cc->index, fg, bg, color, cc->ref_count);
}

/**
 * curses_colors_dump - Log all the Curses colours
 */
void curses_colors_dump(void)
{
  color_debug(LL_DEBUG5, "\033[1;32mCursesColors:\033[0m (%d)\n", NumCursesColors);
  if (TAILQ_EMPTY(&CursesColors))
    return;

  color_debug(LL_DEBUG5, "    | Index | fg      bg      | Colour | rc |\n");

  struct CursesColor *cc = NULL;
  TAILQ_FOREACH(cc, &CursesColors, entries)
  {
    curses_color_dump(cc, "    ");
  }
}

/**
 * quoted_color_dump - Log a Quoted colour
 * @param ac      Quoted colour
 * @param q_level Quote level
 * @param prefix  Prefix for the log line
 */
void quoted_color_dump(struct AttrColor *ac, int q_level, const char *prefix)
{
  if (!ac)
    return;

  int index = ac->curses_color ? ac->curses_color->index : -1;

  if ((index >= 0) || (ac->attrs != 0))
  {
    struct Buffer *color_str = buf_pool_get();
    color_debug_log_attrcolor(ac, color_str);

    const char *attrs = color_debug_log_attrs(ac->attrs);
    color_debug(LL_DEBUG5, "%s| quoted%d | %5d | %s | 0x%08x | %s\n", prefix,
                q_level, index, buf_string(color_str), ac->attrs, attrs);

    buf_pool_release(&color_str);
  }
  else
  {
    color_debug(LL_DEBUG5, "%s| quoted%d | %5d |        |            |\n",
                prefix, q_level, index);
  }
}

/**
 * quoted_color_list_dump - Log all the Quoted colours
 */
void quoted_color_list_dump(void)
{
  color_debug(LL_DEBUG5, "\033[1;32mQuotedColors:\033[0m (%d)\n", NumQuotedColors);
  color_debug(LL_DEBUG5, "    | Name    | Index | Colour | Attrs      | Attrs\n");
  for (size_t i = 0; i < COLOR_QUOTES_MAX; i++)
  {
    quoted_color_dump(&QuotedColors[i], i, "    ");
  }
}

/**
 * regex_color_dump - Dump a Regex colour to the log
 * @param rcol   Regex to dump
 * @param prefix Prefix for the log line
 */
void regex_color_dump(struct RegexColor *rcol, const char *prefix)
{
  if (!rcol)
    return;

  struct AttrColor *ac = &rcol->attr_color;
  int index = ac->curses_color ? ac->curses_color->index : -1;

  struct Buffer *color_str = buf_pool_get();
  color_debug_log_attrcolor(ac, color_str);

  const char *attrs = color_debug_log_attrs(ac->attrs);
  color_debug(LL_DEBUG5, "%s| %5d | %s | 0x%08x | %-8s | %s\n", NONULL(prefix),
              index, buf_string(color_str), ac->attrs, attrs, rcol->pattern);

  buf_pool_release(&color_str);
}

/**
 * regex_color_list_dump - Dump one Regex's colours to the log
 * @param name Name of the Regex
 * @param rcl  RegexColorList to dump
 */
void regex_color_list_dump(const char *name, struct RegexColorList *rcl)
{
  if (!name || !rcl)
    return;

  int count = 0;
  struct RegexColor *rcol = NULL;
  STAILQ_FOREACH(rcol, rcl, entries)
  {
    count++;
  }

  color_debug(LL_DEBUG5, "\033[1;32mRegexColorList %s\033[0m (%d)\n", name, count);
  if (count == 0)
    return;

  color_debug(LL_DEBUG5, "    | Index | Colour | Attrs      | Attrs    | Pattern\n");
  STAILQ_FOREACH(rcol, rcl, entries)
  {
    regex_color_dump(rcol, "    ");
  }
}

/**
 * regex_colors_dump_all - Dump all the Regex colours to the log
 */
void regex_colors_dump_all(void)
{
  regex_color_list_dump("AttachList", &AttachList);
  regex_color_list_dump("BodyList", &BodyList);
  regex_color_list_dump("HeaderList", &HeaderList);
  regex_color_list_dump("IndexAuthorList", &IndexAuthorList);
  regex_color_list_dump("IndexCollapsedList", &IndexCollapsedList);
  regex_color_list_dump("IndexDateList", &IndexDateList);
  regex_color_list_dump("IndexFlagsList", &IndexFlagsList);
  regex_color_list_dump("IndexLabelList", &IndexLabelList);
  regex_color_list_dump("IndexList", &IndexList);
  regex_color_list_dump("IndexNumberList", &IndexNumberList);
  regex_color_list_dump("IndexSizeList", &IndexSizeList);
  regex_color_list_dump("IndexSubjectList", &IndexSubjectList);
  regex_color_list_dump("IndexTagList", &IndexTagList);
  regex_color_list_dump("IndexTagsList", &IndexTagsList);
  regex_color_list_dump("StatusList", &StatusList);
}

/**
 * simple_color_dump - Dump a Simple colour to the log
 * @param cid    Colour Id, e.g. #MT_COLOR_UNDERLINE
 * @param prefix Prefix for the log line
 */
void simple_color_dump(enum ColorId cid, const char *prefix)
{
  struct AttrColor *ac = &SimpleColors[cid];
  int index = ac->curses_color ? ac->curses_color->index : -1;
  const char *name = NULL;
  const char *compose = "";

  name = mutt_map_get_name(cid, ColorFields);
  if (!name)
  {
    name = mutt_map_get_name(cid, ComposeColorFields);
    if (name)
    {
      compose = "compose ";
    }
  }

  struct Buffer *color_str = buf_pool_get();
  color_debug_log_attrcolor(ac, color_str);

  const char *attrs_str = color_debug_log_attrs(ac->attrs);
  color_debug(LL_DEBUG5, "%s| %s%-19s | %5d | %s | 0x%08x | %s\n", prefix,
              compose, name, index, buf_string(color_str), ac->attrs, attrs_str);

  buf_pool_release(&color_str);
}

/**
 * simple_colors_dump - Dump all the Simple colours to the log
 * @param force If true, list unset colours
 */
void simple_colors_dump(bool force)
{
  color_debug(LL_DEBUG5, "\033[1;32mSimpleColors:\033[0m\n");
  color_debug(LL_DEBUG5, "    | Name                | Index | Colour | Attrs      | Attrs\n");
  for (enum ColorId cid = MT_COLOR_NONE; cid < MT_COLOR_MAX; cid++)
  {
    struct AttrColor *ac = &SimpleColors[cid];
    if (!force && !attr_color_is_set(ac))
      continue;

    simple_color_dump(cid, "    ");
  }
}

/**
 * merged_colors_dump - Dump all the Merged colours to the log
 */
void merged_colors_dump(void)
{
  attr_color_list_dump(&MergedColors, "MergedColors");
}

/**
 * color_dump - Display all the colours in the Pager
 */
void color_dump(void)
{
  struct Buffer *tmp_file = buf_pool_get();

  buf_mktemp(tmp_file);
  FILE *fp = mutt_file_fopen(buf_string(tmp_file), "w");
  if (!fp)
  {
    // L10N: '%s' is the file name of the temporary file
    mutt_error(_("Could not create temporary file %s"), buf_string(tmp_file));
    buf_pool_release(&tmp_file);
    return;
  }

  struct Buffer *swatch = buf_pool_get();
  char color_fg[64] = { 0 };
  char color_bg[64] = { 0 };

  fputs("# All Colours\n\n", fp);
  fputs("# Simple Colours\n", fp);
  for (enum ColorId cid = MT_COLOR_NONE + 1; cid < MT_COLOR_MAX; cid++)
  {
    struct AttrColor *ac = simple_color_get(cid);
    if (!ac)
      continue;

    struct CursesColor *cc = ac->curses_color;
    if (!cc && (ac->attrs == 0))
      continue;

    const char *name = mutt_map_get_name(cid, ColorFields);
    if (!name)
      continue;

    color_debug_log_color_attrs(ac, swatch);
    fprintf(fp, "color %-18s %-30s %-16s %-16s # %s\n", name,
            color_debug_log_attrs_list(ac->attrs),
            color_debug_log_name(color_fg, sizeof(color_fg), &ac->fg),
            color_debug_log_name(color_bg, sizeof(color_bg), &ac->bg), buf_string(swatch));
  }

  if (NumQuotedColors > 0)
  {
    fputs("\n# Quoted Colours\n", fp);
    for (int i = 0; i < NumQuotedColors; i++)
    {
      struct AttrColor *ac = quoted_colors_get(i);
      if (!ac)
        continue;

      // struct CursesColor *cc = ac->curses_color;
      // if (!cc)
      //   continue;

      color_debug_log_color_attrs(ac, swatch);
      fprintf(fp, "color quoted%d %-30s %-16s %-16s # %s\n", i,
              color_debug_log_attrs_list(ac->attrs),
              color_debug_log_name(color_fg, sizeof(color_fg), &ac->fg),
              color_debug_log_name(color_bg, sizeof(color_bg), &ac->bg), buf_string(swatch));
    }
  }

  int rl_count = 0;
  for (enum ColorId id = MT_COLOR_NONE; id != MT_COLOR_MAX; id++)
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
    for (enum ColorId id = MT_COLOR_NONE; id != MT_COLOR_MAX; id++)
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

      fprintf(fp, "\n# Regex Colour %s\n", name);

      struct RegexColor *rc = NULL;
      STAILQ_FOREACH(rc, rcl, entries)
      {
        struct AttrColor *ac = &rc->attr_color;
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        color_debug_log_color_attrs(ac, swatch);
        fprintf(fp, "color %-14s %-30s %-16s %-16s %-30s # %s\n", name,
                color_debug_log_attrs_list(ac->attrs),
                color_debug_log_name(color_fg, sizeof(color_fg), &ac->fg),
                color_debug_log_name(color_bg, sizeof(color_bg), &ac->bg),
                rc->pattern, buf_string(swatch));
      }
    }
  }

  if (!TAILQ_EMPTY(&MergedColors))
  {
    fputs("\n# Merged Colours\n", fp);
    struct AttrColor *ac = NULL;
    TAILQ_FOREACH(ac, &MergedColors, entries)
    {
      struct CursesColor *cc = ac->curses_color;
      if (!cc)
        continue;

      color_debug_log_color_attrs(ac, swatch);
      fprintf(fp, "# %-30s %-16s %-16s # %s\n", color_debug_log_attrs_list(ac->attrs),
              color_debug_log_name(color_fg, sizeof(color_fg), &ac->fg),
              color_debug_log_name(color_bg, sizeof(color_bg), &ac->bg),
              buf_string(swatch));
    }
  }

  struct MuttWindow *win = window_get_focus();
  if (win && (win->type == WT_CUSTOM) && win->parent && (win->parent->type == WT_PAGER))
  {
    struct PagerPrivateData *priv = win->parent->wdata;
    if (priv && !TAILQ_EMPTY(&priv->ansi_list))
    {
      fputs("\n# Ansi Colours\n", fp);
      struct AttrColor *ac = NULL;
      TAILQ_FOREACH(ac, &priv->ansi_list, entries)
      {
        struct CursesColor *cc = ac->curses_color;
        if (!cc)
          continue;

        color_debug_log_color_attrs(ac, swatch);
        fprintf(fp, "# %-30s %-16s %-16s # %s\n", color_debug_log_attrs_list(ac->attrs),
                color_debug_log_name(color_fg, sizeof(color_fg), &ac->fg),
                color_debug_log_name(color_bg, sizeof(color_bg), &ac->bg),
                buf_string(swatch));
      }
    }
  }

  mutt_file_fclose(&fp);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tmp_file);

  pview.banner = "color";
  pview.flags = MUTT_SHOWCOLOR;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tmp_file);
  buf_pool_release(&swatch);
}
