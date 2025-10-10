/**
 * @file
 * Regex Colour
 *
 * @authors
 * Copyright (C) 2021-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "attr.h"
#include "color.h"
#include "commands.h"
#include "debug.h"
#include "notify2.h"
#include "regex4.h"

// clang-format off
struct RegexColorList AttachList;         ///< List of colours applied to the attachment headers
struct RegexColorList BodyList;           ///< List of colours applied to the email body
struct RegexColorList HeaderList;         ///< List of colours applied to the email headers
struct RegexColorList StatusList;         ///< List of colours applied to the status bar
// clang-format on

/**
 * regex_colors_init - Initialise the Regex colours
 */
void regex_colors_init(void)
{
  color_debug(LL_DEBUG5, "init AttachList, BodyList, etc\n");
  STAILQ_INIT(&AttachList);
  STAILQ_INIT(&BodyList);
  STAILQ_INIT(&HeaderList);
  STAILQ_INIT(&StatusList);
}

/**
 * regex_colors_reset - Reset the Regex colours
 */
void regex_colors_reset(void)
{
  color_debug(LL_DEBUG5, "reset regex\n");
  regex_color_list_clear(&AttachList);
  regex_color_list_clear(&BodyList);
  regex_color_list_clear(&HeaderList);
  regex_color_list_clear(&StatusList);
}

/**
 * regex_colors_cleanup - Cleanup the Regex colours
 */
void regex_colors_cleanup(void)
{
  regex_colors_reset();
}

/**
 * regex_color_clear - Free the contents of a Regex colour
 * @param rcol RegexColor to empty
 *
 * @note The RegexColor object isn't freed
 */
void regex_color_clear(struct RegexColor *rcol)
{
  if (!rcol)
    return;

  rcol->match = 0;
  rcol->stop_matching = false;

  attr_color_clear(&rcol->attr_color);
  FREE(&rcol->pattern);
  regfree(&rcol->regex);
}

/**
 * regex_color_free - Free a Regex colour
 * @param ptr RegexColor to free
 */
void regex_color_free(struct RegexColor **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct RegexColor *rcol = *ptr;
  regex_color_clear(rcol);

  FREE(ptr);
}

/**
 * regex_color_new - Create a new RegexColor
 * @retval ptr New RegexColor
 */
struct RegexColor *regex_color_new(void)
{
  return MUTT_MEM_CALLOC(1, struct RegexColor);
}

/**
 * regex_color_list_new - Create a new RegexColorList
 * @retval ptr New RegexColorList
 */
struct RegexColorList *regex_color_list_new(void)
{
  struct RegexColorList *rcl = MUTT_MEM_CALLOC(1, struct RegexColorList);

  STAILQ_INIT(rcl);

  struct RegexColor *rcol = regex_color_new();
  STAILQ_INSERT_TAIL(rcl, rcol, entries);

  return rcl;
}

/**
 * regex_color_list_clear - Free the contents of a RegexColorList
 * @param rcl List to clear
 *
 * Free each of the RegexColorList in a list.
 *
 * @note The list object isn't freed, only emptied
 */
void regex_color_list_clear(struct RegexColorList *rcl)
{
  if (!rcl)
    return;

  struct RegexColor *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, rcl, entries, tmp)
  {
    STAILQ_REMOVE(rcl, np, RegexColor, entries);
    regex_color_free(&np);
  }
}

/**
 * regex_colors_get_list - Return the RegexColorList for a Colour ID
 * @param cid Colour ID, e.g. #MT_COLOR_BODY
 * @retval ptr RegexColorList
 */
struct RegexColorList *regex_colors_get_list(enum ColorId cid)
{
  switch (cid)
  {
    case MT_COLOR_ATTACH_HEADERS:
      return &AttachList;
    case MT_COLOR_BODY:
      return &BodyList;
    case MT_COLOR_HEADER:
      return &HeaderList;
    case MT_COLOR_STATUS:
      return &StatusList;
    default:
      return NULL;
  }
}

/**
 * add_pattern - Associate a colour to a pattern
 * @param rcl       List of existing colours
 * @param s         String to match
 * @param ac_val    Colour value to use
 * @param err       Buffer for error messages
 * @param match     Number of regex subexpression to match (0 for entire pattern)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult add_pattern(struct RegexColorList *rcl, const char *s,
                                      struct AttrColor *ac_val, struct Buffer *err, int match)
{
  struct RegexColor *rcol = NULL;

  STAILQ_FOREACH(rcol, rcl, entries)
  {
    if (mutt_str_equal(s, rcol->pattern))
      break;
  }

  if (rcol) // found a matching regex
  {
    struct AttrColor *ac = &rcol->attr_color;
    attr_color_overwrite(ac, ac_val);
  }
  else
  {
    rcol = regex_color_new();
    // Smart case matching
    uint16_t flags = mutt_mb_is_lower(s) ? REG_ICASE : 0;

    const int r = REG_COMP(&rcol->regex, s, flags);
    if (r != 0)
    {
      regerror(r, &rcol->regex, err->data, err->dsize);
      regex_color_free(&rcol);
      return MUTT_CMD_ERROR;
    }

    rcol->pattern = mutt_str_dup(s);
    rcol->match = match;

    struct AttrColor *ac = &rcol->attr_color;

    attr_color_overwrite(ac, ac_val);

    STAILQ_INSERT_TAIL(rcl, rcol, entries);
  }

  return MUTT_CMD_SUCCESS;
}

/**
 * regex_colors_parse_color_list - Parse a Regex 'color' command
 * @param cid     Colour ID, should be #MT_COLOR_STATUS
 * @param pat     Regex pattern
 * @param ac      Colour value to use
 * @param rc      Return code, e.g. #MUTT_CMD_SUCCESS
 * @param err     Buffer for error messages
 * @retval true Colour was parsed
 *
 * Parse a Regex 'color' command, e.g. "color status green default pattern"
 */
bool regex_colors_parse_color_list(enum ColorId cid, const char *pat,
                                   struct AttrColor *ac, int *rc, struct Buffer *err)

{
  if (cid == MT_COLOR_STATUS)
    return false;

  struct RegexColorList *rcl = regex_colors_get_list(cid);
  if (!rcl)
    return false;

  switch (cid)
  {
    case MT_COLOR_ATTACH_HEADERS:
    case MT_COLOR_BODY:
    case MT_COLOR_HEADER:
      break;
    default:
      return false;
  }

  *rc = add_pattern(rcl, pat, ac, err, 0);

  struct Buffer *buf = buf_pool_get();
  get_colorid_name(cid, buf);
  color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf_string(buf));
  buf_pool_release(&buf);

  struct EventColor ev_c = { cid, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);

  return true;
}

/**
 * regex_colors_parse_status_list - Parse a Regex 'color status' command
 * @param cid     Colour ID, should be #MT_COLOR_STATUS
 * @param pat     Regex pattern
 * @param ac      Colour value to use
 * @param match   Use the nth regex submatch
 * @param err     Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
int regex_colors_parse_status_list(enum ColorId cid, const char *pat,
                                   struct AttrColor *ac, int match, struct Buffer *err)
{
  if (cid != MT_COLOR_STATUS)
    return MUTT_CMD_ERROR;

  int rc = add_pattern(&StatusList, pat, ac, err, match);
  if (rc != MUTT_CMD_SUCCESS)
    return rc;

  struct Buffer *buf = buf_pool_get();
  get_colorid_name(cid, buf);
  color_debug(LL_DEBUG5, "NT_COLOR_SET: %s\n", buf_string(buf));
  buf_pool_release(&buf);

  struct EventColor ev_c = { cid, NULL };
  notify_send(ColorsNotify, NT_COLOR, NT_COLOR_SET, &ev_c);

  return rc;
}

/**
 * regex_colors_parse_uncolor - Parse a Regex 'uncolor' command
 * @param cid     Colour ID, e.g. #MT_COLOR_STATUS
 * @param pat     Pattern to remove (NULL to remove all)
 * @retval true If colours were unset
 */
bool regex_colors_parse_uncolor(enum ColorId cid, const char *pat)
{
  struct RegexColorList *cl = regex_colors_get_list(cid);
  if (!cl)
    return false;

  if (!pat) // Reset all patterns
  {
    if (STAILQ_EMPTY(cl))
      return true;

    mutt_debug(LL_NOTIFY, "NT_COLOR_RESET: [ALL]\n");
    struct EventColor ev_c = { cid, NULL };
    notify_send(ColorsNotify, NT_COLOR, NT_COLOR_RESET, &ev_c);

    regex_color_list_clear(cl);
    return true;
  }

  bool rc = false;
  struct RegexColor *np = NULL, *prev = NULL;
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

      regex_color_free(&np);
      break;
    }
    prev = np;
  }

  return rc;
}
