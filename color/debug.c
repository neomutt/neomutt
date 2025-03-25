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
#include "pfile/lib.h"
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
 * @param pf   Paged File to write to
 */
void ansi_colors_dump(struct PagedFile *pf)
{
  struct MuttWindow *win = window_get_focus();
  if (!win || (win->type != WT_CUSTOM) || !win->parent || (win->parent->type != WT_PAGER))
    return;

  struct PagerPrivateData *priv = win->parent->wdata;
  if (!priv || TAILQ_EMPTY(&priv->ansi_list))
    return;

  struct AttrColorList *acl = &priv->ansi_list;

  // Widths of all the text columns
  int w_attrs = 0;
  int w_fg = 0;
  int w_bg = 0;
  struct ColorTableArray cta = ARRAY_HEAD_INITIALIZER;

  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, acl, entries)
  {
    struct ColorTable ct = { 0 };

    ct.attr_color = ac;

    ct.attrs = color_log_attrs_list2(ac->attrs);
    w_attrs = MAX(w_attrs, measure_attrs(ct.attrs));

    ct.fg = color_log_name2(&ac->fg);
    w_fg = MAX(w_fg, mutt_str_len(ct.fg));

    ct.bg = color_log_name2(&ac->bg);
    w_bg = MAX(w_bg, mutt_str_len(ct.bg));

    ARRAY_ADD(&cta, ct);
  }

  struct Buffer *swatch = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, _("# Ansi Colors\n"));

  int len;
  struct ColorTable *ct = NULL;
  ARRAY_FOREACH(ct, &cta)
  {
    pr = paged_file_new_row(pf);

    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "# ");

    buf_printf(buf, "%d", ARRAY_FOREACH_IDX_ct + 1);
    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_NUMBER, buf_string(buf));
    buf_printf(buf, "%*s", 3 - len, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    len = 0;
    if (!slist_is_empty(ct->attrs))
    {
      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, &ct->attrs->head, entries)
      {
        len += paged_row_add_colored_text(pf->source, pr, MT_COLOR_ATTRIBUTE, np->data);
        if (STAILQ_NEXT(np, entries))
          len += paged_row_add_text(pf->source, pr, " ");
      }
    }
    buf_printf(buf, "%*s", w_attrs - len + 1, "");
    paged_row_add_text(pf->source, pr, buf_string(buf));

    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, ct->fg);
    buf_printf(buf, "%*s", w_fg - len + 1, "");
    paged_row_add_text(pf->source, pr, buf_string(buf));

    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, ct->bg);
    buf_printf(buf, "%*s", w_bg - len + 1, "");
    paged_row_add_text(pf->source, pr, buf_string(buf));

    color_log_color_attrs(ct->attr_color, swatch);
    // paged_row_add_raw_text(pf->source, pr, buf_string(swatch));
    paged_row_add_text(pf->source, pr, buf_string(swatch));

    // paged_row_add_ac_text(pf->source, pr, ct->attr_color, "XXXXXX");
    paged_row_add_text(pf->source, pr, "XXXXXX");

    if (!buf_is_empty(swatch))
    {
      // paged_row_add_raw_text(pf->source, pr, "\033[0m");
      paged_row_add_text(pf->source, pr, "\033[0m");
    }

    paged_row_add_newline(pf->source, pr);
  }

  pr = paged_file_new_row(pf);
  paged_row_add_newline(pf->source, pr);

  buf_pool_release(&swatch);
  buf_pool_release(&buf);
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
 * @param pf  Paged File to write to
 * @param acl Storage for temporary AttrColors
 */
void curses_colors_dump(struct PagedFile *pf, struct AttrColorList *acl)
{
  if (TAILQ_EMPTY(&CursesColors))
    return;

  struct Buffer *swatch = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, _("# Curses Colors\n"));

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "# Index rc fg      bg      Color\n");

  int len;
  struct CursesColor *cc = NULL;
  TAILQ_FOREACH(cc, &CursesColors, entries)
  {
    pr = paged_file_new_row(pf);

    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "# ");

    buf_printf(buf, "%d", cc->index);
    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_NUMBER, buf_string(buf));
    buf_printf(buf, "%*s", 6 - len, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    buf_printf(buf, "%d", cc->ref_count);
    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_NUMBER, buf_string(buf));
    buf_printf(buf, "%*s", 3 - len, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    if (cc->fg == -1)
    {
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "-       ");
    }
    else
    {
      buf_printf(buf, "#%06x", cc->fg);
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, buf_string(buf));
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, " ");
    }

    if (cc->bg == -1)
    {
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "-       ");
    }
    else
    {
      buf_printf(buf, "#%06x", cc->bg);
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, buf_string(buf));
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, " ");
    }

    struct AttrColor *ac = attr_color_new();
    ac->fg.type = CT_RGB;
    ac->fg.color = cc->fg;
    ac->bg.type = CT_RGB;
    ac->bg.color = cc->bg;
    cc->ref_count++;
    ac->curses_color = cc;
    TAILQ_INSERT_TAIL(acl, ac, entries);

    color_log_color_attrs(ac, swatch);
    // paged_row_add_raw_text(pf->source, pr, buf_string(swatch));
    paged_row_add_text(pf->source, pr, buf_string(swatch));

    // paged_row_add_ac_text(pf->source, pr, ac, "XXXXXX");
    paged_row_add_text(pf->source, pr, "XXXXXX");

    if (!buf_is_empty(swatch))
    {
      // paged_row_add_raw_text(pf->source, pr, "\033[0m");
      paged_row_add_text(pf->source, pr, "\033[0m");
    }

    paged_row_add_newline(pf->source, pr);
  }

  pr = paged_file_new_row(pf);
  paged_row_add_newline(pf->source, pr);
  buf_pool_release(&swatch);
  buf_pool_release(&buf);
}

/**
 * merged_colors_dump - Dump all the Merged colours
 * @param pf   Paged File to write to
 */
void merged_colors_dump(struct PagedFile *pf)
{
  if (TAILQ_EMPTY(&MergedColors))
    return;

  // Widths of all the text columns
  int w_attrs = 0;
  int w_fg = 0;
  int w_bg = 0;
  struct ColorTableArray cta = ARRAY_HEAD_INITIALIZER;

  struct AttrColor *ac = NULL;
  TAILQ_FOREACH(ac, &MergedColors, entries)
  {
    struct ColorTable ct = { 0 };

    ct.attr_color = ac;

    ct.attrs = color_log_attrs_list2(ac->attrs);
    w_attrs = MAX(w_attrs, measure_attrs(ct.attrs));

    ct.fg = color_log_name2(&ac->fg);
    w_fg = MAX(w_fg, mutt_str_len(ct.fg));

    ct.bg = color_log_name2(&ac->bg);
    w_bg = MAX(w_bg, mutt_str_len(ct.bg));

    ARRAY_ADD(&cta, ct);
  }

  struct Buffer *swatch = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, _("# Merged Colors\n"));

  int len;
  struct ColorTable *ct = NULL;
  ARRAY_FOREACH(ct, &cta)
  {
    pr = paged_file_new_row(pf);

    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, "# ");

    buf_printf(buf, "%d", ARRAY_FOREACH_IDX_ct + 1);
    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_NUMBER, buf_string(buf));
    buf_printf(buf, "%*s", 3 - len, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    len = 0;
    if (!slist_is_empty(ct->attrs))
    {
      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, &ct->attrs->head, entries)
      {
        len += paged_row_add_colored_text(pf->source, pr, MT_COLOR_ATTRIBUTE, np->data);
        if (STAILQ_NEXT(np, entries))
          len += paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, " ");
      }
    }
    buf_printf(buf, "%*s", w_attrs - len + 1, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, ct->fg);
    buf_printf(buf, "%*s", w_fg - len + 1, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    len = paged_row_add_colored_text(pf->source, pr, MT_COLOR_COLOR, ct->bg);
    buf_printf(buf, "%*s", w_bg - len + 1, "");
    paged_row_add_colored_text(pf->source, pr, MT_COLOR_COMMENT, buf_string(buf));

    color_log_color_attrs(ct->attr_color, swatch);
    // paged_row_add_raw_text(pf->source, pr, buf_string(swatch));
    paged_row_add_text(pf->source, pr, buf_string(swatch));

    // paged_row_add_ac_text(pf->source, pr, ct->attr_color, "XXXXXX");
    paged_row_add_text(pf->source, pr, "XXXXXX");

    if (!buf_is_empty(swatch))
    {
      // paged_row_add_raw_text(pf->source, pr, "\033[0m");
      paged_row_add_text(pf->source, pr, "\033[0m");
    }

    paged_row_add_newline(pf->source, pr);
  }

  pr = paged_file_new_row(pf);
  paged_row_add_newline(pf->source, pr);

  buf_pool_release(&swatch);
  buf_pool_release(&buf);

  ARRAY_FOREACH(ct, &cta)
  {
    FREE(&ct->fg);
    FREE(&ct->bg);
  }
  ARRAY_FREE(&cta);
}

/**
 * log_paged_file - Dump a PagedFile to the log
 * @param level Log level, e.g. #LL_DEBUG1
 * @param pf    Paged File to dump
 */
void log_paged_file(enum LogLevel level, struct PagedFile *pf)
{
  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, &pf->rows)
  {
    mutt_debug(level, "%s", paged_row_get_virtual_text(pr, NULL));
  }
}
