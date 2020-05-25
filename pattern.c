/**
 * @file
 * Match patterns to emails
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page pattern Match patterns to emails
 *
 * Match patterns to emails
 */

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "pattern.h"
#include "context.h"
#include "copy.h"
#include "handler.h"
#include "hdrline.h"
#include "init.h"
#include "maillist.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "progress.h"
#include "protos.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifndef USE_FMEMOPEN
#include <sys/stat.h>
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/* These Config Variables are only used in pattern.c */
bool C_ThoroughSearch; ///< Config: Decode headers and messages before searching them

// clang-format off
/* The regexes in a modern format */
#define RANGE_NUM_RX      "([[:digit:]]+|0x[[:xdigit:]]+)[MmKk]?"
#define RANGE_REL_SLOT_RX "[[:blank:]]*([.^$]|-?" RANGE_NUM_RX ")?[[:blank:]]*"
#define RANGE_REL_RX      "^" RANGE_REL_SLOT_RX "," RANGE_REL_SLOT_RX

/* Almost the same, but no negative numbers allowed */
#define RANGE_ABS_SLOT_RX "[[:blank:]]*([.^$]|" RANGE_NUM_RX ")?[[:blank:]]*"
#define RANGE_ABS_RX      "^" RANGE_ABS_SLOT_RX "-" RANGE_ABS_SLOT_RX

/* First group is intentionally empty */
#define RANGE_LT_RX "^()[[:blank:]]*(<[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"
#define RANGE_GT_RX "^()[[:blank:]]*(>[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"

/* Single group for min and max */
#define RANGE_BARE_RX "^[[:blank:]]*([.^$]|" RANGE_NUM_RX ")[[:blank:]]*"
#define RANGE_RX_GROUPS 5

#define KILO 1024
#define MEGA 1048576
#define EMSG(e) (((e)->msgno) + 1)

#define MUTT_MAXRANGE -1

typedef uint16_t ParseDateRangeFlags; ///< Flags for parse_date_range(), e.g. #MUTT_PDR_MINUS
#define MUTT_PDR_NO_FLAGS       0  ///< No flags are set
#define MUTT_PDR_MINUS    (1 << 0) ///< Pattern contains a range
#define MUTT_PDR_PLUS     (1 << 1) ///< Extend the range using '+'
#define MUTT_PDR_WINDOW   (1 << 2) ///< Extend the range in both directions using '*'
#define MUTT_PDR_ABSOLUTE (1 << 3) ///< Absolute pattern range
#define MUTT_PDR_DONE     (1 << 4) ///< Pattern parse successfully
#define MUTT_PDR_ERROR    (1 << 8) ///< Invalid pattern

#define MUTT_PDR_ERRORDONE (MUTT_PDR_ERROR | MUTT_PDR_DONE)

#define RANGE_DOT    '.'
#define RANGE_CIRCUM '^'
#define RANGE_DOLLAR '$'
#define RANGE_LT     '<'
#define RANGE_GT     '>'
// clang-format on

/**
 * enum EatRangeError - Error codes for eat_range_by_regex()
 */
enum EatRangeError
{
  RANGE_E_OK,     ///< Range is valid
  RANGE_E_SYNTAX, ///< Range contains syntax error
  RANGE_E_CTX,    ///< Range requires Context, but none available
};

/**
 * struct RangeRegex - Regular expression representing a range
 */
struct RangeRegex
{
  const char *raw; ///< Regex as string
  int lgrp;        ///< Paren group matching the left side
  int rgrp;        ///< Paren group matching the right side
  bool ready;      ///< Compiled yet?
  regex_t cooked;  ///< Compiled form
};

/**
 * enum RangeType - Type of range
 */
enum RangeType
{
  RANGE_K_REL,  ///< Relative range
  RANGE_K_ABS,  ///< Absolute range
  RANGE_K_LT,   ///< Less-than range
  RANGE_K_GT,   ///< Greater-than range
  RANGE_K_BARE, ///< Single symbol
  /* add new ones HERE */
  RANGE_K_INVALID, ///< Range is invalid
};

/**
 * enum RangeSide - Which side of the range
 */
enum RangeSide
{
  RANGE_S_LEFT,  ///< Left side of range
  RANGE_S_RIGHT, ///< Right side of range
};

/**
 * struct PatternFlags - Mapping between user character and internal constant
 */
struct PatternFlags
{
  int tag;   ///< Character used to represent this operation, e.g. 'A' for '~A'
  int op;    ///< Operation to perform, e.g. #MUTT_PAT_SCORE
  int flags; ///< Pattern flags, e.g. #MUTT_PC_FULL_MSG

  /**
   * eat_arg - Function to parse a pattern
   * @param pat   Pattern to store the results in
   * @param flags Flags, e.g. #MUTT_PC_PATTERN_DYNAMIC
   * @param s     String to parse
   * @param err   Buffer for error messages
   * @retval true If the pattern was read successfully
   */
  bool (*eat_arg)(struct Pattern *pat, int flags, struct Buffer *s, struct Buffer *err);
};

// clang-format off
/**
 * range_regexes - Set of Regexes for various range types
 *
 * This array, will also contain the compiled regexes.
 */
static struct RangeRegex range_regexes[] = {
  [RANGE_K_REL]  = { RANGE_REL_RX,  1, 3, 0, { 0 } },
  [RANGE_K_ABS]  = { RANGE_ABS_RX,  1, 3, 0, { 0 } },
  [RANGE_K_LT]   = { RANGE_LT_RX,   1, 2, 0, { 0 } },
  [RANGE_K_GT]   = { RANGE_GT_RX,   2, 1, 0, { 0 } },
  [RANGE_K_BARE] = { RANGE_BARE_RX, 1, 1, 0, { 0 } },
};
// clang-format on

static struct PatternList *SearchPattern = NULL; ///< current search pattern
static char LastSearch[256] = { 0 };             ///< last pattern searched for
static char LastSearchExpn[1024] = { 0 }; ///< expanded version of LastSearch

/**
 * typedef addr_predicate_t - Test an Address for some condition
 * @param a Address to test
 * @retval bool True if Address matches the test
 */
typedef bool (*addr_predicate_t)(const struct Address *a);

/**
 * eat_regex - Parse a regex - Implements Pattern::eat_arg()
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
 * eat_query - Parse a query for an external search program - Implements Pattern::eat_arg()
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
static bool eval_date_minmax(struct Pattern *pat, const char *s, struct Buffer *err)
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
 * eat_range - Parse a number range - Implements Pattern::eat_arg()
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
 * eat_message_range - Parse a range of message numbers - Implements Pattern::eat_arg()
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
 * eat_date - Parse a date pattern - Implements Pattern::eat_arg()
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
 * patmatch - Compare a string to a Pattern
 * @param pat Pattern to use
 * @param buf String to compare
 * @retval true  Match
 * @retval false No match
 */
static bool patmatch(const struct Pattern *pat, const char *buf)
{
  if (pat->is_multi)
    return (mutt_list_find(&pat->p.multi_cases, buf) != NULL);
  if (pat->string_match)
    return pat->ign_case ? strcasestr(buf, pat->p.str) : strstr(buf, pat->p.str);
  if (pat->group_match)
    return mutt_group_match(pat->p.group, buf);
  return (regexec(pat->p.regex, buf, 0, NULL, 0) == 0);
}

/**
 * msg_search - Search an email
 * @param m   Mailbox
 * @param pat   Pattern to find
 * @param msgno Message to search
 * @retval true Pattern found
 * @retval false Error or pattern not found
 */
static bool msg_search(struct Mailbox *m, struct Pattern *pat, int msgno)
{
  bool match = false;
  struct Message *msg = mx_msg_open(m, msgno);
  if (!msg)
  {
    return match;
  }

  FILE *fp = NULL;
  long len = 0;
  struct Email *e = m->emails[msgno];
#ifdef USE_FMEMOPEN
  char *temp = NULL;
  size_t tempsize = 0;
#else
  struct stat st;
#endif

  if (C_ThoroughSearch)
  {
    /* decode the header / body */
    struct State s = { 0 };
    s.fp_in = msg->fp;
    s.flags = MUTT_CHARCONV;
#ifdef USE_FMEMOPEN
    s.fp_out = open_memstream(&temp, &tempsize);
    if (!s.fp_out)
    {
      mutt_perror(_("Error opening 'memory stream'"));
      return false;
    }
#else
    s.fp_out = mutt_file_mkstemp();
    if (!s.fp_out)
    {
      mutt_perror(_("Can't create temporary file"));
      return false;
    }
#endif

    if (pat->op != MUTT_PAT_BODY)
      mutt_copy_header(msg->fp, e, s.fp_out, CH_FROM | CH_DECODE, NULL, 0);

    if (pat->op != MUTT_PAT_HEADER)
    {
      mutt_parse_mime_message(m, e);

      if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT) &&
          !crypt_valid_passphrase(e->security))
      {
        mx_msg_close(m, &msg);
        if (s.fp_out)
        {
          mutt_file_fclose(&s.fp_out);
#ifdef USE_FMEMOPEN
          FREE(&temp);
#endif
        }
        return false;
      }

      fseeko(msg->fp, e->offset, SEEK_SET);
      mutt_body_handler(e->content, &s);
    }

#ifdef USE_FMEMOPEN
    mutt_file_fclose(&s.fp_out);
    len = tempsize;

    if (tempsize != 0)
    {
      fp = fmemopen(temp, tempsize, "r");
      if (!fp)
      {
        mutt_perror(_("Error re-opening 'memory stream'"));
        FREE(&temp);
        return false;
      }
    }
    else
    { /* fmemopen can't handle empty buffers */
      fp = mutt_file_fopen("/dev/null", "r");
      if (!fp)
      {
        mutt_perror(_("Error opening /dev/null"));
        return false;
      }
    }
#else
    fp = s.fp_out;
    fflush(fp);
    fseek(fp, 0, SEEK_SET);
    fstat(fileno(fp), &st);
    len = (long) st.st_size;
#endif
  }
  else
  {
    /* raw header / body */
    fp = msg->fp;
    if (pat->op != MUTT_PAT_BODY)
    {
      fseeko(fp, e->offset, SEEK_SET);
      len = e->content->offset - e->offset;
    }
    if (pat->op != MUTT_PAT_HEADER)
    {
      if (pat->op == MUTT_PAT_BODY)
        fseeko(fp, e->content->offset, SEEK_SET);
      len += e->content->length;
    }
  }

  size_t blen = 256;
  char *buf = mutt_mem_malloc(blen);

  /* search the file "fp" */
  while (len > 0)
  {
    if (pat->op == MUTT_PAT_HEADER)
    {
      buf = mutt_rfc822_read_line(fp, buf, &blen);
      if (*buf == '\0')
        break;
    }
    else if (!fgets(buf, blen - 1, fp))
      break; /* don't loop forever */
    if (patmatch(pat, buf))
    {
      match = true;
      break;
    }
    len -= mutt_str_len(buf);
  }

  FREE(&buf);

  mx_msg_close(m, &msg);

  if (C_ThoroughSearch)
    mutt_file_fclose(&fp);

#ifdef USE_FMEMOPEN
  FREE(&temp);
#endif
  return match;
}

// clang-format off
/**
 * Flags - Lookup table for all patterns
 */
static const struct PatternFlags Flags[] = {
  { 'A', MUTT_ALL,                 0,                NULL              },
  { 'b', MUTT_PAT_BODY,            MUTT_PC_FULL_MSG|MUTT_PC_SEND_MODE_SEARCH, eat_regex },
  { 'B', MUTT_PAT_WHOLE_MSG,       MUTT_PC_FULL_MSG|MUTT_PC_SEND_MODE_SEARCH, eat_regex },
  { 'c', MUTT_PAT_CC,              0,                eat_regex         },
  { 'C', MUTT_PAT_RECIPIENT,       0,                eat_regex         },
  { 'd', MUTT_PAT_DATE,            0,                eat_date          },
  { 'D', MUTT_DELETED,             0,                NULL              },
  { 'e', MUTT_PAT_SENDER,          0,                eat_regex         },
  { 'E', MUTT_EXPIRED,             0,                NULL              },
  { 'f', MUTT_PAT_FROM,            0,                eat_regex         },
  { 'F', MUTT_FLAG,                0,                NULL              },
  { 'g', MUTT_PAT_CRYPT_SIGN,      0,                NULL              },
  { 'G', MUTT_PAT_CRYPT_ENCRYPT,   0,                NULL              },
  { 'h', MUTT_PAT_HEADER,          MUTT_PC_FULL_MSG|MUTT_PC_SEND_MODE_SEARCH, eat_regex },
  { 'H', MUTT_PAT_HORMEL,          0,                eat_regex         },
  { 'i', MUTT_PAT_ID,              0,                eat_regex         },
  { 'I', MUTT_PAT_ID_EXTERNAL,     0,                eat_query         },
  { 'k', MUTT_PAT_PGP_KEY,         0,                NULL              },
  { 'l', MUTT_PAT_LIST,            0,                NULL              },
  { 'L', MUTT_PAT_ADDRESS,         0,                eat_regex         },
  { 'm', MUTT_PAT_MESSAGE,         0,                eat_message_range },
  { 'M', MUTT_PAT_MIMETYPE,        MUTT_PC_FULL_MSG, eat_regex         },
  { 'n', MUTT_PAT_SCORE,           0,                eat_range         },
  { 'N', MUTT_NEW,                 0,                NULL              },
  { 'O', MUTT_OLD,                 0,                NULL              },
  { 'p', MUTT_PAT_PERSONAL_RECIP,  0,                NULL              },
  { 'P', MUTT_PAT_PERSONAL_FROM,   0,                NULL              },
  { 'Q', MUTT_REPLIED,             0,                NULL              },
  { 'r', MUTT_PAT_DATE_RECEIVED,   0,                eat_date          },
  { 'R', MUTT_READ,                0,                NULL              },
  { 's', MUTT_PAT_SUBJECT,         0,                eat_regex         },
  { 'S', MUTT_SUPERSEDED,          0,                NULL              },
  { 't', MUTT_PAT_TO,              0,                eat_regex         },
  { 'T', MUTT_TAG,                 0,                NULL              },
  { 'u', MUTT_PAT_SUBSCRIBED_LIST, 0,                NULL              },
  { 'U', MUTT_UNREAD,              0,                NULL              },
  { 'v', MUTT_PAT_COLLAPSED,       0,                NULL              },
  { 'V', MUTT_PAT_CRYPT_VERIFIED,  0,                NULL              },
#ifdef USE_NNTP
  { 'w', MUTT_PAT_NEWSGROUPS,      0,                eat_regex         },
#endif
  { 'x', MUTT_PAT_REFERENCE,       0,                eat_regex         },
  { 'X', MUTT_PAT_MIMEATTACH,      0,                eat_range         },
  { 'y', MUTT_PAT_XLABEL,          0,                eat_regex         },
  { 'Y', MUTT_PAT_DRIVER_TAGS,     0,                eat_regex         },
  { 'z', MUTT_PAT_SIZE,            0,                eat_range         },
  { '=', MUTT_PAT_DUPLICATED,      0,                NULL              },
  { '$', MUTT_PAT_UNREFERENCED,    0,                NULL              },
  { '#', MUTT_PAT_BROKEN,          0,                NULL              },
  { '/', MUTT_PAT_SERVERSEARCH,    0,                eat_regex         },
  { 0,   0,                        0,                NULL              },
};
// clang-format on

/**
 * lookup_tag - Lookup a pattern modifier
 * @param tag Letter, e.g. 'b' for pattern '~b'
 * @retval ptr Pattern data
 */
static const struct PatternFlags *lookup_tag(char tag)
{
  for (int i = 0; Flags[i].tag; i++)
    if (Flags[i].tag == tag)
      return &Flags[i];
  return NULL;
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
          if (ps.dptr[0] == '\0')
          {
            mutt_buffer_printf(err, "%s", _("missing parameter"));
            goto cleanup;
          }
          if (!entry->eat_arg(pat, flags, &ps, err))
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

/**
 * perform_and - Perform a logical AND on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param e   Email
 * @param cache Cached Patterns
 * @retval true If ALL of the Patterns evaluates to true
 */
static bool perform_and(struct PatternList *pat, PatternExecFlags flags,
                        struct Mailbox *m, struct Email *e, struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (mutt_pattern_exec(p, flags, m, e, cache) <= 0)
      return false;
  }
  return true;
}

/**
 * perform_or - Perform a logical OR on a set of Patterns
 * @param pat   Patterns to test
 * @param flags Optional flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param e   Email
 * @param cache Cached Patterns
 * @retval true If ONE (or more) of the Patterns evaluates to true
 */
static int perform_or(struct PatternList *pat, PatternExecFlags flags,
                      struct Mailbox *m, struct Email *e, struct PatternCache *cache)
{
  struct Pattern *p = NULL;

  SLIST_FOREACH(p, pat, entries)
  {
    if (mutt_pattern_exec(p, flags, m, e, cache) > 0)
      return true;
  }
  return false;
}

/**
 * match_addrlist - Match a Pattern against and Address list
 * @param pat            Pattern to find
 * @param match_personal If true, also match the pattern against the real name
 * @param n              Number of Addresses supplied
 * @param ...            Variable number of Addresses
 * @retval true
 * - One Address matches (all_addr is false)
 * - All the Addresses match (all_addr is true)
 */
static int match_addrlist(struct Pattern *pat, bool match_personal, int n, ...)
{
  va_list ap;

  va_start(ap, n);
  for (; n; n--)
  {
    struct AddressList *al = va_arg(ap, struct AddressList *);
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (pat->all_addr ^ ((!pat->is_alias || alias_reverse_lookup(a)) &&
                           ((a->mailbox && patmatch(pat, a->mailbox)) ||
                            (match_personal && a->personal && patmatch(pat, a->personal)))))
      {
        va_end(ap);
        return !pat->all_addr; /* Found match, or non-match if all_addr */
      }
    }
  }
  va_end(ap);
  return pat->all_addr; /* No matches, or all matches if all_addr */
}

/**
 * match_reference - Match references against a Pattern
 * @param pat  Pattern to match
 * @param refs List of References
 * @retval true One of the references matches
 */
static bool match_reference(struct Pattern *pat, struct ListHead *refs)
{
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, refs, entries)
  {
    if (patmatch(pat, np->data))
      return true;
  }
  return false;
}

/**
 * mutt_is_predicate_recipient - Test an Envelopes Addresses using a predicate function
 * @param all_addr If true, ALL Addresses must match
 * @param e       Envelope
 * @param p       Predicate function, e.g. mutt_is_subscribed_list()
 * @retval true
 * - One Address matches (all_addr is false)
 * - All the Addresses match (all_addr is true)
 *
 * Test the 'To' and 'Cc' fields of an Address using a test function (the predicate).
 */
static int mutt_is_predicate_recipient(bool all_addr, struct Envelope *e, addr_predicate_t p)
{
  struct AddressList *als[] = { &e->to, &e->cc };
  for (size_t i = 0; i < mutt_array_size(als); ++i)
  {
    struct AddressList *al = als[i];
    struct Address *a = NULL;
    TAILQ_FOREACH(a, al, entries)
    {
      if (all_addr ^ p(a))
        return !all_addr;
    }
  }
  return all_addr;
}

/**
 * mutt_is_subscribed_list_recipient - Matches subscribed mailing lists
 * @param all_addr If true, ALL Addresses must be on the subscribed list
 * @param e       Envelope
 * @retval true
 * - One Address is subscribed (all_addr is false)
 * - All the Addresses are subscribed (all_addr is true)
 */
int mutt_is_subscribed_list_recipient(bool all_addr, struct Envelope *e)
{
  return mutt_is_predicate_recipient(all_addr, e, &mutt_is_subscribed_list);
}

/**
 * mutt_is_list_recipient - Matches known mailing lists
 * @param all_addr If true, ALL Addresses must be mailing lists
 * @param e       Envelope
 * @retval true
 * - One Address is a mailing list (all_addr is false)
 * - All the Addresses are mailing lists (all_addr is true)
 */
int mutt_is_list_recipient(bool all_addr, struct Envelope *e)
{
  return mutt_is_predicate_recipient(all_addr, e, &mutt_is_mail_list);
}

/**
 * match_user - Matches the user's email Address
 * @param all_addr If true, ALL Addresses must refer to the user
 * @param al1     First AddressList
 * @param al2     Second AddressList
 * @retval true
 * - One Address refers to the user (all_addr is false)
 * - All the Addresses refer to the user (all_addr is true)
 */
static int match_user(int all_addr, struct AddressList *al1, struct AddressList *al2)
{
  struct Address *a = NULL;
  if (al1)
  {
    TAILQ_FOREACH(a, al1, entries)
    {
      if (all_addr ^ mutt_addr_is_user(a))
        return !all_addr;
    }
  }

  if (al2)
  {
    TAILQ_FOREACH(a, al2, entries)
    {
      if (all_addr ^ mutt_addr_is_user(a))
        return !all_addr;
    }
  }
  return all_addr;
}

/**
 * match_threadcomplete - Match a Pattern against an email thread
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Email thread
 * @param left  Navigate to the previous email
 * @param up    Navigate to the email's parent
 * @param right Navigate to the next email
 * @param down  Navigate to the email's children
 * @retval 1  Success, match found
 * @retval 0  No match
 */
static int match_threadcomplete(struct PatternList *pat, PatternExecFlags flags,
                                struct Mailbox *m, struct MuttThread *t,
                                int left, int up, int right, int down)
{
  if (!t)
    return 0;

  int a;
  struct Email *e = t->message;
  if (e)
    if (mutt_pattern_exec(SLIST_FIRST(pat), flags, m, e, NULL))
      return 1;

  if (up && (a = match_threadcomplete(pat, flags, m, t->parent, 1, 1, 1, 0)))
    return a;
  if (right && t->parent && (a = match_threadcomplete(pat, flags, m, t->next, 0, 0, 1, 1)))
  {
    return a;
  }
  if (left && t->parent && (a = match_threadcomplete(pat, flags, m, t->prev, 1, 0, 0, 1)))
  {
    return a;
  }
  if (down && (a = match_threadcomplete(pat, flags, m, t->child, 1, 0, 1, 1)))
    return a;
  return 0;
}

/**
 * match_threadparent - Match Pattern against an email's parent
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Thread of email
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int match_threadparent(struct PatternList *pat, PatternExecFlags flags,
                              struct Mailbox *m, struct MuttThread *t)
{
  if (!t || !t->parent || !t->parent->message)
    return 0;

  return mutt_pattern_exec(SLIST_FIRST(pat), flags, m, t->parent->message, NULL);
}

/**
 * match_threadchildren - Match Pattern against an email's children
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param t     Thread of email
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int match_threadchildren(struct PatternList *pat, PatternExecFlags flags,
                                struct Mailbox *m, struct MuttThread *t)
{
  if (!t || !t->child)
    return 0;

  for (t = t->child; t; t = t->next)
    if (t->message && mutt_pattern_exec(SLIST_FIRST(pat), flags, m, t->message, NULL))
      return 1;

  return 0;
}

/**
 * match_content_type - Match a Pattern against an Attachment's Content-Type
 * @param pat   Pattern to match
 * @param b     Attachment
 * @retval true  Success, pattern matched
 * @retval false Pattern did not match
 */
static bool match_content_type(const struct Pattern *pat, struct Body *b)
{
  if (!b)
    return false;

  char buf[256];
  snprintf(buf, sizeof(buf), "%s/%s", TYPE(b), b->subtype);

  if (patmatch(pat, buf))
    return true;
  if (match_content_type(pat, b->parts))
    return true;
  if (match_content_type(pat, b->next))
    return true;
  return false;
}

/**
 * match_update_dynamic_date - Update a dynamic date pattern
 * @param pat Pattern to modify
 * @retval true  Pattern valid and updated
 * @retval false Pattern invalid
 */
static bool match_update_dynamic_date(struct Pattern *pat)
{
  struct Buffer *err = mutt_buffer_pool_get();

  bool rc = eval_date_minmax(pat, pat->p.str, err);
  mutt_buffer_pool_release(&err);

  return rc;
}

/**
 * match_mime_content_type - Match a Pattern against an email's Content-Type
 * @param pat   Pattern to match
 * @param m   Mailbox
 * @param e   Email
 * @retval true  Success, pattern matched
 * @retval false Pattern did not match
 */
static bool match_mime_content_type(const struct Pattern *pat,
                                    struct Mailbox *m, struct Email *e)
{
  mutt_parse_mime_message(m, e);
  return match_content_type(pat, e->content);
}

/**
 * set_pattern_cache_value - Sets a value in the PatternCache cache entry
 * @param cache_entry Cache entry to update
 * @param value       Value to set
 *
 * Normalizes the "true" value to 2.
 */
static void set_pattern_cache_value(int *cache_entry, int value)
{
  *cache_entry = (value != 0) ? 2 : 1;
}

/**
 * get_pattern_cache_value - Get pattern cache value
 * @param cache_entry Cache entry to get
 * @retval 1 if the cache value is set and has a true value
 * @retval 0 otherwise (even if unset!)
 */
static int get_pattern_cache_value(int cache_entry)
{
  return cache_entry == 2;
}

/**
 * is_pattern_cache_set - Is a given Pattern cached?
 * @param cache_entry Cache entry to check
 * @retval true If it is cached
 */
static int is_pattern_cache_set(int cache_entry)
{
  return cache_entry != 0;
}

/**
 * msg_search_sendmode - Search in send-mode
 * @param e   Email to search
 * @param pat Pattern to find
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 */
static int msg_search_sendmode(struct Email *e, struct Pattern *pat)
{
  bool match = false;
  char *buf = NULL;
  size_t blen = 0;
  FILE *fp = NULL;

  if ((pat->op == MUTT_PAT_HEADER) || (pat->op == MUTT_PAT_WHOLE_MSG))
  {
    struct Buffer *tempfile = mutt_buffer_pool_get();
    mutt_buffer_mktemp(tempfile);
    fp = mutt_file_fopen(mutt_b2s(tempfile), "w+");
    if (!fp)
    {
      mutt_perror(mutt_b2s(tempfile));
      mutt_buffer_pool_release(&tempfile);
      return 0;
    }

    mutt_rfc822_write_header(fp, e->env, e->content, MUTT_WRITE_HEADER_POSTPONE,
                             false, false, NeoMutt->sub);
    fflush(fp);
    fseek(fp, 0, 0);

    while ((buf = mutt_file_read_line(buf, &blen, fp, NULL, 0)) != NULL)
    {
      if (patmatch(pat, buf) == 0)
      {
        match = true;
        break;
      }
    }

    FREE(&buf);
    mutt_file_fclose(&fp);
    unlink(mutt_b2s(tempfile));
    mutt_buffer_pool_release(&tempfile);

    if (match)
      return match;
  }

  if ((pat->op == MUTT_PAT_BODY) || (pat->op == MUTT_PAT_WHOLE_MSG))
  {
    fp = mutt_file_fopen(e->content->filename, "r");
    if (!fp)
    {
      mutt_perror(e->content->filename);
      return 0;
    }

    while ((buf = mutt_file_read_line(buf, &blen, fp, NULL, 0)) != NULL)
    {
      if (patmatch(pat, buf) == 0)
      {
        match = true;
        break;
      }
    }

    FREE(&buf);
    mutt_file_fclose(&fp);
  }

  return match;
}

/**
 * mutt_pattern_exec - Match a pattern against an email header
 * @param pat   Pattern to match
 * @param flags Flags, e.g. #MUTT_MATCH_FULL_ADDRESS
 * @param m   Mailbox
 * @param e     Email
 * @param cache Cache for common Patterns
 * @retval  1 Success, pattern matched
 * @retval  0 Pattern did not match
 * @retval -1 Error
 *
 * flags: MUTT_MATCH_FULL_ADDRESS - match both personal and machine address
 * cache: For repeated matches against the same Header, passing in non-NULL will
 *        store some of the cacheable pattern matches in this structure.
 */
int mutt_pattern_exec(struct Pattern *pat, PatternExecFlags flags,
                      struct Mailbox *m, struct Email *e, struct PatternCache *cache)
{
  switch (pat->op)
  {
    case MUTT_PAT_AND:
      return pat->pat_not ^ (perform_and(pat->child, flags, m, e, cache) > 0);
    case MUTT_PAT_OR:
      return pat->pat_not ^ (perform_or(pat->child, flags, m, e, cache) > 0);
    case MUTT_PAT_THREAD:
      return pat->pat_not ^
             match_threadcomplete(pat->child, flags, m, e->thread, 1, 1, 1, 1);
    case MUTT_PAT_PARENT:
      return pat->pat_not ^ match_threadparent(pat->child, flags, m, e->thread);
    case MUTT_PAT_CHILDREN:
      return pat->pat_not ^ match_threadchildren(pat->child, flags, m, e->thread);
    case MUTT_ALL:
      return !pat->pat_not;
    case MUTT_EXPIRED:
      return pat->pat_not ^ e->expired;
    case MUTT_SUPERSEDED:
      return pat->pat_not ^ e->superseded;
    case MUTT_FLAG:
      return pat->pat_not ^ e->flagged;
    case MUTT_TAG:
      return pat->pat_not ^ e->tagged;
    case MUTT_NEW:
      return pat->pat_not ? e->old || e->read : !(e->old || e->read);
    case MUTT_UNREAD:
      return pat->pat_not ? e->read : !e->read;
    case MUTT_REPLIED:
      return pat->pat_not ^ e->replied;
    case MUTT_OLD:
      return pat->pat_not ? (!e->old || e->read) : (e->old && !e->read);
    case MUTT_READ:
      return pat->pat_not ^ e->read;
    case MUTT_DELETED:
      return pat->pat_not ^ e->deleted;
    case MUTT_PAT_MESSAGE:
      return pat->pat_not ^ ((EMSG(e) >= pat->min) && (EMSG(e) <= pat->max));
    case MUTT_PAT_DATE:
      if (pat->dynamic)
        match_update_dynamic_date(pat);
      return pat->pat_not ^ (e->date_sent >= pat->min && e->date_sent <= pat->max);
    case MUTT_PAT_DATE_RECEIVED:
      if (pat->dynamic)
        match_update_dynamic_date(pat);
      return pat->pat_not ^ (e->received >= pat->min && e->received <= pat->max);
    case MUTT_PAT_BODY:
    case MUTT_PAT_HEADER:
    case MUTT_PAT_WHOLE_MSG:
      if (pat->sendmode)
      {
        if (!e->content || !e->content->filename)
          return 0;
        return pat->pat_not ^ msg_search_sendmode(e, pat);
      }
      /* m can be NULL in certain cases, such as when replying to a message
       * from the attachment menu and the user has a reply-hook using "~e".
       * This is also the case when message scoring.  */
      if (!m)
        return 0;
#ifdef USE_IMAP
      /* IMAP search sets e->matched at search compile time */
      if ((m->type == MUTT_IMAP) && pat->string_match)
        return e->matched;
#endif
      return pat->pat_not ^ msg_search(m, pat, e->msgno);
    case MUTT_PAT_SERVERSEARCH:
#ifdef USE_IMAP
      if (!m)
        return 0;
      if (m->type == MUTT_IMAP)
      {
        if (pat->string_match)
          return e->matched;
        return 0;
      }
      mutt_error(_("error: server custom search only supported with IMAP"));
      return 0;
#else
      mutt_error(_("error: server custom search only supported with IMAP"));
      return -1;
#endif
    case MUTT_PAT_SENDER:
      if (!e->env)
        return 0;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           1, &e->env->sender);
    case MUTT_PAT_FROM:
      if (!e->env)
        return 0;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->from);
    case MUTT_PAT_TO:
      if (!e->env)
        return 0;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->to);
    case MUTT_PAT_CC:
      if (!e->env)
        return 0;
      return pat->pat_not ^
             match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS), 1, &e->env->cc);
    case MUTT_PAT_SUBJECT:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (e->env->subject && patmatch(pat, e->env->subject));
    case MUTT_PAT_ID:
    case MUTT_PAT_ID_EXTERNAL:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (e->env->message_id && patmatch(pat, e->env->message_id));
    case MUTT_PAT_SCORE:
      return pat->pat_not ^ (e->score >= pat->min &&
                             (pat->max == MUTT_MAXRANGE || e->score <= pat->max));
    case MUTT_PAT_SIZE:
      return pat->pat_not ^
             (e->content->length >= pat->min &&
              (pat->max == MUTT_MAXRANGE || e->content->length <= pat->max));
    case MUTT_PAT_REFERENCE:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (match_reference(pat, &e->env->references) ||
                             match_reference(pat, &e->env->in_reply_to));
    case MUTT_PAT_ADDRESS:
      if (!e->env)
        return 0;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           4, &e->env->from, &e->env->sender,
                                           &e->env->to, &e->env->cc);
    case MUTT_PAT_RECIPIENT:
      if (!e->env)
        return 0;
      return pat->pat_not ^ match_addrlist(pat, (flags & MUTT_MATCH_FULL_ADDRESS),
                                           2, &e->env->to, &e->env->cc);
    case MUTT_PAT_LIST: /* known list, subscribed or not */
    {
      if (!e->env)
        return 0;

      int result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->list_all : &cache->list_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  mutt_is_list_recipient(pat->all_addr, e->env));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = mutt_is_list_recipient(pat->all_addr, e->env);
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_SUBSCRIBED_LIST:
    {
      if (!e->env)
        return 0;

      int result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->sub_all : &cache->sub_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(
              cache_entry, mutt_is_subscribed_list_recipient(pat->all_addr, e->env));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = mutt_is_subscribed_list_recipient(pat->all_addr, e->env);
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_PERSONAL_RECIP:
    {
      if (!e->env)
        return 0;

      int result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->pers_recip_all : &cache->pers_recip_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(
              cache_entry, match_user(pat->all_addr, &e->env->to, &e->env->cc));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = match_user(pat->all_addr, &e->env->to, &e->env->cc);
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_PERSONAL_FROM:
    {
      if (!e->env)
        return 0;

      int result;
      if (cache)
      {
        int *cache_entry = pat->all_addr ? &cache->pers_from_all : &cache->pers_from_one;
        if (!is_pattern_cache_set(*cache_entry))
        {
          set_pattern_cache_value(cache_entry,
                                  match_user(pat->all_addr, &e->env->from, NULL));
        }
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = match_user(pat->all_addr, &e->env->from, NULL);
      return pat->pat_not ^ result;
    }
    case MUTT_PAT_COLLAPSED:
      return pat->pat_not ^ (e->collapsed && e->num_hidden > 1);
    case MUTT_PAT_CRYPT_SIGN:
      if (!WithCrypto)
        break;
      return pat->pat_not ^ ((e->security & SEC_SIGN) ? 1 : 0);
    case MUTT_PAT_CRYPT_VERIFIED:
      if (!WithCrypto)
        break;
      return pat->pat_not ^ ((e->security & SEC_GOODSIGN) ? 1 : 0);
    case MUTT_PAT_CRYPT_ENCRYPT:
      if (!WithCrypto)
        break;
      return pat->pat_not ^ ((e->security & SEC_ENCRYPT) ? 1 : 0);
    case MUTT_PAT_PGP_KEY:
      if (!(WithCrypto & APPLICATION_PGP))
        break;
      return pat->pat_not ^ ((e->security & PGP_KEY) == PGP_KEY);
    case MUTT_PAT_XLABEL:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (e->env->x_label && patmatch(pat, e->env->x_label));
    case MUTT_PAT_DRIVER_TAGS:
    {
      char *tags = driver_tags_get(&e->tags);
      bool rc = (pat->pat_not ^ (tags && patmatch(pat, tags)));
      FREE(&tags);
      return rc;
    }
    case MUTT_PAT_HORMEL:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (e->env->spam.data && patmatch(pat, e->env->spam.data));
    case MUTT_PAT_DUPLICATED:
      return pat->pat_not ^ (e->thread && e->thread->duplicate_thread);
    case MUTT_PAT_MIMEATTACH:
      if (!m)
        return 0;
      {
        int count = mutt_count_body_parts(m, e);
        return pat->pat_not ^ (count >= pat->min &&
                               (pat->max == MUTT_MAXRANGE || count <= pat->max));
      }
    case MUTT_PAT_MIMETYPE:
      if (!m)
        return 0;
      return pat->pat_not ^ match_mime_content_type(pat, m, e);
    case MUTT_PAT_UNREFERENCED:
      return pat->pat_not ^ (e->thread && !e->thread->child);
    case MUTT_PAT_BROKEN:
      return pat->pat_not ^ (e->thread && e->thread->fake_thread);
#ifdef USE_NNTP
    case MUTT_PAT_NEWSGROUPS:
      if (!e->env)
        return 0;
      return pat->pat_not ^ (e->env->newsgroups && patmatch(pat, e->env->newsgroups));
#endif
  }
  mutt_error(_("error: unknown op %d (report this error)"), pat->op);
  return -1;
}

/**
 * quote_simple - Apply simple quoting to a string
 * @param str    String to quote
 * @param buf    Buffer for the result
 */
static void quote_simple(const char *str, struct Buffer *buf)
{
  mutt_buffer_reset(buf);
  mutt_buffer_addch(buf, '"');
  while (*str)
  {
    if ((*str == '\\') || (*str == '"'))
      mutt_buffer_addch(buf, '\\');
    mutt_buffer_addch(buf, *str++);
  }
  mutt_buffer_addch(buf, '"');
}

/**
 * mutt_check_simple - Convert a simple search into a real request
 * @param buf    Buffer for the result
 * @param simple Search string to convert
 */
void mutt_check_simple(struct Buffer *buf, const char *simple)
{
  bool do_simple = true;

  for (const char *p = mutt_b2s(buf); p && (p[0] != '\0'); p++)
  {
    if ((p[0] == '\\') && (p[1] != '\0'))
      p++;
    else if ((p[0] == '~') || (p[0] == '=') || (p[0] == '%'))
    {
      do_simple = false;
      break;
    }
  }

  /* XXX - is mutt_istr_cmp() right here, or should we use locale's
   * equivalences?  */

  if (do_simple) /* yup, so spoof a real request */
  {
    /* convert old tokens into the new format */
    if (mutt_istr_equal("all", mutt_b2s(buf)) || mutt_str_equal("^", mutt_b2s(buf)) ||
        mutt_str_equal(".", mutt_b2s(buf))) /* ~A is more efficient */
    {
      mutt_buffer_strcpy(buf, "~A");
    }
    else if (mutt_istr_equal("del", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~D");
    else if (mutt_istr_equal("flag", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~F");
    else if (mutt_istr_equal("new", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~N");
    else if (mutt_istr_equal("old", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~O");
    else if (mutt_istr_equal("repl", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~Q");
    else if (mutt_istr_equal("read", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~R");
    else if (mutt_istr_equal("tag", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~T");
    else if (mutt_istr_equal("unread", mutt_b2s(buf)))
      mutt_buffer_strcpy(buf, "~U");
    else
    {
      struct Buffer *tmp = mutt_buffer_pool_get();
      quote_simple(mutt_b2s(buf), tmp);
      mutt_file_expand_fmt(buf, simple, mutt_b2s(tmp));
      mutt_buffer_pool_release(&tmp);
    }
  }
}

/**
 * top_of_thread - Find the first email in the current thread
 * @param e Current Email
 * @retval ptr  Success, email found
 * @retval NULL Error
 */
static struct MuttThread *top_of_thread(struct Email *e)
{
  if (!e)
    return NULL;

  struct MuttThread *t = e->thread;

  while (t && t->parent)
    t = t->parent;

  return t;
}

/**
 * mutt_limit_current_thread - Limit the email view to the current thread
 * @param e Current Email
 * @retval true Success
 * @retval false Failure
 */
bool mutt_limit_current_thread(struct Email *e)
{
  if (!e || !Context || !Context->mailbox)
    return false;

  struct MuttThread *me = top_of_thread(e);
  if (!me)
    return false;

  struct Mailbox *m = Context->mailbox;

  m->vcount = 0;
  Context->vsize = 0;
  Context->collapsed = false;

  for (int i = 0; i < m->msg_count; i++)
  {
    e = m->emails[i];
    if (!e)
      break;

    e->vnum = -1;
    e->limited = false;
    e->collapsed = false;
    e->num_hidden = 0;

    if (top_of_thread(e) == me)
    {
      struct Body *body = e->content;

      e->vnum = m->vcount;
      e->limited = true;
      m->v2r[m->vcount] = i;
      m->vcount++;
      Context->vsize += (body->length + body->offset - body->hdr_offset);
    }
  }
  return true;
}

/**
 * mutt_pattern_func - Perform some Pattern matching
 * @param op     Operation to perform, e.g. #MUTT_LIMIT
 * @param prompt Prompt to show the user
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_pattern_func(int op, char *prompt)
{
  if (!Context || !Context->mailbox)
    return -1;

  struct Buffer err;
  int rc = -1;
  struct Progress progress;
  struct Buffer *buf = mutt_buffer_pool_get();
  struct Mailbox *m = Context->mailbox;

  mutt_buffer_strcpy(buf, NONULL(Context->pattern));
  if (prompt || (op != MUTT_LIMIT))
  {
    if ((mutt_buffer_get_field(prompt, buf, MUTT_PATTERN | MUTT_CLEAR) != 0) ||
        mutt_buffer_is_empty(buf))
    {
      mutt_buffer_pool_release(&buf);
      return -1;
    }
  }

  mutt_message(_("Compiling search pattern..."));

  char *simple = mutt_buffer_strdup(buf);
  mutt_check_simple(buf, NONULL(C_SimpleSearch));

  mutt_buffer_init(&err);
  err.dsize = 256;
  err.data = mutt_mem_malloc(err.dsize);
  struct PatternList *pat = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, &err);
  if (!pat)
  {
    mutt_error("%s", err.data);
    goto bail;
  }

#ifdef USE_IMAP
  if ((m->type == MUTT_IMAP) && (!imap_search(m, pat)))
    goto bail;
#endif

  mutt_progress_init(&progress, _("Executing command on matching messages..."),
                     MUTT_PROGRESS_READ, (op == MUTT_LIMIT) ? m->msg_count : m->vcount);

  if (op == MUTT_LIMIT)
  {
    m->vcount = 0;
    Context->vsize = 0;
    Context->collapsed = false;
    int padding = mx_msg_padding_size(m);

    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      mutt_progress_update(&progress, i, -1);
      /* new limit pattern implicitly uncollapses all threads */
      e->vnum = -1;
      e->limited = false;
      e->collapsed = false;
      e->num_hidden = 0;
      if (mutt_pattern_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, m, e, NULL))
      {
        e->vnum = m->vcount;
        e->limited = true;
        m->v2r[m->vcount] = i;
        m->vcount++;
        struct Body *b = e->content;
        Context->vsize += b->length + b->offset - b->hdr_offset + padding;
      }
    }
  }
  else
  {
    for (int i = 0; i < m->vcount; i++)
    {
      struct Email *e = mutt_get_virt_email(Context->mailbox, i);
      if (!e)
        continue;
      mutt_progress_update(&progress, i, -1);
      if (mutt_pattern_exec(SLIST_FIRST(pat), MUTT_MATCH_FULL_ADDRESS, m, e, NULL))
      {
        switch (op)
        {
          case MUTT_UNDELETE:
            mutt_set_flag(m, e, MUTT_PURGE, false);
          /* fallthrough */
          case MUTT_DELETE:
            mutt_set_flag(m, e, MUTT_DELETE, (op == MUTT_DELETE));
            break;
          case MUTT_TAG:
          case MUTT_UNTAG:
            mutt_set_flag(m, e, MUTT_TAG, (op == MUTT_TAG));
            break;
        }
      }
    }
  }

  mutt_clear_error();

  if (op == MUTT_LIMIT)
  {
    /* drop previous limit pattern */
    FREE(&Context->pattern);
    mutt_pattern_free(&Context->limit_pattern);

    if (m->msg_count && !m->vcount)
      mutt_error(_("No messages matched criteria"));

    /* record new limit pattern, unless match all */
    const char *pbuf = buf->data;
    while (*pbuf == ' ')
      pbuf++;
    if (!mutt_str_equal(pbuf, "~A"))
    {
      Context->pattern = simple;
      simple = NULL; /* don't clobber it */
      Context->limit_pattern = mutt_pattern_comp(buf->data, MUTT_PC_FULL_MSG, &err);
    }
  }

  rc = 0;

bail:
  mutt_buffer_pool_release(&buf);
  FREE(&simple);
  mutt_pattern_free(&pat);
  FREE(&err.data);

  return rc;
}

/**
 * mutt_search_command - Perform a search
 * @param cur Index number of current email
 * @param op  Operation to perform, e.g. OP_SEARCH_NEXT
 * @retval >= 0 Index of matching email
 * @retval -1 No match, or error
 */
int mutt_search_command(int cur, int op)
{
  struct Progress progress;

  if ((*LastSearch == '\0') || ((op != OP_SEARCH_NEXT) && (op != OP_SEARCH_OPPOSITE)))
  {
    char buf[256];
    mutt_str_copy(buf, (LastSearch[0] != '\0') ? LastSearch : "", sizeof(buf));
    if ((mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                            _("Search for: ") :
                            _("Reverse search for: "),
                        buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN) != 0) ||
        (buf[0] == '\0'))
    {
      return -1;
    }

    if ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT))
      OptSearchReverse = false;
    else
      OptSearchReverse = true;

    /* compare the *expanded* version of the search pattern in case
     * $simple_search has changed while we were searching */
    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_strcpy(tmp, buf);
    mutt_check_simple(tmp, NONULL(C_SimpleSearch));

    if (!SearchPattern || !mutt_str_equal(mutt_b2s(tmp), LastSearchExpn))
    {
      struct Buffer err;
      mutt_buffer_init(&err);
      OptSearchInvalid = true;
      mutt_str_copy(LastSearch, buf, sizeof(LastSearch));
      mutt_str_copy(LastSearchExpn, mutt_b2s(tmp), sizeof(LastSearchExpn));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = 256;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(tmp->data, MUTT_PC_FULL_MSG, &err);
      if (!SearchPattern)
      {
        mutt_buffer_pool_release(&tmp);
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        LastSearchExpn[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }

    mutt_buffer_pool_release(&tmp);
  }

  if (OptSearchInvalid)
  {
    for (int i = 0; i < Context->mailbox->msg_count; i++)
      Context->mailbox->emails[i]->searched = false;
#ifdef USE_IMAP
    if ((Context->mailbox->type == MUTT_IMAP) &&
        (!imap_search(Context->mailbox, SearchPattern)))
      return -1;
#endif
    OptSearchInvalid = false;
  }

  int incr = OptSearchReverse ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    incr = -incr;

  mutt_progress_init(&progress, _("Searching..."), MUTT_PROGRESS_READ,
                     Context->mailbox->vcount);

  for (int i = cur + incr, j = 0; j != Context->mailbox->vcount; j++)
  {
    const char *msg = NULL;
    mutt_progress_update(&progress, j, -1);
    if (i > Context->mailbox->vcount - 1)
    {
      i = 0;
      if (C_WrapSearch)
        msg = _("Search wrapped to top");
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        return -1;
      }
    }
    else if (i < 0)
    {
      i = Context->mailbox->vcount - 1;
      if (C_WrapSearch)
        msg = _("Search wrapped to bottom");
      else
      {
        mutt_message(_("Search hit top without finding match"));
        return -1;
      }
    }

    struct Email *e = mutt_get_virt_email(Context->mailbox, i);
    if (e->searched)
    {
      /* if we've already evaluated this message, use the cached value */
      if (e->matched)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }
    else
    {
      /* remember that we've already searched this message */
      e->searched = true;
      e->matched = mutt_pattern_exec(SLIST_FIRST(SearchPattern), MUTT_MATCH_FULL_ADDRESS,
                                     Context->mailbox, e, NULL);
      if (e->matched > 0)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted"));
      SigInt = 0;
      return -1;
    }

    i += incr;
  }

  mutt_error(_("Not found"));
  return -1;
}
