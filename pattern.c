/**
 * @file
 * Match patterns to emails
 *
 * @authors
 * Copyright (C) 1996-2000,2006-2007,2010 Michael R. Elkins <me@mutt.org>, and others
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

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "pattern.h"
#include "body.h"
#include "context.h"
#include "copy.h"
#include "envelope.h"
#include "globals.h"
#include "group.h"
#include "header.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "state.h"
#include "tags.h"
#include "thread.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

/**
 * enum EatRangeError - Error codes for eat_range_by_regex()
 */
enum EatRangeError
{
  RANGE_E_OK,
  RANGE_E_SYNTAX,
  RANGE_E_CTX,
};

static bool eat_regex(struct Pattern *pat, struct Buffer *s, struct Buffer *err)
{
  struct Buffer buf;
  char errmsg[STRING];
  int r;
  char *pexpr = NULL;

  mutt_buffer_init(&buf);
  pexpr = s->dptr;
  if (mutt_extract_token(&buf, s, MUTT_TOKEN_PATTERN | MUTT_TOKEN_COMMENT) != 0 || !buf.data)
  {
    snprintf(err->data, err->dsize, _("Error in expression: %s"), pexpr);
    return false;
  }
  if (!*buf.data)
  {
    snprintf(err->data, err->dsize, "%s", _("Empty expression"));
    return false;
  }

  if (pat->stringmatch)
  {
    pat->p.str = mutt_str_strdup(buf.data);
    pat->ign_case = mutt_mb_is_lower(buf.data);
    FREE(&buf.data);
  }
  else if (pat->groupmatch)
  {
    pat->p.g = mutt_pattern_group(buf.data);
    FREE(&buf.data);
  }
  else
  {
    pat->p.regex = mutt_mem_malloc(sizeof(regex_t));
    int flags = mutt_mb_is_lower(buf.data) ? REG_ICASE : 0;
    r = REGCOMP(pat->p.regex, buf.data, REG_NEWLINE | REG_NOSUB | flags);
    if (r != 0)
    {
      regerror(r, pat->p.regex, errmsg, sizeof(errmsg));
      mutt_buffer_printf(err, "'%s': %s", buf.data, errmsg);
      FREE(&buf.data);
      FREE(&pat->p.regex);
      return false;
    }
    FREE(&buf.data);
  }

  return true;
}

/**
 * get_offset - Calculate a symbolic offset
 *
 * Ny   years
 * Nm   months
 * Nw   weeks
 * Nd   days
 */
static const char *get_offset(struct tm *tm, const char *s, int sign)
{
  char *ps = NULL;
  int offset = strtol(s, &ps, 0);
  if ((sign < 0 && offset > 0) || (sign > 0 && offset < 0))
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
    default:
      return s;
  }
  mutt_date_normalize_time(tm);
  return (ps + 1);
}

static const char *get_date(const char *s, struct tm *t, struct Buffer *err)
{
  char *p = NULL;
  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  t->tm_mday = strtol(s, &p, 10);
  if (t->tm_mday < 1 || t->tm_mday > 31)
  {
    snprintf(err->data, err->dsize, _("Invalid day of month: %s"), s);
    return NULL;
  }
  if (*p != '/')
  {
    /* fill in today's month and year */
    t->tm_mon = tm->tm_mon;
    t->tm_year = tm->tm_year;
    return p;
  }
  p++;
  t->tm_mon = strtol(p, &p, 10) - 1;
  if (t->tm_mon < 0 || t->tm_mon > 11)
  {
    snprintf(err->data, err->dsize, _("Invalid month: %s"), p);
    return NULL;
  }
  if (*p != '/')
  {
    t->tm_year = tm->tm_year;
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

/* The regexes in a modern format */
#define RANGE_NUM_RX "([[:digit:]]+|0x[[:xdigit:]]+)[MmKk]?"

#define RANGE_REL_SLOT_RX "[[:blank:]]*([.^$]|-?" RANGE_NUM_RX ")?[[:blank:]]*"

#define RANGE_REL_RX "^" RANGE_REL_SLOT_RX "," RANGE_REL_SLOT_RX

/* Almost the same, but no negative numbers allowed */
#define RANGE_ABS_SLOT_RX "[[:blank:]]*([.^$]|" RANGE_NUM_RX ")?[[:blank:]]*"

#define RANGE_ABS_RX "^" RANGE_ABS_SLOT_RX "-" RANGE_ABS_SLOT_RX

/* First group is intentionally empty */
#define RANGE_LT_RX "^()[[:blank:]]*(<[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"

#define RANGE_GT_RX "^()[[:blank:]]*(>[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"

/* Single group for min and max */
#define RANGE_BARE_RX "^[[:blank:]]*([.^$]|" RANGE_NUM_RX ")[[:blank:]]*"

#define RANGE_RX_GROUPS 5

/**
 * struct RangeRegex - Regular expression representing a range
 */
struct RangeRegex
{
  const char *raw; /**< regex as string */
  int lgrp;        /**< paren group matching the left side */
  int rgrp;        /**< paren group matching the right side */
  int ready;       /**< compiled yet? */
  regex_t cooked;  /**< compiled form */
};

/**
 * enum RangeType - Type of range
 */
enum RangeType
{
  RANGE_K_REL,
  RANGE_K_ABS,
  RANGE_K_LT,
  RANGE_K_GT,
  RANGE_K_BARE,
  /* add new ones HERE */
  RANGE_K_INVALID
};

static struct RangeRegex range_regexes[] = {
  [RANGE_K_REL] = { .raw = RANGE_REL_RX, .lgrp = 1, .rgrp = 3, .ready = 0 },
  [RANGE_K_ABS] = { .raw = RANGE_ABS_RX, .lgrp = 1, .rgrp = 3, .ready = 0 },
  [RANGE_K_LT] = { .raw = RANGE_LT_RX, .lgrp = 1, .rgrp = 2, .ready = 0 },
  [RANGE_K_GT] = { .raw = RANGE_GT_RX, .lgrp = 2, .rgrp = 1, .ready = 0 },
  [RANGE_K_BARE] = { .raw = RANGE_BARE_RX, .lgrp = 1, .rgrp = 1, .ready = 0 },
};

#define KILO 1024
#define MEGA 1048576
#define HMSG(h) (((h)->msgno) + 1)
#define CTX_MSGNO(c) (HMSG((c)->hdrs[(c)->v2r[(c)->menu->current]]))

#define MUTT_MAXRANGE -1

/* constants for parse_date_range() */
#define MUTT_PDR_NONE 0x0000
#define MUTT_PDR_MINUS 0x0001
#define MUTT_PDR_PLUS 0x0002
#define MUTT_PDR_WINDOW 0x0004
#define MUTT_PDR_ABSOLUTE 0x0008
#define MUTT_PDR_DONE 0x0010
#define MUTT_PDR_ERROR 0x0100
#define MUTT_PDR_ERRORDONE (MUTT_PDR_ERROR | MUTT_PDR_DONE)

#define RANGE_DOT '.'
#define RANGE_CIRCUM '^'
#define RANGE_DOLLAR '$'
#define RANGE_LT '<'
#define RANGE_GT '>'

/**
 * enum RangeSide - Which side of the range
 */
enum RangeSide
{
  RANGE_S_LEFT,
  RANGE_S_RIGHT
};

static const char *parse_date_range(const char *pc, struct tm *min, struct tm *max,
                                    int have_min, struct tm *base_min, struct Buffer *err)
{
  int flag = MUTT_PDR_NONE;
  while (*pc && ((flag & MUTT_PDR_DONE) == 0))
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
          if (flag == MUTT_PDR_NONE)
          { /* nothing yet and no offset parsed => absolute date? */
            if (!get_date(pc, max, err))
              flag |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_ERRORDONE); /* done bad */
            else
            {
              /* reestablish initial base minimum if not specified */
              if (!have_min)
                memcpy(min, base_min, sizeof(struct tm));
              flag |= (MUTT_PDR_ABSOLUTE | MUTT_PDR_DONE); /* done good */
            }
          }
          else
            flag |= MUTT_PDR_ERRORDONE;
        }
        else
        {
          pc = pt;
          if (flag == MUTT_PDR_NONE && !have_min)
          { /* the very first "-3d" without a previous absolute date */
            max->tm_year = min->tm_year;
            max->tm_mon = min->tm_mon;
            max->tm_mday = min->tm_mday;
          }
          flag |= MUTT_PDR_MINUS;
        }
      }
      break;
      case '+':
      { /* enlarge plus range */
        pt = get_offset(max, pc, 1);
        if (pc == pt)
          flag |= MUTT_PDR_ERRORDONE;
        else
        {
          pc = pt;
          flag |= MUTT_PDR_PLUS;
        }
      }
      break;
      case '*':
      { /* enlarge window in both directions */
        pt = get_offset(min, pc, -1);
        if (pc == pt)
          flag |= MUTT_PDR_ERRORDONE;
        else
        {
          pc = get_offset(max, pc, 1);
          flag |= MUTT_PDR_WINDOW;
        }
      }
      break;
      default:
        flag |= MUTT_PDR_ERRORDONE;
    }
    SKIPWS(pc);
  }
  if ((flag & MUTT_PDR_ERROR) && !(flag & MUTT_PDR_ABSOLUTE))
  { /* get_date has its own error message, don't overwrite it here */
    snprintf(err->data, err->dsize, _("Invalid relative date: %s"), pc - 1);
  }
  return ((flag & MUTT_PDR_ERROR) ? NULL : pc);
}

static void adjust_date_range(struct tm *min, struct tm *max)
{
  if (min->tm_year > max->tm_year ||
      (min->tm_year == max->tm_year && min->tm_mon > max->tm_mon) ||
      (min->tm_year == max->tm_year && min->tm_mon == max->tm_mon && min->tm_mday > max->tm_mday))
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

    min->tm_hour = min->tm_min = min->tm_sec = 0;
    max->tm_hour = 23;
    max->tm_min = max->tm_sec = 59;
  }
}

static bool eat_date(struct Pattern *pat, struct Buffer *s, struct Buffer *err)
{
  struct Buffer buffer;
  struct tm min, max;
  char *pexpr = NULL;

  mutt_buffer_init(&buffer);
  pexpr = s->dptr;
  if (mutt_extract_token(&buffer, s, MUTT_TOKEN_COMMENT | MUTT_TOKEN_PATTERN) != 0 ||
      !buffer.data)
  {
    snprintf(err->data, err->dsize, _("Error in expression: %s"), pexpr);
    return false;
  }
  if (!*buffer.data)
  {
    snprintf(err->data, err->dsize, "%s", _("Empty expression"));
    return false;
  }

  memset(&min, 0, sizeof(min));
  /* the `0' time is Jan 1, 1970 UTC, so in order to prevent a negative time
     when doing timezone conversion, we use Jan 2, 1970 UTC as the base
     here */
  min.tm_mday = 2;
  min.tm_year = 70;

  memset(&max, 0, sizeof(max));

  /* Arbitrary year in the future.  Don't set this too high
     or mutt_date_make_time() returns something larger than will
     fit in a time_t on some systems */
  max.tm_year = 130;
  max.tm_mon = 11;
  max.tm_mday = 31;
  max.tm_hour = 23;
  max.tm_min = 59;
  max.tm_sec = 59;

  if (strchr("<>=", buffer.data[0]))
  {
    /* offset from current time
       <3d      less than three days ago
       >3d      more than three days ago
       =3d      exactly three days ago */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int exact = 0;

    if (buffer.data[0] == '<')
    {
      memcpy(&min, tm, sizeof(min));
      tm = &min;
    }
    else
    {
      memcpy(&max, tm, sizeof(max));
      tm = &max;

      if (buffer.data[0] == '=')
        exact++;
    }
    tm->tm_hour = 23;
    tm->tm_min = tm->tm_sec = 59;

    /* force negative offset */
    get_offset(tm, buffer.data + 1, -1);

    if (exact)
    {
      /* start at the beginning of the day in question */
      memcpy(&min, &max, sizeof(max));
      min.tm_hour = min.tm_sec = min.tm_min = 0;
    }
  }
  else
  {
    const char *pc = buffer.data;

    int have_min = false;
    int until_now = false;
    if (isdigit((unsigned char) *pc))
    {
      /* minimum date specified */
      pc = get_date(pc, &min, err);
      if (!pc)
      {
        FREE(&buffer.data);
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
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        memcpy(&base_min, &min, sizeof(base_min));
        memcpy(&min, tm, sizeof(min));
        min.tm_hour = min.tm_sec = min.tm_min = 0;
      }

      /* preset max date for relative offsets,
         if nothing follows we search for messages on a specific day */
      max.tm_year = min.tm_year;
      max.tm_mon = min.tm_mon;
      max.tm_mday = min.tm_mday;

      if (!parse_date_range(pc, &min, &max, have_min, &base_min, err))
      { /* bail out on any parsing error */
        FREE(&buffer.data);
        return false;
      }
    }
  }

  /* Since we allow two dates to be specified we'll have to adjust that. */
  adjust_date_range(&min, &max);

  pat->min = mutt_date_make_time(&min, 1);
  pat->max = mutt_date_make_time(&max, 1);

  FREE(&buffer.data);

  return true;
}

static bool eat_range(struct Pattern *pat, struct Buffer *s, struct Buffer *err)
{
  char *tmp = NULL;
  bool do_exclusive = false;
  bool skip_quote = false;

  /*
   * If simple_search is set to "~m %s", the range will have double quotes
   * around it...
   */
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

  if (skip_quote && *tmp == '"')
    tmp++;

  SKIPWS(tmp);
  s->dptr = tmp;
  return true;
}

static int report_regerror(int regerr, regex_t *preg, struct Buffer *err)
{
  size_t ds = err->dsize;

  if (regerror(regerr, preg, err->data, ds) > ds)
    mutt_debug(2, "warning: buffer too small for regerror\n");
  /* The return value is fixed, exists only to shorten code at callsite */
  return RANGE_E_SYNTAX;
}

static bool is_context_available(struct Buffer *s, regmatch_t pmatch[],
                                 int kind, struct Buffer *err)
{
  char *context_loc = NULL;
  const char *context_req_chars[] = {
    [RANGE_K_REL] = ".0123456789",
    [RANGE_K_ABS] = ".",
    [RANGE_K_LT] = "",
    [RANGE_K_GT] = "",
    [RANGE_K_BARE] = ".",
  };

  /* First decide if we're going to need the context at all.
   * Relative patterns need it iff they contain a dot or a number.
   * Absolute patterns only need it if they contain a dot. */
  context_loc = strpbrk(s->dptr + pmatch[0].rm_so, context_req_chars[kind]);
  if (!context_loc || (context_loc >= &s->dptr[pmatch[0].rm_eo]))
    return true;

  /* We need a current message.  Do we actually have one? */
  if (Context && Context->menu)
    return true;

  /* Nope. */
  mutt_str_strfcpy(err->data, _("No current message"), err->dsize);
  return false;
}

static int scan_range_num(struct Buffer *s, regmatch_t pmatch[], int group, int kind)
{
  int num;
  unsigned char c;

  /* this cast looks dangerous, but is already all over this code
   * (explicit or not) */
  num = (int) strtol(&s->dptr[pmatch[group].rm_so], NULL, 0);
  c = (unsigned char) (s->dptr[pmatch[group].rm_eo - 1]);
  if (toupper(c) == 'K')
    num *= KILO;
  else if (toupper(c) == 'M')
    num *= MEGA;
  switch (kind)
  {
    case RANGE_K_REL:
      return num + CTX_MSGNO(Context);
    case RANGE_K_LT:
      return num - 1;
    case RANGE_K_GT:
      return num + 1;
    default:
      return num;
  }
}

static int scan_range_slot(struct Buffer *s, regmatch_t pmatch[], int grp, int side, int kind)
{
  unsigned char c;

  /* This means the left or right subpattern was empty, e.g. ",." */
  if ((pmatch[grp].rm_so == -1) || (pmatch[grp].rm_so == pmatch[grp].rm_eo))
  {
    if (side == RANGE_S_LEFT)
      return 1;
    else if (side == RANGE_S_RIGHT)
      return Context->msgcount;
  }
  /* We have something, so determine what */
  c = (unsigned char) (s->dptr[pmatch[grp].rm_so]);
  switch (c)
  {
    case RANGE_CIRCUM:
      return 1;
    case RANGE_DOLLAR:
      return Context->msgcount;
    case RANGE_DOT:
      return CTX_MSGNO(Context);
    case RANGE_LT:
    case RANGE_GT:
      return scan_range_num(s, pmatch, grp + 1, kind);
    default:
      /* Only other possibility: a number */
      return scan_range_num(s, pmatch, grp, kind);
  }
}

static void order_range(struct Pattern *pat)
{
  int num;

  if (pat->min <= pat->max)
    return;
  num = pat->min;
  pat->min = pat->max;
  pat->max = num;
}

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
    if (regerr)
      return report_regerror(regerr, &pspec->cooked, err);
    pspec->ready = 1;
  }

  /* Match the pattern buffer against the compiled regex.
   * No match means syntax error. */
  regerr = regexec(&pspec->cooked, s->dptr, RANGE_RX_GROUPS, pmatch, 0);
  if (regerr)
    return report_regerror(regerr, &pspec->cooked, err);

  if (!is_context_available(s, pmatch, kind, err))
    return RANGE_E_CTX;

  /* Snarf the contents of the two sides of the range. */
  pat->min = scan_range_slot(s, pmatch, pspec->lgrp, RANGE_S_LEFT, kind);
  pat->max = scan_range_slot(s, pmatch, pspec->rgrp, RANGE_S_RIGHT, kind);
  mutt_debug(1, "pat->min=%d pat->max=%d\n", pat->min, pat->max);

  /* Special case for a bare 0. */
  if ((kind == RANGE_K_BARE) && (pat->min == 0) && (pat->max == 0))
  {
    if (!Context->menu)
    {
      mutt_str_strfcpy(err->data, _("No current message"), err->dsize);
      return RANGE_E_CTX;
    }
    pat->min = pat->max = CTX_MSGNO(Context);
  }

  /* Since we don't enforce order, we must swap bounds if they're backward */
  order_range(pat);

  /* Slide pointer past the entire match. */
  s->dptr += pmatch[0].rm_eo;
  return RANGE_E_OK;
}

static bool eat_message_range(struct Pattern *pat, struct Buffer *s, struct Buffer *err)
{
  bool skip_quote = false;

  /* We need a Context for pretty much anything. */
  if (!Context)
  {
    mutt_str_strfcpy(err->data, _("No Context"), err->dsize);
    return false;
  }

  /*
   * If simple_search is set to "~m %s", the range will have double quotes
   * around it...
   */
  if (*s->dptr == '"')
  {
    s->dptr++;
    skip_quote = true;
  }

  for (int i_kind = 0; i_kind != RANGE_K_INVALID; ++i_kind)
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
 * struct PatternFlags - Mapping between user character and internal constant
 */
static const struct PatternFlags
{
  int tag; /**< character used to represent this op */
  int op;  /**< operation to perform */
  int class;
  bool (*eat_arg)(struct Pattern *, struct Buffer *, struct Buffer *);
} Flags[] = {
  { 'A', MUTT_ALL, 0, NULL },
  { 'b', MUTT_BODY, MUTT_FULL_MSG, eat_regex },
  { 'B', MUTT_WHOLE_MSG, MUTT_FULL_MSG, eat_regex },
  { 'c', MUTT_CC, 0, eat_regex },
  { 'C', MUTT_RECIPIENT, 0, eat_regex },
  { 'd', MUTT_DATE, 0, eat_date },
  { 'D', MUTT_DELETED, 0, NULL },
  { 'e', MUTT_SENDER, 0, eat_regex },
  { 'E', MUTT_EXPIRED, 0, NULL },
  { 'f', MUTT_FROM, 0, eat_regex },
  { 'F', MUTT_FLAG, 0, NULL },
  { 'g', MUTT_CRYPT_SIGN, 0, NULL },
  { 'G', MUTT_CRYPT_ENCRYPT, 0, NULL },
  { 'h', MUTT_HEADER, MUTT_FULL_MSG, eat_regex },
  { 'H', MUTT_HORMEL, 0, eat_regex },
  { 'i', MUTT_ID, 0, eat_regex },
  { 'k', MUTT_PGP_KEY, 0, NULL },
  { 'l', MUTT_LIST, 0, NULL },
  { 'L', MUTT_ADDRESS, 0, eat_regex },
  { 'm', MUTT_MESSAGE, 0, eat_message_range },
  { 'n', MUTT_SCORE, 0, eat_range },
  { 'N', MUTT_NEW, 0, NULL },
  { 'O', MUTT_OLD, 0, NULL },
  { 'p', MUTT_PERSONAL_RECIP, 0, NULL },
  { 'P', MUTT_PERSONAL_FROM, 0, NULL },
  { 'Q', MUTT_REPLIED, 0, NULL },
  { 'r', MUTT_DATE_RECEIVED, 0, eat_date },
  { 'R', MUTT_READ, 0, NULL },
  { 's', MUTT_SUBJECT, 0, eat_regex },
  { 'S', MUTT_SUPERSEDED, 0, NULL },
  { 't', MUTT_TO, 0, eat_regex },
  { 'T', MUTT_TAG, 0, NULL },
  { 'u', MUTT_SUBSCRIBED_LIST, 0, NULL },
  { 'U', MUTT_UNREAD, 0, NULL },
  { 'v', MUTT_COLLAPSED, 0, NULL },
  { 'V', MUTT_CRYPT_VERIFIED, 0, NULL },
#ifdef USE_NNTP
  { 'w', MUTT_NEWSGROUPS, 0, eat_regex },
#endif
  { 'x', MUTT_REFERENCE, 0, eat_regex },
  { 'X', MUTT_MIMEATTACH, 0, eat_range },
  { 'y', MUTT_XLABEL, 0, eat_regex },
  { 'Y', MUTT_DRIVER_TAGS, 0, eat_regex },
  { 'z', MUTT_SIZE, 0, eat_range },
  { '=', MUTT_DUPLICATED, 0, NULL },
  { '$', MUTT_UNREFERENCED, 0, NULL },
  { '#', MUTT_BROKEN, 0, NULL },
  { '/', MUTT_SERVERSEARCH, 0, eat_regex },
  { 0, 0, 0, NULL },
};

static struct Pattern *SearchPattern = NULL; /**< current search pattern */
static char LastSearch[STRING] = { 0 };      /**< last pattern searched for */
static char LastSearchExpn[LONG_STRING] = { 0 }; /**< expanded version of LastSearch */

static int patmatch(const struct Pattern *pat, const char *buf)
{
  if (pat->stringmatch)
    return pat->ign_case ? !strcasestr(buf, pat->p.str) : !strstr(buf, pat->p.str);
  else if (pat->groupmatch)
    return !mutt_group_match(pat->p.g, buf);
  else
    return regexec(pat->p.regex, buf, 0, NULL, 0);
}

static int msg_search(struct Context *ctx, struct Pattern *pat, int msgno)
{
  int match = 0;
  struct Message *msg = mx_open_message(ctx, msgno);
  if (!msg)
  {
    return match;
  }

  FILE *fp = NULL;
  long lng = 0;
  struct Header *h = ctx->hdrs[msgno];
#ifdef USE_FMEMOPEN
  char *temp = NULL;
  size_t tempsize;
#else
  char tempfile[_POSIX_PATH_MAX];
  struct stat st;
#endif

  if (ThoroughSearch)
  {
    /* decode the header / body */
    struct State s;
    memset(&s, 0, sizeof(s));
    s.fpin = msg->fp;
    s.flags = MUTT_CHARCONV;
#ifdef USE_FMEMOPEN
    s.fpout = open_memstream(&temp, &tempsize);
    if (!s.fpout)
    {
      mutt_perror(_("Error opening memstream"));
      return 0;
    }
#else
    mutt_mktemp(tempfile, sizeof(tempfile));
    s.fpout = mutt_file_fopen(tempfile, "w+");
    if (!s.fpout)
    {
      mutt_perror(tempfile);
      return 0;
    }
#endif

    if (pat->op != MUTT_BODY)
      mutt_copy_header(msg->fp, h, s.fpout, CH_FROM | CH_DECODE, NULL);

    if (pat->op != MUTT_HEADER)
    {
      mutt_parse_mime_message(ctx, h);

      if ((WithCrypto != 0) && (h->security & ENCRYPT) && !crypt_valid_passphrase(h->security))
      {
        mx_close_message(ctx, &msg);
        if (s.fpout)
        {
          mutt_file_fclose(&s.fpout);
#ifdef USE_FMEMOPEN
          FREE(&temp);
#else
          unlink(tempfile);
#endif
        }
        return 0;
      }

      fseeko(msg->fp, h->offset, SEEK_SET);
      mutt_body_handler(h->content, &s);
    }

#ifdef USE_FMEMOPEN
    fclose(s.fpout);
    lng = tempsize;

    if (tempsize)
    {
      fp = fmemopen(temp, tempsize, "r");
      if (!fp)
      {
        mutt_perror(_("Error re-opening memstream"));
        return 0;
      }
    }
    else
    { /* fmemopen cannot handle empty buffers */
      fp = mutt_file_fopen("/dev/null", "r");
      if (!fp)
      {
        mutt_perror(_("Error opening /dev/null"));
        return 0;
      }
    }
#else
    fp = s.fpout;
    fflush(fp);
    fseek(fp, 0, SEEK_SET);
    fstat(fileno(fp), &st);
    lng = (long) st.st_size;
#endif
  }
  else
  {
    /* raw header / body */
    fp = msg->fp;
    if (pat->op != MUTT_BODY)
    {
      fseeko(fp, h->offset, SEEK_SET);
      lng = h->content->offset - h->offset;
    }
    if (pat->op != MUTT_HEADER)
    {
      if (pat->op == MUTT_BODY)
        fseeko(fp, h->content->offset, SEEK_SET);
      lng += h->content->length;
    }
  }

  size_t blen = STRING;
  char *buf = mutt_mem_malloc(blen);

  /* search the file "fp" */
  while (lng > 0)
  {
    if (pat->op == MUTT_HEADER)
    {
      buf = mutt_read_rfc822_line(fp, buf, &blen);
      if (*buf == '\0')
        break;
    }
    else if (fgets(buf, blen - 1, fp) == NULL)
      break; /* don't loop forever */
    if (patmatch(pat, buf) == 0)
    {
      match = 1;
      break;
    }
    lng -= mutt_str_strlen(buf);
  }

  FREE(&buf);

  mx_close_message(ctx, &msg);

  if (ThoroughSearch)
  {
    mutt_file_fclose(&fp);
#ifdef USE_FMEMOPEN
    if (tempsize)
      FREE(&temp);
#else
    unlink(tempfile);
#endif
  }

  return match;
}

static const struct PatternFlags *lookup_tag(char tag)
{
  for (int i = 0; Flags[i].tag; i++)
    if (Flags[i].tag == tag)
      return (&Flags[i]);
  return NULL;
}

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
      if (!level)
        break;
    }
  }
  return s;
}

void mutt_pattern_free(struct Pattern **pat)
{
  struct Pattern *tmp = NULL;

  while (*pat)
  {
    tmp = *pat;
    *pat = (*pat)->next;

    if (tmp->stringmatch)
      FREE(&tmp->p.str);
    else if (tmp->groupmatch)
      tmp->p.g = NULL;
    else if (tmp->p.regex)
    {
      regfree(tmp->p.regex);
      FREE(&tmp->p.regex);
    }

    if (tmp->child)
      mutt_pattern_free(&tmp->child);
    FREE(&tmp);
  }
}

struct Pattern *mutt_pattern_comp(/* const */ char *s, int flags, struct Buffer *err)
{
  struct Pattern *curlist = NULL;
  struct Pattern *tmp = NULL, *tmp2 = NULL;
  struct Pattern *last = NULL;
  bool not = false;
  bool alladdr = false;
  bool or = false;
  bool implicit = true; /* used to detect logical AND operator */
  bool isalias = false;
  short thread_op;
  const struct PatternFlags *entry = NULL;
  char *p = NULL;
  char *buf = NULL;
  struct Buffer ps;

  mutt_buffer_init(&ps);
  ps.dptr = s;
  ps.dsize = mutt_str_strlen(s);

  while (*ps.dptr)
  {
    SKIPWS(ps.dptr);
    switch (*ps.dptr)
    {
      case '^':
        ps.dptr++;
        alladdr = !alladdr;
        break;
      case '!':
        ps.dptr++;
        not = !not;
        break;
      case '@':
        ps.dptr++;
        isalias = !isalias;
        break;
      case '|':
        if (! or)
        {
          if (!curlist)
          {
            snprintf(err->data, err->dsize, _("error in pattern at: %s"), ps.dptr);
            return NULL;
          }
          if (curlist->next)
          {
            /* A & B | C == (A & B) | C */
            tmp = new_pattern();
            tmp->op = MUTT_AND;
            tmp->child = curlist;

            curlist = tmp;
            last = curlist;
          }

          or = true;
        }
        ps.dptr++;
        implicit = false;
        not = false;
        alladdr = false;
        isalias = false;
        break;
      case '%':
      case '=':
      case '~':
        if (!*(ps.dptr + 1))
        {
          snprintf(err->data, err->dsize, _("missing pattern: %s"), ps.dptr);
          mutt_pattern_free(&curlist);
          return NULL;
        }
        thread_op = 0;
        if (*(ps.dptr + 1) == '(')
          thread_op = MUTT_THREAD;
        else if ((*(ps.dptr + 1) == '<') && (*(ps.dptr + 2) == '('))
          thread_op = MUTT_PARENT;
        else if ((*(ps.dptr + 1) == '>') && (*(ps.dptr + 2) == '('))
          thread_op = MUTT_CHILDREN;
        if (thread_op)
        {
          ps.dptr++; /* skip ~ */
          if (thread_op == MUTT_PARENT || thread_op == MUTT_CHILDREN)
            ps.dptr++;
          p = find_matching_paren(ps.dptr + 1);
          if (*p != ')')
          {
            snprintf(err->data, err->dsize, _("mismatched brackets: %s"), ps.dptr);
            mutt_pattern_free(&curlist);
            return NULL;
          }
          tmp = new_pattern();
          tmp->op = thread_op;
          if (last)
            last->next = tmp;
          else
            curlist = tmp;
          last = tmp;
          tmp->not ^= not;
          tmp->alladdr |= alladdr;
          tmp->isalias |= isalias;
          not = false;
          alladdr = false;
          isalias = false;
          /* compile the sub-expression */
          buf = mutt_str_substr_dup(ps.dptr + 1, p);
          tmp2 = mutt_pattern_comp(buf, flags, err);
          if (!tmp2)
          {
            FREE(&buf);
            mutt_pattern_free(&curlist);
            return NULL;
          }
          FREE(&buf);
          tmp->child = tmp2;
          ps.dptr = p + 1; /* restore location */
          break;
        }
        if (implicit && or)
        {
          /* A | B & C == (A | B) & C */
          tmp = new_pattern();
          tmp->op = MUTT_OR;
          tmp->child = curlist;
          curlist = tmp;
          last = tmp;
          or = false;
        }

        tmp = new_pattern();
        tmp->not = not;
        tmp->alladdr = alladdr;
        tmp->isalias = isalias;
        tmp->stringmatch = (*ps.dptr == '=') ? true : false;
        tmp->groupmatch = (*ps.dptr == '%') ? true : false;
        not = false;
        alladdr = false;
        isalias = false;

        if (last)
          last->next = tmp;
        else
          curlist = tmp;
        last = tmp;

        ps.dptr++; /* move past the ~ */
        entry = lookup_tag(*ps.dptr);
        if (!entry)
        {
          snprintf(err->data, err->dsize, _("%c: invalid pattern modifier"), *ps.dptr);
          mutt_pattern_free(&curlist);
          return NULL;
        }
        if (entry->class && (flags & entry->class) == 0)
        {
          snprintf(err->data, err->dsize, _("%c: not supported in this mode"), *ps.dptr);
          mutt_pattern_free(&curlist);
          return NULL;
        }
        tmp->op = entry->op;

        ps.dptr++; /* eat the operator and any optional whitespace */
        SKIPWS(ps.dptr);

        if (entry->eat_arg)
        {
          if (!*ps.dptr)
          {
            snprintf(err->data, err->dsize, "%s", _("missing parameter"));
            mutt_pattern_free(&curlist);
            return NULL;
          }
          if (!entry->eat_arg(tmp, &ps, err))
          {
            mutt_pattern_free(&curlist);
            return NULL;
          }
        }
        implicit = true;
        break;
      case '(':
        p = find_matching_paren(ps.dptr + 1);
        if (*p != ')')
        {
          snprintf(err->data, err->dsize, _("mismatched parenthesis: %s"), ps.dptr);
          mutt_pattern_free(&curlist);
          return NULL;
        }
        /* compile the sub-expression */
        buf = mutt_str_substr_dup(ps.dptr + 1, p);
        tmp = mutt_pattern_comp(buf, flags, err);
        if (!tmp)
        {
          FREE(&buf);
          mutt_pattern_free(&curlist);
          return NULL;
        }
        FREE(&buf);
        if (last)
          last->next = tmp;
        else
          curlist = tmp;
        last = tmp;
        tmp->not ^= not;
        tmp->alladdr |= alladdr;
        tmp->isalias |= isalias;
        not = false;
        alladdr = false;
        isalias = false;
        ps.dptr = p + 1; /* restore location */
        break;
      default:
        snprintf(err->data, err->dsize, _("error in pattern at: %s"), ps.dptr);
        mutt_pattern_free(&curlist);
        return NULL;
    }
  }
  if (!curlist)
  {
    mutt_str_strfcpy(err->data, _("empty pattern"), err->dsize);
    return NULL;
  }
  if (curlist->next)
  {
    tmp = new_pattern();
    tmp->op = or ? MUTT_OR : MUTT_AND;
    tmp->child = curlist;
    curlist = tmp;
  }
  return curlist;
}

static bool perform_and(struct Pattern *pat, enum PatternExecFlag flags,
                        struct Context *ctx, struct Header *hdr, struct PatternCache *cache)
{
  for (; pat; pat = pat->next)
    if (mutt_pattern_exec(pat, flags, ctx, hdr, cache) <= 0)
      return false;
  return true;
}

static int perform_or(struct Pattern *pat, enum PatternExecFlag flags,
                      struct Context *ctx, struct Header *hdr, struct PatternCache *cache)
{
  for (; pat; pat = pat->next)
    if (mutt_pattern_exec(pat, flags, ctx, hdr, cache) > 0)
      return true;
  return false;
}

static int match_addrlist(struct Pattern *pat, int match_personal, int n, ...)
{
  va_list ap;

  va_start(ap, n);
  for (; n; n--)
  {
    for (struct Address *a = va_arg(ap, struct Address *); a; a = a->next)
    {
      if (pat->alladdr ^ ((!pat->isalias || alias_reverse_lookup(a)) &&
                          ((a->mailbox && !patmatch(pat, a->mailbox)) ||
                           (match_personal && a->personal && !patmatch(pat, a->personal)))))
      {
        va_end(ap);
        return (!pat->alladdr); /* Found match, or non-match if alladdr */
      }
    }
  }
  va_end(ap);
  return pat->alladdr; /* No matches, or all matches if alladdr */
}

static bool match_reference(struct Pattern *pat, struct ListHead *refs)
{
  struct ListNode *np;
  STAILQ_FOREACH(np, refs, entries)
  {
    if (patmatch(pat, np->data) == 0)
      return true;
  }
  return false;
}

/**
 * mutt_is_list_recipient - Matches subscribed mailing lists
 */
int mutt_is_list_recipient(int alladdr, struct Address *a1, struct Address *a2)
{
  for (; a1; a1 = a1->next)
    if (alladdr ^ mutt_is_subscribed_list(a1))
      return (!alladdr);
  for (; a2; a2 = a2->next)
    if (alladdr ^ mutt_is_subscribed_list(a2))
      return (!alladdr);
  return alladdr;
}

/**
 * mutt_is_list_cc - Matches known mailing lists
 *
 * The function name may seem a little bit misleading: It checks all
 * recipients in To and Cc for known mailing lists, subscribed or not.
 */
int mutt_is_list_cc(int alladdr, struct Address *a1, struct Address *a2)
{
  for (; a1; a1 = a1->next)
    if (alladdr ^ mutt_is_mail_list(a1))
      return (!alladdr);
  for (; a2; a2 = a2->next)
    if (alladdr ^ mutt_is_mail_list(a2))
      return (!alladdr);
  return alladdr;
}

static int match_user(int alladdr, struct Address *a1, struct Address *a2)
{
  for (; a1; a1 = a1->next)
    if (alladdr ^ mutt_addr_is_user(a1))
      return (!alladdr);
  for (; a2; a2 = a2->next)
    if (alladdr ^ mutt_addr_is_user(a2))
      return (!alladdr);
  return alladdr;
}

static int match_threadcomplete(struct Pattern *pat, enum PatternExecFlag flags,
                                struct Context *ctx, struct MuttThread *t,
                                int left, int up, int right, int down)
{
  int a;
  struct Header *h = NULL;

  if (!t)
    return 0;
  h = t->message;
  if (h)
    if (mutt_pattern_exec(pat, flags, ctx, h, NULL))
      return 1;

  if (up && (a = match_threadcomplete(pat, flags, ctx, t->parent, 1, 1, 1, 0)))
    return a;
  if (right && t->parent &&
      (a = match_threadcomplete(pat, flags, ctx, t->next, 0, 0, 1, 1)))
  {
    return a;
  }
  if (left && t->parent &&
      (a = match_threadcomplete(pat, flags, ctx, t->prev, 1, 0, 0, 1)))
  {
    return a;
  }
  if (down && (a = match_threadcomplete(pat, flags, ctx, t->child, 1, 0, 1, 1)))
    return a;
  return 0;
}

static int match_threadparent(struct Pattern *pat, enum PatternExecFlag flags,
                              struct Context *ctx, struct MuttThread *t)
{
  if (!t || !t->parent || !t->parent->message)
    return 0;

  return mutt_pattern_exec(pat, flags, ctx, t->parent->message, NULL);
}

static int match_threadchildren(struct Pattern *pat, enum PatternExecFlag flags,
                                struct Context *ctx, struct MuttThread *t)
{
  if (!t || !t->child)
    return 0;

  for (t = t->child; t; t = t->next)
    if (t->message && mutt_pattern_exec(pat, flags, ctx, t->message, NULL))
      return 1;

  return 0;
}

/**
 * set_pattern_cache_value - Sets a value in the PatternCache cache entry
 *
 * Normalizes the "true" value to 2.
 */
static void set_pattern_cache_value(int *cache_entry, int value)
{
  *cache_entry = value ? 2 : 1;
}

/**
 * get_pattern_cache_value - Get pattern cache value
 * @retval 1 if the cache value is set and has a true value
 * @retval 0 otherwise (even if unset!)
 */
static int get_pattern_cache_value(int cache_entry)
{
  return cache_entry == 2;
}

static int is_pattern_cache_set(int cache_entry)
{
  return cache_entry != 0;
}

/**
 * mutt_pattern_exec - Match a pattern against an email header
 *
 * flags: MUTT_MATCH_FULL_ADDRESS - match both personal and machine address
 * cache: For repeated matches against the same Header, passing in non-NULL will
 *        store some of the cacheable pattern matches in this structure.
 */
int mutt_pattern_exec(struct Pattern *pat, enum PatternExecFlag flags,
                      struct Context *ctx, struct Header *h, struct PatternCache *cache)
{
  int result;
  int *cache_entry = NULL;

  switch (pat->op)
  {
    case MUTT_AND:
      return (pat->not ^ (perform_and(pat->child, flags, ctx, h, cache) > 0));
    case MUTT_OR:
      return (pat->not ^ (perform_or(pat->child, flags, ctx, h, cache) > 0));
    case MUTT_THREAD:
      return (pat->not ^
              match_threadcomplete(pat->child, flags, ctx, h->thread, 1, 1, 1, 1));
    case MUTT_PARENT:
      return (pat->not ^ match_threadparent(pat->child, flags, ctx, h->thread));
    case MUTT_CHILDREN:
      return (pat->not ^ match_threadchildren(pat->child, flags, ctx, h->thread));
    case MUTT_ALL:
      return !pat->not;
    case MUTT_EXPIRED:
      return (pat->not ^ h->expired);
    case MUTT_SUPERSEDED:
      return (pat->not ^ h->superseded);
    case MUTT_FLAG:
      return (pat->not ^ h->flagged);
    case MUTT_TAG:
      return (pat->not ^ h->tagged);
    case MUTT_NEW:
      return (pat->not? h->old || h->read : !(h->old || h->read));
    case MUTT_UNREAD:
      return (pat->not? h->read : !h->read);
    case MUTT_REPLIED:
      return (pat->not ^ h->replied);
    case MUTT_OLD:
      return (pat->not? (!h->old || h->read) : (h->old && !h->read));
    case MUTT_READ:
      return (pat->not ^ h->read);
    case MUTT_DELETED:
      return (pat->not ^ h->deleted);
    case MUTT_MESSAGE:
      return (pat->not ^ ((HMSG(h) >= pat->min) && (HMSG(h) <= pat->max)));
    case MUTT_DATE:
      return (pat->not ^ (h->date_sent >= pat->min && h->date_sent <= pat->max));
    case MUTT_DATE_RECEIVED:
      return (pat->not ^ (h->received >= pat->min && h->received <= pat->max));
    case MUTT_BODY:
    case MUTT_HEADER:
    case MUTT_WHOLE_MSG:
      /*
       * ctx can be NULL in certain cases, such as when replying to a message from the attachment menu and
       * the user has a reply-hook using "~h" (bug #2190).
       * This is also the case when message scoring.
       */
      if (!ctx)
        return 0;
#ifdef USE_IMAP
      /* IMAP search sets h->matched at search compile time */
      if (ctx->magic == MUTT_IMAP && pat->stringmatch)
        return h->matched;
#endif
      return (pat->not ^ msg_search(ctx, pat, h->msgno));
    case MUTT_SERVERSEARCH:
#ifdef USE_IMAP
      if (!ctx)
        return 0;
      if (ctx->magic == MUTT_IMAP)
      {
        if (pat->stringmatch)
          return (h->matched);
        return 0;
      }
      mutt_error(_("error: server custom search only supported with IMAP."));
      return 0;
#else
      mutt_error(_("error: server custom search only supported with IMAP."));
      return -1;
#endif
    case MUTT_SENDER:
      if (!h->env)
        return 0;
      return (pat->not ^ match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 1,
                                        h->env->sender));
    case MUTT_FROM:
      if (!h->env)
        return 0;
      return (pat->not ^
              match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 1, h->env->from));
    case MUTT_TO:
      if (!h->env)
        return 0;
      return (pat->not ^
              match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 1, h->env->to));
    case MUTT_CC:
      if (!h->env)
        return 0;
      return (pat->not ^
              match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 1, h->env->cc));
    case MUTT_SUBJECT:
      if (!h->env)
        return 0;
      return (pat->not ^ (h->env->subject && patmatch(pat, h->env->subject) == 0));
    case MUTT_ID:
      if (!h->env)
        return 0;
      return (pat->not ^ (h->env->message_id && patmatch(pat, h->env->message_id) == 0));
    case MUTT_SCORE:
      return (pat->not ^ (h->score >= pat->min &&
                          (pat->max == MUTT_MAXRANGE || h->score <= pat->max)));
    case MUTT_SIZE:
      return (pat->not ^ (h->content->length >= pat->min &&
                          (pat->max == MUTT_MAXRANGE || h->content->length <= pat->max)));
    case MUTT_REFERENCE:
      if (!h->env)
        return 0;
      return (pat->not ^ (match_reference(pat, &h->env->references) ||
                          match_reference(pat, &h->env->in_reply_to)));
    case MUTT_ADDRESS:
      if (!h->env)
        return 0;
      return (pat->not ^ match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 4,
                                        h->env->from, h->env->sender,
                                        h->env->to, h->env->cc));
    case MUTT_RECIPIENT:
      if (!h->env)
        return 0;
      return (pat->not ^ match_addrlist(pat, flags & MUTT_MATCH_FULL_ADDRESS, 2,
                                        h->env->to, h->env->cc));
    case MUTT_LIST: /* known list, subscribed or not */
      if (!h->env)
        return 0;
      if (cache)
      {
        cache_entry = pat->alladdr ? &cache->list_all : &cache->list_one;
        if (!is_pattern_cache_set(*cache_entry))
          set_pattern_cache_value(
              cache_entry, mutt_is_list_cc(pat->alladdr, h->env->to, h->env->cc));
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = mutt_is_list_cc(pat->alladdr, h->env->to, h->env->cc);
      return (pat->not ^ result);
    case MUTT_SUBSCRIBED_LIST:
      if (!h->env)
        return 0;
      if (cache)
      {
        cache_entry = pat->alladdr ? &cache->sub_all : &cache->sub_one;
        if (!is_pattern_cache_set(*cache_entry))
          set_pattern_cache_value(
              cache_entry,
              mutt_is_list_recipient(pat->alladdr, h->env->to, h->env->cc));
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = mutt_is_list_recipient(pat->alladdr, h->env->to, h->env->cc);
      return (pat->not ^ result);
    case MUTT_PERSONAL_RECIP:
      if (!h->env)
        return 0;
      if (cache)
      {
        cache_entry = pat->alladdr ? &cache->pers_recip_all : &cache->pers_recip_one;
        if (!is_pattern_cache_set(*cache_entry))
          set_pattern_cache_value(cache_entry,
                                  match_user(pat->alladdr, h->env->to, h->env->cc));
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = match_user(pat->alladdr, h->env->to, h->env->cc);
      return (pat->not ^ result);
    case MUTT_PERSONAL_FROM:
      if (!h->env)
        return 0;
      if (cache)
      {
        cache_entry = pat->alladdr ? &cache->pers_from_all : &cache->pers_from_one;
        if (!is_pattern_cache_set(*cache_entry))
          set_pattern_cache_value(cache_entry, match_user(pat->alladdr, h->env->from, NULL));
        result = get_pattern_cache_value(*cache_entry);
      }
      else
        result = match_user(pat->alladdr, h->env->from, NULL);
      return (pat->not ^ result);
    case MUTT_COLLAPSED:
      return (pat->not ^ (h->collapsed && h->num_hidden > 1));
    case MUTT_CRYPT_SIGN:
      if (!WithCrypto)
        break;
      return (pat->not ^ ((h->security & SIGN) ? 1 : 0));
    case MUTT_CRYPT_VERIFIED:
      if (!WithCrypto)
        break;
      return (pat->not ^ ((h->security & GOODSIGN) ? 1 : 0));
    case MUTT_CRYPT_ENCRYPT:
      if (!WithCrypto)
        break;
      return (pat->not ^ ((h->security & ENCRYPT) ? 1 : 0));
    case MUTT_PGP_KEY:
      if (!(WithCrypto & APPLICATION_PGP))
        break;
      return (pat->not ^ ((h->security & PGPKEY) == PGPKEY));
    case MUTT_XLABEL:
      if (!h->env)
        return 0;
      return (pat->not ^ (h->env->x_label && patmatch(pat, h->env->x_label) == 0));
    case MUTT_DRIVER_TAGS:
    {
      char *tags = driver_tags_get(&h->tags);
      bool ret = (pat->not ^ (tags && patmatch(pat, tags) == 0));
      FREE(&tags);
      return ret;
    }
    case MUTT_HORMEL:
      if (!h->env)
        return 0;
      return (pat->not ^ (h->env->spam && h->env->spam->data &&
                          patmatch(pat, h->env->spam->data) == 0));
    case MUTT_DUPLICATED:
      return (pat->not ^ (h->thread && h->thread->duplicate_thread));
    case MUTT_MIMEATTACH:
      if (!ctx)
        return 0;
      {
        int count = mutt_count_body_parts(ctx, h);
        return (pat->not ^ (count >= pat->min &&
                            (pat->max == MUTT_MAXRANGE || count <= pat->max)));
      }
    case MUTT_UNREFERENCED:
      return (pat->not ^ (h->thread && !h->thread->child));
    case MUTT_BROKEN:
      return (pat->not ^ (h->thread && h->thread->fake_thread));
#ifdef USE_NNTP
    case MUTT_NEWSGROUPS:
      if (!h->env)
        return 0;
      return (pat->not ^ (h->env->newsgroups && patmatch(pat, h->env->newsgroups) == 0));
#endif
  }
  mutt_error(_("error: unknown op %d (report this error)."), pat->op);
  return -1;
}

static void quote_simple(char *tmp, size_t len, const char *p)
{
  int i = 0;

  tmp[i++] = '"';
  while (*p && i < len - 3)
  {
    if (*p == '\\' || *p == '"')
      tmp[i++] = '\\';
    tmp[i++] = *p++;
  }
  tmp[i++] = '"';
  tmp[i] = 0;
}

/**
 * mutt_check_simple - convert a simple search into a real request
 */
void mutt_check_simple(char *s, size_t len, const char *simple)
{
  bool do_simple = true;

  for (char *p = s; p && *p; p++)
  {
    if (*p == '\\' && *(p + 1))
      p++;
    else if (*p == '~' || *p == '=' || *p == '%')
    {
      do_simple = false;
      break;
    }
  }

  /* XXX - is mutt_str_strcasecmp() right here, or should we use locale's
   * equivalences?
   */

  if (do_simple) /* yup, so spoof a real request */
  {
    /* convert old tokens into the new format */
    if ((mutt_str_strcasecmp("all", s) == 0) || (mutt_str_strcmp("^", s) == 0) ||
        (mutt_str_strcmp(".", s) == 0)) /* ~A is more efficient */
    {
      mutt_str_strfcpy(s, "~A", len);
    }
    else if (mutt_str_strcasecmp("del", s) == 0)
      mutt_str_strfcpy(s, "~D", len);
    else if (mutt_str_strcasecmp("flag", s) == 0)
      mutt_str_strfcpy(s, "~F", len);
    else if (mutt_str_strcasecmp("new", s) == 0)
      mutt_str_strfcpy(s, "~N", len);
    else if (mutt_str_strcasecmp("old", s) == 0)
      mutt_str_strfcpy(s, "~O", len);
    else if (mutt_str_strcasecmp("repl", s) == 0)
      mutt_str_strfcpy(s, "~Q", len);
    else if (mutt_str_strcasecmp("read", s) == 0)
      mutt_str_strfcpy(s, "~R", len);
    else if (mutt_str_strcasecmp("tag", s) == 0)
      mutt_str_strfcpy(s, "~T", len);
    else if (mutt_str_strcasecmp("unread", s) == 0)
      mutt_str_strfcpy(s, "~U", len);
    else
    {
      char tmp[LONG_STRING];
      quote_simple(tmp, sizeof(tmp), s);
      mutt_expand_fmt(s, len, simple, tmp);
    }
  }
}

/**
 * top_of_thread - Find the first email in the current thread
 * @param h Header of current email
 * @retval ptr  Success, email found
 * @retval NULL On error
 */
static struct MuttThread *top_of_thread(struct Header *h)
{
  struct MuttThread *t = NULL;

  if (!h)
    return NULL;

  t = h->thread;

  while (t && t->parent)
    t = t->parent;

  return t;
}

/**
 * mutt_limit_current_thread - Limit the email view to the current thread
 * @param h Header of current email
 * @retval true Success
 * @retval false Failure
 */
bool mutt_limit_current_thread(struct Header *h)
{
  struct MuttThread *me = NULL;

  if (!h)
    return false;

  me = top_of_thread(h);
  if (!me)
    return false;

  Context->vcount = 0;
  Context->vsize = 0;
  Context->collapsed = false;

  for (int i = 0; i < Context->msgcount; i++)
  {
    Context->hdrs[i]->virtual = -1;
    Context->hdrs[i]->limited = false;
    Context->hdrs[i]->collapsed = false;
    Context->hdrs[i]->num_hidden = 0;

    if (top_of_thread(Context->hdrs[i]) == me)
    {
      struct Body *body = Context->hdrs[i]->content;

      Context->hdrs[i]->virtual = Context->vcount;
      Context->hdrs[i]->limited = true;
      Context->v2r[Context->vcount] = i;
      Context->vcount++;
      Context->vsize += (body->length + body->offset - body->hdr_offset);
    }
  }
  return true;
}

int mutt_pattern_func(int op, char *prompt)
{
  struct Pattern *pat = NULL;
  char buf[LONG_STRING] = "", *simple = NULL;
  struct Buffer err;
  int rc = -1;
  struct Progress progress;

  mutt_str_strfcpy(buf, NONULL(Context->pattern), sizeof(buf));
  if (prompt || op != MUTT_LIMIT)
    if (mutt_get_field(prompt, buf, sizeof(buf), MUTT_PATTERN | MUTT_CLEAR) != 0 || !buf[0])
      return -1;

  mutt_message(_("Compiling search pattern..."));

  simple = mutt_str_strdup(buf);
  mutt_check_simple(buf, sizeof(buf), NONULL(SimpleSearch));

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  pat = mutt_pattern_comp(buf, MUTT_FULL_MSG, &err);
  if (!pat)
  {
    mutt_error("%s", err.data);
    goto bail;
  }

#ifdef USE_IMAP
  if (Context->magic == MUTT_IMAP && imap_search(Context, pat) < 0)
    goto bail;
#endif

  mutt_progress_init(&progress, _("Executing command on matching messages..."),
                     MUTT_PROGRESS_MSG, ReadInc,
                     (op == MUTT_LIMIT) ? Context->msgcount : Context->vcount);

  if (op == MUTT_LIMIT)
  {
    Context->vcount = 0;
    Context->vsize = 0;
    Context->collapsed = false;

    for (int i = 0; i < Context->msgcount; i++)
    {
      mutt_progress_update(&progress, i, -1);
      /* new limit pattern implicitly uncollapses all threads */
      Context->hdrs[i]->virtual = -1;
      Context->hdrs[i]->limited = false;
      Context->hdrs[i]->collapsed = false;
      Context->hdrs[i]->num_hidden = 0;
      if (mutt_pattern_exec(pat, MUTT_MATCH_FULL_ADDRESS, Context, Context->hdrs[i], NULL))
      {
        Context->hdrs[i]->virtual = Context->vcount;
        Context->hdrs[i]->limited = true;
        Context->v2r[Context->vcount] = i;
        Context->vcount++;
        struct Body *b = Context->hdrs[i]->content;
        Context->vsize += b->length + b->offset - b->hdr_offset;
      }
    }
  }
  else
  {
    for (int i = 0; i < Context->vcount; i++)
    {
      mutt_progress_update(&progress, i, -1);
      if (mutt_pattern_exec(pat, MUTT_MATCH_FULL_ADDRESS, Context,
                            Context->hdrs[Context->v2r[i]], NULL))
      {
        switch (op)
        {
          case MUTT_UNDELETE:
            mutt_set_flag(Context, Context->hdrs[Context->v2r[i]], MUTT_PURGE, 0);
          /* fallthrough */
          case MUTT_DELETE:
            mutt_set_flag(Context, Context->hdrs[Context->v2r[i]], MUTT_DELETE,
                          (op == MUTT_DELETE));
            break;
          case MUTT_TAG:
          case MUTT_UNTAG:
            mutt_set_flag(Context, Context->hdrs[Context->v2r[i]], MUTT_TAG,
                          (op == MUTT_TAG));
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
    if (Context->limit_pattern)
      mutt_pattern_free(&Context->limit_pattern);

    if (Context->msgcount && !Context->vcount)
      mutt_error(_("No messages matched criteria."));

    /* record new limit pattern, unless match all */
    if (mutt_str_strcmp(buf, "~A") != 0)
    {
      Context->pattern = simple;
      simple = NULL; /* don't clobber it */
      Context->limit_pattern = mutt_pattern_comp(buf, MUTT_FULL_MSG, &err);
    }
  }

  rc = 0;

bail:
  FREE(&simple);
  mutt_pattern_free(&pat);
  FREE(&err.data);

  return rc;
}

int mutt_search_command(int cur, int op)
{
  struct Progress progress;

  if (!*LastSearch || (op != OP_SEARCH_NEXT && op != OP_SEARCH_OPPOSITE))
  {
    char buf[STRING];
    mutt_str_strfcpy(buf, *LastSearch ? LastSearch : "", sizeof(buf));
    if (mutt_get_field((op == OP_SEARCH || op == OP_SEARCH_NEXT) ?
                           _("Search for: ") :
                           _("Reverse search for: "),
                       buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN) != 0 ||
        !buf[0])
      return -1;

    if (op == OP_SEARCH || op == OP_SEARCH_NEXT)
      OPT_SEARCH_REVERSE = false;
    else
      OPT_SEARCH_REVERSE = true;

    /* compare the *expanded* version of the search pattern in case
       $simple_search has changed while we were searching */
    char temp[LONG_STRING];
    mutt_str_strfcpy(temp, buf, sizeof(temp));
    mutt_check_simple(temp, sizeof(temp), NONULL(SimpleSearch));

    if (!SearchPattern || (mutt_str_strcmp(temp, LastSearchExpn) != 0))
    {
      struct Buffer err;
      mutt_buffer_init(&err);
      OPT_SEARCH_INVALID = true;
      mutt_str_strfcpy(LastSearch, buf, sizeof(LastSearch));
      mutt_message(_("Compiling search pattern..."));
      mutt_pattern_free(&SearchPattern);
      err.dsize = STRING;
      err.data = mutt_mem_malloc(err.dsize);
      SearchPattern = mutt_pattern_comp(temp, MUTT_FULL_MSG, &err);
      if (!SearchPattern)
      {
        mutt_error("%s", err.data);
        FREE(&err.data);
        LastSearch[0] = '\0';
        return -1;
      }
      FREE(&err.data);
      mutt_clear_error();
    }
  }

  if (OPT_SEARCH_INVALID)
  {
    for (int i = 0; i < Context->msgcount; i++)
      Context->hdrs[i]->searched = false;
#ifdef USE_IMAP
    if (Context->magic == MUTT_IMAP && imap_search(Context, SearchPattern) < 0)
      return -1;
#endif
    OPT_SEARCH_INVALID = false;
  }

  int incr = (OPT_SEARCH_REVERSE) ? -1 : 1;
  if (op == OP_SEARCH_OPPOSITE)
    incr = -incr;

  mutt_progress_init(&progress, _("Searching..."), MUTT_PROGRESS_MSG, ReadInc,
                     Context->vcount);

  for (int i = cur + incr, j = 0; j != Context->vcount; j++)
  {
    const char *msg = NULL;
    mutt_progress_update(&progress, j, -1);
    if (i > Context->vcount - 1)
    {
      i = 0;
      if (WrapSearch)
        msg = _("Search wrapped to top.");
      else
      {
        mutt_message(_("Search hit bottom without finding match"));
        return -1;
      }
    }
    else if (i < 0)
    {
      i = Context->vcount - 1;
      if (WrapSearch)
        msg = _("Search wrapped to bottom.");
      else
      {
        mutt_message(_("Search hit top without finding match"));
        return -1;
      }
    }

    struct Header *h = Context->hdrs[Context->v2r[i]];
    if (h->searched)
    {
      /* if we've already evaluated this message, use the cached value */
      if (h->matched)
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
      h->searched = true;
      h->matched =
          mutt_pattern_exec(SearchPattern, MUTT_MATCH_FULL_ADDRESS, Context, h, NULL);
      if (h->matched > 0)
      {
        mutt_clear_error();
        if (msg && *msg)
          mutt_message(msg);
        return i;
      }
    }

    if (SigInt)
    {
      mutt_error(_("Search interrupted."));
      SigInt = 0;
      return -1;
    }

    i += incr;
  }

  mutt_error(_("Not found."));
  return -1;
}
