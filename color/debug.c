/**
 * @file
 * Colour Debugging
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "debug.h"
#include "pager/lib.h"
#include "attr.h"
#include "curses2.h"
#include "dump.h"
#include "pager/private_data.h"

extern struct AttrColorList MergedColors;
extern struct CursesColorList CursesColors;

/**
 * color_log_color - Get a colourful string to represent a colour in the log
 * @param fg Foreground colour
 * @param bg Background colour
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
const char *color_log_color(color_t fg, color_t bg)
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
 * ansi_colors_dump - Dump all the ANSI colours
 * @param buf   Buffer for result
 */
void ansi_colors_dump(struct Buffer *buf)
{
  struct MuttWindow *win = window_get_focus();
  if (!win || (win->type != WT_CUSTOM) || !win->parent || (win->parent->type != WT_PAGER))
    return;

  struct PagerPrivateData *priv = win->parent->wdata;
  if (!priv || TAILQ_EMPTY(&priv->ansi_list))
    return;

  struct Buffer *swatch = buf_pool_get();
  char color_fg[64] = { 0 };
  char color_bg[64] = { 0 };

  buf_addstr(buf, "# Ansi Colors\n");
  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, &priv->ansi_list, entries)
  {
    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    color_log_color_attrs(ac, swatch);
    buf_add_printf(buf, "# %-30s %-16s %-16s # %s\n", color_log_attrs_list(ac->attrs),
                   color_log_name(color_fg, sizeof(color_fg), &ac->fg),
                   color_log_name(color_bg, sizeof(color_bg), &ac->bg),
                   buf_string(swatch));
  }

  buf_addstr(buf, "\n");
  buf_pool_release(&swatch);
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

  const char *color = color_log_color(cc->fg, cc->bg);
  color_debug(LL_DEBUG5, "%s index %d, %s %s %s rc %d\n", NONULL(prefix),
              cc->index, fg, bg, color, cc->ref_count);
}

/**
 * curses_colors_dump - Dump all the Curses colours
 * @param buf   Buffer for result
 */
void curses_colors_dump(struct Buffer *buf)
{
  if (TAILQ_EMPTY(&CursesColors))
    return;

  struct Buffer *swatch = buf_pool_get();

  buf_addstr(buf, "# Curses Colors\n");
  buf_add_printf(buf, "# Index fg      bg      Color  rc\n");

  struct CursesColor *cc = NULL;
  TAILQ_FOREACH(cc, &CursesColors, entries)
  {
    char fg[16] = "-";
    char bg[16] = "-";

    if (cc->fg != -1)
      snprintf(fg, sizeof(fg), "#%06x", cc->fg);
    if (cc->bg != -1)
      snprintf(bg, sizeof(bg), "#%06x", cc->bg);

    const char *color = color_log_color(cc->fg, cc->bg);
    buf_add_printf(buf, "# %5d %-7s %-7s %s %2d\n", cc->index, fg, bg, color, cc->ref_count);
  }

  buf_addstr(buf, "\n");
  buf_pool_release(&swatch);
}

/**
 * merged_colors_dump - Dump all the Merged colours
 */
void merged_colors_dump(struct Buffer *buf)
{
  if (TAILQ_EMPTY(&MergedColors))
    return;

  struct Buffer *swatch = buf_pool_get();
  char color_fg[64] = { 0 };
  char color_bg[64] = { 0 };

  buf_addstr(buf, "# Merged Colors\n");
  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, &MergedColors, entries)
  {
    struct CursesColor *cc = ac->curses_color;
    if (!cc)
      continue;

    color_log_color_attrs(ac, swatch);
    buf_add_printf(buf, "# %-30s %-16s %-16s # %s\n", color_log_attrs_list(ac->attrs),
                   color_log_name(color_fg, sizeof(color_fg), &ac->fg),
                   color_log_name(color_bg, sizeof(color_bg), &ac->bg),
                   buf_string(swatch));
  }

  buf_addstr(buf, "\n");
  buf_pool_release(&swatch);
}
