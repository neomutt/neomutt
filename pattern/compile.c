/**
 * @file
 * Compile a Pattern
 *
 * @authors
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page pattern_compile Compile a Pattern
 *
 * Compile a Pattern
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "parse/lib.h"
#include "globals.h"
#include "mview.h"

struct Menu;

// clang-format off
typedef uint16_t ParseDateRangeFlags; ///< Flags for parse_date_range(), e.g. #MUTT_PDR_MINUS
#define MUTT_PDR_NO_FLAGS       0  ///< No flags are set
#define MUTT_PDR_MINUS    (1 << 0) ///< Pattern contains a range
#define MUTT_PDR_PLUS     (1 << 1) ///< Extend the range using '+'
#define MUTT_PDR_WINDOW   (1 << 2) ///< Extend the range in both directions using '*'
#define MUTT_PDR_ABSOLUTE (1 << 3) ///< Absolute pattern range
#define MUTT_PDR_DONE     (1 << 4) ///< Pattern parse successfully
#define MUTT_PDR_ERROR    (1 << 8) ///< Invalid pattern
// clang-format on

#define MUTT_PDR_ERRORDONE (MUTT_PDR_ERROR | MUTT_PDR_DONE)

/**
 * eat_regex - Parse a regex - Implements ::eat_arg_t - @ingroup eat_arg_api
 */
static bool eat_regex(struct Pattern *pat, PatternCompFlags flags,
                      struct Buffer *s, struct Buffer *err)
{
  struct Buffer *buf = buf_pool_get();
  bool rc = false;
  char *pexpr = s->dptr;
  if ((parse_extract_token(buf, s, TOKEN_PATTERN | TOKEN_COMMENT) != 0) || !buf->data)
  {
    buf_printf(err, _("Error in expression: %s"), pexpr);
    goto out;
  }
  if (buf_is_empty(buf))
  {
    buf_addstr(err, _("Empty expression"));
    goto out;
  }

  if (pat->string_match)
  {
    pat->p.str = mutt_str_dup(buf->data);
    pat->ign_case = mutt_mb_is_lower(buf->data);
  }
  else if (pat->group_match)
  {
    pat->p.group = mutt_pattern_group(buf->data);
  }
  else
  {
    pat->p.regex = MUTT_MEM_CALLOC(1, regex_t);
#ifdef USE_DEBUG_GRAPHVIZ
    pat->raw_pattern = mutt_str_dup(buf->data);
#endif
    uint16_t case_flags = mutt_mb_is_lower(buf->data) ? REG_ICASE : 0;
    int rc2 = REG_COMP(pat->p.regex, buf->data, REG_NEWLINE | REG_NOSUB | case_flags);
    if (rc2 != 0)
    {
      char errmsg[256] = { 0 };
      regerror(rc2, pat->p.regex, errmsg, sizeof(errmsg));
      buf_printf(err, "'%s': %s", buf->data, errmsg);
      FREE(&pat->p.regex);
      goto out;
    }
  }

  rc = true;

out:
  buf_pool_release(&buf);
  return rc;
}

/**
 * add_query_msgid - Parse a Message-Id and add it to a list - Implements ::mutt_file_map_t - @ingroup mutt_file_map_api
 * @retval true Always
 */
static bool add_query_msgid(char *line, int line_num, void *user_data)
{
  struct ListHead *msgid_list = (struct ListHead *) (user_data);
  char *nows = mutt_str_skip_whitespace(line);
  if (*nows == '\0')
    return true;
  mutt_str_remove_trailing_ws(nows);
  mutt_list_insert_tail(msgid_list, mutt_str_dup(nows));
  return true;
}

/**
 * eat_query - Parse a query for an external search program - Implements ::eat_arg_t - @ingroup eat_arg_api
 * @param pat   Pattern to store the results in
 * @param flags Flags, e.g. #MUTT_PC_PATTERN_DYNAMIC
 * @param s     String to parse
 * @param err   Buffer for error messages
 * @param m     Mailbox
 * @retval true The pattern was read successfully
 */
static bool eat_query(struct Pattern *pat, PatternCompFlags flags,
                      struct Buffer *s, struct Buffer *err, struct Mailbox *m)
{
  struct Buffer *cmd_buf = buf_pool_get();
  struct Buffer *tok_buf = buf_pool_get();
  bool rc = false;

  FILE *fp = NULL;

  const char *const c_external_search_command = cs_subset_string(NeoMutt->sub, "external_search_command");
  if (!c_external_search_command)
  {
    buf_addstr(err, _("No search command defined"));
    goto out;
  }

  char *pexpr = s->dptr;
  if ((parse_extract_token(tok_buf, s, TOKEN_PATTERN | TOKEN_COMMENT) != 0) ||
      !tok_buf->data)
  {
    buf_printf(err, _("Error in expression: %s"), pexpr);
    goto out;
  }
  if (*tok_buf->data == '\0')
  {
    buf_addstr(err, _("Empty expression"));
    goto out;
  }

  buf_addstr(cmd_buf, c_external_search_command);
  buf_addch(cmd_buf, ' ');

  if (m)
  {
    char *escaped_folder = mutt_path_escape(mailbox_path(m));
    mutt_debug(LL_DEBUG2, "escaped folder path: %s\n", escaped_folder);
    buf_addch(cmd_buf, '\'');
    buf_addstr(cmd_buf, escaped_folder);
    buf_addch(cmd_buf, '\'');
  }
  else
  {
    buf_addch(cmd_buf, '/');
  }
  buf_addch(cmd_buf, ' ');
  buf_addstr(cmd_buf, tok_buf->data);

  mutt_message(_("Running search command: %s ..."), cmd_buf->data);
  pat->is_multi = true;
  mutt_list_clear(&pat->p.multi_cases);
  pid_t pid = filter_create(cmd_buf->data, NULL, &fp, NULL, EnvList);
  if (pid < 0)
  {
    buf_printf(err, "unable to fork command: %s\n", cmd_buf->data);
    goto out;
  }

  mutt_file_map_lines(add_query_msgid, &pat->p.multi_cases, fp, MUTT_RL_NO_FLAGS);
  mutt_file_fclose(&fp);
  filter_wait(pid);

  rc = true;

out:
  buf_pool_release(&cmd_buf);
  buf_pool_release(&tok_buf);
  return rc;
}

/**
 * get_offset - Calculate a symbolic offset
 * @param tm   Store the time here
 * @param s    string to parse
 * @param sign Sign of range, 1 for positive, -1 for negative
 * @retval ptr Next char after parsed offset
 *
 * - Ny years
 * - Nm months
 * - Nw weeks
 * - Nd days
 */
static const char *get_offset(struct tm *tm, const char *s, int sign)
{
  char *ps = NULL;
  int offset = strtol(s, &ps, 0);
  if (((sign < 0) && (offset > 0)) || ((sign > 0) && (offset < 0)))
    offset = -offset;

  switch (*ps)
  {
    case 'y':
      tm->tm_year += offset;
      break;
    case 'm':
      tm->tm_mon += offset;
      break;
    case 'w':
      tm->tm_mday += 7 * offset;
      break;
    case 'd':
      tm->tm_mday += offset;
      break;
    case 'H':
      tm->tm_hour += offset;
      break;
    case 'M':
      tm->tm_min += offset;
      break;
    case 'S':
      tm->tm_sec += offset;
      break;
    default:
      return s;
  }
  mutt_date_normalize_time(tm);
  return ps + 1;
}

/**
 * get_date - Parse a (partial) date in dd/mm/yyyy format
 * @param s   String to parse
 * @param t   Store the time here
 * @param err Buffer for error messages
 * @retval ptr First character after the date
 *
 * This function parses a (partial) date separated by '/'.  The month and year
 * are optional and if the year is less than 70 it's assumed to be after 2000.
 *
 * Examples:
 * - "10"         = 10 of this month, this year
 * - "10/12"      = 10 of December,   this year
 * - "10/12/04"   = 10 of December,   2004
 * - "10/12/2008" = 10 of December,   2008
 * - "20081210"   = 10 of December,   2008
 */
static const char *get_date(const char *s, struct tm *t, struct Buffer *err)
{
  char *p = NULL;
  struct tm tm = mutt_date_localtime(mutt_date_now());
  bool iso8601 = true;

  for (int v = 0; v < 8; v++)
  {
    if (s[v] && (s[v] >= '0') && (s[v] <= '9'))
      continue;

    iso8601 = false;
    break;
  }

  if (iso8601)
  {
    int year = 0;
    int month = 0;
    int mday = 0;
    sscanf(s, "%4d%2d%2d", &year, &month, &mday);

    t->tm_year = year;
    if (t->tm_year > 1900)
      t->tm_year -= 1900;
    t->tm_mon = month - 1;
    t->tm_mday = mday;

    if ((t->tm_mday < 1) || (t->tm_mday > 31))
    {
      buf_printf(err, _("Invalid day of month: %s"), s);
      return NULL;
    }
    if ((t->tm_mon < 0) || (t->tm_mon > 11))
    {
      buf_printf(err, _("Invalid month: %s"), s);
      return NULL;
    }

    return (s + 8);
  }

  t->tm_mday = strtol(s, &p, 10);
  if ((t->tm_mday < 1) || (t->tm_mday > 31))
  {
    buf_printf(err, _("Invalid day of month: %s"), s);
    return NULL;
  }
  if (*p != '/')
  {
    /* fill in today's month and year */
    t->tm_mon = tm.tm_mon;
    t->tm_year = tm.tm_year;
    return p;
  }
  p++;
  t->tm_mon = strtol(p, &p, 10) - 1;
  if ((t->tm_mon < 0) || (t->tm_mon > 11))
  {
    buf_printf(err, _("Invalid month: %s"), p);
    return NULL;
  }
  if (*p != '/')
  {
    t->tm_year = tm.tm_year;
    return p;
  }
  p++;
  t->tm_year = strtol(p, &p, 10);
  if (t->tm_year < 70) /* year 2000+ */
    t->tm_year += 100;
  else if (t->tm_year > 1900)
    t->tm_year -= 1900;
  return p;
}

/**
 * parse_date_range - Parse a date range
 * @param pc       String to parse
 * @param min      Earlier date
 * @param max      Later date
 * @param have_min Do we have a base minimum date?
 * @param base_min Base minimum date
 * @param err      Buffer for error messages
 * @retval ptr First character after the date
 */
static const char *parse_date_range(const char *pc, struct tm *min, struct tm *max,
                                    bool have_min, struct tm *base_min, struct Buffer *err)
{
  ParseDateRangeFlags flags = MUTT_PDR_NO_FLAGS;
  while (*pc && ((flags & MUTT_PDR_DONE) == 0))
  {
    const char *pt = NULL;
    char ch = *pc++;
    SKIPWS(pc);
    switch (ch)
    {
      case '-':
      {
        /* try a range of absolute date minus offset of Ndwmy */
        pt = get_offset(min, pc, -1);
        if (pc == pt)
        {
          if (flags == MUTT_PDR_NO_FLAGS)
          { /* nothing yet and no offset parsed => absolute date? */
            if (!get_date(pc, max, err))
            {
              flags |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_ERRORDONE); /* done bad */
            }
            else
            {
              /* reestablish initial base minimum if not specified */
              if (!have_min)
                memcpy(min, base_min, sizeof(struct tm));
              flags |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_DONE); /* done good */
            }
          }
          else
          {
            flags |= MUTT_PDR_ERRORDONE;
          }
        }
        else
        {
          pc = pt;
          if ((flags == MUTT_PDR_NO_FLAGS) && !have_min)
          { /* the very first "-3d" without a previous absolute date */
            max->tm_year = min->tm_year;
            max->tm_mon = min->tm_mon;
            max->tm_mday = min->tm_mday;
          }
          flags |= MUTT_PDR_MINUS;
        }
        break;
      }
      case '+':
      { /* enlarge plus range */
        pt = get_offset(max, pc, 1);
        if (pc == pt)
        {
          flags |= MUTT_PDR_ERRORDONE;
        }
        else
        {
          pc = pt;
          flags |= MUTT_PDR_PLUS;
        }
        break;
      }
      case '*':
      { /* enlarge window in both directions */
        pt = get_offset(min, pc, -1);
        if (pc == pt)
        {
          flags |= MUTT_PDR_ERRORDONE;
        }
        else
        {
          pc = get_offset(max, pc, 1);
          flags |= MUTT_PDR_WINDOW;
        }
        break;
      }
      default:
        flags |= MUTT_PDR_ERRORDONE;
    }
    SKIPWS(pc);
  }
  if ((flags & MUTT_PDR_ERROR) && !(flags & MUTT_PDR_ABSOLUTE))
  { /* get_date has its own error message, don't overwrite it here */
    buf_printf(err, _("Invalid relative date: %s"), pc - 1);
  }
  return (flags & MUTT_PDR_ERROR) ? NULL : pc;
}

/**
 * adjust_date_range - Put a date range in the correct order
 * @param[in,out] min Earlier date
 * @param[in,out] max Later date
 */
static void adjust_date_range(struct tm *min, struct tm *max)
{
  if ((min->tm_year > max->tm_year) ||
      ((min->tm_year == max->tm_year) && (min->tm_mon > max->tm_mon)) ||
      ((min->tm_year == max->tm_year) && (min->tm_mon == max->tm_mon) &&
       (min->tm_mday > max->tm_mday)))
  {
    int tmp;

    tmp = min->tm_year;
    min->tm_year = max->tm_year;
    max->tm_year = tmp;

    tmp = min->tm_mon;
    min->tm_mon = max->tm_mon;
    max->tm_mon = tmp;

    tmp = min->tm_mday;
    min->tm_mday = max->tm_mday;
    max->tm_mday = tmp;

    min->tm_hour = 0;
    min->tm_min = 0;
    min->tm_sec = 0;
    max->tm_hour = 23;
    max->tm_min = 59;
    max->tm_sec = 59;
  }
}

/**
 * eval_date_minmax - Evaluate a date-range pattern against 'now'
 * @param pat Pattern to modify
 * @param s   Pattern string to use
 * @param err Buffer for error messages
 * @retval true  Pattern valid and updated
 * @retval false Pattern invalid
 */
bool eval_date_minmax(struct Pattern *pat, const char *s, struct Buffer *err)
{
  /* the '0' time is Jan 1, 1970 UTC, so in order to prevent a negative time
   * when doing timezone conversion, we use Jan 2, 1970 UTC as the base here */
  struct tm min = { 0 };
  min.tm_mday = 2;
  min.tm_year = 70;

  /* Arbitrary year in the future.  Don't set this too high or
   * mutt_date_make_time() returns something larger than will fit in a time_t
   * on some systems */
  struct tm max = { 0 };
  max.tm_year = 130;
  max.tm_mon = 11;
  max.tm_mday = 31;
  max.tm_hour = 23;
  max.tm_min = 59;
  max.tm_sec = 59;

  if (strchr("<>=", s[0]))
  {
    /* offset from current time
     *  <3d  less than three days ago
     *  >3d  more than three days ago
     *  =3d  exactly three days ago */
    struct tm *tm = NULL;
    bool exact = false;

    if (s[0] == '<')
    {
      min = mutt_date_localtime(mutt_date_now());
      tm = &min;
    }
    else
    {
      max = mutt_date_localtime(mutt_date_now());
      tm = &max;

      if (s[0] == '=')
        exact = true;
    }

    /* Reset the HMS unless we are relative matching using one of those
     * offsets. */
    char *offset_type = NULL;
    strtol(s + 1, &offset_type, 0);
    if (!(*offset_type && strchr("HMS", *offset_type)))
    {
      tm->tm_hour = 23;
      tm->tm_min = 59;
      tm->tm_sec = 59;
    }

    /* force negative offset */
    get_offset(tm, s + 1, -1);

    if (exact)
    {
      /* start at the beginning of the day in question */
      memcpy(&min, &max, sizeof(max));
      min.tm_hour = 0;
      min.tm_sec = 0;
      min.tm_min = 0;
    }
  }
  else
  {
    const char *pc = s;

    bool have_min = false;
    bool until_now = false;
    if (isdigit((unsigned char) *pc))
    {
      /* minimum date specified */
      pc = get_date(pc, &min, err);
      if (!pc)
      {
        return false;
      }
      have_min = true;
      SKIPWS(pc);
      if (*pc == '-')
      {
        const char *pt = pc + 1;
        SKIPWS(pt);
        until_now = (*pt == '\0');
      }
    }

    if (!until_now)
    { /* max date or relative range/window */

      struct tm base_min = { 0 };

      if (!have_min)
      { /* save base minimum and set current date, e.g. for "-3d+1d" */
        memcpy(&base_min, &min, sizeof(base_min));
        min = mutt_date_localtime(mutt_date_now());
        min.tm_hour = 0;
        min.tm_sec = 0;
        min.tm_min = 0;
      }

      /* preset max date for relative offsets,
       * if nothing follows we search for messages on a specific day */
      max.tm_year = min.tm_year;
      max.tm_mon = min.tm_mon;
      max.tm_mday = min.tm_mday;

      if (!parse_date_range(pc, &min, &max, have_min, &base_min, err))
      { /* bail out on any parsing error */
        return false;
      }
    }
  }

  /* Since we allow two dates to be specified we'll have to adjust that. */
  adjust_date_range(&min, &max);

  pat->min = mutt_date_make_time(&min, true);
  pat->max = mutt_date_make_time(&max, true);

  return true;
}

/**
 * eat_range - Parse a number range - Implements ::eat_arg_t - @ingroup eat_arg_api
 */
static bool eat_range(struct Pattern *pat, PatternCompFlags flags,
                      struct Buffer *s, struct Buffer *err)
{
  char *tmp = NULL;
  bool do_exclusive = false;
  bool skip_quote = false;

  /* If simple_search is set to "~m %s", the range will have double quotes
   * around it...  */
  if (*s->dptr == '"')
  {
    s->dptr++;
    skip_quote = true;
  }
  if (*s->dptr == '<')
    do_exclusive = true;
  if ((*s->dptr != '-') && (*s->dptr != '<'))
  {
    /* range minimum */
    if (*s->dptr == '>')
    {
      pat->max = MUTT_MAXRANGE;
      pat->min = strtol(s->dptr + 1, &tmp, 0) + 1; /* exclusive range */
    }
    else
    {
      pat->min = strtol(s->dptr, &tmp, 0);
    }
    if (toupper((unsigned char) *tmp) == 'K') /* is there a prefix? */
    {
      pat->min *= 1024;
      tmp++;
    }
    else if (toupper((unsigned char) *tmp) == 'M')
    {
      pat->min *= 1048576;
      tmp++;
    }
    if (*s->dptr == '>')
    {
      s->dptr = tmp;
      return true;
    }
    if (*tmp != '-')
    {
      /* exact value */
      pat->max = pat->min;
      s->dptr = tmp;
      return true;
    }
    tmp++;
  }
  else
  {
    s->dptr++;
    tmp = s->dptr;
  }

  if (isdigit((unsigned char) *tmp))
  {
    /* range maximum */
    pat->max = strtol(tmp, &tmp, 0);
    if (toupper((unsigned char) *tmp) == 'K')
    {
      pat->max *= 1024;
      tmp++;
    }
    else if (toupper((unsigned char) *tmp) == 'M')
    {
      pat->max *= 1048576;
      tmp++;
    }
    if (do_exclusive)
      (pat->max)--;
  }
  else
  {
    pat->max = MUTT_MAXRANGE;
  }

  if (skip_quote && (*tmp == '"'))
    tmp++;

  SKIPWS(tmp);
  s->dptr = tmp;
  return true;
}

/**
 * eat_date - Parse a date pattern - Implements ::eat_arg_t - @ingroup eat_arg_api
 */
static bool eat_date(struct Pattern *pat, PatternCompFlags flags,
                     struct Buffer *s, struct Buffer *err)
{
  struct Buffer *tmp = buf_pool_get();
  bool rc = false;

  char *pexpr = s->dptr;
  if (parse_extract_token(tmp, s, TOKEN_COMMENT | TOKEN_PATTERN) != 0)
  {
    buf_printf(err, _("Error in expression: %s"), pexpr);
    goto out;
  }

  if (buf_is_empty(tmp))
  {
    buf_addstr(err, _("Empty expression"));
    goto out;
  }

  if (flags & MUTT_PC_PATTERN_DYNAMIC)
  {
    pat->dynamic = true;
    pat->p.str = mutt_str_dup(tmp->data);
  }

  rc = eval_date_minmax(pat, tmp->data, err);

out:
  buf_pool_release(&tmp);
  return rc;
}

/**
 * find_matching_paren - Find the matching parenthesis
 * @param s string to search
 * @retval ptr
 * - Matching close parenthesis
 * - End of string NUL, if not found
 */
static /* const */ char *find_matching_paren(/* const */ char *s)
{
  int level = 1;

  for (; *s; s++)
  {
    if (*s == '(')
    {
      level++;
    }
    else if (*s == ')')
    {
      level--;
      if (level == 0)
        break;
    }
  }
  return s;
}

/**
 * mutt_pattern_free - Free a Pattern
 * @param[out] pat Pattern to free
 */
void mutt_pattern_free(struct PatternList **pat)
{
  if (!pat || !*pat)
    return;

  struct Pattern *np = SLIST_FIRST(*pat), *next = NULL;

  while (np)
  {
    next = SLIST_NEXT(np, entries);

    if (np->is_multi)
    {
      mutt_list_free(&np->p.multi_cases);
    }
    else if (np->string_match || np->dynamic)
    {
      FREE(&np->p.str);
    }
    else if (np->group_match)
    {
      np->p.group = NULL;
    }
    else if (np->p.regex)
    {
      regfree(np->p.regex);
      FREE(&np->p.regex);
    }

#ifdef USE_DEBUG_GRAPHVIZ
    FREE(&np->raw_pattern);
#endif
    mutt_pattern_free(&np->child);
    FREE(&np);

    np = next;
  }

  FREE(pat);
}

/**
 * mutt_pattern_new - Create a new Pattern
 * @retval ptr Newly created Pattern
 */
static struct Pattern *mutt_pattern_new(void)
{
  return MUTT_MEM_CALLOC(1, struct Pattern);
}

/**
 * mutt_pattern_list_new - Create a new list containing a Pattern
 * @retval ptr Newly created list containing a single node with a Pattern
 */
static struct PatternList *mutt_pattern_list_new(void)
{
  struct PatternList *h = MUTT_MEM_CALLOC(1, struct PatternList);
  SLIST_INIT(h);
  struct Pattern *p = mutt_pattern_new();
  SLIST_INSERT_HEAD(h, p, entries);
  return h;
}

/**
 * attach_leaf - Attach a Pattern to a Pattern List
 * @param list Pattern List to attach to
 * @param leaf Pattern to attach
 * @retval ptr Attached leaf
 */
static struct Pattern *attach_leaf(struct PatternList *list, struct Pattern *leaf)
{
  struct Pattern *last = NULL;
  SLIST_FOREACH(last, list, entries)
  {
    // TODO - or we could use a doubly-linked list
    if (!SLIST_NEXT(last, entries))
    {
      SLIST_NEXT(last, entries) = leaf;
      break;
    }
  }
  return leaf;
}

/**
 * attach_new_root - Create a new Pattern as a parent for a List
 * @param curlist Pattern List
 * @retval ptr First Pattern in the original List
 *
 * @note curlist will be altered to the new root Pattern
 */
static struct Pattern *attach_new_root(struct PatternList **curlist)
{
  struct PatternList *root = mutt_pattern_list_new();
  struct Pattern *leaf = SLIST_FIRST(root);
  leaf->child = *curlist;
  *curlist = root;
  return leaf;
}

/**
 * attach_new_leaf - Attach a new Pattern to a List
 * @param curlist Pattern List
 * @retval ptr New Pattern in the original List
 *
 * @note curlist may be altered
 */
static struct Pattern *attach_new_leaf(struct PatternList **curlist)
{
  if (*curlist)
  {
    return attach_leaf(*curlist, mutt_pattern_new());
  }
  else
  {
    return attach_new_root(curlist);
  }
}

/**
 * mutt_pattern_comp - Create a Pattern
 * @param mv    Mailbox view
 * @param menu  Current Menu
 * @param s     Pattern string
 * @param flags Flags, e.g. #MUTT_PC_FULL_MSG
 * @param err   Buffer for error messages
 * @retval ptr Newly allocated Pattern
 */
struct PatternList *mutt_pattern_comp(struct MailboxView *mv, struct Menu *menu,
                                      const char *s, PatternCompFlags flags,
                                      struct Buffer *err)
{
  /* curlist when assigned will always point to a list containing at least one node
   * with a Pattern value.  */
  struct PatternList *curlist = NULL;
  bool pat_not = false;
  bool all_addr = false;
  bool pat_or = false;
  bool implicit = true; /* used to detect logical AND operator */
  bool is_alias = false;
  const struct PatternFlags *entry = NULL;
  char *p = NULL;
  char *buf = NULL;
  struct Mailbox *m = mv ? mv->mailbox : NULL;

  if (!s || (s[0] == '\0'))
  {
    buf_strcpy(err, _("empty pattern"));
    return NULL;
  }

  struct Buffer *ps = buf_pool_get();
  buf_strcpy(ps, s);
  buf_seek(ps, 0);

  SKIPWS(ps->dptr);
  while (*ps->dptr)
  {
    switch (*ps->dptr)
    {
      case '^':
        ps->dptr++;
        all_addr = !all_addr;
        break;
      case '!':
        ps->dptr++;
        pat_not = !pat_not;
        break;
      case '@':
        ps->dptr++;
        is_alias = !is_alias;
        break;
      case '|':
        if (!pat_or)
        {
          if (!curlist)
          {
            buf_printf(err, _("error in pattern at: %s"), ps->dptr);
            buf_pool_release(&ps);
            return NULL;
          }

          struct Pattern *pat = SLIST_FIRST(curlist);
          if (SLIST_NEXT(pat, entries))
          {
            /* A & B | C == (A & B) | C */
            struct Pattern *root = attach_new_root(&curlist);
            root->op = MUTT_PAT_AND;
          }

          pat_or = true;
        }
        ps->dptr++;
        implicit = false;
        pat_not = false;
        all_addr = false;
        is_alias = false;
        break;
      case '%':
      case '=':
      case '~':
      {
        if (ps->dptr[1] == '\0')
        {
          buf_printf(err, _("missing pattern: %s"), ps->dptr);
          goto cleanup;
        }
        short thread_op = 0;
        if (ps->dptr[1] == '(')
          thread_op = MUTT_PAT_THREAD;
        else if ((ps->dptr[1] == '<') && (ps->dptr[2] == '('))
          thread_op = MUTT_PAT_PARENT;
        else if ((ps->dptr[1] == '>') && (ps->dptr[2] == '('))
          thread_op = MUTT_PAT_CHILDREN;
        if (thread_op != 0)
        {
          ps->dptr++; /* skip ~ */
          if ((thread_op == MUTT_PAT_PARENT) || (thread_op == MUTT_PAT_CHILDREN))
            ps->dptr++;
          p = find_matching_paren(ps->dptr + 1);
          if (p[0] != ')')
          {
            buf_printf(err, _("mismatched parentheses: %s"), ps->dptr);
            goto cleanup;
          }
          struct Pattern *leaf = attach_new_leaf(&curlist);
          leaf->op = thread_op;
          leaf->pat_not = pat_not;
          leaf->all_addr = all_addr;
          leaf->is_alias = is_alias;
          pat_not = false;
          all_addr = false;
          is_alias = false;
          /* compile the sub-expression */
          buf = mutt_strn_dup(ps->dptr + 1, p - (ps->dptr + 1));
          leaf->child = mutt_pattern_comp(mv, menu, buf, flags, err);
          if (!leaf->child)
          {
            FREE(&buf);
            goto cleanup;
          }
          FREE(&buf);
          ps->dptr = p + 1; /* restore location */
          break;
        }
        if (implicit && pat_or)
        {
          /* A | B & C == (A | B) & C */
          struct Pattern *root = attach_new_root(&curlist);
          root->op = MUTT_PAT_OR;
          pat_or = false;
        }

        entry = lookup_tag(ps->dptr[1]);
        if (!entry)
        {
          buf_printf(err, _("%c: invalid pattern modifier"), *ps->dptr);
          goto cleanup;
        }
        if (entry->flags && ((flags & entry->flags) == 0))
        {
          buf_printf(err, _("%c: not supported in this mode"), *ps->dptr);
          goto cleanup;
        }

        struct Pattern *leaf = attach_new_leaf(&curlist);
        leaf->pat_not = pat_not;
        leaf->all_addr = all_addr;
        leaf->is_alias = is_alias;
        leaf->string_match = (ps->dptr[0] == '=');
        leaf->group_match = (ps->dptr[0] == '%');
        leaf->sendmode = (flags & MUTT_PC_SEND_MODE_SEARCH);
        leaf->op = entry->op;
        pat_not = false;
        all_addr = false;
        is_alias = false;

        ps->dptr++; /* move past the ~ */
        ps->dptr++; /* eat the operator and any optional whitespace */
        SKIPWS(ps->dptr);
        if (entry->eat_arg)
        {
          if (ps->dptr[0] == '\0')
          {
            buf_addstr(err, _("missing parameter"));
            goto cleanup;
          }
          switch (entry->eat_arg)
          {
            case EAT_REGEX:
              if (!eat_regex(leaf, flags, ps, err))
                goto cleanup;
              break;
            case EAT_DATE:
              if (!eat_date(leaf, flags, ps, err))
                goto cleanup;
              break;
            case EAT_RANGE:
              if (!eat_range(leaf, flags, ps, err))
                goto cleanup;
              break;
            case EAT_MESSAGE_RANGE:
              if (!eat_message_range(leaf, flags, ps, err, mv))
                goto cleanup;
              break;
            case EAT_QUERY:
              if (!eat_query(leaf, flags, ps, err, m))
                goto cleanup;
              break;
            default:
              break;
          }
        }
        implicit = true;
        break;
      }

      case '(':
      {
        p = find_matching_paren(ps->dptr + 1);
        if (p[0] != ')')
        {
          buf_printf(err, _("mismatched parentheses: %s"), ps->dptr);
          goto cleanup;
        }
        /* compile the sub-expression */
        buf = mutt_strn_dup(ps->dptr + 1, p - (ps->dptr + 1));
        struct PatternList *sub = mutt_pattern_comp(mv, menu, buf, flags, err);
        FREE(&buf);
        if (!sub)
          goto cleanup;
        struct Pattern *leaf = SLIST_FIRST(sub);
        if (curlist)
        {
          attach_leaf(curlist, leaf);
          FREE(&sub);
        }
        else
        {
          curlist = sub;
        }
        leaf->pat_not ^= pat_not;
        leaf->all_addr |= all_addr;
        leaf->is_alias |= is_alias;
        pat_not = false;
        all_addr = false;
        is_alias = false;
        ps->dptr = p + 1; /* restore location */
        break;
      }

      default:
        buf_printf(err, _("error in pattern at: %s"), ps->dptr);
        goto cleanup;
    }
    SKIPWS(ps->dptr);
  }
  buf_pool_release(&ps);

  if (!curlist)
  {
    buf_strcpy(err, _("empty pattern"));
    return NULL;
  }

  if (SLIST_NEXT(SLIST_FIRST(curlist), entries))
  {
    struct Pattern *root = attach_new_root(&curlist);
    root->op = pat_or ? MUTT_PAT_OR : MUTT_PAT_AND;
  }

  return curlist;

cleanup:
  mutt_pattern_free(&curlist);
  buf_pool_release(&ps);
  return NULL;
}
