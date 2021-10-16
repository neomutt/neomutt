/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page color_color Color and attribute parsing
 *
 * Color and attribute parsing
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "pattern/lib.h"
#include "context.h"
#include "init.h"
#include "mutt_globals.h"
#include "options.h"

struct ColorList *UserColors; ///< Array of user colours
int NumUserColors;            ///< Number of user colours

/**
 * struct ColorList - A set of colors
 */
struct ColorList
{
  /* TrueColor uses 24bit. Use fixed-width integer type to make sure it fits.
   * Use the upper 8 bits to store flags.  */
  uint32_t fg;            ///< Foreground colour
  uint32_t bg;            ///< Background colour
  short index;            ///< Index number
  short ref_count;        ///< Number of users
  struct ColorList *next; ///< Linked list
};

const struct Mapping ColorNames[] = {
  // clang-format off
  { "black",   COLOR_BLACK },
  { "blue",    COLOR_BLUE },
  { "cyan",    COLOR_CYAN },
  { "green",   COLOR_GREEN },
  { "magenta", COLOR_MAGENTA },
  { "red",     COLOR_RED },
  { "white",   COLOR_WHITE },
  { "yellow",  COLOR_YELLOW },
  { "default", COLOR_DEFAULT },
  { 0, 0 },
  // clang-format on
};

/**
 * color_list_free - Free the list of curses colours
 * @param ptr Colours
 */
static void color_list_free(struct ColorList **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ColorList *cl = *ptr;
  struct ColorList *next = NULL;

  while (cl)
  {
    next = cl->next;
    FREE(&cl);
    cl = next;
  }
  *ptr = NULL;
}

/**
 * mutt_color_free - Free a colour
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 *
 * If there are no more users, the resource will be freed.
 */
void mutt_color_free(uint32_t fg, uint32_t bg)
{
  struct ColorList *q = NULL;

  struct ColorList *p = UserColors;
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->ref_count)--;
      if (p->ref_count > 0)
        return;

      NumUserColors--;
      mutt_debug(LL_DEBUG1, "Color pairs used so far: %d\n", NumUserColors);

      if (p == UserColors)
      {
        UserColors = UserColors->next;
        FREE(&p);
        return;
      }
      q = UserColors;
      while (q)
      {
        if (q->next == p)
        {
          q->next = p->next;
          FREE(&p);
          return;
        }
        q = q->next;
      }
      /* can't get here */
    }
    p = p->next;
  }
}

/**
 * color_line_free - Free a ColorLine
 * @param ptr         ColorLine to free
 * @param free_colors If true, free its colours too
 */
void color_line_free(struct ColorLine **ptr, bool free_colors)
{
  if (!ptr || !*ptr)
    return;

  struct ColorLine *cl = *ptr;

  if (free_colors && (cl->fg != COLOR_UNSET) && (cl->bg != COLOR_UNSET))
    mutt_color_free(cl->fg, cl->bg);

  regfree(&cl->regex);
  mutt_pattern_free(&cl->color_pattern);
  FREE(&cl->pattern);
  FREE(ptr);
}

/**
 * color_line_list_clear - Clear a list of colours
 * @param list ColorLine List
 */
void color_line_list_clear(struct ColorLineList *list)
{
  struct ColorLine *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, list, entries, tmp)
  {
    STAILQ_REMOVE(list, np, ColorLine, entries);
    color_line_free(&np, true);
  }
}

/**
 * colors_clear - Reset all the colours
 */
void colors_clear(void)
{
  simple_colors_clear();
  quoted_colors_clear();
  regex_colors_clear();

  color_list_free(&UserColors);
}

/**
 * mutt_colors_cleanup - Cleanup all the colours
 */
void mutt_colors_cleanup(void)
{
  colors_clear();
  color_notify_free();
}

/**
 * mutt_colors_init - Initialize colours
 */
void mutt_colors_init(void)
{
  color_notify_init();
  simple_colors_init();
  quoted_colors_init();
  regex_colors_init();

  start_color();
}

/**
 * mutt_color_alloc - Allocate a colour pair
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 * @retval num Combined colour pair
 */
int mutt_color_alloc(uint32_t fg, uint32_t bg)
{
  struct ColorList *p = UserColors;

  /* check to see if this color is already allocated to save space */
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->ref_count)++;
      return COLOR_PAIR(p->index);
    }
    p = p->next;
  }

  /* check to see if there are colors left */
  if (++NumUserColors > COLOR_PAIRS)
    return A_NORMAL;

  /* find the smallest available index (object) */
  int i = 1;
  while (true)
  {
    p = UserColors;
    while (p)
    {
      if (p->index == i)
        break;
      p = p->next;
    }
    if (!p)
      break;
    i++;
  }

  /* Check for pair overflow too.
   * We are currently using init_pair(), which only accepts size short. */
  if (i > SHRT_MAX)
    return (0);

  p = mutt_mem_malloc(sizeof(struct ColorList));
  p->next = UserColors;
  UserColors = p;

  p->index = i;
  p->ref_count = 1;
  p->bg = bg;
  p->fg = fg;

  if (fg == COLOR_DEFAULT)
    fg = COLOR_UNSET;
  if (bg == COLOR_DEFAULT)
    bg = COLOR_UNSET;
  init_pair(i, fg, bg);

  mutt_debug(LL_DEBUG3, "Color pairs used so far: %d\n", NumUserColors);

  return COLOR_PAIR(p->index);
}

/**
 * mutt_lookup_color - Get the colours from a colour pair
 * @param[in]  pair Colour pair
 * @param[out] fg   Foreground colour (OPTIONAL)
 * @param[out] bg   Background colour (OPTIONAL)
 * @retval  0 Success
 * @retval -1 Error
 */
static int mutt_lookup_color(short pair, uint32_t *fg, uint32_t *bg)
{
  struct ColorList *p = UserColors;

  while (p)
  {
    if (COLOR_PAIR(p->index) == pair)
    {
      if (fg)
        *fg = p->fg;
      if (bg)
        *bg = p->bg;
      return 0;
    }
    p = p->next;
  }
  return -1;
}

/**
 * mutt_color_combine - Combine two colours
 * @param fg_attr Colour pair of foreground to use
 * @param bg_attr Colour pair of background to use
 * @retval num Colour pair of combined colour
 */
int mutt_color_combine(uint32_t fg_attr, uint32_t bg_attr)
{
  uint32_t fg = COLOR_DEFAULT;
  uint32_t bg = COLOR_DEFAULT;

  mutt_lookup_color(fg_attr, &fg, NULL);
  mutt_lookup_color(bg_attr, NULL, &bg);

  if ((fg == COLOR_DEFAULT) && (bg == COLOR_DEFAULT))
    return A_NORMAL;
  return mutt_color_alloc(fg, bg);
}

/**
 * mutt_color_quote - Return the color of a quote, cycling through the used quotes
 * @param q Quote number
 * @retval num Color ID, e.g. MT_COLOR_QUOTED
 */
int mutt_color_quote(int q)
{
  const int used = NumQuotedColors;
  if (used == 0)
    return 0;
  return QuotedColors[q % used];
}

/**
 * mutt_color_quotes_used - Return the number of used quotes
 * @retval num Number of used quotes
 */
int mutt_color_quotes_used(void)
{
  return NumQuotedColors;
}
