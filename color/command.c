/**
 * @file
 * Parse colour commands
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
 * @page color_command Parse colour commands
 *
 * Parse NeoMutt 'color', 'uncolor', 'mono' and 'unmono' commands.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "init.h"
#include "options.h"

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
 * parse_color_name - Parse a colour name
 * @param[in]  s     String to parse
 * @param[out] col   Number for 'colorNNN' colours
 * @param[out] attrs Attribute flags, e.g. A_BOLD
 * @param[in]  is_fg true if this is a foreground colour
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * Parse a colour name, such as "red", "brightgreen", "color123".
 */
static enum CommandResult parse_color_name(const char *s, uint32_t *col, int *attrs,
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
      *attrs |= A_BOLD;
      *attrs |= A_BLINK;
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
        *attrs |= A_BOLD;
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
 * parse_attr_spec - Parse an attribute description - Implements ::parser_callback_t
 */
static enum CommandResult parse_attr_spec(struct Buffer *buf, struct Buffer *s,
                                          uint32_t *fg, uint32_t *bg,
                                          int *attrs, struct Buffer *err)
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
    *attrs |= A_BOLD;
  else if (mutt_istr_equal("none", buf->data))
    *attrs = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("normal", buf->data))
    *attrs = A_NORMAL; // Use '=' to clear other bits
  else if (mutt_istr_equal("reverse", buf->data))
    *attrs |= A_REVERSE;
  else if (mutt_istr_equal("standout", buf->data))
    *attrs |= A_STANDOUT;
  else if (mutt_istr_equal("underline", buf->data))
    *attrs |= A_UNDERLINE;
  else
  {
    mutt_buffer_printf(err, _("%s: no such attribute"), buf->data);
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_color_pair - Parse a pair of colours - Implements ::parser_callback_t
 */
static enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s,
                                           uint32_t *fg, uint32_t *bg,
                                           int *attrs, struct Buffer *err)
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
      *attrs |= A_BOLD;
    else if (mutt_istr_equal("none", buf->data))
      *attrs = A_NORMAL; // Use '=' to clear other bits
    else if (mutt_istr_equal("normal", buf->data))
      *attrs = A_NORMAL; // Use '=' to clear other bits
    else if (mutt_istr_equal("reverse", buf->data))
      *attrs |= A_REVERSE;
    else if (mutt_istr_equal("standout", buf->data))
      *attrs |= A_STANDOUT;
    else if (mutt_istr_equal("underline", buf->data))
      *attrs |= A_UNDERLINE;
    else
    {
      enum CommandResult rc = parse_color_name(buf->data, fg, attrs, true, err);
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

  return parse_color_name(buf->data, bg, attrs, false, err);
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
                       struct RegexColorList *cl, bool uncolor)
{
  struct RegexColor *np = NULL, *prev = NULL;
  bool rc = false;

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf->data))
    {
      rc = STAILQ_FIRST(cl);
      regex_color_list_clear(cl);
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
        regex_color_free(&np, uncolor);
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
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);
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
    QuotedColors[ql] = A_NORMAL;
    /* fallthrough to simple case */
  }

  if ((object != MT_COLOR_ATTACH_HEADERS) && (object != MT_COLOR_BODY) &&
      (object != MT_COLOR_HEADER) && (object != MT_COLOR_INDEX) &&
      (object != MT_COLOR_INDEX_AUTHOR) && (object != MT_COLOR_INDEX_FLAGS) &&
      (object != MT_COLOR_INDEX_SUBJECT) && (object != MT_COLOR_INDEX_TAG) &&
      (object != MT_COLOR_STATUS))
  {
    // Simple colours
    SimpleColors[object] = A_NORMAL;

    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);
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
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_ATTACH_HEADERS), uncolor);
  else if (object == MT_COLOR_BODY)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_BODY), uncolor);
  else if (object == MT_COLOR_HEADER)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_HEADER), uncolor);
  else if (object == MT_COLOR_INDEX)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_INDEX), uncolor);
  else if (object == MT_COLOR_INDEX_AUTHOR)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_INDEX_AUTHOR), uncolor);
  else if (object == MT_COLOR_INDEX_FLAGS)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_INDEX_FLAGS), uncolor);
  else if (object == MT_COLOR_INDEX_SUBJECT)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_INDEX_SUBJECT), uncolor);
  else if (object == MT_COLOR_INDEX_TAG)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_INDEX_TAG), uncolor);
  else if (object == MT_COLOR_STATUS)
    changed |= do_uncolor(buf, s, regex_colors_get_list(MT_COLOR_STATUS), uncolor);

  if (changed)
  {
    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * fgbgattr_to_color - Convert a foreground, background, attribute triplet into a colour
 * @param fg    Foreground colour ID
 * @param bg    Background colour ID
 * @param attrs Attribute flags, e.g. A_BOLD
 * @retval num Combined colour pair
 */
static int fgbgattr_to_color(int fg, int bg, int attrs)
{
  if ((fg != COLOR_UNSET) && (bg != COLOR_UNSET))
    return attrs | mutt_color_alloc(fg, bg);
  return attrs;
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
  int attrs = 0, q_level = 0;
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

  rc = callback(buf, s, &fg, &bg, &attrs, err);
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
    rc = add_pattern(regex_colors_get_list(MT_COLOR_ATTACH_HEADERS), buf->data,
                     true, fg, bg, attrs, err, false, match);
  else if (object == MT_COLOR_BODY)
    rc = add_pattern(regex_colors_get_list(MT_COLOR_BODY), buf->data, true, fg,
                     bg, attrs, err, false, match);
  else if (object == MT_COLOR_HEADER)
    rc = add_pattern(regex_colors_get_list(MT_COLOR_HEADER), buf->data, false,
                     fg, bg, attrs, err, false, match);
  else if (object == MT_COLOR_INDEX)
  {
    rc = add_pattern(regex_colors_get_list(MT_COLOR_INDEX), buf->data, true, fg,
                     bg, attrs, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_AUTHOR)
  {
    rc = add_pattern(regex_colors_get_list(MT_COLOR_INDEX_AUTHOR), buf->data,
                     true, fg, bg, attrs, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_FLAGS)
  {
    rc = add_pattern(regex_colors_get_list(MT_COLOR_INDEX_FLAGS), buf->data,
                     true, fg, bg, attrs, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_SUBJECT)
  {
    rc = add_pattern(regex_colors_get_list(MT_COLOR_INDEX_SUBJECT), buf->data,
                     true, fg, bg, attrs, err, true, match);
  }
  else if (object == MT_COLOR_INDEX_TAG)
  {
    rc = add_pattern(regex_colors_get_list(MT_COLOR_INDEX_TAG), buf->data, true,
                     fg, bg, attrs, err, true, match);
  }
  else if (object == MT_COLOR_QUOTED)
  {
    if (q_level >= COLOR_QUOTES_MAX)
    {
      mutt_buffer_printf(err, _("Maximum quoting level is %d"), COLOR_QUOTES_MAX - 1);
      return MUTT_CMD_WARNING;
    }

    if (q_level >= NumQuotedColors)
      NumQuotedColors = q_level + 1;
    if (q_level == 0)
    {
      SimpleColors[MT_COLOR_QUOTED] = fgbgattr_to_color(fg, bg, attrs);

      QuotedColors[0] = SimpleColors[MT_COLOR_QUOTED];
      for (q_level = 1; q_level < NumQuotedColors; q_level++)
      {
        if (QuotedColors[q_level] == A_NORMAL)
          QuotedColors[q_level] = SimpleColors[MT_COLOR_QUOTED];
      }
    }
    else
    {
      QuotedColors[q_level] = fgbgattr_to_color(fg, bg, attrs);
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

    rc = add_pattern(regex_colors_get_list(MT_COLOR_STATUS), buf->data, true,
                     fg, bg, attrs, err, false, match);
  }
  else // Remaining simple colours
  {
    SimpleColors[object] = fgbgattr_to_color(fg, bg, attrs);
    rc = MUTT_CMD_SUCCESS;
  }

  if (rc == MUTT_CMD_SUCCESS)
  {
    get_colorid_name(object, buf);
    mutt_debug(LL_NOTIFY, "NT_COLOR_SET: %s\n", buf->data);
    struct EventColor ev_c = { object };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);
  }

  return rc;
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
