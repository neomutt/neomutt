/**
 * @file
 * Regex Colour
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
 * @page color_regex Regex Colour
 *
 * A set of regexes and colours that should be applied to a graphical object,
 * e.g Body of an Email.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "pattern/lib.h"
#include "color.h"
#include "context.h"
#include "mutt_globals.h"
#include "regex4.h"

/**
 * color_line_new - Create a new ColorLine
 * @retval ptr Newly allocated ColorLine
 */
static struct ColorLine *color_line_new(void)
{
  struct ColorLine *cl = mutt_mem_calloc(1, sizeof(struct ColorLine));

  cl->fg = COLOR_UNSET;
  cl->bg = COLOR_UNSET;

  return cl;
}

/**
 * add_pattern - Associate a colour to a pattern
 * @param top       List of existing colours
 * @param s         String to match
 * @param sensitive true if the pattern case-sensitive
 * @param fg        Foreground colour ID
 * @param bg        Background colour ID
 * @param attr      Attribute flags, e.g. A_BOLD
 * @param err       Buffer for error messages
 * @param is_index  true of this is for the index
 * @param match     Number of regex subexpression to match (0 for entire pattern)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * is_index used to store compiled pattern only for 'index' color object when
 * called from mutt_parse_color()
 */
enum CommandResult add_pattern(struct ColorLineList *top, const char *s,
                               bool sensitive, uint32_t fg, uint32_t bg, int attr,
                               struct Buffer *err, bool is_index, int match)
{
  struct ColorLine *tmp = NULL;

  STAILQ_FOREACH(tmp, top, entries)
  {
    if ((sensitive && mutt_str_equal(s, tmp->pattern)) ||
        (!sensitive && mutt_istr_equal(s, tmp->pattern)))
    {
      break;
    }
  }

  if (tmp)
  {
    if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    {
      if ((tmp->fg != fg) || (tmp->bg != bg))
      {
        mutt_color_free(tmp->fg, tmp->bg);
        tmp->fg = fg;
        tmp->bg = bg;
        attr |= mutt_color_alloc(fg, bg);
      }
      else
        attr |= (tmp->pair & ~A_BOLD);
    }
    tmp->pair = attr;
  }
  else
  {
    tmp = color_line_new();
    if (is_index)
    {
      struct Buffer *buf = mutt_buffer_pool_get();
      mutt_buffer_strcpy(buf, s);
      const char *const c_simple_search =
          cs_subset_string(NeoMutt->sub, "simple_search");
      mutt_check_simple(buf, NONULL(c_simple_search));
      tmp->color_pattern =
          mutt_pattern_comp(ctx_mailbox(Context), Context ? Context->menu : NULL,
                            buf->data, MUTT_PC_FULL_MSG, err);
      mutt_buffer_pool_release(&buf);
      if (!tmp->color_pattern)
      {
        color_line_free(&tmp, true);
        return MUTT_CMD_ERROR;
      }
    }
    else
    {
      uint16_t flags = 0;
      if (sensitive)
        flags = mutt_mb_is_lower(s) ? REG_ICASE : 0;
      else
        flags = REG_ICASE;

      const int r = REG_COMP(&tmp->regex, s, flags);
      if (r != 0)
      {
        regerror(r, &tmp->regex, err->data, err->dsize);
        color_line_free(&tmp, true);
        return MUTT_CMD_ERROR;
      }
    }
    tmp->pattern = mutt_str_dup(s);
    tmp->match = match;
    if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    {
      tmp->fg = fg;
      tmp->bg = bg;
      attr |= mutt_color_alloc(fg, bg);
    }
    tmp->pair = attr;
    STAILQ_INSERT_HEAD(top, tmp, entries);
  }

  /* force re-caching of index colors */
  const struct Mailbox *m = ctx_mailbox(Context);
  if (is_index && m)
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  return MUTT_CMD_SUCCESS;
}

