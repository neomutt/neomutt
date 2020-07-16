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
 * @page gui_color Color and attribute parsing
 *
 * Color and attribute parsing
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "color.h"
#include "context.h"
#include "init.h"
#include "mutt_commands.h"
#include "mutt_curses.h"
#include "mutt_globals.h"
#include "options.h"
#include "pattern/lib.h"
#ifdef USE_SLANG_CURSES
#include <assert.h>
#endif

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

#define COLOR_UNSET UINT32_MAX
#define COLOR_QUOTES_MAX                                                       \
  10 ///< Ten colours, quoted0..quoted9 (quoted and quoted0 are equivalent)

#ifdef HAVE_COLOR

#define COLOR_DEFAULT (-2)

/*
 * Flags for the high 8bits of the color value.
 *
 * Note that no flag means it's a palette color.
 */
#define RGB24 (1U << 24)

// clang-format off
static const struct Mapping ColorNames[] = {
  { "black",   COLOR_BLACK },
  { "blue",    COLOR_BLUE },
  { "cyan",    COLOR_CYAN },
  { "green",   COLOR_GREEN },
  { "magenta", COLOR_MAGENTA },
  { "red",     COLOR_RED },
  { "white",   COLOR_WHITE },
  { "yellow",  COLOR_YELLOW },
#if defined(USE_SLANG_CURSES) || defined(HAVE_USE_DEFAULT_COLORS)
  { "default", COLOR_DEFAULT },
#endif
  { 0,         0 },
};
// clang-format on
#endif /* HAVE_COLOR */

// clang-format off
const struct Mapping Fields[] = {
  { "attachment",        MT_COLOR_ATTACHMENT },
  { "attach_headers",    MT_COLOR_ATTACH_HEADERS },
  { "body",              MT_COLOR_BODY },
  { "bold",              MT_COLOR_BOLD },
  { "error",             MT_COLOR_ERROR },
  { "hdrdefault",        MT_COLOR_HDRDEFAULT },
  { "header",            MT_COLOR_HEADER },
  { "index",             MT_COLOR_INDEX },
  { "index_author",      MT_COLOR_INDEX_AUTHOR },
  { "index_collapsed",   MT_COLOR_INDEX_COLLAPSED },
  { "index_date",        MT_COLOR_INDEX_DATE },
  { "index_flags",       MT_COLOR_INDEX_FLAGS },
  { "index_label",       MT_COLOR_INDEX_LABEL },
  { "index_number",      MT_COLOR_INDEX_NUMBER },
  { "index_size",        MT_COLOR_INDEX_SIZE },
  { "index_subject",     MT_COLOR_INDEX_SUBJECT },
  { "index_tag",         MT_COLOR_INDEX_TAG },
  { "index_tags",        MT_COLOR_INDEX_TAGS },
  { "indicator",         MT_COLOR_INDICATOR },
  { "markers",           MT_COLOR_MARKERS },
  { "message",           MT_COLOR_MESSAGE },
  { "normal",            MT_COLOR_NORMAL },
  { "options",           MT_COLOR_OPTIONS },
  { "progress",          MT_COLOR_PROGRESS },
  { "prompt",            MT_COLOR_PROMPT },
  { "quoted",            MT_COLOR_QUOTED },
  { "search",            MT_COLOR_SEARCH },
#ifdef USE_SIDEBAR
  { "sidebar_divider",   MT_COLOR_SIDEBAR_DIVIDER },
  { "sidebar_flagged",   MT_COLOR_SIDEBAR_FLAGGED },
  { "sidebar_highlight", MT_COLOR_SIDEBAR_HIGHLIGHT },
  { "sidebar_indicator", MT_COLOR_SIDEBAR_INDICATOR },
  { "sidebar_new",       MT_COLOR_SIDEBAR_NEW },
  { "sidebar_ordinary",  MT_COLOR_SIDEBAR_ORDINARY },
  { "sidebar_spoolfile", MT_COLOR_SIDEBAR_SPOOLFILE },
  { "sidebar_unread",    MT_COLOR_SIDEBAR_UNREAD },
#endif
  { "signature",         MT_COLOR_SIGNATURE },
  { "status",            MT_COLOR_STATUS },
  { "tilde",             MT_COLOR_TILDE },
  { "tree",              MT_COLOR_TREE },
  { "underline",         MT_COLOR_UNDERLINE },
  { "warning",           MT_COLOR_WARNING },
  { NULL,                0 },
};

const struct Mapping ComposeFields[] = {
  { "header",            MT_COLOR_COMPOSE_HEADER },
  { "security_encrypt",  MT_COLOR_COMPOSE_SECURITY_ENCRYPT },
  { "security_sign",     MT_COLOR_COMPOSE_SECURITY_SIGN },
  { "security_both",     MT_COLOR_COMPOSE_SECURITY_BOTH },
  { "security_none",     MT_COLOR_COMPOSE_SECURITY_NONE },
  { NULL,                0 }
};
// clang-format off

/**
 * defs_free - Free the simple colour definitions
 * @param c Colours
 */
static void defs_free(struct Colors *c)
{
  FREE(&c->defs);
}

/**
 * defs_init - Initialise the simple colour definitions
 * @param c Colours
 */
static void defs_init(struct Colors *c)
{
  c->defs = mutt_mem_malloc(MT_COLOR_MAX * sizeof(int));
  memset(c->defs, A_NORMAL, MT_COLOR_MAX * sizeof(int));

  // Set some defaults
  c->defs[MT_COLOR_INDICATOR] = A_REVERSE;
  c->defs[MT_COLOR_MARKERS] = A_REVERSE;
  c->defs[MT_COLOR_SEARCH] = A_REVERSE;
#ifdef USE_SIDEBAR
  c->defs[MT_COLOR_SIDEBAR_HIGHLIGHT] = A_UNDERLINE;
#endif
  c->defs[MT_COLOR_STATUS] = A_REVERSE;
}

/**
 * defs_clear - Reset the simple colour definitions
 * @param c Colours
 */
static void defs_clear(struct Colors *c)
{
  memset(c->defs, A_NORMAL, MT_COLOR_MAX * sizeof(int));
}

/**
 * quotes_free - Free the quoted-email colours
 * @param c Colours
 */
static void quotes_free(struct Colors *c)
{
  FREE(&c->quotes);
}

/**
 * quotes_init - Initialise the quoted-email colours
 * @param c Colours
 */
static void quotes_init(struct Colors *c)
{
  c->quotes = mutt_mem_malloc(COLOR_QUOTES_MAX * sizeof(int));
  memset(c->quotes, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  c->quotes_used = 0;
}

/**
 * quotes_clear - Reset the quoted-email colours
 * @param c Colours
 */
static void quotes_clear(struct Colors *c)
{
  memset(c->quotes, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  c->quotes_used = 0;
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
 * @param c  Colours
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 *
 * If there are no more users, the resource will be freed.
 */
void mutt_color_free(struct Colors *c, uint32_t fg, uint32_t bg)
{
  struct ColorList *q = NULL;

  struct ColorList *p = c->user_colors;
  while (p)
  {
    if ((p->fg == fg) && (p->bg == bg))
    {
      (p->count)--;
      if (p->count > 0)
        return;

      c->num_user_colors--;
      mutt_debug(LL_DEBUG1, "Color pairs used so far: %d\n", c->num_user_colors);

      if (p == c->user_colors)
      {
        c->user_colors = c->user_colors->next;
        FREE(&p);
        return;
      }
      q = c->user_colors;
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
 * @param c           Colours
 * @param ptr         ColorLine to free
 * @param free_colors If true, free its colours too
 */
static void color_line_free(struct Colors *c, struct ColorLine **ptr, bool free_colors)
{
  if (!ptr || !*ptr)
    return;

  struct ColorLine *cl = *ptr;

#ifdef HAVE_COLOR
  if (free_colors && (cl->fg != COLOR_UNSET) && (cl->bg != COLOR_UNSET))
    mutt_color_free(c, cl->fg, cl->bg);
#endif

  regfree(&cl->regex);
  mutt_pattern_free(&cl->color_pattern);
  FREE(&cl->pattern);
  FREE(ptr);
}

/**
 * color_line_list_clear - Clear a list of colours
 * @param c    Colours
 * @param list ColorLine List
 */
static void color_line_list_clear(struct Colors *c, struct ColorLineList *list)
{
  struct ColorLine *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, list, entries, tmp)
  {
    STAILQ_REMOVE(list, np, ColorLine, entries);
    color_line_free(c, &np, true);
  }
}

/**
 * colors_clear - Reset all the colours
 * @param c Colours
 */
static void colors_clear(struct Colors *c)
{
  color_line_list_clear(c, &c->attach_list);
  color_line_list_clear(c, &c->body_list);
  color_line_list_clear(c, &c->hdr_list);
  color_line_list_clear(c, &c->index_author_list);
  color_line_list_clear(c, &c->index_flags_list);
  color_line_list_clear(c, &c->index_list);
  color_line_list_clear(c, &c->index_subject_list);
  color_line_list_clear(c, &c->index_tag_list);
  color_line_list_clear(c, &c->status_list);

  defs_clear(c);
  quotes_clear(c);

  color_list_free(&c->user_colors);
}

/**
 * mutt_colors_free - Free all the colours
 * @param ptr Colours
 */
void mutt_colors_free(struct Colors **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Colors *c = *ptr;

  colors_clear(c);
  defs_free(c);
  quotes_free(c);
  notify_free(&c->notify);
  FREE(ptr);
}

/**
 * mutt_colors_new - Create new colours
 * @retval ptr New Colors
 */
struct Colors *mutt_colors_new(void)
{
  struct Colors *c = mutt_mem_calloc(1, sizeof(*c));
  c->notify = notify_new();

  quotes_init(c);
  defs_init(c);

  STAILQ_INIT(&c->attach_list);
  STAILQ_INIT(&c->body_list);
  STAILQ_INIT(&c->hdr_list);
  STAILQ_INIT(&c->index_author_list);
  STAILQ_INIT(&c->index_flags_list);
  STAILQ_INIT(&c->index_list);
  STAILQ_INIT(&c->index_subject_list);
  STAILQ_INIT(&c->index_tag_list);
  STAILQ_INIT(&c->status_list);

#ifdef HAVE_COLOR
  start_color();
#endif

  notify_set_parent(c->notify, NeoMutt->notify);
  return c;
}

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
      mutt_str_copy(dest, missing[0], destlen);
      return dest;

    case COLOR_WHITE:
      mutt_str_copy(dest, missing[1], destlen);
      return dest;

    case COLOR_DEFAULT:
      mutt_str_copy(dest, missing[2], destlen);
      return dest;
  }

  for (int i = 0; ColorNames[i].name; i++)
  {
    if (ColorNames[i].value == val)
    {
      mutt_str_copy(dest, ColorNames[i].name, destlen);
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
 * @param c  Colours
 * @param fg Foreground colour ID
 * @param bg Background colour ID
 * @retval num Combined colour pair
 */
int mutt_color_alloc(struct Colors *c, uint32_t fg, uint32_t bg)
{
#ifdef USE_SLANG_CURSES
  char fgc[128], bgc[128];
#endif
  struct ColorList *p = c->user_colors;

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
  if (++c->num_user_colors > COLOR_PAIRS)
    return A_NORMAL;

  /* find the smallest available index (object) */
  int i = 1;
  while (true)
  {
    p = c->user_colors;
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
  p->next = c->user_colors;
  c->user_colors = p;

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

  mutt_debug(LL_DEBUG3, "Color pairs used so far: %d\n", c->num_user_colors);

  return COLOR_PAIR(p->index);
}

/**
 * mutt_lookup_color - Get the colours from a colour pair
 * @param[in]  c    Colours
 * @param[in]  pair Colour pair
 * @param[out] fg   Foreground colour (OPTIONAL)
 * @param[out] bg   Background colour (OPTIONAL)
 * @retval  0 Success
 * @retval -1 Error
 */
static int mutt_lookup_color(struct Colors *c, short pair, uint32_t *fg, uint32_t *bg)
{
  struct ColorList *p = c->user_colors;

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
 * @param c       Colours
 * @param fg_attr Colour pair of foreground to use
 * @param bg_attr Colour pair of background to use
 * @retval num Colour pair of combined colour
 */
int mutt_color_combine(struct Colors *c, uint32_t fg_attr, uint32_t bg_attr)
{
  uint32_t fg = COLOR_DEFAULT;
  uint32_t bg = COLOR_DEFAULT;

  mutt_lookup_color(c, fg_attr, &fg, NULL);
  mutt_lookup_color(c, bg_attr, NULL, &bg);

  if ((fg == COLOR_DEFAULT) && (bg == COLOR_DEFAULT))
    return A_NORMAL;
  return mutt_color_alloc(c, fg, bg);
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
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Parse a colour name, such as "red", "brightgreen", "color123".
 */
static enum CommandResult parse_color_name(const char *s, uint32_t *col, int *attr,
                                           bool is_fg, struct Buffer *err)
{
  char *eptr = NULL;
  bool is_alert = false, is_bright = false, is_light = false;
  int clen;

  if ((clen = mutt_istr_startswith(s, "bright")))
  {
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_istr_startswith(s, "alert")))
  {
    is_alert = true;
    is_bright = true;
    s += clen;
  }
  else if ((clen = mutt_istr_startswith(s, "light")))
  {
    is_light = true;
    s += clen;
  }

  /* allow aliases for xterm color resources */
  if ((clen = mutt_istr_startswith(s, "color")))
  {
    s += clen;
    *col = strtoul(s, &eptr, 10);
    if ((*s == '\0') || (*eptr != '\0') || ((*col >= COLORS) && !OptNoCurses && has_colors()))
    {
      mutt_buffer_printf(err, _("%s: color not supported by term"), s);
      return MUTT_CMD_ERROR;
    }
  }
#ifdef HAVE_DIRECTCOLOR
  else if (*s == '#')
  {
    s += 1;
    *col = strtoul(s, &eptr, 16);
    if ((*s == '\0') || (*eptr != '\0') || ((*col == COLOR_UNSET) && !OptNoCurses && has_colors()))
    {
      snprintf(err->data, err->dsize, _("%s: color not supported by term"), s);
      return MUTT_CMD_ERROR;
    }
    *col |= RGB24;
  }
#endif
  else if ((*col = mutt_map_get_value(s, ColorNames)) == -1)
  {
    mutt_buffer_printf(err, _("%s: no such color"), s);
    return MUTT_CMD_WARNING;
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

  return MUTT_CMD_SUCCESS;
}
#endif

/**
 * parse_object - Identify a colour object
 * @param[in]  buf Temporary Buffer space
 * @param[in]  s   Buffer containing string to be parsed
 * @param[out] obj Object type, e.g. #MT_COLOR_TILDE
 * @param[out] ql  Quote level, if type #MT_COLOR_QUOTED
 * @param[out] err Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult parse_object(struct Buffer *buf, struct Buffer *s,
                                       enum ColorId *obj, int *ql, struct Buffer *err)
{
  int rc;

  if (mutt_str_startswith(buf->data, "quoted") != 0)
  {
    int val = 0;
    if (buf->data[6] != '\0')
    {
      rc = mutt_str_atoi(buf->data + 6, &val);
      if ((rc != 0) || (val > COLOR_QUOTES_MAX))
      {
        mutt_buffer_printf(err, _("%s: no such object"), buf->data);
        return MUTT_CMD_WARNING;
      }
    }

    *ql = val;
    *obj = MT_COLOR_QUOTED;
    return MUTT_CMD_SUCCESS;
  }

  if (mutt_istr_equal(buf->data, "compose"))
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    rc = mutt_map_get_value(buf->data, ComposeFields);
    if (rc == -1)
    {
      mutt_buffer_printf(err, _("%s: no such object"), buf->data);
      return MUTT_CMD_WARNING;
    }

    *obj = rc;
    return MUTT_CMD_SUCCESS;
  }

  rc = mutt_map_get_value(buf->data, Fields);
  if (rc == -1)
  {
    mutt_buffer_printf(err, _("%s: no such object"), buf->data);
    return MUTT_CMD_WARNING;
  }

  *obj = rc;
  return MUTT_CMD_SUCCESS;
}

/**
 * do_uncolor - Parse the 'uncolor' or 'unmono' command
 * @param c       Colours
 * @param buf     Buffer for temporary storage
 * @param s       Buffer containing the uncolor command
 * @param cl      List of existing colours
 * @param uncolor If true, 'uncolor', else 'unmono'
 * @retval bool True if a colour was freed
 */
static bool do_uncolor(struct Colors *c, struct Buffer *buf, struct Buffer *s,
                       struct ColorLineList *cl, bool uncolor)
{
  struct ColorLine *np = NULL, *prev = NULL;
  bool rc = false;

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      rc = STAILQ_FIRST(cl);
      color_line_list_clear(c, cl);
      return rc;
    }

    prev = NULL;
    STAILQ_FOREACH(np, cl, entries)
    {
      if (mutt_str_equal(buf->data, np->pattern))
      {
        rc = true;

        mutt_debug(LL_DEBUG1, "Freeing pattern \"%s\" from user_colors\n", buf->data);
        if (prev)
          STAILQ_REMOVE_AFTER(cl, prev, entries);
        else
          STAILQ_REMOVE_HEAD(cl, entries);
        color_line_free(c, &np, uncolor);
        break;
      }
      prev = np;
    }
  } while (MoreArgs(s));

  return rc;
}

/**
 * parse_uncolor - Parse an 'uncolor' command
 * @param buf     Temporary Buffer space
 * @param s       Buffer containing string to be parsed
 * @param c       Global colours to update
 * @param err     Buffer for error messages
 * @param uncolor If true, 'uncolor', else 'unmono'
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * usage:
 * * uncolor index pattern [pattern...]
 * * unmono  index pattern [pattern...]
 */
static enum CommandResult parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                        struct Colors *c, struct Buffer *err, bool uncolor)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (mutt_str_equal(buf->data, "*"))
  {
    colors_clear(c);
    struct EventColor ec = { false }; // Color reset/removed
    notify_send(c->notify, NT_COLOR, MT_COLOR_MAX, &ec);
    return MUTT_CMD_SUCCESS;
  }

  unsigned int object = MT_COLOR_NONE;
  int ql = 0;
  enum CommandResult rc = parse_object(buf, s, &object, &ql, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  if (object == -1)
  {
    mutt_buffer_printf(err, _("%s: no such object"), buf->data);
    return MUTT_CMD_ERROR;
  }

  if (object == MT_COLOR_QUOTED)
  {
    c->quotes[ql] = A_NORMAL;
    /* fallthrough to simple case */
  }

  if ((object != MT_COLOR_ATTACH_HEADERS) && (object != MT_COLOR_BODY) &&
      (object != MT_COLOR_HEADER) && (object != MT_COLOR_INDEX) &&
      (object != MT_COLOR_INDEX_AUTHOR) && (object != MT_COLOR_INDEX_FLAGS) &&
      (object != MT_COLOR_INDEX_SUBJECT) && (object != MT_COLOR_INDEX_TAG) &&
      (object != MT_COLOR_STATUS))
  {
    // Simple colours
    c->defs[object] = A_NORMAL;

    struct EventColor ec = { false }; // Color reset/removed
    notify_send(c->notify, NT_COLOR, object, &ec);
    return MUTT_CMD_SUCCESS;
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), uncolor ? "uncolor" : "unmono");
    return MUTT_CMD_WARNING;
  }

#ifdef HAVE_COLOR
  if (OptNoCurses ||                // running without curses
      (uncolor && !has_colors()) || // parsing an uncolor command, and have no colors
      (!uncolor && has_colors())) // parsing an unmono command, and have colors
#else
  if (uncolor) // We don't even have colors compiled in
#endif
  {
    do
    {
      /* just eat the command, but don't do anything real about it */
      mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    } while (MoreArgs(s));

    return MUTT_CMD_SUCCESS;
  }

  bool changed = false;
  if (object == MT_COLOR_ATTACH_HEADERS)
    changed |= do_uncolor(c, buf, s, &c->attach_list, uncolor);
  else if (object == MT_COLOR_BODY)
    changed |= do_uncolor(c, buf, s, &c->body_list, uncolor);
  else if (object == MT_COLOR_HEADER)
    changed |= do_uncolor(c, buf, s, &c->hdr_list, uncolor);
  else if (object == MT_COLOR_INDEX)
    changed |= do_uncolor(c, buf, s, &c->index_list, uncolor);
  else if (object == MT_COLOR_INDEX_AUTHOR)
    changed |= do_uncolor(c, buf, s, &c->index_author_list, uncolor);
  else if (object == MT_COLOR_INDEX_FLAGS)
    changed |= do_uncolor(c, buf, s, &c->index_flags_list, uncolor);
  else if (object == MT_COLOR_INDEX_SUBJECT)
    changed |= do_uncolor(c, buf, s, &c->index_subject_list, uncolor);
  else if (object == MT_COLOR_INDEX_TAG)
    changed |= do_uncolor(c, buf, s, &c->index_tag_list, uncolor);
  else if (object == MT_COLOR_STATUS)
    changed |= do_uncolor(c, buf, s, &c->status_list, uncolor);

  if (changed)
  {
    struct EventColor ec = { false }; // Color reset/removed
    notify_send(c->notify, NT_COLOR, object, &ec);
  }

  return MUTT_CMD_SUCCESS;
}

#ifdef HAVE_COLOR
/**
 * mutt_parse_uncolor - Parse the 'uncolor' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  return parse_uncolor(buf, s, Colors, err, true);
}
#endif

/**
 * mutt_parse_unmono - Parse the 'unmono' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_unmono(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  return parse_uncolor(buf, s, Colors, err, false);
}

/**
 * add_pattern - Associate a colour to a pattern
 * @param c         Colours
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
static enum CommandResult add_pattern(struct Colors *c, struct ColorLineList *top, const char *s,
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
#ifdef HAVE_COLOR
    if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    {
      if ((tmp->fg != fg) || (tmp->bg != bg))
      {
        mutt_color_free(c, tmp->fg, tmp->bg);
        tmp->fg = fg;
        tmp->bg = bg;
        attr |= mutt_color_alloc(c, fg, bg);
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
        color_line_free(c, &tmp, true);
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
        color_line_free(c, &tmp, true);
        return MUTT_CMD_ERROR;
      }
    }
    tmp->pattern = mutt_str_dup(s);
    tmp->match = match;
#ifdef HAVE_COLOR
    if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    {
      tmp->fg = fg;
      tmp->bg = bg;
      attr |= mutt_color_alloc(c, fg, bg);
    }
#endif
    tmp->pair = attr;
    STAILQ_INSERT_HEAD(top, tmp, entries);
  }

  /* force re-caching of index colors */
  if (is_index && Context && Context->mailbox)
  {
    const struct Mailbox *m = Context->mailbox;
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

#ifdef HAVE_COLOR
/**
 * parse_color_pair - Parse a pair of colours - Implements ::parser_callback_t
 */
static enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s,
                                           uint32_t *fg, uint32_t *bg,
                                           int *attr, struct Buffer *err)
{
  while (true)
  {
    if (!MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too few arguments"), "color");
      return MUTT_CMD_WARNING;
    }

    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (mutt_istr_equal("bold", buf->data))
      *attr |= A_BOLD;
    else if (mutt_istr_equal("none", buf->data))
      *attr = A_NORMAL; // Use '=' to clear other bits
    else if (mutt_istr_equal("normal", buf->data))
      *attr = A_NORMAL; // Use '=' to clear other bits
    else if (mutt_istr_equal("reverse", buf->data))
      *attr |= A_REVERSE;
    else if (mutt_istr_equal("standout", buf->data))
      *attr |= A_STANDOUT;
    else if (mutt_istr_equal("underline", buf->data))
      *attr |= A_UNDERLINE;
    else
    {
      enum CommandResult rc = parse_color_name(buf->data, fg, attr, true, err);
      if (rc != MUTT_CMD_SUCCESS)
        return rc;
      break;
    }
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  return parse_color_name(buf->data, bg, attr, false, err);
}
#endif

/**
 * parse_attr_spec - Parse an attribute description - Implements ::parser_callback_t
 */
static enum CommandResult parse_attr_spec(struct Buffer *buf, struct Buffer *s,
                                          uint32_t *fg, uint32_t *bg, int *attr,
                                          struct Buffer *err)
{
  if (fg)
    *fg = COLOR_UNSET;
  if (bg)
    *bg = COLOR_UNSET;

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "mono");
    return MUTT_CMD_WARNING;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (mutt_istr_equal("bold", buf->data))
    *attr |= A_BOLD;
  else if (mutt_istr_equal("none", buf->data))
    *attr = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("normal", buf->data))
    *attr = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("reverse", buf->data))
    *attr |= A_REVERSE;
  else if (mutt_istr_equal("standout", buf->data))
    *attr |= A_STANDOUT;
  else if (mutt_istr_equal("underline", buf->data))
    *attr |= A_UNDERLINE;
  else
  {
    mutt_buffer_printf(err, _("%s: no such attribute"), buf->data);
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * fgbgattr_to_color - Convert a foreground, background, attribute triplet into a colour
 * @param c    Colours
 * @param fg   Foreground colour ID
 * @param bg   Background colour ID
 * @param attr Attribute flags, e.g. A_BOLD
 * @retval num Combined colour pair
 */
static int fgbgattr_to_color(struct Colors *c, int fg, int bg, int attr)
{
#ifdef HAVE_COLOR
  if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    return attr | mutt_color_alloc(c, fg, bg);
#endif
  return attr;
}

/**
 * parse_color - Parse a 'color' command
 * @param c        Colours
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
static enum CommandResult parse_color(struct Colors *c, struct Buffer *buf, struct Buffer *s,
                                      struct Buffer *err, parser_callback_t callback,
                                      bool dry_run, bool color)
{
  int attr = 0, q_level = 0;
  uint32_t fg = 0, bg = 0, match = 0;
  enum ColorId object = MT_COLOR_NONE;
  enum CommandResult rc;

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "color");
    return MUTT_CMD_WARNING;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  rc = parse_object(buf, s, &object, &q_level, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  rc = callback(buf, s, &fg, &bg, &attr, err);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  /* extract a regular expression if needed */

  if ((object == MT_COLOR_ATTACH_HEADERS) || (object == MT_COLOR_BODY) ||
      (object == MT_COLOR_HEADER) || (object == MT_COLOR_INDEX) ||
      (object == MT_COLOR_INDEX_AUTHOR) || (object == MT_COLOR_INDEX_FLAGS) ||
      (object == MT_COLOR_INDEX_SUBJECT) || (object == MT_COLOR_INDEX_TAG))
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

  if (object == MT_COLOR_ATTACH_HEADERS)
    rc = add_pattern(c, &c->attach_list, buf->data, true, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_BODY)
    rc = add_pattern(c, &c->body_list, buf->data, true, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_HEADER)
    rc = add_pattern(c, &c->hdr_list, buf->data, false, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_INDEX)
  {
    rc = add_pattern(c, &c->index_list, buf->data, true, fg, bg, attr, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_AUTHOR)
  {
    rc = add_pattern(c, &c->index_author_list, buf->data, true, fg, bg, attr,
                     err, true, match);
  }
  else if (object == MT_COLOR_INDEX_FLAGS)
  {
    rc = add_pattern(c, &c->index_flags_list, buf->data, true, fg, bg, attr, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_SUBJECT)
  {
    rc = add_pattern(c, &c->index_subject_list, buf->data, true, fg, bg, attr,
                     err, true, match);
  }
  else if (object == MT_COLOR_INDEX_TAG)
  {
    rc = add_pattern(c, &c->index_tag_list, buf->data, true, fg, bg, attr, err, true, match);
  }
  else if (object == MT_COLOR_QUOTED)
  {
    if (q_level >= COLOR_QUOTES_MAX)
    {
      mutt_buffer_printf(err, _("Maximum quoting level is %d"), COLOR_QUOTES_MAX - 1);
      return MUTT_CMD_WARNING;
    }

    if (q_level >= c->quotes_used)
      c->quotes_used = q_level + 1;
    if (q_level == 0)
    {
      c->defs[MT_COLOR_QUOTED] = fgbgattr_to_color(c, fg, bg, attr);

      c->quotes[0] = c->defs[MT_COLOR_QUOTED];
      for (q_level = 1; q_level < c->quotes_used; q_level++)
      {
        if (c->quotes[q_level] == A_NORMAL)
          c->quotes[q_level] = c->defs[MT_COLOR_QUOTED];
      }
    }
    else
    {
      c->quotes[q_level] = fgbgattr_to_color(c, fg, bg, attr);
    }
    rc = MUTT_CMD_SUCCESS;
  }
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

    rc = add_pattern(c, &c->status_list, buf->data, true, fg, bg, attr, err, false, match);
  }
  else // Remaining simple colours
  {
    c->defs[object] = fgbgattr_to_color(c, fg, bg, attr);
    rc = MUTT_CMD_SUCCESS;
  }

  if (rc == MUTT_CMD_SUCCESS)
  {
    struct EventColor ec = { true }; // Color set/added
    notify_send(c->notify, NT_COLOR, object, &ec);
  }

  return rc;
}

#ifdef HAVE_COLOR
/**
 * mutt_parse_color - Parse the 'color' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_color(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  bool dry_run = false;

  if (OptNoCurses || !has_colors())
    dry_run = true;

  return parse_color(Colors, buf, s, err, parse_color_pair, dry_run, true);
}
#endif

/**
 * mutt_parse_mono - Parse the 'mono' command - Implements Command::parse()
 */
enum CommandResult mutt_parse_mono(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  bool dry_run = false;

#ifdef HAVE_COLOR
  if (OptNoCurses || has_colors())
    dry_run = true;
#else
  if (OptNoCurses)
    dry_run = true;
#endif

  return parse_color(Colors, buf, s, err, parse_attr_spec, dry_run, false);
}
