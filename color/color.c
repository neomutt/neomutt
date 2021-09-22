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

#define COLOR_UNSET UINT32_MAX
/// Ten colours, quoted0..quoted9 (quoted and quoted0 are equivalent)
#define COLOR_QUOTES_MAX 10

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

/**
 * Wrapper for all the colours
 */
static struct
{
  int defs[MT_COLOR_MAX]; ///< Array of all fixed colours, see enum ColorId

  /* These are lazily initialized, so make sure to always refer to them using
   * the mutt_color_<object>() wrappers. */
  // clang-format off
  struct ColorLineList attach_list;        ///< List of colours applied to the attachment headers
  struct ColorLineList body_list;          ///< List of colours applied to the email body
  struct ColorLineList hdr_list;           ///< List of colours applied to the email headers
  struct ColorLineList index_author_list;  ///< List of colours applied to the author in the index
  struct ColorLineList index_flags_list;   ///< List of colours applied to the flags in the index
  struct ColorLineList index_list;         ///< List of default colours applied to the index
  struct ColorLineList index_subject_list; ///< List of colours applied to the subject in the index
  struct ColorLineList index_tag_list;     ///< List of colours applied to tags in the index
  struct ColorLineList status_list;        ///< List of colours applied to the status bar
  // clang-format on

  int quotes[COLOR_QUOTES_MAX]; ///< Array of colours for quoted email text
  int quotes_used;              ///< Number of colours for quoted email text

  struct ColorList *user_colors; ///< Array of user colours
  int num_user_colors;           ///< Number of user colours

  struct Notify *notify; ///< Notifications: #ColorId, #EventColor
} Colors;

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

#define COLOR_DEFAULT (-2)

/*
 * Flags for the high 8bits of the color value.
 *
 * Note that no flag means it's a palette color.
 */
#define RGB24 (1U << 24)

static const struct Mapping ColorNames[] = {
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

const struct Mapping ColorFields[] = {
  // clang-format off
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
  { "sidebar_spool_file", MT_COLOR_SIDEBAR_SPOOLFILE },
  { "sidebar_unread",    MT_COLOR_SIDEBAR_UNREAD },
#endif
  { "signature",         MT_COLOR_SIGNATURE },
  { "status",            MT_COLOR_STATUS },
  { "tilde",             MT_COLOR_TILDE },
  { "tree",              MT_COLOR_TREE },
  { "underline",         MT_COLOR_UNDERLINE },
  { "warning",           MT_COLOR_WARNING },
  { NULL, 0 },
  // clang-format on
};

const struct Mapping ComposeColorFields[] = {
  // clang-format off
  { "header",            MT_COLOR_COMPOSE_HEADER },
  { "security_encrypt",  MT_COLOR_COMPOSE_SECURITY_ENCRYPT },
  { "security_sign",     MT_COLOR_COMPOSE_SECURITY_SIGN },
  { "security_both",     MT_COLOR_COMPOSE_SECURITY_BOTH },
  { "security_none",     MT_COLOR_COMPOSE_SECURITY_NONE },
  { NULL, 0 }
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
static void color_line_free(struct ColorLine **ptr, bool free_colors)
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
static void color_line_list_clear(struct ColorLineList *list)
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
static void colors_clear(void)
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
    if ((*s == '\0') || (*eptr != '\0') ||
        ((*col >= COLORS) && !OptNoCurses && has_colors()))
    {
      mutt_buffer_printf(err, _("%s: color not supported by term"), s);
      return MUTT_CMD_ERROR;
    }
  }
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

    rc = mutt_map_get_value(buf->data, ComposeColorFields);
    if (rc == -1)
    {
      mutt_buffer_printf(err, _("%s: no such object"), buf->data);
      return MUTT_CMD_WARNING;
    }

    *obj = rc;
    return MUTT_CMD_SUCCESS;
  }

  rc = mutt_map_get_value(buf->data, ColorFields);
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
 * @param buf     Buffer for temporary storage
 * @param s       Buffer containing the uncolor command
 * @param cl      List of existing colours
 * @param uncolor If true, 'uncolor', else 'unmono'
 * @retval true A colour was freed
 */
static bool do_uncolor(struct Buffer *buf, struct Buffer *s,
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
      color_line_list_clear(cl);
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
        color_line_free(&np, uncolor);
        break;
      }
      prev = np;
    }
  } while (MoreArgs(s));

  return rc;
}

/**
 * get_colorid_name - Get the name of a color id
 * @param color_id Colour, e.g. #MT_COLOR_HEADER
 * @param buf      Buffer for result
 */
void get_colorid_name(unsigned int color_id, struct Buffer *buf)
{
  const char *name = NULL;

  if ((color_id >= MT_COLOR_COMPOSE_HEADER) && (color_id <= MT_COLOR_COMPOSE_SECURITY_SIGN))
  {
    name = mutt_map_get_name(color_id, ComposeColorFields);
    if (name)
    {
      mutt_buffer_printf(buf, "compose %s", name);
      return;
    }
  }

  name = mutt_map_get_name(color_id, ColorFields);
  if (name)
    mutt_buffer_printf(buf, "%s", name);
  else
    mutt_buffer_printf(buf, "UNKNOWN %d", color_id);
}

/**
 * parse_uncolor - Parse an 'uncolor' command
 * @param buf     Temporary Buffer space
 * @param s       Buffer containing string to be parsed
 * @param err     Buffer for error messages
 * @param uncolor If true, 'uncolor', else 'unmono'
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * usage:
 * * uncolor index pattern [pattern...]
 * * unmono  index pattern [pattern...]
 */
static enum CommandResult parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                        struct Buffer *err, bool uncolor)
{
  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (mutt_str_equal(buf->data, "*"))
  {
    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");
    colors_clear();
    struct EventColor ev_c = { MT_COLOR_MAX };
    notify_send(Colors.notify, NT_COLOR, NT_COLOR_RESET, &ev_c);
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
    Colors.quotes[ql] = A_NORMAL;
    /* fallthrough to simple case */
  }

  if ((object != MT_COLOR_ATTACH_HEADERS) && (object != MT_COLOR_BODY) &&
      (object != MT_COLOR_HEADER) && (object != MT_COLOR_INDEX) &&
      (object != MT_COLOR_INDEX_AUTHOR) && (object != MT_COLOR_INDEX_FLAGS) &&
      (object != MT_COLOR_INDEX_SUBJECT) && (object != MT_COLOR_INDEX_TAG) &&
      (object != MT_COLOR_STATUS))
  {
    // Simple colours
    Colors.defs[object] = A_NORMAL;

    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(Colors.notify, NT_COLOR, NT_COLOR_RESET, &ev_c);
    return MUTT_CMD_SUCCESS;
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), uncolor ? "uncolor" : "unmono");
    return MUTT_CMD_WARNING;
  }

  if (OptNoCurses ||                // running without curses
      (uncolor && !has_colors()) || // parsing an uncolor command, and have no colors
      (!uncolor && has_colors())) // parsing an unmono command, and have colors
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
    changed |= do_uncolor(buf, s, mutt_color_attachments(), uncolor);
  else if (object == MT_COLOR_BODY)
    changed |= do_uncolor(buf, s, mutt_color_body(), uncolor);
  else if (object == MT_COLOR_HEADER)
    changed |= do_uncolor(buf, s, mutt_color_headers(), uncolor);
  else if (object == MT_COLOR_INDEX)
    changed |= do_uncolor(buf, s, mutt_color_index(), uncolor);
  else if (object == MT_COLOR_INDEX_AUTHOR)
    changed |= do_uncolor(buf, s, mutt_color_index_author(), uncolor);
  else if (object == MT_COLOR_INDEX_FLAGS)
    changed |= do_uncolor(buf, s, mutt_color_index_flags(), uncolor);
  else if (object == MT_COLOR_INDEX_SUBJECT)
    changed |= do_uncolor(buf, s, mutt_color_index_subject(), uncolor);
  else if (object == MT_COLOR_INDEX_TAG)
    changed |= do_uncolor(buf, s, mutt_color_index_tags(), uncolor);
  else if (object == MT_COLOR_STATUS)
    changed |= do_uncolor(buf, s, mutt_color_status_line(), uncolor);

  if (changed)
  {
    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(Colors.notify, NT_COLOR, NT_COLOR_RESET, &ev_c);
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_parse_uncolor - Parse the 'uncolor' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  if (OptNoCurses || !has_colors())
  {
    *s->dptr = '\0'; /* fake that we're done parsing */
    return MUTT_CMD_SUCCESS;
  }
  return parse_uncolor(buf, s, err, true);
}

/**
 * mutt_parse_unmono - Parse the 'unmono' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_unmono(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  if (OptNoCurses || !has_colors())
  {
    *s->dptr = '\0'; /* fake that we're done parsing */
    return MUTT_CMD_SUCCESS;
  }
  return parse_uncolor(buf, s, err, false);
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
static enum CommandResult add_pattern(struct ColorLineList *top, const char *s,
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
 * @param fg   Foreground colour ID
 * @param bg   Background colour ID
 * @param attr Attribute flags, e.g. A_BOLD
 * @retval num Combined colour pair
 */
static int fgbgattr_to_color(int fg, int bg, int attr)
{
  if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    return attr | mutt_color_alloc(fg, bg);
  return attr;
}

/**
 * parse_color - Parse a 'color' command
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

  if (object == MT_COLOR_ATTACH_HEADERS)
    rc = add_pattern(mutt_color_attachments(), buf->data, true, fg, bg, attr,
                     err, false, match);
  else if (object == MT_COLOR_BODY)
    rc = add_pattern(mutt_color_body(), buf->data, true, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_HEADER)
    rc = add_pattern(mutt_color_headers(), buf->data, false, fg, bg, attr, err, false, match);
  else if (object == MT_COLOR_INDEX)
  {
    rc = add_pattern(mutt_color_index(), buf->data, true, fg, bg, attr, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_AUTHOR)
  {
    rc = add_pattern(mutt_color_index_author(), buf->data, true, fg, bg, attr,
                     err, true, match);
  }
  else if (object == MT_COLOR_INDEX_FLAGS)
  {
    rc = add_pattern(mutt_color_index_flags(), buf->data, true, fg, bg, attr,
                     err, true, match);
  }
  else if (object == MT_COLOR_INDEX_SUBJECT)
  {
    rc = add_pattern(mutt_color_index_subject(), buf->data, true, fg, bg, attr,
                     err, true, match);
  }
  else if (object == MT_COLOR_INDEX_TAG)
  {
    rc = add_pattern(mutt_color_index_tags(), buf->data, true, fg, bg, attr, err, true, match);
  }
  else if (object == MT_COLOR_QUOTED)
  {
    if (q_level >= COLOR_QUOTES_MAX)
    {
      mutt_buffer_printf(err, _("Maximum quoting level is %d"), COLOR_QUOTES_MAX - 1);
      return MUTT_CMD_WARNING;
    }

    if (q_level >= Colors.quotes_used)
      Colors.quotes_used = q_level + 1;
    if (q_level == 0)
    {
      Colors.defs[MT_COLOR_QUOTED] = fgbgattr_to_color(fg, bg, attr);

      Colors.quotes[0] = Colors.defs[MT_COLOR_QUOTED];
      for (q_level = 1; q_level < Colors.quotes_used; q_level++)
      {
        if (Colors.quotes[q_level] == A_NORMAL)
          Colors.quotes[q_level] = Colors.defs[MT_COLOR_QUOTED];
      }
    }
    else
    {
      Colors.quotes[q_level] = fgbgattr_to_color(fg, bg, attr);
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

    rc = add_pattern(mutt_color_status_line(), buf->data, true, fg, bg, attr,
                     err, false, match);
  }
  else // Remaining simple colours
  {
    Colors.defs[object] = fgbgattr_to_color(fg, bg, attr);
    rc = MUTT_CMD_SUCCESS;
  }

  if (rc == MUTT_CMD_SUCCESS)
  {
    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_SET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(Colors.notify, NT_COLOR, NT_COLOR_SET, &ev_c);
  }

  return rc;
}

/**
 * mutt_parse_color - Parse the 'color' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_color(struct Buffer *buf, struct Buffer *s,
                                    intptr_t data, struct Buffer *err)
{
  bool dry_run = false;

  if (OptNoCurses || !has_colors())
    dry_run = true;

  return parse_color(buf, s, err, parse_color_pair, dry_run, true);
}

/**
 * mutt_parse_mono - Parse the 'mono' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_parse_mono(struct Buffer *buf, struct Buffer *s,
                                   intptr_t data, struct Buffer *err)
{
  bool dry_run = false;

  if (OptNoCurses || has_colors())
    dry_run = true;

  return parse_color(buf, s, err, parse_attr_spec, dry_run, false);
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
 * mutt_color_observer_add - Add an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_add(observer_t callback, void *global_data)
{
  notify_observer_add(Colors.notify, NT_COLOR, callback, global_data);
}

/**
 * mutt_color_observer_remove - Remove an observer
 * @param callback The callback
 * @param global_data The data
 */
void mutt_color_observer_remove(observer_t callback, void *global_data)
{
  notify_observer_remove(Colors.notify, callback, global_data);
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
