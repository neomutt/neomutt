/**
 * @file
 * Quoted-Email colours
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
 * @page color_quote Quoted-Email colours
 *
 * Manage the colours of quoted emails.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "quoted.h"
#include "attr.h"
#include "color.h"
#include "command2.h"
#include "curses2.h"
#include "debug.h"
#include "notify2.h"
#include "simple2.h"

struct AttrColor QuotedColors[COLOR_QUOTES_MAX]; ///< Array of colours for quoted email text
static int NumQuotedColors = 0; ///< Number of colours for quoted email text

/**
 * quoted_colors_init - Initialise the Quoted colours
 */
void quoted_colors_init(void)
{
  for (size_t i = 0; i < COLOR_QUOTES_MAX; i++)
  {
    struct AttrColor *ac = &QuotedColors[i];
    ac->fg.color = COLOR_DEFAULT;
    ac->bg.color = COLOR_DEFAULT;
  }
  NumQuotedColors = 0;
}

/**
 * quoted_colors_reset - Reset the quoted-email colours
 */
void quoted_colors_reset(void)
{
  color_debug(LL_DEBUG5, "QuotedColors: reset\n");
  for (size_t i = 0; i < COLOR_QUOTES_MAX; i++)
  {
    attr_color_clear(&QuotedColors[i]);
  }
  NumQuotedColors = 0;
}

/**
 * quoted_colors_cleanup - Cleanup the quoted-email colours
 */
void quoted_colors_cleanup(void)
{
  quoted_colors_reset();
}

/**
 * quoted_colors_get - Return the color of a quote, cycling through the used quotes
 * @param q Quote level
 * @retval enum #ColorId, e.g. #MT_COLOR_QUOTED
 */
struct AttrColor *quoted_colors_get(int q)
{
  if (NumQuotedColors == 0)
    return NULL;
  return &QuotedColors[q % NumQuotedColors];
}

/**
 * quoted_colors_num_used - Return the number of used quotes
 * @retval num Number of used quotes
 */
int quoted_colors_num_used(void)
{
  return NumQuotedColors;
}

/**
 * find_highest_used - Find the highest-numbered quotedN in use
 * @retval num Highest number
 */
static int find_highest_used(void)
{
  for (int i = COLOR_QUOTES_MAX - 1; i >= 0; i--)
  {
    if (attr_color_is_set(&QuotedColors[i]))
      return i + 1;
  }
  return 0;
}

/**
 * quoted_colors_parse_color - Parse the 'color quoted' command
 * @param cid     Colour Id, should be #MT_COLOR_QUOTED
 * @param ac_val  Colour value to use
 * @param q_level Quoting depth level
 * @param rc      Return code, e.g. #MUTT_CMD_SUCCESS
 * @param err     Buffer for error messages
 * @retval true Colour was parsed
 */
bool quoted_colors_parse_color(enum ColorId cid, struct AttrColor *ac_val,
                               int q_level, int *rc, struct Buffer *err)
{
  if (!COLOR_QUOTED(cid))
    return false;

  color_debug(LL_DEBUG5, "quoted %d\n", q_level);
  if (q_level >= COLOR_QUOTES_MAX)
  {
    buf_printf(err, _("Maximum quoting level is %d"), COLOR_QUOTES_MAX - 1);
    return false;
  }

  if (q_level >= NumQuotedColors)
    NumQuotedColors = q_level + 1;

  struct AttrColor *ac = &QuotedColors[q_level];

  attr_color_overwrite(ac, ac_val);

  struct CursesColor *cc = ac->curses_color;
  if (!cc)
    NumQuotedColors = find_highest_used();

  struct Buffer *buf = buf_pool_get();
  get_colorid_name(cid, buf);
  color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf->data);
  buf_pool_release(&buf);

  if (q_level == 0)
  {
    // Copy the colour into the SimpleColors
    struct AttrColor *ac_quoted = simple_color_get(MT_COLOR_QUOTED);
    curses_color_free(&ac_quoted->curses_color);
    *ac_quoted = *ac;
    ac_quoted->ref_count = 1;
    if (ac_quoted->curses_color)
    {
      ac_quoted->curses_color->ref_count++;
      curses_color_dump(cc, "curses rc++");
    }
  }

  struct EventColor ev_c = { cid, ac };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);

  curses_colors_dump(buf);

  *rc = MUTT_CMD_SUCCESS;
  return true;
}

/**
 * quoted_colors_parse_uncolor - Parse the 'uncolor quoted' command
 * @param cid     Colour Id, should be #MT_COLOR_QUOTED
 * @param q_level Quoting depth level
 * @param err     Buffer for error messages
 * @retval enum CommandResult, e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult quoted_colors_parse_uncolor(enum ColorId cid, int q_level,
                                               struct Buffer *err)
{
  color_debug(LL_DEBUG5, "unquoted %d\n", q_level);

  struct AttrColor *ac = &QuotedColors[q_level];
  attr_color_clear(ac);

  NumQuotedColors = find_highest_used();

  struct EventColor ev_c = { cid, ac };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

  return MUTT_CMD_SUCCESS;
}
