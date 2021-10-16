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

// clang-format off
struct ColorLineList AttachList;       ///< List of colours applied to the attachment headers
struct ColorLineList BodyList;         ///< List of colours applied to the email body
struct ColorLineList HeaderList;       ///< List of colours applied to the email headers
struct ColorLineList IndexAuthorList;  ///< List of colours applied to the author in the index
struct ColorLineList IndexFlagsList;   ///< List of colours applied to the flags in the index
struct ColorLineList IndexList;        ///< List of default colours applied to the index
struct ColorLineList IndexSubjectList; ///< List of colours applied to the subject in the index
struct ColorLineList IndexTagList;     ///< List of colours applied to tags in the index
struct ColorLineList StatusList;       ///< List of colours applied to the status bar
// clang-format on

/**
 * regex_color_free - Free a ColorLine
 * @param ptr         ColorLine to free
 * @param free_colors If true, free its colours too
 */
void regex_color_free(struct ColorLine **ptr, bool free_colors)
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
 * regex_color_list_clear - Clear a list of colours
 * @param list ColorLine List
 */
void regex_color_list_clear(struct ColorLineList *list)
{
  struct ColorLine *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, list, entries, tmp)
  {
    STAILQ_REMOVE(list, np, ColorLine, entries);
    regex_color_free(&np, true);
  }
}

/**
 * regex_colors_init - Initialise the Regex colours
 */
void regex_colors_init(void)
{
  STAILQ_INIT(&AttachList);
  STAILQ_INIT(&BodyList);
  STAILQ_INIT(&HeaderList);
  STAILQ_INIT(&IndexAuthorList);
  STAILQ_INIT(&IndexFlagsList);
  STAILQ_INIT(&IndexList);
  STAILQ_INIT(&IndexSubjectList);
  STAILQ_INIT(&IndexTagList);
  STAILQ_INIT(&StatusList);
}

/**
 * regex_colors_clear - Clear the Regex colours
 */
void regex_colors_clear(void)
{
  regex_color_list_clear(&AttachList);
  regex_color_list_clear(&BodyList);
  regex_color_list_clear(&HeaderList);
  regex_color_list_clear(&IndexList);
  regex_color_list_clear(&IndexAuthorList);
  regex_color_list_clear(&IndexFlagsList);
  regex_color_list_clear(&IndexSubjectList);
  regex_color_list_clear(&IndexTagList);
  regex_color_list_clear(&StatusList);
}

/**
 * regex_colors_get_list - Return the RegexColorList for a colour id
 * @param id Colour ID
 * @retval ptr RegexColorList
 */
struct ColorLineList *regex_colors_get_list(enum ColorId id)
{
  switch (id)
  {
    case MT_COLOR_ATTACH_HEADERS:
      return &AttachList;
    case MT_COLOR_BODY:
      return &BodyList;
    case MT_COLOR_HEADER:
      return &HeaderList;
    case MT_COLOR_INDEX:
      return &IndexList;
    case MT_COLOR_INDEX_AUTHOR:
      return &IndexAuthorList;
    case MT_COLOR_INDEX_FLAGS:
      return &IndexFlagsList;
    case MT_COLOR_INDEX_SUBJECT:
      return &IndexSubjectList;
    case MT_COLOR_INDEX_TAG:
      return &IndexTagList;
    case MT_COLOR_STATUS:
      return &StatusList;
    default:
      return NULL;
  }
}

/**
 * regex_color_new - Create a new ColorLine
 * @retval ptr Newly allocated ColorLine
 */
static struct ColorLine *regex_color_new(void)
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
    tmp = regex_color_new();
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
        regex_color_free(&tmp, true);
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
        regex_color_free(&tmp, true);
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
