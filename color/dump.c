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
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "dump.h"
#include "pfile/lib.h"
#include "attr.h"
#include "color.h"
#include "parse_color.h"
#include "regex4.h"
#include "simple2.h"
#ifdef USE_DEBUG_COLOR
#include "debug.h"
#endif

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
  // if (ac->attrs == A_NORMAL)
  //   buf_add_printf(swatch, "\033[0m");
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

  // buf_addstr(swatch, "XXXXXX\033[0m");
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
 * color_log_attrs_list2 - Get a string to represent some attributes in the log
 * @param attrs Attributes, e.g. A_UNDERLINE
 * @retval ptr Generated string
 *
 * @note Do not free the returned string
 */
struct Slist *color_log_attrs_list2(int attrs)
{
  struct Slist *sl = slist_new(D_SLIST_SEP_SPACE);

  // We can ignore the A_NORMAL case
  if (attrs & A_BLINK)
    slist_add_string(sl, "blink");
  if (attrs & A_BOLD)
    slist_add_string(sl, "bold");
  if (attrs & A_ITALIC)
    slist_add_string(sl, "italic");
  if (attrs & A_REVERSE)
    slist_add_string(sl, "reverse");
  if (attrs & A_STANDOUT)
    slist_add_string(sl, "standout");
  if (attrs & A_UNDERLINE)
    slist_add_string(sl, "underline");

  if (slist_is_empty(sl))
    slist_free(&sl);

  return sl;
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
 * color_log_name2 - Get a string to represent a colour name
 * @param elem Colour to describe
 * @retval str Generated string
 *
 * @note Caller must free returned string
 */
const char *color_log_name2(struct ColorElement *elem)
{
  char name[64] = { 0 };

  if (elem->color < 0)
    return mutt_str_dup("default");

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

      snprintf(name, sizeof(name), "%s%s", prefix, mutt_map_get_name(elem->color, ColorNames));
      break;
    }

    case CT_PALETTE:
    {
      if (elem->color < 256)
        snprintf(name, sizeof(name), "color%d", elem->color);
      else
        snprintf(name, sizeof(name), "BAD:%d", elem->color); // LCOV_EXCL_LINE
      break;
    }

    case CT_RGB:
    {
      int r = (elem->color >> 16) & 0xff;
      int g = (elem->color >> 8) & 0xff;
      int b = (elem->color >> 0) & 0xff;
      snprintf(name, sizeof(name), "#%02x%02x%02x", r, g, b);
      break;
    }
  }

  return mutt_str_dup(name);
}

/**
 * measure_attrs - Measure a set of attributes
 * @param sl List of attribute strings
 *
 * Measure the length of all the attribute strings, including a space between each.
 */
int measure_attrs(const struct Slist *sl)
{
  if (slist_is_empty(sl))
    return 0;

  int len = 0;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &sl->head, entries)
  {
    len += mutt_str_len(np->data);
  }

  len += sl->count - 1;

  return len;
}

/**
 * regex_colors_dump - Dump all the Regex colours
 * @param pf  PagerFile to write to
 * @param all Show all the regex colours
 */
void regex_colors_dump(struct PagedFile *pf, bool all)
{
  struct Buffer *swatch = buf_pool_get();
  struct Buffer *pattern = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;
  struct ColorTableArray cta = ARRAY_HEAD_INITIALIZER;
  int len;

  for (enum ColorId cid = MT_COLOR_NONE; cid != MT_COLOR_MAX; cid++)
  {
    if (cid == MT_COLOR_STATUS)
      continue;

    if (!mutt_color_has_pattern(cid))
      continue;

    struct RegexColorList *rcl = regex_colors_get_list(cid);
    if (STAILQ_EMPTY(rcl) && !all)
      continue;

    const char *name = mutt_map_get_name(cid, ColorFields);

    // Widths of all the text columns
    int w_name = mutt_str_len(name);
    int w_attrs = 0;
    int w_fg = 0;
    int w_bg = 0;
    int w_pattern = 0;
    int w_match = 0;

    struct RegexColor *rc = NULL;
    STAILQ_FOREACH(rc, rcl, entries)
    {
      struct ColorTable ct = { 0 };

      struct AttrColor *ac = &rc->attr_color;
      ct.attr_color = ac;

      ct.attrs = color_log_attrs_list2(ac->attrs);
      w_attrs = MAX(w_attrs, measure_attrs(ct.attrs));

      ct.fg = color_log_name2(&ac->fg);
      w_fg = MAX(w_fg, mutt_str_len(ct.fg));

      ct.bg = color_log_name2(&ac->bg);
      w_bg = MAX(w_bg, mutt_str_len(ct.bg));

      buf_reset(pattern);
      pretty_var(rc->pattern, pattern);
      ct.pattern = buf_strdup(pattern);
      w_pattern = MAX(w_pattern, mutt_str_len(ct.pattern));

      ct.match = rc->match;
      if (ct.match > 0)
        w_match = 1;

      ARRAY_ADD(&cta, ct);
    }

    pr = paged_file_new_row(pf);
    buf_printf(buf, _("# Regex Color %s\n"), name);
    paged_row_add_colored_text(pr, MT_COLOR_COMMENT, buf_string(buf));

    struct ColorTable *ct = NULL;
    ARRAY_FOREACH(ct, &cta)
    {
      pr = paged_file_new_row(pf);
      paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "color");
      paged_row_add_text(pr, " ");

      len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, name);
      buf_printf(buf, "%*s", w_name - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = 0;
      if (!slist_is_empty(ct->attrs))
      {
        struct ListNode *np = NULL;
        STAILQ_FOREACH(np, &ct->attrs->head, entries)
        {
          len += paged_row_add_colored_text(pr, MT_COLOR_ATTRIBUTE, np->data);
          if (STAILQ_NEXT(np, entries))
            len += paged_row_add_text(pr, " ");
        }
      }
      buf_printf(buf, "%*s", w_attrs - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->fg);
      buf_printf(buf, "%*s", w_fg - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->bg);
      buf_printf(buf, "%*s", w_bg - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = paged_row_add_colored_text(pr, MT_COLOR_STRING, ct->pattern);
      buf_printf(buf, "%*s", w_pattern - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      if (ct->match > 0)
      {
        buf_printf(buf, "%d", ct->match);
        paged_row_add_colored_text(pr, MT_COLOR_NUMBER, buf_string(buf));
        paged_row_add_text(pr, " ");
      }
      else if (w_match > 0)
      {
        paged_row_add_text(pr, "  ");
      }

      color_log_color_attrs(ct->attr_color, swatch);
      paged_row_add_raw_text(pr, buf_string(swatch));

      paged_row_add_ac_text(pr, ct->attr_color, "XXXXXX");

      paged_row_add_raw_text(pr, "\033[0m");
      paged_row_add_newline(pr);
    }

    ARRAY_FOREACH(ct, &cta)
    {
      // Don't free name
      FREE(&ct->fg);
      FREE(&ct->bg);
      FREE(&ct->pattern);
      slist_free(&ct->attrs);
    }
    ARRAY_FREE(&cta);

    pr = paged_file_new_row(pf);
    paged_row_add_newline(pr);
  }

  buf_pool_release(&buf);
  buf_pool_release(&swatch);
  buf_pool_release(&pattern);
}

/**
 * simple_colors_dump - Dump all the Simple colours
 * @param pf  PagerFile to write to
 * @param all Show all simple colours
 */
void simple_colors_dump(struct PagedFile *pf, bool all)
{
  struct Buffer *swatch = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;

  // Widths of all the text columns
  int w_name = 0;
  int w_attrs = 0;
  int w_fg = 0;
  int w_bg = 0;
  struct ColorTableArray cta = ARRAY_HEAD_INITIALIZER;

  for (enum ColorId cid = MT_COLOR_NONE + 1; cid < MT_COLOR_MAX; cid++)
  {
    if (cid == MT_COLOR_STATUS)
      continue;

    if (mutt_color_has_pattern(cid))
      continue;

    struct AttrColor *ac = simple_color_get(cid);
    if (!attr_color_is_set(ac) && !all)
      continue;

    struct ColorTable ct = { 0 };

    ct.attr_color = ac;

    ct.name = mutt_map_get_name(cid, ColorFields);
    w_name = MAX(w_name, mutt_str_len(ct.name));

    ct.attrs = color_log_attrs_list2(ac->attrs);
    w_attrs = MAX(w_attrs, measure_attrs(ct.attrs));

    ct.fg = color_log_name2(&ac->fg);
    w_fg = MAX(w_fg, mutt_str_len(ct.fg));

    ct.bg = color_log_name2(&ac->bg);
    w_bg = MAX(w_bg, mutt_str_len(ct.bg));

    ARRAY_ADD(&cta, ct);
  }

  if (ARRAY_SIZE(&cta) > 0)
  {
    pr = paged_file_new_row(pf);
    paged_row_add_colored_text(pr, MT_COLOR_COMMENT, _("# Simple Colors\n"));

    int len;
    struct ColorTable *ct = NULL;
    ARRAY_FOREACH(ct, &cta)
    {
      pr = paged_file_new_row(pf);
      paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "color");
      paged_row_add_text(pr, " ");

      len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, ct->name);
      buf_printf(buf, "%*s", w_name - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = 0;
      if (!slist_is_empty(ct->attrs))
      {
        struct ListNode *np = NULL;
        STAILQ_FOREACH(np, &ct->attrs->head, entries)
        {
          len += paged_row_add_colored_text(pr, MT_COLOR_ATTRIBUTE, np->data);
          if (STAILQ_NEXT(np, entries))
            len += paged_row_add_text(pr, " ");
        }
      }
      buf_printf(buf, "%*s", w_attrs - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->fg);
      buf_printf(buf, "%*s", w_fg - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->bg);
      buf_printf(buf, "%*s", w_bg - len + 1, "");
      paged_row_add_text(pr, buf_string(buf));

      paged_row_add_colored_text(pr, MT_COLOR_COMMENT, "# ");

      color_log_color_attrs(ct->attr_color, swatch);
      paged_row_add_raw_text(pr, buf_string(swatch));

      paged_row_add_ac_text(pr, ct->attr_color, "XXXXXX");

      paged_row_add_raw_text(pr, "\033[0m");
      paged_row_add_newline(pr);
    }

    pr = paged_file_new_row(pf);
    paged_row_add_newline(pr);
  }

  struct ColorTable *ct = NULL;
  ARRAY_FOREACH(ct, &cta)
  {
    // Don't free name
    FREE(&ct->fg);
    FREE(&ct->bg);
    FREE(&ct->pattern);
    slist_free(&ct->attrs);
  }
  ARRAY_FREE(&cta);

  buf_pool_release(&buf);
  buf_pool_release(&swatch);
}

/**
 * status_colors_dump - Dump all the Status colours
 * @param pf  PagerFile to write to
 * @param all Show all status colours
 */
void status_colors_dump(struct PagedFile *pf, bool all)
{
  struct Buffer *swatch = buf_pool_get();
  struct Buffer *pattern = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  struct PagedRow *pr = NULL;
  const char *simple_fg = NULL;
  const char *simple_bg = NULL;
  struct Slist *simple_attrs = NULL;
  struct ColorTableArray cta = ARRAY_HEAD_INITIALIZER;
  int len;

  bool set = false;

  const enum ColorId cid = MT_COLOR_STATUS;
  struct AttrColor *ac = simple_color_get(cid);
  if (attr_color_is_set(ac))
    set = true;

  struct RegexColorList *rcl = regex_colors_get_list(cid);
  if (!STAILQ_EMPTY(rcl))
    set = true;

  if (!set && !all)
    goto done;

  const char *simple_name = "status";
  simple_attrs = color_log_attrs_list2(ac->attrs);
  simple_fg = color_log_name2(&ac->fg);
  simple_bg = color_log_name2(&ac->bg);

  // Widths of all the text columns
  int w_name = mutt_str_len(simple_name);
  int w_attrs = 0;
  int w_fg = 0;
  int w_bg = 0;
  int w_pattern = 0;
  int w_match = 0;

  w_attrs = measure_attrs(simple_attrs);
  w_fg = mutt_str_len(simple_fg);
  w_bg = mutt_str_len(simple_bg);

  struct RegexColor *rc = NULL;
  STAILQ_FOREACH(rc, rcl, entries)
  {
    struct ColorTable ct = { 0 };

    ac = &rc->attr_color;
    ct.attr_color = ac;

    ct.attrs = color_log_attrs_list2(ac->attrs);
    w_attrs = MAX(w_attrs, measure_attrs(ct.attrs));

    ct.fg = color_log_name2(&ac->fg);
    w_fg = MAX(w_fg, mutt_str_len(ct.fg));

    ct.bg = color_log_name2(&ac->bg);
    w_bg = MAX(w_bg, mutt_str_len(ct.bg));

    buf_reset(pattern);
    pretty_var(rc->pattern, pattern);
    ct.pattern = buf_strdup(pattern);
    w_pattern = MAX(w_pattern, mutt_str_len(ct.pattern));

    ct.match = rc->match;
    if (ct.match > 0)
      w_match = 1;

    ARRAY_ADD(&cta, ct);
  }

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pr, MT_COLOR_COMMENT, _("# Status Colors\n"));

  //--------------------------------------------------------------------------------------------------

  pr = paged_file_new_row(pf);
  paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "color");
  paged_row_add_text(pr, " ");

  len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, simple_name);
  buf_printf(buf, "%*s", w_name - len + 1, "");
  paged_row_add_text(pr, buf_string(buf));

  len = 0;
  if (!slist_is_empty(simple_attrs))
  {
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &simple_attrs->head, entries)
    {
      len += paged_row_add_colored_text(pr, MT_COLOR_ATTRIBUTE, np->data);
      if (STAILQ_NEXT(np, entries))
        len += paged_row_add_text(pr, " ");
    }
  }
  buf_printf(buf, "%*s", w_attrs - len + 1, "");
  paged_row_add_text(pr, buf_string(buf));

  len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, simple_fg);
  buf_printf(buf, "%*s", w_fg - len + 1, "");
  paged_row_add_text(pr, buf_string(buf));

  len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, simple_bg);
  buf_printf(buf, "%*s", w_bg - len + 1, "");
  paged_row_add_text(pr, buf_string(buf));

  len = w_pattern + 1;
  if (w_match > 0)
    len += 2;

  buf_printf(buf, "%*s", len, "");
  paged_row_add_text(pr, buf_string(buf));

  paged_row_add_colored_text(pr, MT_COLOR_COMMENT, "# ");

  color_log_color_attrs(ac, swatch);
  paged_row_add_raw_text(pr, buf_string(swatch));

  paged_row_add_ac_text(pr, ac, "XXXXXX");

  paged_row_add_raw_text(pr, "\033[0m");
  paged_row_add_newline(pr);

  //--------------------------------------------------------------------------------------------------

  struct ColorTable *ct = NULL;
  ARRAY_FOREACH(ct, &cta)
  {
    pr = paged_file_new_row(pf);
    paged_row_add_colored_text(pr, MT_COLOR_FUNCTION, "color");
    paged_row_add_text(pr, " ");

    len = paged_row_add_colored_text(pr, MT_COLOR_IDENTIFIER, simple_name);
    buf_printf(buf, "%*s", w_name - len + 1, "");
    paged_row_add_text(pr, buf_string(buf));

    len = 0;
    if (!slist_is_empty(ct->attrs))
    {
      struct ListNode *np = NULL;
      STAILQ_FOREACH(np, &ct->attrs->head, entries)
      {
        len += paged_row_add_colored_text(pr, MT_COLOR_ATTRIBUTE, np->data);
        if (STAILQ_NEXT(np, entries))
          len += paged_row_add_text(pr, " ");
      }
    }
    buf_printf(buf, "%*s", w_attrs - len + 1, "");
    paged_row_add_text(pr, buf_string(buf));

    len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->fg);
    buf_printf(buf, "%*s", w_fg - len + 1, "");
    paged_row_add_text(pr, buf_string(buf));

    len = paged_row_add_colored_text(pr, MT_COLOR_COLOR, ct->bg);
    buf_printf(buf, "%*s", w_bg - len + 1, "");
    paged_row_add_text(pr, buf_string(buf));

    len = paged_row_add_colored_text(pr, MT_COLOR_STRING, ct->pattern);
    buf_printf(buf, "%*s", w_pattern - len + 1, "");
    paged_row_add_text(pr, buf_string(buf));

    if (ct->match > 0)
    {
      buf_printf(buf, "%d", ct->match);
      paged_row_add_colored_text(pr, MT_COLOR_NUMBER, buf_string(buf));
      paged_row_add_text(pr, " ");
    }
    else if (w_match > 0)
    {
      paged_row_add_text(pr, "  ");
    }

    paged_row_add_colored_text(pr, MT_COLOR_COMMENT, "# ");

    color_log_color_attrs(ct->attr_color, swatch);
    paged_row_add_raw_text(pr, buf_string(swatch));

    paged_row_add_ac_text(pr, ct->attr_color, "XXXXXX");

    paged_row_add_raw_text(pr, "\033[0m");
    paged_row_add_newline(pr);
  }

  ARRAY_FOREACH(ct, &cta)
  {
    // Don't free name
    FREE(&ct->fg);
    FREE(&ct->bg);
    FREE(&ct->pattern);
    slist_free(&ct->attrs);
  }
  ARRAY_FREE(&cta);

  pr = paged_file_new_row(pf);
  paged_row_add_newline(pr);

done:
  FREE(&simple_fg);
  FREE(&simple_bg);
  slist_free(&simple_attrs);

  buf_pool_release(&swatch);
  buf_pool_release(&pattern);
  buf_pool_release(&buf);
}

/**
 * color_dump - Display all the colours in the Pager
 * @param pf  PagerFile to write to
 * @param acl Storage for temporary AttrColors
 * @param all Show all colours
 */
void color_dump(struct PagedFile *pf, struct AttrColorList *acl, bool all)
{
  if (!pf)
    return;

  simple_colors_dump(pf, all);
  status_colors_dump(pf, all);
  regex_colors_dump(pf, all);

#ifdef USE_DEBUG_COLOR
  merged_colors_dump(pf);
  ansi_colors_dump(pf);
  curses_colors_dump(pf, acl);
  log_paged_file(LL_DEBUG1, pf);
#endif
}
