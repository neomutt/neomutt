/**
 * @file
 * Compile a Pattern
 *
 * @authors
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include <time.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "context.h"
#include "init.h"
#include "mutt_globals.h"
#include "mutt_menu.h"

typedef uint16_t ParseDateRangeFlags; ///< Flags for parse_date_range(), e.g. #MUTT_PDR_MINUS
#define MUTT_PDR_NO_FLAGS       0  ///< No flags are set
#define MUTT_PDR_MINUS    (1 << 0) ///< Pattern contains a range
#define MUTT_PDR_PLUS     (1 << 1) ///< Extend the range using '+'
#define MUTT_PDR_WINDOW   (1 << 2) ///< Extend the range in both directions using '*'
#define MUTT_PDR_ABSOLUTE (1 << 3) ///< Absolute pattern range
#define MUTT_PDR_DONE     (1 << 4) ///< Pattern parse successfully
#define MUTT_PDR_ERROR    (1 << 8) ///< Invalid pattern

#define MUTT_PDR_ERRORDONE (MUTT_PDR_ERROR | MUTT_PDR_DONE)

/**
 * enum EatRangeError - Error codes for eat_range_by_regex()
 */
enum EatRangeError
{
  RANGE_E_OK,     ///< Range is valid
  RANGE_E_SYNTAX, ///< Range contains syntax error
  RANGE_E_CTX,    ///< Range requires Context, but none available
};

#define KILO 1024
#define MEGA 1048576

/**
 * eat_regex - Parse a regex - Implements ::eat_arg_t
 */
static bool eat_regex(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err)
{
  struct Buffer buf;

  mutt_buffer_init(&buf);
  char *pexpr = s->dptr;
  if ((mutt_extract_token(&buf, s, MUTT_TOKEN_PATTERN | MUTT_TOKEN_COMMENT) != 0) || !buf.data)
  {
    mutt_buffer_printf(err, _("Error in expression: %s"), pexpr);
    FREE(&buf.data);
    return false;
  }
  if (buf.data[0] == '\0')
  {
    mutt_buffer_printf(err, "%s", _("Empty expression"));
    FREE(&buf.data);
    return false;
  }

  if (pat->string_match)
  {
    pat->p.str = mutt_str_dup(buf.data);
    pat->ign_case = mutt_mb_is_lower(buf.data);
    FREE(&buf.data);
  }
  else if (pat->group_match)
  {
    pat->p.group = mutt_pattern_group(buf.data);
    FREE(&buf.data);
  }
  else
  {
    pat->p.regex = mutt_mem_malloc(sizeof(regex_t));
    int case_flags = mutt_mb_is_lower(buf.data) ? REG_ICASE : 0;
    int rc = REG_COMP(pat->p.regex, buf.data, REG_NEWLINE | REG_NOSUB | case_flags);
    if (rc != 0)
    {
      char errmsg[256];
      regerror(rc, pat->p.regex, errmsg, sizeof(errmsg));
      mutt_buffer_add_printf(err, "'%s': %s", buf.data, errmsg);
      FREE(&buf.data);
      FREE(&pat->p.regex);
      return false;
    }
    FREE(&buf.data);
  }

  return true;
}

/**
 * add_query_msgid - Parse a Message-Id and add it to a list - Implements ::mutt_file_map_t
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
 * eat_query - Parse a query for an external search program - Implements ::eat_arg_t
 */
static bool eat_query(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err)
{
  struct Buffer cmd_buf;
  struct Buffer tok_buf;
  FILE *fp = NULL;

  if (!C_ExternalSearchCommand)
  {
    mutt_buffer_printf(err, "%s", _("No search command defined"));
    return false;
  }

  mutt_buffer_init(&tok_buf);
  char *pexpr = s->dptr;
  if ((mutt_extract_token(&tok_buf, s, MUTT_TOKEN_PATTERN | MUTT_TOKEN_COMMENT) != 0) ||
      !tok_buf.data)
  {
    mutt_buffer_printf(err, _("Error in expression: %s"), pexpr);
    return false;
  }
  if (*tok_buf.data == '\0')
  {
    mutt_buffer_printf(err, "%s", _("Empty expression"));
    FREE(&tok_buf.data);
    return false;
  }

  mutt_buffer_init(&cmd_buf);
  mutt_buffer_addstr(&cmd_buf, C_ExternalSearchCommand);
  mutt_buffer_addch(&cmd_buf, ' ');
  if (!Context || !Context->mailbox)
  {
    mutt_buffer_addch(&cmd_buf, '/');
  }
  else
  {
    char *escaped_folder = mutt_path_escape(mailbox_path(Context->mailbox));
    mutt_debug(LL_DEBUG2, "escaped folder path: %s\n", escaped_folder);
    mutt_buffer_addch(&cmd_buf, '\'');
    mutt_buffer_addstr(&cmd_buf, escaped_folder);
    mutt_buffer_addch(&cmd_buf, '\'');
  }
  mutt_buffer_addch(&cmd_buf, ' ');
  mutt_buffer_addstr(&cmd_buf, tok_buf.data);
  FREE(&tok_buf.data);

  mutt_message(_("Running search command: %s ..."), cmd_buf.data);
  pat->is_multi = true;
  mutt_list_clear(&pat->p.multi_cases);
  pid_t pid = filter_create(cmd_buf.data, NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_buffer_printf(err, "unable to fork command: %s\n", cmd_buf.data);
    FREE(&cmd_buf.data);
    return false;
  }

  mutt_file_map_lines(add_query_msgid, &pat->p.multi_cases, fp, 0);
  mutt_file_fclose(&fp);
  filter_wait(pid);
  FREE(&cmd_buf.data);
  return true;
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
  struct tm tm = mutt_date_localtime(MUTT_DATE_NOW);
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
      snprintf(err->data, err->dsize, _("Invalid day of month: %s"), s);
      return NULL;
    }
    if ((t->tm_mon < 0) || (t->tm_mon > 11))
    {
      snprintf(err->data, err->dsize, _("Invalid month: %s"), s);
      return NULL;
    }

    return (s + 8);
  }

  t->tm_mday = strtol(s, &p, 10);
  if ((t->tm_mday < 1) || (t->tm_mday > 31))
  {
    mutt_buffer_printf(err, _("Invalid day of month: %s"), s);
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
    mutt_buffer_printf(err, _("Invalid month: %s"), p);
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
              flags |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_ERRORDONE); /* done bad */
            else
            {
              /* reestablish initial base minimum if not specified */
              if (!have_min)
                memcpy(min, base_min, sizeof(struct tm));
              flags |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_DONE); /* done good */
            }
          }
          else
            flags |= MUTT_PDR_ERRORDONE;
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
          flags |= MUTT_PDR_ERRORDONE;
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
          flags |= MUTT_PDR_ERRORDONE;
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
    mutt_buffer_printf(err, _("Invalid relative date: %s"), pc - 1);
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
      min = mutt_date_localtime(MUTT_DATE_NOW);
      tm = &min;
    }
    else
    {
      max = mutt_date_localtime(MUTT_DATE_NOW);
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

      struct tm base_min;

      if (!have_min)
      { /* save base minimum and set current date, e.g. for "-3d+1d" */
        memcpy(&base_min, &min, sizeof(base_min));
        min = mutt_date_localtime(MUTT_DATE_NOW);
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
 * eat_range - Parse a number range - Implements ::eat_arg_t
 */
static bool eat_range(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err)
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
      pat->min = strtol(s->dptr, &tmp, 0);
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
    pat->max = MUTT_MAXRANGE;

  if (skip_quote && (*tmp == '"'))
    tmp++;

  SKIPWS(tmp);
  s->dptr = tmp;
  return true;
}

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
 * is_context_available - Do we need a Context for this Pattern?
 * @param s      String to check
 * @param pmatch Regex matches
 * @param kind   Range type, e.g. #RANGE_K_REL
 * @param err    Buffer for error messages
 * @retval false If context is required, but not available
 * @retval true  Otherwise
 */
static bool is_context_available(struct Buffer *s, regmatch_t pmatch[],
                                 int kind, struct Buffer *err)
{
  const char *context_req_chars[] = {
    [RANGE_K_REL] = ".0123456789",
    [RANGE_K_ABS] = ".",
    [RANGE_K_LT] = "",
    [RANGE_K_GT] = "",
    [RANGE_K_BARE] = ".",
  };

  /* First decide if we're going to need the context at all.
   * Relative patterns need it if they contain a dot or a number.
   * Absolute patterns only need it if they contain a dot. */
  char *context_loc = strpbrk(s->dptr + pmatch[0].rm_so, context_req_chars[kind]);
  if (!context_loc || (context_loc >= &s->dptr[pmatch[0].rm_eo]))
    return true;

  /* We need a current message.  Do we actually have one? */
  if (Context && Context->menu)
    return true;

  /* Nope. */
  mutt_buffer_strcpy(err, _("No current message"));
  return false;
}

/**
 * scan_range_num - Parse a number range
 * @param s      String to parse
 * @param pmatch Array of regex matches
 * @param group  Index of regex match to use
 * @param kind   Range type, e.g. #RANGE_K_REL
 * @retval num Parse number
 */
static int scan_range_num(struct Buffer *s, regmatch_t pmatch[], int group, int kind)
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
      struct Email *e = mutt_get_virt_email(Context->mailbox, Context->menu->current);
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
 * @retval num Index number for the message specified
 */
static int scan_range_slot(struct Buffer *s, regmatch_t pmatch[], int grp, int side, int kind)
{
  /* This means the left or right subpattern was empty, e.g. ",." */
  if ((pmatch[grp].rm_so == -1) || (pmatch[grp].rm_so == pmatch[grp].rm_eo))
  {
    if (side == RANGE_S_LEFT)
      return 1;
    if (side == RANGE_S_RIGHT)
      return Context->mailbox->msg_count;
  }
  /* We have something, so determine what */
  unsigned char c = (unsigned char) (s->dptr[pmatch[grp].rm_so]);
  switch (c)
  {
    case RANGE_CIRCUM:
      return 1;
    case RANGE_DOLLAR:
      return Context->mailbox->msg_count;
    case RANGE_DOT:
    {
      struct Email *e = mutt_get_virt_email(Context->mailbox, Context->menu->current);
      return EMSG(e);
    }
    case RANGE_LT:
    case RANGE_GT:
      return scan_range_num(s, pmatch, grp + 1, kind);
    default:
      /* Only other possibility: a number */
      return scan_range_num(s, pmatch, grp, kind);
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
  int num = pat->min;
  pat->min = pat->max;
  pat->max = num;
}

/**
 * eat_range_by_regex - Parse a range given as a regex
 * @param pat  Pattern to store the range in
 * @param s    String to parse
 * @param kind Range type, e.g. #RANGE_K_REL
 * @param err  Buffer for error messages
 * @retval num EatRangeError code, e.g. #RANGE_E_OK
 */
static int eat_range_by_regex(struct Pattern *pat, struct Buffer *s, int kind,
                              struct Buffer *err)
{
  int regerr;
  regmatch_t pmatch[RANGE_RX_GROUPS];
  struct RangeRegex *pspec = &range_regexes[kind];

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

  if (!is_context_available(s, pmatch, kind, err))
    return RANGE_E_CTX;

  /* Snarf the contents of the two sides of the range. */
  pat->min = scan_range_slot(s, pmatch, pspec->lgrp, RANGE_S_LEFT, kind);
  pat->max = scan_range_slot(s, pmatch, pspec->rgrp, RANGE_S_RIGHT, kind);
  mutt_debug(LL_DEBUG1, "pat->min=%d pat->max=%d\n", pat->min, pat->max);

  /* Special case for a bare 0. */
  if ((kind == RANGE_K_BARE) && (pat->min == 0) && (pat->max == 0))
  {
    if (!Context->menu)
    {
      mutt_buffer_strcpy(err, _("No current message"));
      return RANGE_E_CTX;
    }
    struct Email *e = mutt_get_virt_email(Context->mailbox, Context->menu->current);
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
 * eat_message_range - Parse a range of message numbers - Implements ::eat_arg_t
 */
static bool eat_message_range(struct Pattern *pat, int flags, struct Buffer *s,
                              struct Buffer *err)
{
  bool skip_quote = false;

  /* We need a Context for pretty much anything. */
  if (!Context)
  {
    mutt_buffer_strcpy(err, _("No Context"));
    return false;
  }

  /* If simple_search is set to "~m %s", the range will have double quotes
   * around it...  */
  if (*s->dptr == '"')
  {
    s->dptr++;
    skip_quote = true;
  }

  for (int i_kind = 0; i_kind != RANGE_K_INVALID; i_kind++)
  {
    switch (eat_range_by_regex(pat, s, i_kind, err))
    {
      case RANGE_E_CTX:
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

/**
 * eat_date - Parse a date pattern - Implements ::eat_arg_t
 */
static bool eat_date(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err)
{
  struct Buffer *tmp = mutt_buffer_pool_get();
  bool rc = false;

  char *pexpr = s->dptr;
  if (mutt_extract_token(tmp, s, MUTT_TOKEN_COMMENT | MUTT_TOKEN_PATTERN) != 0)
  {
    snprintf(err->data, err->dsize, _("Error in expression: %s"), pexpr);
    goto out;
  }

  if (mutt_buffer_is_empty(tmp))
  {
    snprintf(err->data, err->dsize, "%s", _("Empty expression"));
    goto out;
  }

  if (flags & MUTT_PC_PATTERN_DYNAMIC)
  {
    pat->dynamic = true;
    pat->p.str = mutt_str_dup(tmp->data);
  }

  rc = eval_date_minmax(pat, tmp->data, err);

out:
  mutt_buffer_pool_release(&tmp);

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
      level++;
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
      mutt_list_free(&np->p.multi_cases);
    else if (np->string_match || np->dynamic)
      FREE(&np->p.str);
    else if (np->group_match)
      np->p.group = NULL;
    else if (np->p.regex)
    {
      regfree(np->p.regex);
      FREE(&np->p.regex);
    }

    mutt_pattern_free(&np->child);
    FREE(&np);

    np = next;
  }

  FREE(pat);
}

/**
 * mutt_pattern_node_new - Create a new list containing a Pattern
 * @retval ptr Newly created list containing a single node with a Pattern
 */
static struct PatternList *mutt_pattern_node_new(void)
{
  struct PatternList *h = mutt_mem_calloc(1, sizeof(struct PatternList));
  SLIST_INIT(h);
  struct Pattern *p = mutt_mem_calloc(1, sizeof(struct Pattern));
  SLIST_INSERT_HEAD(h, p, entries);
  return h;
}

/**
 * mutt_pattern_comp - Create a Pattern
 * @param s     Pattern string
 * @param flags Flags, e.g. #MUTT_PC_FULL_MSG
 * @param err   Buffer for error messages
 * @retval ptr Newly allocated Pattern
 */
struct PatternList *mutt_pattern_comp(const char *s, PatternCompFlags flags, struct Buffer *err)
{
  /* curlist when assigned will always point to a list containing at least one node
   * with a Pattern value.  */
  struct PatternList *curlist = NULL;
  struct PatternList *tmp = NULL, *tmp2 = NULL;
  struct PatternList *last = NULL;
  bool pat_not = false;
  bool all_addr = false;
  bool pat_or = false;
  bool implicit = true; /* used to detect logical AND operator */
  bool is_alias = false;
  short thread_op;
  const struct PatternFlags *entry = NULL;
  char *p = NULL;
  char *buf = NULL;
  struct Buffer ps;

  if (!s || !*s)
  {
    mutt_str_copy(err->data, _("empty pattern"), err->dsize);
    return NULL;
  }

  mutt_buffer_init(&ps);
  ps.dptr = (char *) s;
  ps.dsize = mutt_str_len(s);

  while (*ps.dptr)
  {
    SKIPWS(ps.dptr);
    switch (*ps.dptr)
    {
      case '^':
        ps.dptr++;
        all_addr = !all_addr;
        break;
      case '!':
        ps.dptr++;
        pat_not = !pat_not;
        break;
      case '@':
        ps.dptr++;
        is_alias = !is_alias;
        break;
      case '|':
        if (!pat_or)
        {
          if (!curlist)
          {
            mutt_buffer_printf(err, _("error in pattern at: %s"), ps.dptr);
            return NULL;
          }

          struct Pattern *pat = SLIST_FIRST(curlist);

          if (SLIST_NEXT(pat, entries))
          {
            /* A & B | C == (A & B) | C */
            tmp = mutt_pattern_node_new();
            pat = SLIST_FIRST(tmp);
            pat->op = MUTT_PAT_AND;
            pat->child = curlist;

            curlist = tmp;
            last = curlist;
          }

          pat_or = true;
        }
        ps.dptr++;
        implicit = false;
        pat_not = false;
        all_addr = false;
        is_alias = false;
        break;
      case '%':
      case '=':
      case '~':
      {
        struct Pattern *pat = NULL;
        if (ps.dptr[1] == '\0')
        {
          mutt_buffer_printf(err, _("missing pattern: %s"), ps.dptr);
          goto cleanup;
        }
        thread_op = 0;
        if (ps.dptr[1] == '(')
          thread_op = MUTT_PAT_THREAD;
        else if ((ps.dptr[1] == '<') && (ps.dptr[2] == '('))
          thread_op = MUTT_PAT_PARENT;
        else if ((ps.dptr[1] == '>') && (ps.dptr[2] == '('))
          thread_op = MUTT_PAT_CHILDREN;
        if (thread_op)
        {
          ps.dptr++; /* skip ~ */
          if ((thread_op == MUTT_PAT_PARENT) || (thread_op == MUTT_PAT_CHILDREN))
            ps.dptr++;
          p = find_matching_paren(ps.dptr + 1);
          if (p[0] != ')')
          {
            mutt_buffer_printf(err, _("mismatched parentheses: %s"), ps.dptr);
            goto cleanup;
          }
          tmp = mutt_pattern_node_new();
          pat = SLIST_FIRST(tmp);
          pat->op = thread_op;
          if (last)
            SLIST_NEXT(SLIST_FIRST(last), entries) = pat;
          else
            curlist = tmp;
          last = tmp;
          pat->pat_not ^= pat_not;
          pat->all_addr |= all_addr;
          pat->is_alias |= is_alias;
          pat_not = false;
          all_addr = false;
          is_alias = false;
          /* compile the sub-expression */
          buf = mutt_strn_dup(ps.dptr + 1, p - (ps.dptr + 1));
          tmp2 = mutt_pattern_comp(buf, flags, err);
          if (!tmp2)
          {
            FREE(&buf);
            goto cleanup;
          }
          FREE(&buf);
          pat->child = tmp2;
          ps.dptr = p + 1; /* restore location */
          break;
        }
        if (implicit && pat_or)
        {
          /* A | B & C == (A | B) & C */
          tmp = mutt_pattern_node_new();
          pat = SLIST_FIRST(tmp);
          pat->op = MUTT_PAT_OR;
          pat->child = curlist;
          curlist = tmp;
          last = tmp;
          pat_or = false;
        }

        tmp = mutt_pattern_node_new();
        pat = SLIST_FIRST(tmp);
        pat->pat_not = pat_not;
        pat->all_addr = all_addr;
        pat->is_alias = is_alias;
        pat->string_match = (ps.dptr[0] == '=');
        pat->group_match = (ps.dptr[0] == '%');
        pat_not = false;
        all_addr = false;
        is_alias = false;

        if (last)
          SLIST_NEXT(SLIST_FIRST(last), entries) = pat;
        else
          curlist = tmp;
        if (curlist != last)
          FREE(&last);
        last = tmp;

        ps.dptr++; /* move past the ~ */
        entry = lookup_tag(*ps.dptr);
        if (!entry)
        {
          mutt_buffer_printf(err, _("%c: invalid pattern modifier"), *ps.dptr);
          goto cleanup;
        }
        if (entry->flags && ((flags & entry->flags) == 0))
        {
          mutt_buffer_printf(err, _("%c: not supported in this mode"), *ps.dptr);
          goto cleanup;
        }
        if (flags & MUTT_PC_SEND_MODE_SEARCH)
          pat->sendmode = true;

        pat->op = entry->op;

        ps.dptr++; /* eat the operator and any optional whitespace */
        SKIPWS(ps.dptr);

        if (entry->eat_arg)
        {
          int rc_eat = 0;
          if (ps.dptr[0] == '\0')
          {
            mutt_buffer_printf(err, "%s", _("missing parameter"));
            goto cleanup;
          }
          switch (entry->eat_arg)
          {
            case EAT_REGEX:
              rc_eat = eat_regex(pat, flags, &ps, err);
              break;
            case EAT_DATE:
              rc_eat = eat_date(pat, flags, &ps, err);
              break;
            case EAT_RANGE:
              rc_eat = eat_range(pat, flags, &ps, err);
              break;
            case EAT_MESSAGE_RANGE:
              rc_eat = eat_message_range(pat, flags, &ps, err);
              break;
            case EAT_QUERY:
              rc_eat = eat_query(pat, flags, &ps, err);
              break;
            default:
              break;
          }
          if (rc_eat == -1)
          {
            goto cleanup;
          }
        }
        implicit = true;
        break;
      }

      case '(':
      {
        p = find_matching_paren(ps.dptr + 1);
        if (p[0] != ')')
        {
          mutt_buffer_printf(err, _("mismatched parentheses: %s"), ps.dptr);
          goto cleanup;
        }
        /* compile the sub-expression */
        buf = mutt_strn_dup(ps.dptr + 1, p - (ps.dptr + 1));
        tmp = mutt_pattern_comp(buf, flags, err);
        FREE(&buf);
        if (!tmp)
          goto cleanup;
        struct Pattern *pat = SLIST_FIRST(tmp);
        if (last)
          SLIST_NEXT(SLIST_FIRST(last), entries) = pat;
        else
          curlist = tmp;
        last = tmp;
        pat = SLIST_FIRST(tmp);
        pat->pat_not ^= pat_not;
        pat->all_addr |= all_addr;
        pat->is_alias |= is_alias;
        pat_not = false;
        all_addr = false;
        is_alias = false;
        ps.dptr = p + 1; /* restore location */
        break;
      }

      default:
        mutt_buffer_printf(err, _("error in pattern at: %s"), ps.dptr);
        goto cleanup;
    }
  }
  if (!curlist)
  {
    mutt_buffer_strcpy(err, _("empty pattern"));
    return NULL;
  }
  if (curlist != tmp)
    FREE(&tmp);
  if (SLIST_NEXT(SLIST_FIRST(curlist), entries))
  {
    tmp = mutt_pattern_node_new();
    struct Pattern *pat = SLIST_FIRST(tmp);
    pat->op = pat_or ? MUTT_PAT_OR : MUTT_PAT_AND;
    pat->child = curlist;
    curlist = tmp;
  }

  return curlist;

cleanup:
  mutt_pattern_free(&curlist);
  return NULL;
}

