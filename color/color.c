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

struct Colors Colors;

/**
 * get_color_line_list - Sanitize and return a ColorLineList
 * @param cll A ColorLineList
 * @retval cll The ColorLineList
 */
static struct ColorLineList *get_color_line_list(struct ColorLineList *cll)
{
  if (cll->stqh_last == NULL)
  {
    STAILQ_INIT(cll);
  }
  return cll;
}

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
 * defs_init - Initialise the simple colour definitions
 */
static void defs_init(void)
{
  memset(Colors.defs, A_NORMAL, MT_COLOR_MAX * sizeof(int));

  // Set some defaults
  Colors.defs[MT_COLOR_INDICATOR] = A_REVERSE;
  Colors.defs[MT_COLOR_MARKERS] = A_REVERSE;
  Colors.defs[MT_COLOR_SEARCH] = A_REVERSE;
#ifdef USE_SIDEBAR
  Colors.defs[MT_COLOR_SIDEBAR_HIGHLIGHT] = A_UNDERLINE;
#endif
  Colors.defs[MT_COLOR_STATUS] = A_REVERSE;
}

/**
 * defs_clear - Reset the simple colour definitions
 */
static void defs_clear(void)
{
  memset(Colors.defs, A_NORMAL, MT_COLOR_MAX * sizeof(int));
}

/**
 * quotes_init - Initialise the quoted-email colours
 */
static void quotes_init(void)
{
  memset(Colors.quotes, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  Colors.quotes_used = 0;
}

/**
 * quotes_clear - Reset the quoted-email colours
 */
static void quotes_clear(void)
{
  memset(Colors.quotes, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  Colors.quotes_used = 0;
}

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

  struct ColorList *p = Colors.user_colors;
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->ref_count)--;
      if (p->ref_count > 0)
        return;

      Colors.num_user_colors--;
      mutt_debug(LL_DEBUG1, "Color pairs used so far: %d\n", Colors.num_user_colors);

      if (p == Colors.user_colors)
      {
        Colors.user_colors = Colors.user_colors->next;
        FREE(&p);
        return;
      }
      q = Colors.user_colors;
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
  color_line_list_clear(mutt_color_attachments());
  color_line_list_clear(mutt_color_body());
  color_line_list_clear(mutt_color_headers());
  color_line_list_clear(mutt_color_index_author());
  color_line_list_clear(mutt_color_index_flags());
  color_line_list_clear(mutt_color_index());
  color_line_list_clear(mutt_color_index_subject());
  color_line_list_clear(mutt_color_index_tags());
  color_line_list_clear(mutt_color_status_line());

  defs_clear();
  quotes_clear();

  color_list_free(&Colors.user_colors);
}

/**
 * mutt_colors_cleanup - Cleanup all the colours
 */
void mutt_colors_cleanup(void)
{
  colors_clear();
  notify_free(&Colors.notify);
}

/**
 * mutt_colors_init - Initialize colours
 */
void mutt_colors_init(void)
{
  Colors.notify = notify_new();

  quotes_init();
  defs_init();

  start_color();

  notify_set_parent(Colors.notify, NeoMutt->notify);
}

/**
 * mutt_color_alloc - Allocate a colour pair
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 * @retval num Combined colour pair
 */
int mutt_color_alloc(uint32_t fg, uint32_t bg)
{
  struct ColorList *p = Colors.user_colors;

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
  if (++Colors.num_user_colors > COLOR_PAIRS)
    return A_NORMAL;

  /* find the smallest available index (object) */
  int i = 1;
  while (true)
  {
    p = Colors.user_colors;
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
  p->next = Colors.user_colors;
  Colors.user_colors = p;

  p->index = i;
  p->ref_count = 1;
  p->bg = bg;
  p->fg = fg;

  if (fg == COLOR_DEFAULT)
    fg = COLOR_UNSET;
  if (bg == COLOR_DEFAULT)
    bg = COLOR_UNSET;
  init_pair(i, fg, bg);

  mutt_debug(LL_DEBUG3, "Color pairs used so far: %d\n", Colors.num_user_colors);

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
  struct ColorList *p = Colors.user_colors;

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
 * mutt_color - Return the color of an object
 * @param id Object id
 * @retval num Color ID, e.g. #MT_COLOR_HEADER
 */
int mutt_color(enum ColorId id)
{
  return Colors.defs[id];
}

/**
 * mutt_color_status_line - Return the ColorLineList for the status_line
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_status_line(void)
{
  return get_color_line_list(&Colors.status_list);
}

/**
 * mutt_color_index - Return the ColorLineList for the index
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_index(void)
{
  return get_color_line_list(&Colors.index_list);
}

/**
 * mutt_color_headers - Return the ColorLineList for headers
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_headers(void)
{
  return get_color_line_list(&Colors.hdr_list);
}

/**
 * mutt_color_body - Return the ColorLineList for the body
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_body(void)
{
  return get_color_line_list(&Colors.body_list);
}

/**
 * mutt_color_attachments - Return the ColorLineList for the attachments
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_attachments(void)
{
  return get_color_line_list(&Colors.attach_list);
}

/**
 * mutt_color_index_author - Return the ColorLineList for author in the index
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_index_author(void)
{
  return get_color_line_list(&Colors.index_author_list);
}

/**
 * mutt_color_index_flags - Return the ColorLineList for flags in the index
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_index_flags(void)
{
  return get_color_line_list(&Colors.index_flags_list);
}

/**
 * mutt_color_index_subject - Return the ColorLineList for subject in the index
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_index_subject(void)
{
  return get_color_line_list(&Colors.index_subject_list);
}

/**
 * mutt_color_index_tags - Return the ColorLineList for tags in the index
 * @retval ptr ColorLineList
 */
struct ColorLineList *mutt_color_index_tags(void)
{
  return get_color_line_list(&Colors.index_tag_list);
}

/**
 * mutt_color_quote - Return the color of a quote, cycling through the used quotes
 * @param q Quote number
 * @retval num Color ID, e.g. MT_COLOR_QUOTED
 */
int mutt_color_quote(int q)
{
  const int used = Colors.quotes_used;
  if (used == 0)
    return 0;
  return Colors.quotes[q % used];
}

/**
 * mutt_color_quotes_used - Return the number of used quotes
 * @retval num Number of used quotes
 */
int mutt_color_quotes_used(void)
{
  return Colors.quotes_used;
}

/**
 * mutt_color_is_header - Colour is for an Email header
 * @param color_id Colour, e.g. #MT_COLOR_HEADER
 * @retval true Colour is for an Email header
 */
bool mutt_color_is_header(enum ColorId color_id)
{
  return (color_id == MT_COLOR_HEADER) || (color_id == MT_COLOR_HDRDEFAULT);
}
