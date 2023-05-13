/**
 * @file
 * Pattern handling for messages
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page pattern_message Pattern handling for messages
 *
 * Pattern handling for messages
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h" // IWYU pragma: keep
#include "core/lib.h"
#include "lib.h"
#include "menu/lib.h"
#include "mview.h"

#define KILO 1024
#define MEGA 1048576

/**
 * enum EatRangeError - Error codes for eat_range_by_regex()
 */
enum EatRangeError
{
  RANGE_E_OK,     ///< Range is valid
  RANGE_E_SYNTAX, ///< Range contains syntax error
  RANGE_E_MVIEW,  ///< Range requires MailboxView, but none available
};

/**
 * report_regerror - Create a regex error message
 * @param regerr Regex error code
 * @param preg   Regex pattern buffer
 * @param err    Buffer for error messages
 * @retval #RANGE_E_SYNTAX Always
 */
static int report_regerror(int regerr, regex_t *preg, struct Buffer *err)
{
  size_t ds = err->dsize;

  if (regerror(regerr, preg, err->data, ds) > ds)
    mutt_debug(LL_DEBUG2, "warning: buffer too small for regerror\n");
  /* The return value is fixed, exists only to shorten code at callsite */
  return RANGE_E_SYNTAX;
}

/**
 * is_menu_available - Do we need a MailboxView for this Pattern?
 * @param s      String to check
 * @param pmatch Regex matches
 * @param kind   Range type, e.g. #RANGE_K_REL
 * @param err    Buffer for error messages
 * @param menu   Current Menu
 * @retval false MailboxView is required, but not available
 * @retval true  Otherwise
 */
static bool is_menu_available(struct Buffer *s, regmatch_t pmatch[], int kind,
                              struct Buffer *err, const struct Menu *menu)
{
  const char *context_req_chars[] = {
    [RANGE_K_REL] = ".0123456789",
    [RANGE_K_ABS] = ".",
    [RANGE_K_LT] = "",
    [RANGE_K_GT] = "",
    [RANGE_K_BARE] = ".",
  };

  /* First decide if we're going to need the menu at all.
   * Relative patterns need it if they contain a dot or a number.
   * Absolute patterns only need it if they contain a dot. */
  char *context_loc = strpbrk(s->dptr + pmatch[0].rm_so, context_req_chars[kind]);
  if (!context_loc || (context_loc >= &s->dptr[pmatch[0].rm_eo]))
    return true;

  /* We need a current message.  Do we actually have one? */
  if (menu)
    return true;

  /* Nope. */
  buf_strcpy(err, _("No current message"));
  return false;
}

/**
 * scan_range_num - Parse a number range
 * @param s      String to parse
 * @param pmatch Array of regex matches
 * @param group  Index of regex match to use
 * @param kind   Range type, e.g. #RANGE_K_REL
 * @param mv     Mailbox view
 * @retval num Parse number
 */
static int scan_range_num(struct Buffer *s, regmatch_t pmatch[], int group,
                          int kind, struct MailboxView *mv)
{
  int num = (int) strtol(&s->dptr[pmatch[group].rm_so], NULL, 0);
  unsigned char c = (unsigned char) (s->dptr[pmatch[group].rm_eo - 1]);
  if (toupper(c) == 'K')
    num *= KILO;
  else if (toupper(c) == 'M')
    num *= MEGA;
  switch (kind)
  {
    case RANGE_K_REL:
    {
      struct Mailbox *m = mv->mailbox;
      struct Menu *menu = mv->menu;
      struct Email *e = mutt_get_virt_email(m, menu_get_index(menu));
      if (!e)
        return num;
      return num + EMSG(e);
    }
    case RANGE_K_LT:
      return num - 1;
    case RANGE_K_GT:
      return num + 1;
    default:
      return num;
  }
}

/**
 * scan_range_slot - Parse a range of message numbers
 * @param s      String to parse
 * @param pmatch Regex matches
 * @param grp    Which regex match to use
 * @param side   Which side of the range is this?  #RANGE_S_LEFT or #RANGE_S_RIGHT
 * @param kind   Range type, e.g. #RANGE_K_REL
 * @param mv     Mailbox view
 * @retval num Index number for the message specified
 */
static int scan_range_slot(struct Buffer *s, regmatch_t pmatch[], int grp,
                           int side, int kind, struct MailboxView *mv)
{
  struct Mailbox *m = mv->mailbox;
  struct Menu *menu = mv->menu;

  /* This means the left or right subpattern was empty, e.g. ",." */
  if ((pmatch[grp].rm_so == -1) || (pmatch[grp].rm_so == pmatch[grp].rm_eo))
  {
    if (side == RANGE_S_LEFT)
      return 1;
    if (side == RANGE_S_RIGHT)
      return m->msg_count;
  }
  /* We have something, so determine what */
  unsigned char c = (unsigned char) (s->dptr[pmatch[grp].rm_so]);
  switch (c)
  {
    case RANGE_CIRCUM:
      return 1;
    case RANGE_DOLLAR:
      return m->msg_count;
    case RANGE_DOT:
    {
      struct Email *e = mutt_get_virt_email(m, menu_get_index(menu));
      if (!e)
        return 1;
      return EMSG(e);
    }
    case RANGE_LT:
    case RANGE_GT:
      return scan_range_num(s, pmatch, grp + 1, kind, mv);
    default:
      /* Only other possibility: a number */
      return scan_range_num(s, pmatch, grp, kind, mv);
  }
}

/**
 * order_range - Put a range in order
 * @param pat Pattern to check
 */
static void order_range(struct Pattern *pat)
{
  if (pat->min <= pat->max)
    return;
  long num = pat->min;
  pat->min = pat->max;
  pat->max = num;
}

/**
 * eat_range_by_regex - Parse a range given as a regex
 * @param pat  Pattern to store the range in
 * @param s    String to parse
 * @param kind Range type, e.g. #RANGE_K_REL
 * @param err  Buffer for error messages
 * @param mv   Mailbox view
 * @retval num EatRangeError code, e.g. #RANGE_E_OK
 */
static int eat_range_by_regex(struct Pattern *pat, struct Buffer *s, int kind,
                              struct Buffer *err, struct MailboxView *mv)
{
  int regerr;
  regmatch_t pmatch[RANGE_RX_GROUPS];
  struct RangeRegex *pspec = &RangeRegexes[kind];

  /* First time through, compile the big regex */
  if (!pspec->ready)
  {
    regerr = regcomp(&pspec->cooked, pspec->raw, REG_EXTENDED);
    if (regerr != 0)
      return report_regerror(regerr, &pspec->cooked, err);
    pspec->ready = true;
  }

  /* Match the pattern buffer against the compiled regex.
   * No match means syntax error. */
  regerr = regexec(&pspec->cooked, s->dptr, RANGE_RX_GROUPS, pmatch, 0);
  if (regerr != 0)
    return report_regerror(regerr, &pspec->cooked, err);

  struct Mailbox *m = mv->mailbox;
  struct Menu *menu = mv->menu;
  if (!is_menu_available(s, pmatch, kind, err, menu))
    return RANGE_E_MVIEW;

  /* Snarf the contents of the two sides of the range. */
  pat->min = scan_range_slot(s, pmatch, pspec->lgrp, RANGE_S_LEFT, kind, mv);
  pat->max = scan_range_slot(s, pmatch, pspec->rgrp, RANGE_S_RIGHT, kind, mv);
  mutt_debug(LL_DEBUG1, "pat->min=%ld pat->max=%ld\n", pat->min, pat->max);

  /* Special case for a bare 0. */
  if ((kind == RANGE_K_BARE) && (pat->min == 0) && (pat->max == 0))
  {
    if (!m || !menu)
    {
      buf_strcpy(err, _("No current message"));
      return RANGE_E_MVIEW;
    }
    struct Email *e = mutt_get_virt_email(m, menu_get_index(menu));
    if (!e)
      return RANGE_E_MVIEW;

    pat->max = EMSG(e);
    pat->min = pat->max;
  }

  /* Since we don't enforce order, we must swap bounds if they're backward */
  order_range(pat);

  /* Slide pointer past the entire match. */
  s->dptr += pmatch[0].rm_eo;
  return RANGE_E_OK;
}

/**
 * eat_message_range - Parse a range of message numbers - Implements ::eat_arg_t - @ingroup eat_arg_api
 * @param pat   Pattern to store the results in
 * @param flags Flags, e.g. #MUTT_PC_PATTERN_DYNAMIC
 * @param s     String to parse
 * @param err   Buffer for error messages
 * @param mv    Mailbox view
 * @retval true The pattern was read successfully
 */
bool eat_message_range(struct Pattern *pat, PatternCompFlags flags,
                       struct Buffer *s, struct Buffer *err, struct MailboxView *mv)
{
  if (!mv || !mv->mailbox || !mv->menu)
  {
    // We need these for pretty much anything
    buf_strcpy(err, _("No mailbox is open"));
    return false;
  }

  bool skip_quote = false;

  /* If simple_search is set to "~m %s", the range will have double quotes
   * around it...  */
  if (*s->dptr == '"')
  {
    s->dptr++;
    skip_quote = true;
  }

  for (int i_kind = 0; i_kind != RANGE_K_INVALID; i_kind++)
  {
    switch (eat_range_by_regex(pat, s, i_kind, err, mv))
    {
      case RANGE_E_MVIEW:
        /* This means it matched syntactically but lacked context.
         * No point in continuing. */
        break;
      case RANGE_E_SYNTAX:
        /* Try another syntax, then */
        continue;
      case RANGE_E_OK:
        if (skip_quote && (*s->dptr == '"'))
          s->dptr++;
        SKIPWS(s->dptr);
        return true;
    }
  }
  return false;
}
