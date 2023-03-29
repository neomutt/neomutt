/**
 * @file
 * Pager Debugging
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page debug_pager Pager Debugging
 *
 * Pager Debugging
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "color/lib.h"
#include "pager/lib.h"
#include "pager/display.h"
#include "pager/private_data.h"

void dump_text_syntax(struct TextSyntax *ts, int num)
{
  if (!ts || (num == 0))
    return;

  for (int i = 0; i < num; i++)
  {
    int index = -1;
    const char *swatch = "";
    if (!ts[i].attr_color)
      continue;
    struct CursesColor *cc = ts[i].attr_color->curses_color;
    if (cc)
    {
      index = cc->index;
      swatch = color_debug_log_color(cc->fg, cc->bg);
    }
    mutt_debug(LL_DEBUG1, "\t\t%3d %4d %4d %s\n", index, ts[i].first, ts[i].last, swatch);
  }
}

void dump_line(int i, struct Line *line)
{
  mutt_debug(LL_DEBUG1, "Line: %d (offset: %ld)\n", i, line->offset);
  // mutt_debug(LL_DEBUG1, "\toffset: %ld\n", line->offset);
  if ((line->cid > 0) && (line->cid != MT_COLOR_NORMAL))
  {
    struct Buffer *buf = buf_pool_get();
    get_colorid_name(line->cid, buf);

    const char *swatch = "";
    struct AttrColor *ac = simple_color_get(line->cid);
    if (ac && ac->curses_color)
    {
      struct CursesColor *cc = ac->curses_color;
      swatch = color_debug_log_color(cc->fg, cc->bg);
    }

    mutt_debug(LL_DEBUG1, "\tcolor: %d %s (%s)\n", line->cid, swatch, buf_string(buf));
    buf_pool_release(&buf);
  }
  if (line->cont_line)
  {
    mutt_debug(LL_DEBUG1, "\tcont_line: %s\n",
               line->cont_line ? "\033[1;32myes\033[0m" : "\033[31mno\033[0m");
  }
  if (line->cont_header)
  {
    mutt_debug(LL_DEBUG1, "\tcont_header: %s\n",
               line->cont_header ? "\033[1;32myes\033[0m" : "\033[31mno\033[0m");
  }

  if (line->syntax_arr_size > 0)
  {
    mutt_debug(LL_DEBUG1, "\tsyntax: %d\n", line->syntax_arr_size);
    dump_text_syntax(line->syntax, line->syntax_arr_size);
  }
  if (line->search_arr_size > 0)
  {
    mutt_debug(LL_DEBUG1, "\t\033[1;36msearch\033[0m: %d\n", line->search_arr_size);
    dump_text_syntax(line->search, line->search_arr_size);
  }
}

void dump_pager(struct PagerPrivateData *priv)
{
  if (!priv)
    return;

  mutt_debug(LL_DEBUG1, "----------------------------------------------\n");
  mutt_debug(LL_DEBUG1, "Pager: %d lines (fd %d)\n", priv->lines_used, fileno(priv->fp));
  for (int i = 0; i < priv->lines_used; i++)
  {
    dump_line(i, &priv->lines[i]);
  }
}
