/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2012 Michael R. Elkins <me@mutt.org>
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
 * @page color Color and attribute parsing
 *
 * Color and attribute parsing
 */

#include "config.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "color.h"
#include "context.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "options.h"
#include "pattern.h"
#ifdef USE_SLANG_CURSES
#include <assert.h>
#endif

/* globals */
int *ColorQuote = NULL;
int ColorQuoteUsed;
int ColorDefs[MT_COLOR_MAX];
struct ColorLineList ColorAttachList = STAILQ_HEAD_INITIALIZER(ColorAttachList);
struct ColorLineList ColorBodyList = STAILQ_HEAD_INITIALIZER(ColorBodyList);
struct ColorLineList ColorHdrList = STAILQ_HEAD_INITIALIZER(ColorHdrList);
struct ColorLineList ColorIndexAuthorList = STAILQ_HEAD_INITIALIZER(ColorIndexAuthorList);
struct ColorLineList ColorIndexFlagsList = STAILQ_HEAD_INITIALIZER(ColorIndexFlagsList);
struct ColorLineList ColorIndexList = STAILQ_HEAD_INITIALIZER(ColorIndexList);
struct ColorLineList ColorIndexSubjectList = STAILQ_HEAD_INITIALIZER(ColorIndexSubjectList);
struct ColorLineList ColorIndexTagList = STAILQ_HEAD_INITIALIZER(ColorIndexTagList);
struct ColorLineList ColorStatusList = STAILQ_HEAD_INITIALIZER(ColorStatusList);

/* local to this file */
static int ColorQuoteSize;

#ifdef HAVE_COLOR

#define COLOR_DEFAULT (-2)
#define COLOR_UNSET UINT32_MAX

/*
 * Flags for the high 8bits of the color value.
 *
 * Note that no flag means it's a palette color.
 */
#define RGB24 (1u << 24)

/**
 * struct ColorList - A set of colors
 */
struct ColorList
{
  /* TrueColor uses 24bit. Use fixed-width integer type to make sure it fits.
   * Use the upper 8 bits to store flags.  */
  uint32_t fg;
  uint32_t bg;
  short index;
  short count;
  struct ColorList *next;
};

static struct ColorList *ColorList = NULL;
static int UserColors = 0;

static const struct Mapping Colors[] = {
  { "black", COLOR_BLACK },
  { "blue", COLOR_BLUE },
  { "cyan", COLOR_CYAN },
  { "green", COLOR_GREEN },
  { "magenta", COLOR_MAGENTA },
  { "red", COLOR_RED },
  { "white", COLOR_WHITE },
  { "yellow", COLOR_YELLOW },
#if defined(USE_SLANG_CURSES) || defined(HAVE_USE_DEFAULT_COLORS)
  { "default", COLOR_DEFAULT },
#endif
  { 0, 0 },
};

#endif /* HAVE_COLOR */

static const struct Mapping Fields[] = {
  { "attachment", MT_COLOR_ATTACHMENT },
  { "attach_headers", MT_COLOR_ATTACH_HEADERS },
  { "body", MT_COLOR_BODY },
  { "bold", MT_COLOR_BOLD },
  { "error", MT_COLOR_ERROR },
  { "hdrdefault", MT_COLOR_HDEFAULT },
  { "header", MT_COLOR_HEADER },
  { "index", MT_COLOR_INDEX },
  { "index_author", MT_COLOR_INDEX_AUTHOR },
  { "index_collapsed", MT_COLOR_INDEX_COLLAPSED },
  { "index_date", MT_COLOR_INDEX_DATE },
  { "index_flags", MT_COLOR_INDEX_FLAGS },
  { "index_label", MT_COLOR_INDEX_LABEL },
  { "index_number", MT_COLOR_INDEX_NUMBER },
  { "index_size", MT_COLOR_INDEX_SIZE },
  { "index_subject", MT_COLOR_INDEX_SUBJECT },
  { "index_tag", MT_COLOR_INDEX_TAG },
  { "index_tags", MT_COLOR_INDEX_TAGS },
  { "indicator", MT_COLOR_INDICATOR },
  { "markers", MT_COLOR_MARKERS },
  { "message", MT_COLOR_MESSAGE },
  { "normal", MT_COLOR_NORMAL },
  { "options", MT_COLOR_OPTIONS },
  { "progress", MT_COLOR_PROGRESS },
  { "prompt", MT_COLOR_PROMPT },
  { "quoted", MT_COLOR_QUOTED },
  { "search", MT_COLOR_SEARCH },
#ifdef USE_SIDEBAR
  { "sidebar_divider", MT_COLOR_DIVIDER },
  { "sidebar_flagged", MT_COLOR_FLAGGED },
  { "sidebar_highlight", MT_COLOR_HIGHLIGHT },
  { "sidebar_indicator", MT_COLOR_SB_INDICATOR },
  { "sidebar_new", MT_COLOR_NEW },
  { "sidebar_ordinary", MT_COLOR_ORDINARY },
  { "sidebar_spoolfile", MT_COLOR_SB_SPOOLFILE },
#endif
  { "signature", MT_COLOR_SIGNATURE },
  { "status", MT_COLOR_STATUS },
  { "tilde", MT_COLOR_TILDE },
  { "tree", MT_COLOR_TREE },
  { "underline", MT_COLOR_UNDERLINE },
  { NULL, 0 },
};

static const struct Mapping ComposeFields[] = {
  { "header", MT_COLOR_COMPOSE_HEADER },
  { "security_encrypt", MT_COLOR_COMPOSE_SECURITY_ENCRYPT },
  { "security_sign", MT_COLOR_COMPOSE_SECURITY_SIGN },
  { "security_both", MT_COLOR_COMPOSE_SECURITY_BOTH },
  { "security_none", MT_COLOR_COMPOSE_SECURITY_NONE },
  { NULL, 0 }
};

#define COLOR_QUOTE_INIT 8

/**
 * color_line_new - Create a new ColorLine
 * @retval ptr Newly allocated ColorLine
 */
static struct ColorLine *color_line_new(void)
{
  struct ColorLine *p = mutt_mem_calloc(1, sizeof(struct ColorLine));

  p->fg = COLOR_UNSET;
  p->bg = COLOR_UNSET;

  return p;
}

/**
 * color_line_free - Free a ColorLine
 * @param ptr         ColorLine to free
 * @param free_colors If true, free its colours too
 */
static void color_line_free(struct ColorLine **ptr, bool free_colors)
{
  if (!ptr || !*ptr)
    return;

  struct ColorLine *cl = *ptr;

#ifdef HAVE_COLOR
  if (free_colors && (cl->fg != COLOR_UNSET) && (cl->bg != COLOR_UNSET))
    mutt_color_free(cl->fg, cl->bg);
#endif

  regfree(&cl->regex);
  mutt_pattern_free(&cl->color_pattern);
  FREE(&cl->pattern);
  FREE(ptr);
}

/**
 * mutt_color_init - Set up the default colours
 */
void mutt_color_init(void)
{
  memset(ColorDefs, A_NORMAL, sizeof(int) * MT_COLOR_MAX);
  ColorQuote = mutt_mem_malloc(COLOR_QUOTE_INIT * sizeof(int));
  memset(ColorQuote, A_NORMAL, sizeof(int) * COLOR_QUOTE_INIT);
  ColorQuoteSize = COLOR_QUOTE_INIT;
  ColorQuoteUsed = 0;

  /* set some defaults */
  ColorDefs[MT_COLOR_STATUS] = A_REVERSE;
  ColorDefs[MT_COLOR_INDICATOR] = A_REVERSE;
  ColorDefs[MT_COLOR_SEARCH] = A_REVERSE;
  ColorDefs[MT_COLOR_MARKERS] = A_REVERSE;
#ifdef USE_SIDEBAR
  ColorDefs[MT_COLOR_HIGHLIGHT] = A_UNDERLINE;
#endif
  /* special meaning: toggle the relevant attribute */
  ColorDefs[MT_COLOR_BOLD] = 0;
  ColorDefs[MT_COLOR_UNDERLINE] = 0;

#ifdef HAVE_COLOR
  start_color();
#endif
}

#ifdef HAVE_COLOR

#ifdef USE_SLANG_CURSES
/**
 * get_color_name - Get a colour's name from its ID
 * @param dest    Buffer for the result
 * @param destlen Length of buffer
 * @param val     Colour ID to look up
 * @retval ptr Pointer to the results buffer
 */
static char *get_color_name(char *dest, size_t destlen, uint32_t val)
{
  static const char *const missing[3] = { "brown", "lightgray", "default" };

  if (val & RGB24)
  {
    assert(snprintf(dest, destlen, "#%06X", val & 0xffffff) == 7);
    return dest;
  }

  switch (val)
  {
    case COLOR_YELLOW:
      mutt_str_strfcpy(dest, missing[0], destlen);
      return dest;

    case COLOR_WHITE:
      mutt_str_strfcpy(dest, missing[1], destlen);
      return dest;

    case COLOR_DEFAULT:
      mutt_str_strfcpy(dest, missing[2], destlen);
      return dest;
  }

  for (int i = 0; Colors[i].name; i++)
  {
    if (Colors[i].value == val)
    {
      mutt_str_strfcpy(dest, Colors[i].name, destlen);
      return dest;
    }
  }

  /* Sigh. If we got this far, the color is of the form 'colorN'
   * Slang can handle this itself, so just return 'colorN' */
  snprintf(dest, destlen, "color%d", val);

  return dest;
}
#endif

/**
 * mutt_color_alloc - Allocate a colour pair
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 * @retval num Combined colour pair
 */
int mutt_color_alloc(uint32_t fg, uint32_t bg)
{
#ifdef USE_SLANG_CURSES
  char fgc[128], bgc[128];
#endif
  struct ColorList *p = ColorList;
  int i;

  /* check to see if this color is already allocated to save space */
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->count)++;
      return COLOR_PAIR(p->index);
    }
    p = p->next;
  }

  /* check to see if there are colors left */
  if (++UserColors > COLOR_PAIRS)
    return A_NORMAL;

  /* find the smallest available index (object) */
  i = 1;
  while (true)
  {
    p = ColorList;
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

  p = mutt_mem_malloc(sizeof(struct ColorList));
  p->next = ColorList;
  ColorList = p;

  p->index = i;
  p->count = 1;
  p->bg = bg;
  p->fg = fg;

#ifdef USE_SLANG_CURSES
  /*
   * If using s-lang always use SLtt_set_color which allows using truecolor
   * values. Note that I couldn't figure out if s-lang somehow reports
   * truecolor support.
   */
  SLtt_set_color(i, NULL, get_color_name(fgc, sizeof(fgc), fg),
                 get_color_name(bgc, sizeof(bgc), bg));
#else
#ifdef HAVE_USE_DEFAULT_COLORS
  if (fg == COLOR_DEFAULT)
    fg = COLOR_UNSET;
  if (bg == COLOR_DEFAULT)
    bg = COLOR_UNSET;
#endif
  init_pair(i, fg, bg);
#endif

  mutt_debug(LL_DEBUG3, "Color pairs used so far: %d\n", UserColors);

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
  struct ColorList *p = ColorList;

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
 * mutt_color_free - Free a colour
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 *
 * If there are no more users, the resource will be freed.
 */
void mutt_color_free(uint32_t fg, uint32_t bg)
{
  struct ColorList *q = NULL;

  struct ColorList *p = ColorList;
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->count)--;
      if (p->count > 0)
        return;

      UserColors--;
      mutt_debug(LL_DEBUG1, "Color pairs used so far: %d\n", UserColors);

      if (p == ColorList)
      {
        ColorList = ColorList->next;
        FREE(&p);
        return;
      }
      q = ColorList;
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

#endif /* HAVE_COLOR */

#ifdef HAVE_COLOR

/**
 * parse_color_name - Parse a colour name
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attr  Attribute flags, e.g. A_BOLD
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 *
 * Parse a colour name, such as "red", "brightgreen", "color123".
 */
static int parse_color_name(const char *s, uint32_t *col, int *attr, bool is_fg,
                            struct Buffer *err)
{
  char *eptr = NULL;
  bool is_alert = false, is_bright = false, is_light = false;
  int clen;

  if ((clen = mutt_str_startswith(s, "bright", CASE_IGNORE)))
  {
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_str_startswith(s, "alert", CASE_IGNORE)))
  {
    is_alert = true;
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_str_startswith(s, "light", CASE_IGNORE)))
  {
    is_light = true;
    s += clen;
  }

  /* allow aliases for xterm color resources */
  if ((clen = mutt_str_startswith(s, "color", CASE_IGNORE)))
  {
    s += clen;
    *col = strtoul(s, &eptr, 10);
    if (!*s || *eptr || ((*col >= COLORS) && !OptNoCurses && has_colors()))
    {
      mutt_buffer_printf(err, _("%s: color not supported by term"), s);
      return -1;
    }
  }
#ifdef HAVE_DIRECTCOLOR
  else if (*s == '#')
  {
    s += 1;
    *col = strtoul(s, &eptr, 16);
    if (!*s || *eptr || ((*col == COLOR_UNSET) && !OptNoCurses && has_colors()))
    {
      snprintf(err->data, err->dsize, _("%s: color not supported by term"), s);
      return -1;
    }
    *col |= RGB24;
  }
#endif
  else if ((*col = mutt_map_get_value(s, Colors)) == -1)
  {
    mutt_buffer_printf(err, _("%s: no such color"), s);
    return -1;
  }

  if (is_bright || is_light)
  {
    if (is_alert)
    {
      *attr |= A_BOLD;
      *attr |= A_BLINK;
    }
    else if (is_fg)
    {
      if ((COLORS >= 16) && is_light)
      {
        if (*col <= 7)
        {
          /* Advance the color 0-7 by 8 to get the light version */
          *col += 8;
        }
      }
      else
      {
        *attr |= A_BOLD;
      }
    }
    else if (!(*col & RGB24))
    {
      if (COLORS >= 16)
      {
        if (*col <= 7)
        {
          /* Advance the color 0-7 by 8 to get the light version */
          *col += 8;
        }
      }
    }
  }

  return 0;
}

#endif

/**
 * do_uncolor - Parse the "uncolor" or "unmono" command
 * @param[in]     buf           Buffer for temporary storage
 * @param[in]     s             Buffer containing the uncolor command
 * @param[in]     cl            List of existing colours
 * @param[in,out] do_cache      Set to true if colours were freed
 * @param[in]     parse_uncolor If true, 'uncolor', else 'unmono'
 */
static void do_uncolor(struct Buffer *buf, struct Buffer *s,
                       struct ColorLineList *cl, bool *do_cache, bool parse_uncolor)
{
  struct ColorLine *np = NULL, *tmp = NULL;
  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      np = STAILQ_FIRST(cl);
      while (np)
      {
        tmp = STAILQ_NEXT(np, entries);
        if (!*do_cache)
        {
          *do_cache = true;
        }
        color_line_free(&np, parse_uncolor);
        np = tmp;
      }
      STAILQ_INIT(cl);
      return;
    }
    else
    {
      tmp = NULL;
      STAILQ_FOREACH(np, cl, entries)
      {
        if (mutt_str_strcmp(buf->data, np->pattern) == 0)
        {
          if (!*do_cache)
          {
            *do_cache = true;
          }
          mutt_debug(LL_DEBUG1, "Freeing pattern \"%s\" from ColorList\n", buf->data);
          if (tmp)
            STAILQ_REMOVE_AFTER(cl, tmp, entries);
          else
            STAILQ_REMOVE_HEAD(cl, entries);
          color_line_free(&np, parse_uncolor);
          break;
        }
        tmp = np;
      }
    }
  } while (MoreArgs(s));
}

/**
 * parse_uncolor - Parse an 'uncolor' command
 * @param buf           Temporary Buffer space
 * @param s             Buffer containing string to be parsed
 * @param data          Flags associated with the command
 * @param err           Buffer for error messages
 * @param parse_uncolor If true, 'uncolor', else 'unmono'
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * usage:
 * * uncolor index pattern [pattern...]
 * * unmono  index pattern [pattern...]
 */
static enum CommandResult parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                        unsigned long data, struct Buffer *err, bool parse_uncolor)
{
  int object = 0;
  bool do_cache = false;

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  object = mutt_map_get_value(buf->data, Fields);
  if (object == -1)
  {
    mutt_buffer_printf(err, _("%s: no such object"), buf->data);
    return MUTT_CMD_ERROR;
  }

  if (object > MT_COLOR_INDEX_SUBJECT)
  { /* uncolor index column */
    ColorDefs[object] = 0;
    mutt_menu_set_redraw_full(MENU_MAIN);
    return MUTT_CMD_SUCCESS;
  }

  if (!mutt_str_startswith(buf->data, "body", CASE_MATCH) &&
      !mutt_str_startswith(buf->data, "header", CASE_MATCH) &&
      !mutt_str_startswith(buf->data, "index", CASE_MATCH))
  {
    mutt_buffer_printf(err, _("%s: command valid only for index, body, header objects"),
                       parse_uncolor ? "uncolor" : "unmono");
    return MUTT_CMD_WARNING;
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), parse_uncolor ? "uncolor" : "unmono");
    return MUTT_CMD_WARNING;
  }

  if (
#ifdef HAVE_COLOR
      /* we're running without curses */
      OptNoCurses || /* we're parsing an uncolor command, and have no colors */
      (parse_uncolor && !has_colors())
      /* we're parsing an unmono command, and have colors */
      || (!parse_uncolor && has_colors())
#else
      /* We don't even have colors compiled in */
      parse_uncolor
#endif
  )
  {
    /* just eat the command, but don't do anything real about it */
    do
      mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    while (MoreArgs(s));

    return MUTT_CMD_SUCCESS;
  }

  if (object == MT_COLOR_BODY)
    do_uncolor(buf, s, &ColorBodyList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_HEADER)
    do_uncolor(buf, s, &ColorHdrList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_ATTACH_HEADERS)
    do_uncolor(buf, s, &ColorAttachList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_INDEX)
    do_uncolor(buf, s, &ColorIndexList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_INDEX_AUTHOR)
    do_uncolor(buf, s, &ColorIndexAuthorList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_INDEX_FLAGS)
    do_uncolor(buf, s, &ColorIndexFlagsList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_INDEX_SUBJECT)
    do_uncolor(buf, s, &ColorIndexSubjectList, &do_cache, parse_uncolor);
  else if (object == MT_COLOR_INDEX_TAG)
    do_uncolor(buf, s, &ColorIndexTagList, &do_cache, parse_uncolor);

  bool is_index = ((object == MT_COLOR_INDEX) || (object == MT_COLOR_INDEX_AUTHOR) ||
                   (object == MT_COLOR_INDEX_FLAGS) || (object == MT_COLOR_INDEX_SUBJECT) ||
                   (object == MT_COLOR_INDEX_TAG));

  if (is_index && do_cache && !OptNoCurses)
  {
    mutt_menu_set_redraw_full(MENU_MAIN);
    /* force re-caching of index colors */
    for (int i = 0; Context && i < Context->mailbox->msg_count; i++)
      Context->mailbox->emails[i]->pair = 0;
  }
  return MUTT_CMD_SUCCESS;
}

#ifdef HAVE_COLOR

/**
 * mutt_parse_uncolor - Parse the 'uncolor' command - Implements ::command_t
 */
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                      unsigned long data, struct Buffer *err)
{
  return parse_uncolor(buf, s, data, err, true);
}

#endif

/**
 * mutt_parse_unmono - Parse the 'unmono' command - Implements ::command_t
 */
enum CommandResult mutt_parse_unmono(struct Buffer *buf, struct Buffer *s,
                                     unsigned long data, struct Buffer *err)
{
  return parse_uncolor(buf, s, data, err, false);
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
 */
static enum CommandResult add_pattern(struct ColorLineList *top, const char *s,
                                      bool sensitive, uint32_t fg, uint32_t bg, int attr,
                                      struct Buffer *err, bool is_index, int match)
{
  /* is_index used to store compiled pattern
   * only for 'index' color object
   * when called from mutt_parse_color() */

  struct ColorLine *tmp = NULL;

  STAILQ_FOREACH(tmp, top, entries)
  {
    if (sensitive)
    {
      if (mutt_str_strcmp(s, tmp->pattern) == 0)
        break;
    }
    else
    {
      if (mutt_str_strcasecmp(s, tmp->pattern) == 0)
        break;
    }
  }

  if (tmp)
  {
#ifdef HAVE_COLOR
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
#endif /* HAVE_COLOR */
    tmp->pair = attr;
  }
  else
  {
    tmp = color_line_new();
    if (is_index)
    {
      struct Buffer *buf = mutt_buffer_pool_get();
      mutt_buffer_strcpy(buf, s);
      mutt_check_simple(buf, NONULL(C_SimpleSearch));
      tmp->color_pattern = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, err);
      mutt_buffer_pool_release(&buf);
      if (!tmp->color_pattern)
      {
        color_line_free(&tmp, true);
        return MUTT_CMD_ERROR;
      }
    }
    else
    {
      int flags = 0;
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
    tmp->pattern = mutt_str_strdup(s);
    tmp->match = match;
#ifdef HAVE_COLOR
    if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    {
      tmp->fg = fg;
      tmp->bg = bg;
      attr |= mutt_color_alloc(fg, bg);
    }
#endif
    tmp->pair = attr;
    STAILQ_INSERT_HEAD(top, tmp, entries);
  }

  /* force re-caching of index colors */
  if (is_index)
  {
    for (int i = 0; Context && i < Context->mailbox->msg_count; i++)
      Context->mailbox->emails[i]->pair = 0;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_object - Parse a colour config line
 * @param[in]  buf Temporary Buffer space
 * @param[in]  s   Buffer containing string to be parsed
 * @param[out] o   Index into the fields map
 * @param[out] ql  Quote level
 * @param[out] err Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
static int parse_object(struct Buffer *buf, struct Buffer *s, uint32_t *o,
                        uint32_t *ql, struct Buffer *err)
{
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "color");
    return -1;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  if (mutt_str_startswith(buf->data, "quoted", CASE_MATCH))
  {
    if (buf->data[6])
    {
      char *eptr = NULL;
      *ql = strtoul(buf->data + 6, &eptr, 10);
      if (*eptr || (*ql == COLOR_UNSET))
      {
        mutt_buffer_printf(err, _("%s: no such object"), buf->data);
        return -1;
      }
    }
    else
      *ql = 0;

    *o = MT_COLOR_QUOTED;
  }
  else if (mutt_str_strcasecmp(buf->data, "compose") == 0)
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), "color");
      return -1;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    *o = mutt_map_get_value(buf->data, ComposeFields);
    if (*o == -1)
    {
      mutt_buffer_printf(err, _("%s: no such object"), buf->data);
      return -1;
    }
  }
  else if ((*o = mutt_map_get_value(buf->data, Fields)) == -1)
  {
    mutt_buffer_printf(err, _("%s: no such object"), buf->data);
    return -1;
  }

  return 0;
}

/**
 * typedef parser_callback_t - Prototype for a function to parse color config
 * @param[in]  buf Temporary Buffer space
 * @param[in]  s    Buffer containing string to be parsed
 * @param[out] fg   Foreground colour (set to -1)
 * @param[out] bg   Background colour (set to -1)
 * @param[out] attr Attribute flags
 * @param[out] err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
typedef int (*parser_callback_t)(struct Buffer *buf, struct Buffer *s, uint32_t *fg,
                                 uint32_t *bg, int *attr, struct Buffer *err);

#ifdef HAVE_COLOR

/**
 * parse_color_pair - Parse a pair of colours - Implements ::parser_callback_t
 */
static int parse_color_pair(struct Buffer *buf, struct Buffer *s, uint32_t *fg,
                            uint32_t *bg, int *attr, struct Buffer *err)
{
  while (true)
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), "color");
      return -1;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (mutt_str_strcasecmp("bold", buf->data) == 0)
      *attr |= A_BOLD;
    else if (mutt_str_strcasecmp("underline", buf->data) == 0)
      *attr |= A_UNDERLINE;
    else if (mutt_str_strcasecmp("none", buf->data) == 0)
      *attr = A_NORMAL;
    else if (mutt_str_strcasecmp("reverse", buf->data) == 0)
      *attr |= A_REVERSE;
    else if (mutt_str_strcasecmp("standout", buf->data) == 0)
      *attr |= A_STANDOUT;
    else if (mutt_str_strcasecmp("normal", buf->data) == 0)
      *attr = A_NORMAL; /* needs use = instead of |= to clear other bits */
    else
    {
      if (parse_color_name(buf->data, fg, attr, true, err) != 0)
        return -1;
      break;
    }
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "color");
    return -1;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (parse_color_name(buf->data, bg, attr, false, err) != 0)
    return -1;

  return 0;
}

#endif

/**
 * parse_attr_spec - Parse an attribute description - Implements ::parser_callback_t
 */
static int parse_attr_spec(struct Buffer *buf, struct Buffer *s, uint32_t *fg,
                           uint32_t *bg, int *attr, struct Buffer *err)
{
  if (fg)
    *fg = COLOR_UNSET;
  if (bg)
    *bg = COLOR_UNSET;

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "mono");
    return -1;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (mutt_str_strcasecmp("bold", buf->data) == 0)
    *attr |= A_BOLD;
  else if (mutt_str_strcasecmp("underline", buf->data) == 0)
    *attr |= A_UNDERLINE;
  else if (mutt_str_strcasecmp("none", buf->data) == 0)
    *attr = A_NORMAL;
  else if (mutt_str_strcasecmp("reverse", buf->data) == 0)
    *attr |= A_REVERSE;
  else if (mutt_str_strcasecmp("standout", buf->data) == 0)
    *attr |= A_STANDOUT;
  else if (mutt_str_strcasecmp("normal", buf->data) == 0)
    *attr = A_NORMAL; /* needs use = instead of |= to clear other bits */
  else
  {
    mutt_buffer_printf(err, _("%s: no such attribute"), buf->data);
    return -1;
  }

  return 0;
}

/**
 * fgbgattr_to_color - Convert a foreground, background, attribute triplet into a colour
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 * @param attr Attribute flags, e.g. A_BOLD
 * @retval num Combined colour pair
 */
static int fgbgattr_to_color(int fg, int bg, int attr)
{
#ifdef HAVE_COLOR
  if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    return attr | mutt_color_alloc(fg, bg);
  else
#endif
    return attr;
}

/**
 * parse_color - Parse a "color" command
 * @param buf      Temporary Buffer space
 * @param s        Buffer containing string to be parsed
 * @param err      Buffer for error messages
 * @param callback Function to handle command - Implements ::parser_callback_t
 * @param dry_run  If true, test the command, but don't apply it
 * @param color    If true "color", else "mono"
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * usage: color OBJECT FG BG [ REGEX ]
 *        mono  OBJECT ATTR [ REGEX ]
 */
static enum CommandResult parse_color(struct Buffer *buf, struct Buffer *s,
                                      struct Buffer *err, parser_callback_t callback,
                                      bool dry_run, bool color)
{
  int attr = 0;
  uint32_t fg = 0, bg = 0, object = 0, q_level = 0, match = 0;
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  if (parse_object(buf, s, &object, &q_level, err) == -1)
    return MUTT_CMD_ERROR;

  if (callback(buf, s, &fg, &bg, &attr, err) == -1)
    return MUTT_CMD_ERROR;

  /* extract a regular expression if needed */

  if ((object == MT_COLOR_BODY) || (object == MT_COLOR_HEADER) ||
      (object == MT_COLOR_ATTACH_HEADERS) || (object == MT_COLOR_INDEX) ||
      (object == MT_COLOR_INDEX_AUTHOR) || (object == MT_COLOR_INDEX_FLAGS) ||
      (object == MT_COLOR_INDEX_TAG) || (object == MT_COLOR_INDEX_SUBJECT))
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), color ? "color" : "mono");
      return MUTT_CMD_WARNING;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
  }

  if (MoreArgs(s) && (object != MT_COLOR_STATUS))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), color ? "color" : "mono");
    return MUTT_CMD_WARNING;
  }

  /* dry run? */

  if (dry_run)
  {
    *s->dptr = '\0'; /* fake that we're done parsing */
    return MUTT_CMD_SUCCESS;
  }

#ifdef HAVE_COLOR
#ifdef HAVE_USE_DEFAULT_COLORS
  if (!OptNoCurses &&
      has_colors()
      /* delay use_default_colors() until needed, since it initializes things */
      && ((fg == COLOR_DEFAULT) || (bg == COLOR_DEFAULT) || (object == MT_COLOR_TREE)) &&
      (use_default_colors() != OK))
  /* the case of the tree object is special, because a non-default fg color of
   * the tree element may be combined dynamically with the default bg color of
   * an index line, not necessarily defined in a rc file.  */
  {
    mutt_buffer_strcpy(err, _("default colors not supported"));
    return MUTT_CMD_ERROR;
  }
#endif /* HAVE_USE_DEFAULT_COLORS */
#endif

  if (object == MT_COLOR_HEADER)
    rc = add_pattern(&ColorHdrList, buf->data, false, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_BODY)
    rc = add_pattern(&ColorBodyList, buf->data, true, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_ATTACH_HEADERS)
    rc = add_pattern(&ColorAttachList, buf->data, true, fg, bg, attr, err, false, match);
  else if ((object == MT_COLOR_STATUS) && MoreArgs(s))
  {
    /* 'color status fg bg' can have up to 2 arguments:
     * 0 arguments: sets the default status color (handled below by else part)
     * 1 argument : colorize pattern on match
     * 2 arguments: colorize nth submatch of pattern */
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (MoreArgs(s))
    {
      struct Buffer tmp = mutt_buffer_make(0);
      mutt_extract_token(&tmp, s, MUTT_TOKEN_NO_FLAGS);
      if (mutt_str_atoui(tmp.data, &match) < 0)
      {
        mutt_buffer_printf(err, _("%s: invalid number: %s"),
                           color ? "color" : "mono", tmp.data);
        mutt_buffer_dealloc(&tmp);
        return MUTT_CMD_WARNING;
      }
      mutt_buffer_dealloc(&tmp);
    }

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), color ? "color" : "mono");
      return MUTT_CMD_WARNING;
    }

    rc = add_pattern(&ColorStatusList, buf->data, true, fg, bg, attr, err, false, match);
  }
  else if (object == MT_COLOR_INDEX)
  {
    rc = add_pattern(&ColorIndexList, buf->data, true, fg, bg, attr, err, true, match);
    mutt_menu_set_redraw_full(MENU_MAIN);
  }
  else if (object == MT_COLOR_INDEX_AUTHOR)
  {
    rc = add_pattern(&ColorIndexAuthorList, buf->data, true, fg, bg, attr, err, true, match);
    mutt_menu_set_redraw_full(MENU_MAIN);
  }
  else if (object == MT_COLOR_INDEX_FLAGS)
  {
    rc = add_pattern(&ColorIndexFlagsList, buf->data, true, fg, bg, attr, err, true, match);
    mutt_menu_set_redraw_full(MENU_MAIN);
  }
  else if (object == MT_COLOR_INDEX_SUBJECT)
  {
    rc = add_pattern(&ColorIndexSubjectList, buf->data, true, fg, bg, attr, err, true, match);
    mutt_menu_set_redraw_full(MENU_MAIN);
  }
  else if (object == MT_COLOR_INDEX_TAG)
  {
    rc = add_pattern(&ColorIndexTagList, buf->data, true, fg, bg, attr, err, true, match);
    mutt_menu_set_redraw_full(MENU_MAIN);
  }
  else if (object == MT_COLOR_QUOTED)
  {
    if (q_level >= ColorQuoteSize)
    {
      mutt_mem_realloc(&ColorQuote, (ColorQuoteSize += 2) * sizeof(int));
      ColorQuote[ColorQuoteSize - 2] = ColorDefs[MT_COLOR_QUOTED];
      ColorQuote[ColorQuoteSize - 1] = ColorDefs[MT_COLOR_QUOTED];
    }
    if (q_level >= ColorQuoteUsed)
      ColorQuoteUsed = q_level + 1;
    if (q_level == 0)
    {
      ColorDefs[MT_COLOR_QUOTED] = fgbgattr_to_color(fg, bg, attr);

      ColorQuote[0] = ColorDefs[MT_COLOR_QUOTED];
      for (q_level = 1; q_level < ColorQuoteUsed; q_level++)
      {
        if (ColorQuote[q_level] == A_NORMAL)
          ColorQuote[q_level] = ColorDefs[MT_COLOR_QUOTED];
      }
    }
    else
      ColorQuote[q_level] = fgbgattr_to_color(fg, bg, attr);
  }
  else
  {
    ColorDefs[object] = fgbgattr_to_color(fg, bg, attr);
    if (object > MT_COLOR_INDEX_AUTHOR)
      mutt_menu_set_redraw_full(MENU_MAIN);
  }

  return rc;
}

#ifdef HAVE_COLOR

/**
 * mutt_parse_color - Parse the 'color' command - Implements ::command_t
 */
enum CommandResult mutt_parse_color(struct Buffer *buf, struct Buffer *s,
                                    unsigned long data, struct Buffer *err)
{
  bool dry_run = false;

  if (OptNoCurses || !has_colors())
    dry_run = true;

  return parse_color(buf, s, err, parse_color_pair, dry_run, true);
}

#endif

/**
 * mutt_parse_mono - Parse the 'mono' command - Implements ::command_t
 */
enum CommandResult mutt_parse_mono(struct Buffer *buf, struct Buffer *s,
                                   unsigned long data, struct Buffer *err)
{
  bool dry_run = false;

#ifdef HAVE_COLOR
  if (OptNoCurses || has_colors())
    dry_run = true;
#else
  if (OptNoCurses)
    dry_run = true;
#endif

  return parse_color(buf, s, err, parse_attr_spec, dry_run, false);
}

/**
 * mutt_color_list_free - Free a list of colours
 * @param list ColorLine List
 */
static void mutt_color_list_free(struct ColorLineList *list)
{
  struct ColorLine *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, list, entries, tmp)
  {
    STAILQ_REMOVE(list, np, ColorLine, entries);
    color_line_free(&np, true);
  }
}

/**
 * mutt_colors_free - Free all the colours (on shutdown)
 */
void mutt_colors_free(void)
{
  mutt_color_list_free(&ColorAttachList);
  mutt_color_list_free(&ColorBodyList);
  mutt_color_list_free(&ColorHdrList);
  mutt_color_list_free(&ColorIndexAuthorList);
  mutt_color_list_free(&ColorIndexFlagsList);
  mutt_color_list_free(&ColorIndexList);
  mutt_color_list_free(&ColorIndexSubjectList);
  mutt_color_list_free(&ColorIndexTagList);
  mutt_color_list_free(&ColorStatusList);

  struct ColorList *cl = ColorList;
  struct ColorList *next = NULL;
  while (cl)
  {
    next = cl->next;
    FREE(&cl);
    cl = next;
  }
  ColorList = NULL;
}
