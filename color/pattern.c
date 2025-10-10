/**
 * @file
 * Pattern Colour
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page color_pattern Pattern Colour
 *
 * A set of patterns and colours that should be applied to a graphical object,
 * e.g List of Emails.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "pattern.h"
#include "index/lib.h"
#include "pattern/lib.h"
#include "attr.h"
#include "color.h"
#include "debug.h"
#include "domain.h"
#include "notify2.h"

// clang-format off
struct PatternColorList IndexList;          ///< List of default colours applied to the index
struct PatternColorList IndexAuthorList;    ///< List of colours applied to the author in the index
struct PatternColorList IndexCollapsedList; ///< List of colours applied to a collapsed thread in the index
struct PatternColorList IndexDateList;      ///< List of colours applied to the date in the index
struct PatternColorList IndexFlagsList;     ///< List of colours applied to the flags in the index
struct PatternColorList IndexLabelList;     ///< List of colours applied to the label in the index
struct PatternColorList IndexNumberList;    ///< List of colours applied to the message number in the index
struct PatternColorList IndexSizeList;      ///< List of colours applied to the size in the index
struct PatternColorList IndexSubjectList;   ///< List of colours applied to the subject in the index
struct PatternColorList IndexTagList;       ///< List of colours applied to tags in the index
struct PatternColorList IndexTagsList;      ///< List of colours applied to the tags in the index
// clang-format on

/**
 * pattern_colors_init - Initialise the Pattern colours
 */
void pattern_colors_init(void)
{
  color_debug(LL_DEBUG5, "init Pattern colours\n");
  STAILQ_INIT(&IndexList);
  STAILQ_INIT(&IndexAuthorList);
  STAILQ_INIT(&IndexCollapsedList);
  STAILQ_INIT(&IndexDateList);
  STAILQ_INIT(&IndexFlagsList);
  STAILQ_INIT(&IndexLabelList);
  STAILQ_INIT(&IndexNumberList);
  STAILQ_INIT(&IndexSizeList);
  STAILQ_INIT(&IndexSubjectList);
  STAILQ_INIT(&IndexTagList);
  STAILQ_INIT(&IndexTagsList);
}

/**
 * pattern_colors_reset - Reset the Pattern colours
 */
void pattern_colors_reset(void)
{
  color_debug(LL_DEBUG5, "reset Pattern colours\n");
  pattern_color_list_clear(&IndexList);
  pattern_color_list_clear(&IndexAuthorList);
  pattern_color_list_clear(&IndexCollapsedList);
  pattern_color_list_clear(&IndexDateList);
  pattern_color_list_clear(&IndexFlagsList);
  pattern_color_list_clear(&IndexLabelList);
  pattern_color_list_clear(&IndexNumberList);
  pattern_color_list_clear(&IndexSizeList);
  pattern_color_list_clear(&IndexSubjectList);
  pattern_color_list_clear(&IndexTagList);
  pattern_color_list_clear(&IndexTagsList);
}

/**
 * pattern_colors_cleanup - Cleanup the Pattern colours
 */
void pattern_colors_cleanup(void)
{
  pattern_colors_reset();
}

/**
 * pattern_color_clear - Free the contents of a Pattern colour
 * @param pcol PatternColor to empty
 *
 * @note The PatternColor object isn't freed
 */
void pattern_color_clear(struct PatternColor *pcol)
{
  if (!pcol)
    return;

  attr_color_clear(&pcol->attr_color);
  FREE(&pcol->pattern);
  mutt_pattern_free(&pcol->color_pattern);
}

/**
 * pattern_color_free - Free a Pattern colour
 * @param list PatternColorList holding the colour
 * @param ptr  PatternColor to free
 */
void pattern_color_free(struct PatternColorList *list, struct PatternColor **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct PatternColor *pcol = *ptr;
  pattern_color_clear(pcol);

  FREE(ptr);
}

/**
 * pattern_color_new - Create a new PatternColor
 * @retval ptr New PatternColor
 */
struct PatternColor *pattern_color_new(void)
{
  return MUTT_MEM_CALLOC(1, struct PatternColor);
}

/**
 * pattern_color_list_clear - Free the contents of a PatternColorList
 * @param pcl List to clear
 *
 * Free each of the PatternColorList in a list.
 *
 * @note The list object isn't freed, only emptied
 */
void pattern_color_list_clear(struct PatternColorList *pcl)
{
  if (!pcl)
    return;

  struct PatternColor *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, pcl, entries, tmp)
  {
    STAILQ_REMOVE(pcl, np, PatternColor, entries);
    pattern_color_free(pcl, &np);
  }
}

/**
 * pattern_color_list_new - Create a new PatternColorList
 * @retval ptr New PatternColorList
 */
struct PatternColorList *pattern_color_list_new(void)
{
  struct PatternColorList *pcl = MUTT_MEM_CALLOC(1, struct PatternColorList);

  STAILQ_INIT(pcl);

  struct PatternColor *rcol = pattern_color_new();
  STAILQ_INSERT_TAIL(pcl, rcol, entries);

  return pcl;
}

/**
 * pattern_colors_get_list - Return the PatternColorList for a Colour ID
 * @param cid Colour ID, e.g. #MT_COLOR_BODY
 * @retval ptr PatternColorList
 */
struct PatternColorList *pattern_colors_get_list(enum ColorId cid)
{
  switch (cid)
  {
    case MT_COLOR_INDEX:
      return &IndexList;
    case MT_COLOR_INDEX_AUTHOR:
      return &IndexAuthorList;
    case MT_COLOR_INDEX_COLLAPSED:
      return &IndexCollapsedList;
    case MT_COLOR_INDEX_DATE:
      return &IndexDateList;
    case MT_COLOR_INDEX_FLAGS:
      return &IndexFlagsList;
    case MT_COLOR_INDEX_LABEL:
      return &IndexLabelList;
    case MT_COLOR_INDEX_NUMBER:
      return &IndexNumberList;
    case MT_COLOR_INDEX_SIZE:
      return &IndexSizeList;
    case MT_COLOR_INDEX_SUBJECT:
      return &IndexSubjectList;
    case MT_COLOR_INDEX_TAG:
      return &IndexTagList;
    case MT_COLOR_INDEX_TAGS:
      return &IndexTagsList;
    default:
      return NULL;
  }
}

/**
 * add_pattern - Associate a colour to a pattern
 * @param pcl       List of existing colours
 * @param s         String to match
 * @param ac_val    Colour value to use
 * @param err       Buffer for error messages
 * @retval true Success
 */
static bool add_pattern(struct PatternColorList *pcl, const char *s,
                        const struct AttrColor *ac_val, struct Buffer *err)
{
  struct PatternColor *pcol = NULL;

  STAILQ_FOREACH(pcol, pcl, entries)
  {
    if (mutt_str_equal(s, pcol->pattern))
      break;
  }

  if (pcol) // found a matching Pattern
  {
    struct AttrColor *ac = &pcol->attr_color;
    attr_color_overwrite(ac, ac_val);
  }
  else
  {
    pcol = pattern_color_new();

    struct Buffer *buf = buf_pool_get();
    buf_strcpy(buf, s);
    const char *const c_simple_search = cs_subset_string(NeoMutt->sub, "simple_search");
    mutt_check_simple(buf, NONULL(c_simple_search));
    struct MailboxView *mv_cur = get_current_mailbox_view();
    pcol->color_pattern = mutt_pattern_comp(mv_cur, buf_string(buf), MUTT_PC_FULL_MSG, err);
    buf_pool_release(&buf);
    if (!pcol->color_pattern)
    {
      pattern_color_free(pcl, &pcol); //QWQ free? really?
      return false;
    }

    pcol->pattern = mutt_str_dup(s);

    struct AttrColor *ac = &pcol->attr_color;

    attr_color_overwrite(ac, ac_val);

    STAILQ_INSERT_TAIL(pcl, pcol, entries);
  }

  /* force re-caching of index colors */
  struct EventColor ev_c = { MT_COLOR_INDEX, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);

  return true;
}

/**
 * pattern_colors_parse_color_list - Parse a Pattern 'color' command
 * @param uc  Colour to modify
 * @param pat Pattern
 * @param ac  Colour value to use
 * @param err Buffer for error messages
 * @retval true Success
 *
 * Parse a Pattern 'color' command, e.g. "color index green default pattern"
 */
bool pattern_colors_parse_color_list(struct UserColor *uc, const char *pat,
                                     struct AttrColor *ac, struct Buffer *err)
{
  if (!uc || !uc->cdef || (uc->cdef->type != CDT_PATTERN) || !pat || !ac)
    return false;

  struct PatternColorList *pcl = uc->cdata;

  bool rc = add_pattern(pcl, pat, ac, err);

  color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", uc->cdef->name);

  return rc;
}

/**
 * pattern_colors_parse_uncolor - Parse a Pattern 'uncolor' command
 * @param cid     Colour ID, e.g. #MT_COLOR_STATUS
 * @param pat     Pattern to remove (NULL to remove all)
 * @retval true If colours were unset
 */
bool pattern_colors_parse_uncolor(enum ColorId cid, const char *pat)
{
  struct PatternColorList *cl = pattern_colors_get_list(cid);
  if (!cl)
    return false;

  if (!pat) // Reset all patterns
  {
    if (STAILQ_EMPTY(cl))
      return true;

    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");
    struct EventColor ev_c = { cid, NULL };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

    pattern_color_list_clear(cl);
    return true;
  }

  bool rc = false;
  struct PatternColor *np = NULL, *prev = NULL;
  prev = NULL;
  STAILQ_FOREACH(np, cl, entries)
  {
    if (mutt_str_equal(pat, np->pattern))
    {
      rc = true;

      color_debug(LL_DEBUG1, "Freeing pattern \"%s\" from XXX\n", pat);
      if (prev)
        STAILQ_REMOVE_AFTER(cl, prev, entries);
      else
        STAILQ_REMOVE_HEAD(cl, entries);

      mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: XXX\n");
      struct EventColor ev_c = { cid, &np->attr_color };
      notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

      pattern_color_free(cl, &np);
      break;
    }
    prev = np;
  }

  return rc;
}
