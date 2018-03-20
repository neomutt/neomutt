/**
 * @file
 * String processing routines to generate the mail index
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Ian Zimmerman <itz@primate.net>
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
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "mbtable.h"
#include "mutt_curses.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#include "tags.h"
#include "thread.h"

/**
 * enum FlagChars - Index into the FlagChars variable ($flag_chars)
 */
enum FlagChars
{
  FlagCharTagged,
  FlagCharImportant,
  FlagCharDeleted,
  FlagCharDeletedAttach,
  FlagCharReplied,
  FlagCharOld,
  FlagCharNew,
  FlagCharOldThread,
  FlagCharNewThread,
  FlagCharSEmpty,
  FlagCharZEmpty
};

bool mutt_is_mail_list(struct Address *addr)
{
  if (!mutt_regexlist_match(UnMailLists, addr->mailbox))
    return mutt_regexlist_match(MailLists, addr->mailbox);
  return false;
}

bool mutt_is_subscribed_list(struct Address *addr)
{
  if (!mutt_regexlist_match(UnMailLists, addr->mailbox) &&
      !mutt_regexlist_match(UnSubscribedLists, addr->mailbox))
  {
    return mutt_regexlist_match(SubscribedLists, addr->mailbox);
  }
  return false;
}

/**
 * check_for_mailing_list - Search list of addresses for a mailing list
 * @param addr    List of addreses to search
 * @param pfx     Prefix string
 * @param buf     Buffer to store results
 * @param buflen  Buffer length
 * @retval 1 Mailing list found
 * @retval 0 No list found
 *
 * Search for a mailing list in the list of addresses pointed to by addr.
 * If one is found, print pfx and the name of the list into buf.
 */
static bool check_for_mailing_list(struct Address *addr, const char *pfx, char *buf, int buflen)
{
  for (; addr; addr = addr->next)
  {
    if (mutt_is_subscribed_list(addr))
    {
      if (pfx && buf && buflen)
        snprintf(buf, buflen, "%s%s", pfx, mutt_get_name(addr));
      return true;
    }
  }
  return false;
}

/**
 * check_for_mailing_list_addr - Check an address list for a mailing list
 *
 * If one is found, print the address of the list into buf, then return 1.
 * Otherwise, simply return 0.
 */
static bool check_for_mailing_list_addr(struct Address *addr, char *buf, int buflen)
{
  for (; addr; addr = addr->next)
  {
    if (mutt_is_subscribed_list(addr))
    {
      if (buf && buflen)
        snprintf(buf, buflen, "%s", addr->mailbox);
      return true;
    }
  }
  return false;
}

static bool first_mailing_list(char *buf, size_t buflen, struct Address *a)
{
  for (; a; a = a->next)
  {
    if (mutt_is_subscribed_list(a))
    {
      mutt_save_path(buf, buflen, a);
      return true;
    }
  }
  return false;
}

/**
 * add_index_color - Insert a color marker into a string
 * @param buf    Buffer to store marker
 * @param buflen Buffer length
 * @param flags  Flags, e.g. MUTT_FORMAT_INDEX
 * @param color  Color, e.g. MT_COLOR_MESSAGE
 * @retval n Number of characters written
 *
 * The colors are stored as "magic" strings embedded in the text.
 */
static size_t add_index_color(char *buf, size_t buflen, enum FormatFlag flags, char color)
{
  /* only add color markers if we are operating on main index entries. */
  if (!(flags & MUTT_FORMAT_INDEX))
    return 0;

  /* this item is going to be passed to an external filter */
  if (flags & MUTT_FORMAT_NOFILTER)
    return 0;

  if (color == MT_COLOR_INDEX)
  { /* buf might be uninitialized other cases */
    const size_t len = mutt_str_strlen(buf);
    buf += len;
    buflen -= len;
  }

  if (buflen <= 2)
    return 0;

  buf[0] = MUTT_SPECIAL_INDEX;
  buf[1] = color;
  buf[2] = '\0';

  return 2;
}

/**
 * enum FieldType - Header types
 */
enum FieldType
{
  DISP_TO,
  DISP_CC,
  DISP_BCC,
  DISP_FROM,
  DISP_NUM
};

/**
 * get_nth_wchar - Extract one char from a multi-byte table
 * @param table  Multi-byte table
 * @param index  Select this character
 * @retval ptr String pointer to the character
 *
 * Extract one multi-byte character from a string table.
 * If the index is invalid, then a space character will be returned.
 * If the character selected is '\n' (Ctrl-M), then "" will be returned.
 */
static char *get_nth_wchar(struct MbTable *table, int index)
{
  if (!table || !table->chars || (index < 0) || (index >= table->len))
    return " ";

  if (table->chars[index][0] == '\r')
    return "";

  return table->chars[index];
}

/**
 * make_from_prefix - Create a prefix for an author field
 * @param disp   Type of field
 * @retval string Prefix string (do not free it)
 *
 * If $from_chars is set, pick an appropriate character from it.
 * If not, use the default prefix: "To", "Cc", etc
 */
static const char *make_from_prefix(enum FieldType disp)
{
  /* need 2 bytes at the end, one for the space, another for NUL */
  static char padded[8];
  static const char *long_prefixes[DISP_NUM] = {
    [DISP_TO] = "To ",
    [DISP_CC] = "Cc ",
    [DISP_BCC] = "Bcc ",
    [DISP_FROM] = "",
  };

  if (!FromChars || !FromChars->chars || (FromChars->len == 0))
    return long_prefixes[disp];

  char *pchar = get_nth_wchar(FromChars, disp);
  if (mutt_str_strlen(pchar) == 0)
    return "";

  snprintf(padded, sizeof(padded), "%s ", pchar);
  return padded;
}

/**
 * make_from - Generate a From: field (with optional prefix)
 * @param env      Envelope of the email
 * @param buf      Buffer to store the result
 * @param len      Size of the buffer
 * @param do_lists Should we check for mailing lists?
 *
 * Generate the %F or %L field in $index_format.
 * This is the author, or recipient of the email.
 *
 * The field can optionally be prefixed by a character from $from_chars.
 * If $from_chars is not set, the prefix will be, "To", "Cc", etc
 */
static void make_from(struct Envelope *env, char *buf, size_t len, int do_lists)
{
  if (!env || !buf)
    return;

  bool me;
  enum FieldType disp;
  struct Address *name = NULL;

  me = mutt_addr_is_user(env->from);

  if (do_lists || me)
  {
    if (check_for_mailing_list(env->to, make_from_prefix(DISP_TO), buf, len))
      return;
    if (check_for_mailing_list(env->cc, make_from_prefix(DISP_CC), buf, len))
      return;
  }

  if (me && env->to)
  {
    disp = DISP_TO;
    name = env->to;
  }
  else if (me && env->cc)
  {
    disp = DISP_CC;
    name = env->cc;
  }
  else if (me && env->bcc)
  {
    disp = DISP_BCC;
    name = env->bcc;
  }
  else if (env->from)
  {
    disp = DISP_FROM;
    name = env->from;
  }
  else
  {
    *buf = '\0';
    return;
  }

  snprintf(buf, len, "%s%s", make_from_prefix(disp), mutt_get_name(name));
}

static void make_from_addr(struct Envelope *hdr, char *buf, size_t len, int do_lists)
{
  if (!hdr || !buf)
    return;

  bool me = mutt_addr_is_user(hdr->from);

  if (do_lists || me)
  {
    if (check_for_mailing_list_addr(hdr->to, buf, len))
      return;
    if (check_for_mailing_list_addr(hdr->cc, buf, len))
      return;
  }

  if (me && hdr->to)
    snprintf(buf, len, "%s", hdr->to->mailbox);
  else if (me && hdr->cc)
    snprintf(buf, len, "%s", hdr->cc->mailbox);
  else if (hdr->from)
    mutt_str_strfcpy(buf, hdr->from->mailbox, len);
  else
    *buf = 0;
}

static bool user_in_addr(struct Address *a)
{
  for (; a; a = a->next)
    if (mutt_addr_is_user(a))
      return true;
  return false;
}

/**
 * user_is_recipient - Is the user a recipient of the message
 * @retval 0 User is not in list
 * @retval 1 User is unique recipient
 * @retval 2 User is in the TO list
 * @retval 3 User is in the CC list
 * @retval 4 User is originator
 * @retval 5 Sent to a subscribed mailinglist
 */
static int user_is_recipient(struct Header *h)
{
  if (!h || !h->env)
    return 0;

  struct Envelope *env = h->env;

  if (!h->recip_valid)
  {
    h->recip_valid = true;

    if (mutt_addr_is_user(env->from))
      h->recipient = 4;
    else if (user_in_addr(env->to))
    {
      if (env->to->next || env->cc)
        h->recipient = 2; /* non-unique recipient */
      else
        h->recipient = 1; /* unique recipient */
    }
    else if (user_in_addr(env->cc))
      h->recipient = 3;
    else if (check_for_mailing_list(env->to, NULL, NULL, 0))
      h->recipient = 5;
    else if (check_for_mailing_list(env->cc, NULL, NULL, 0))
      h->recipient = 5;
    else
      h->recipient = 0;
  }

  return h->recipient;
}

static char *apply_subject_mods(struct Envelope *env)
{
  if (!env)
    return NULL;

  if (!SubjectRegexList)
    return env->subject;

  if (env->subject == NULL || *env->subject == '\0')
    return env->disp_subj = NULL;

  env->disp_subj = mutt_replacelist_apply(SubjectRegexList, NULL, 0, env->subject);
  return env->disp_subj;
}

static bool thread_is_new(struct Context *ctx, struct Header *hdr)
{
  return (hdr->collapsed && (hdr->num_hidden > 1) &&
          (mutt_thread_contains_unread(ctx, hdr) == 1));
}

static bool thread_is_old(struct Context *ctx, struct Header *hdr)
{
  return (hdr->collapsed && (hdr->num_hidden > 1) &&
          (mutt_thread_contains_unread(ctx, hdr) == 2));
}

/**
 * index_format_str - Format a string for the index list
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * index_format_str() is a callback function for mutt_expando_format().
 *
 * | Expando | Description
 * |:--------|:-----------------------------------------------------------------
 * | \%a     | Address of author
 * | \%A     | Reply-to address (if present; otherwise: address of author
 * | \%b     | Filename of the originating folder
 * | \%B     | The list to which the letter was sent, or else the folder name (%b).
 * | \%C     | Current message number
 * | \%c     | Size of message in bytes
 * | \%D     | Date and time of message using $date_format and local timezone
 * | \%d     | Date and time of message using $date_format and sender's timezone
 * | \%e     | Current message number in thread
 * | \%E     | Number of messages in current thread
 * | \%f     | Entire from line
 * | \%F     | Like %n, unless from self
 * | \%g     | Message tags (e.g. notmuch tags/imap flags)
 * | \%Gx    | Individual message tag (e.g. notmuch tags/imap flags)
 * | \%H     | Spam attribute(s) of this message
 * | \%I     | Initials of author
 * | \%i     | Message-id
 * | \%J     | Message tags (if present, tree unfolded, and != parent's tags)
 * | \%K     | The list to which the letter was sent (if any; otherwise: empty)
 * | \%L     | Like %F, except 'lists' are displayed first
 * | \%l     | Number of lines in the message
 * | \%M     | Number of hidden messages if the thread is collapsed
 * | \%m     | Number of messages in the mailbox
 * | \%n     | Name of author
 * | \%N     | Score
 * | \%O     | Like %L, except using address instead of name
 * | \%P     | Progress indicator for builtin pager
 * | \%q     | Newsgroup name (if compiled with NNTP support)
 * | \%R     | Comma separated list of Cc: recipients
 * | \%r     | Comma separated list of To: recipients
 * | \%S     | Short message status (e.g., N/O/D/!/r/-)
 * | \%s     | Subject
 * | \%T     | $to_chars
 * | \%t     | 'To:' field (recipients)
 * | \%u     | User (login) name of author
 * | \%v     | First name of author, unless from self
 * | \%W     | Where user is (organization)
 * | \%x     | 'X-Comment-To:' field (if present and compiled with NNTP support)
 * | \%X     | Number of MIME attachments
 * | \%y     | 'X-Label:' field (if present)
 * | \%Y     | 'X-Label:' field (if present, tree unfolded, and != parent's x-label)
 * | \%Z     | Combined message flags
 * | \%zc    | Message crypto flags
 * | \%zs    | Message status flags
 * | \%zt    | Message tag flags
 * | \%(fmt) | Date/time when the message was received
 * | \%[fmt] | Message date/time converted to the local time zone
 * | \%{fmt} | Message date/time converted to sender's time zone
 */
static const char *index_format_str(char *buf, size_t buflen, size_t col, int cols,
                                    char op, const char *src, const char *prec,
                                    const char *if_str, const char *else_str,
                                    unsigned long data, enum FormatFlag flags)
{
  struct HdrFormatInfo *hfi = (struct HdrFormatInfo *) data;
  struct Header *hdr = NULL, *htmp = NULL;
  struct Context *ctx = NULL;
  char fmt[SHORT_STRING], tmp[LONG_STRING], *p, *tags = NULL;
  char *wch = NULL;
  int i;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  int threads = ((Sort & SORT_MASK) == SORT_THREADS);
  int is_index = (flags & MUTT_FORMAT_INDEX);
  size_t colorlen;

  hdr = hfi->hdr;
  ctx = hfi->ctx;

  if (!hdr || !hdr->env)
    return src;
  buf[0] = 0;
  switch (op)
  {
    case 'A':
    case 'I':
      if (op == 'A')
      {
        if (hdr->env->reply_to && hdr->env->reply_to->mailbox)
        {
          colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
          mutt_format_s(buf + colorlen, buflen - colorlen, prec,
                        mutt_addr_for_display(hdr->env->reply_to));
          add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
          break;
        }
      }
      else
      {
        if (mutt_mb_get_initials(mutt_get_name(hdr->env->from), tmp, sizeof(tmp)))
        {
          colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
          mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
          add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
          break;
        }
      }
      /* fallthrough */

    case 'a':
      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
      if (hdr->env->from && hdr->env->from->mailbox)
        mutt_format_s(buf + colorlen, buflen - colorlen, prec,
                      mutt_addr_for_display(hdr->env->from));
      else
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, "");
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'B':
    case 'K':
      if (!first_mailing_list(buf, buflen, hdr->env->to) &&
          !first_mailing_list(buf, buflen, hdr->env->cc))
      {
        buf[0] = 0;
      }
      if (buf[0])
      {
        mutt_str_strfcpy(tmp, buf, sizeof(tmp));
        mutt_format_s(buf, buflen, prec, tmp);
        break;
      }
      if (op == 'K')
      {
        if (optional)
          optional = 0;
        /* break if 'K' returns nothing */
        break;
      }
      /* if 'B' returns nothing */
      /* fallthrough */

    case 'b':
      if (ctx)
      {
        p = strrchr(ctx->path, '/');
        if (p)
          mutt_str_strfcpy(buf, p + 1, buflen);
        else
          mutt_str_strfcpy(buf, ctx->path, buflen);
      }
      else
        mutt_str_strfcpy(buf, "(null)", buflen);
      mutt_str_strfcpy(tmp, buf, sizeof(tmp));
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'c':
      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_SIZE);
      mutt_str_pretty_size(tmp, sizeof(tmp), (long) hdr->content->length);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'C':
      colorlen = add_index_color(fmt, sizeof(fmt), flags, MT_COLOR_INDEX_NUMBER);
      snprintf(fmt + colorlen, sizeof(fmt) - colorlen, "%%%sd", prec);
      add_index_color(fmt + colorlen, sizeof(fmt) - colorlen, flags, MT_COLOR_INDEX);
      snprintf(buf, buflen, fmt, hdr->msgno + 1);
      break;

    case 'd':
    case 'D':
    case '{':
    case '[':
    case '(':
    case '<':
      /* preprocess $date_format to handle %Z */
      {
        const char *cp = NULL;
        struct tm *tm = NULL;
        time_t T;
        int j = 0;

        if (optional && ((op == '[') || (op == '(')))
        {
          char *is = NULL;
          T = time(NULL);
          tm = localtime(&T);
          T -= (op == '(') ? hdr->received : hdr->date_sent;

          is = (char *) prec;
          int invert = 0;
          if (*is == '>')
          {
            invert = 1;
            is++;
          }

          while (*is && (*is != '?'))
          {
            int t = strtol(is, &is, 10);
            /* semi-broken (assuming 30 days in all months) */
            switch (*(is++))
            {
              case 'y':
                if (t > 1)
                {
                  t--;
                  t *= (60 * 60 * 24 * 365);
                }
                t += ((tm->tm_mon * 60 * 60 * 24 * 30) + (tm->tm_mday * 60 * 60 * 24) +
                      (tm->tm_hour * 60 * 60) + (tm->tm_min * 60) + tm->tm_sec);
                break;

              case 'm':
                if (t > 1)
                {
                  t--;
                  t *= (60 * 60 * 24 * 30);
                }
                t += ((tm->tm_mday * 60 * 60 * 24) + (tm->tm_hour * 60 * 60) +
                      (tm->tm_min * 60) + tm->tm_sec);
                break;

              case 'w':
                if (t > 1)
                {
                  t--;
                  t *= (60 * 60 * 24 * 7);
                }
                t += ((tm->tm_wday * 60 * 60 * 24) + (tm->tm_hour * 60 * 60) +
                      (tm->tm_min * 60) + tm->tm_sec);
                break;

              case 'd':
                if (t > 1)
                {
                  t--;
                  t *= (60 * 60 * 24);
                }
                t += ((tm->tm_hour * 60 * 60) + (tm->tm_min * 60) + tm->tm_sec);
                break;

              case 'H':
                if (t > 1)
                {
                  t--;
                  t *= (60 * 60);
                }
                t += ((tm->tm_min * 60) + tm->tm_sec);
                break;

              case 'M':
                if (t > 1)
                {
                  t--;
                  t *= (60);
                }
                t += (tm->tm_sec);
                break;

              default:
                break;
            }
            j += t;
          }

          if (j < 0)
            j *= -1;

          if (((T > j) || (T < (-1 * j))) ^ invert)
            optional = 0;
          break;
        }

        p = buf;

        cp = (op == 'd' || op == 'D') ? (NONULL(DateFormat)) : src;
        int do_locales;
        if (*cp == '!')
        {
          do_locales = 0;
          cp++;
        }
        else
          do_locales = 1;

        size_t len = buflen - 1;
        while (len > 0 && (((op == 'd' || op == 'D') && *cp) ||
                           (op == '{' && *cp != '}') || (op == '[' && *cp != ']') ||
                           (op == '(' && *cp != ')') || (op == '<' && *cp != '>')))
        {
          if (*cp == '%')
          {
            cp++;
            if ((*cp == 'Z' || *cp == 'z') && (op == 'd' || op == '{'))
            {
              if (len >= 5)
              {
                sprintf(p, "%c%02u%02u", hdr->zoccident ? '-' : '+',
                        hdr->zhours, hdr->zminutes);
                p += 5;
                len -= 5;
              }
              else
                break; /* not enough space left */
            }
            else
            {
              if (len >= 2)
              {
                *p++ = '%';
                *p++ = *cp;
                len -= 2;
              }
              else
                break; /* not enough space */
            }
            cp++;
          }
          else
          {
            *p++ = *cp++;
            len--;
          }
        }
        *p = 0;

        if (op == '[' || op == 'D')
          tm = localtime(&hdr->date_sent);
        else if (op == '(')
          tm = localtime(&hdr->received);
        else if (op == '<')
        {
          T = time(NULL);
          tm = localtime(&T);
        }
        else
        {
          /* restore sender's time zone */
          T = hdr->date_sent;
          if (hdr->zoccident)
            T -= (hdr->zhours * 3600 + hdr->zminutes * 60);
          else
            T += (hdr->zhours * 3600 + hdr->zminutes * 60);
          tm = gmtime(&T);
        }

        if (!do_locales)
          setlocale(LC_TIME, "C");
        strftime(tmp, sizeof(tmp), buf, tm);
        if (!do_locales)
          setlocale(LC_TIME, "");

        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_DATE);
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);

        if (len > 0 && op != 'd' && op != 'D') /* Skip ending op */
          src = cp + 1;
      }
      break;

    case 'e':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, mutt_messages_in_thread(ctx, hdr, 1));
      break;

    case 'E':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, mutt_messages_in_thread(ctx, hdr, 0));
      }
      else if (mutt_messages_in_thread(ctx, hdr, 0) <= 1)
        optional = 0;
      break;

    case 'f':
      tmp[0] = 0;
      mutt_addr_write(tmp, sizeof(tmp), hdr->env->from, true);
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'F':
      if (!optional)
      {
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
        make_from(hdr->env, tmp, sizeof(tmp), 0);
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (mutt_addr_is_user(hdr->env->from))
        optional = 0;
      break;

    case 'g':
      tags = driver_tags_get_transformed(&hdr->tags);
      if (!optional)
      {
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_TAGS);
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, NONULL(tags));
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (!tags)
        optional = 0;
      FREE(&tags);
      break;

    case 'G':;
      char format[3];
      char *tag = NULL;

      if (!optional)
      {
        format[0] = op;
        format[1] = *src;
        format[2] = 0;

        tag = mutt_hash_find(TagFormats, format);
        if (tag)
        {
          tags = driver_tags_get_transformed_for(tag, &hdr->tags);
          colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_TAG);
          mutt_format_s(buf + colorlen, buflen - colorlen, prec, NONULL(tags));
          add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
          FREE(&tags);
        }
        src++;
      }
      else
      {
        format[0] = op;
        format[1] = *prec;
        format[2] = 0;

        tag = mutt_hash_find(TagFormats, format);
        if (tag)
        {
          tags = driver_tags_get_transformed_for(tag, &hdr->tags);
          if (!tags)
            optional = 0;
          FREE(&tags);
        }
      }
      break;

    case 'H':
      /* (Hormel) spam score */
      if (optional)
        optional = hdr->env->spam ? 1 : 0;

      if (hdr->env->spam)
        mutt_format_s(buf, buflen, prec, NONULL(hdr->env->spam->data));
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    case 'i':
      mutt_format_s(buf, buflen, prec, hdr->env->message_id ? hdr->env->message_id : "<no.id>");
      break;

    case 'J':
      tags = driver_tags_get_transformed(&hdr->tags);
      if (tags)
      {
        i = 1; /* reduce reuse recycle */
        if (flags & MUTT_FORMAT_TREE)
        {
          char *parent_tags = NULL;
          if (hdr->thread->prev && hdr->thread->prev->message)
            parent_tags =
                driver_tags_get_transformed(&hdr->thread->prev->message->tags);
          if (!parent_tags && hdr->thread->parent && hdr->thread->parent->message)
            parent_tags =
                driver_tags_get_transformed(&hdr->thread->parent->message->tags);
          if (parent_tags && mutt_str_strcasecmp(tags, parent_tags) == 0)
            i = 0;
          FREE(&parent_tags);
        }
      }
      else
        i = 0;

      if (optional)
        optional = i;

      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_TAGS);
      if (i)
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, tags);
      else
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, "");
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      FREE(&tags);
      break;

    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_SIZE);
        snprintf(buf + colorlen, buflen - colorlen, fmt, (int) hdr->lines);
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (hdr->lines <= 0)
        optional = 0;
      break;

    case 'L':
      if (!optional)
      {
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
        make_from(hdr->env, tmp, sizeof(tmp), 1);
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (!check_for_mailing_list(hdr->env->to, NULL, NULL, 0) &&
               !check_for_mailing_list(hdr->env->cc, NULL, NULL, 0))
      {
        optional = 0;
      }
      break;

    case 'm':
      if (ctx)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, ctx->msgcount);
      }
      else
        mutt_str_strfcpy(buf, "(null)", buflen);
      break;

    case 'n':
      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_AUTHOR);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec,
                    mutt_get_name(hdr->env->from));
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'M':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      if (!optional)
      {
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_COLLAPSED);
        if (threads && is_index && hdr->collapsed && hdr->num_hidden > 1)
        {
          snprintf(buf + colorlen, buflen - colorlen, fmt, hdr->num_hidden);
          add_index_color(buf, buflen - colorlen, flags, MT_COLOR_INDEX);
        }
        else if (is_index && threads)
        {
          mutt_format_s(buf + colorlen, buflen - colorlen, prec, " ");
          add_index_color(buf, buflen - colorlen, flags, MT_COLOR_INDEX);
        }
        else
          *buf = '\0';
      }
      else
      {
        if (!(threads && is_index && hdr->collapsed && hdr->num_hidden > 1))
          optional = 0;
      }
      break;

    case 'N':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, hdr->score);
      }
      else
      {
        if (hdr->score == 0)
          optional = 0;
      }
      break;

    case 'O':
      if (!optional)
      {
        make_from_addr(hdr->env, tmp, sizeof(tmp), 1);
        if (!SaveAddress && (p = strpbrk(tmp, "%@")))
          *p = 0;
        mutt_format_s(buf, buflen, prec, tmp);
      }
      else if (!check_for_mailing_list_addr(hdr->env->to, NULL, 0) &&
               !check_for_mailing_list_addr(hdr->env->cc, NULL, 0))
      {
        optional = 0;
      }
      break;

    case 'P':
      mutt_str_strfcpy(buf, NONULL(hfi->pager_progress), buflen);
      break;

#ifdef USE_NNTP
    case 'q':
      mutt_format_s(buf, buflen, prec, hdr->env->newsgroups ? hdr->env->newsgroups : "");
      break;
#endif

    case 'r':
      tmp[0] = 0;
      mutt_addr_write(tmp, sizeof(tmp), hdr->env->to, true);
      if (optional && tmp[0] == '\0')
        optional = 0;
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'R':
      tmp[0] = 0;
      mutt_addr_write(tmp, sizeof(tmp), hdr->env->cc, true);
      if (optional && tmp[0] == '\0')
        optional = 0;
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 's':
    {
      char *subj = NULL;
      if (hdr->env->disp_subj)
        subj = hdr->env->disp_subj;
      else if (SubjectRegexList)
        subj = apply_subject_mods(hdr->env);
      else
        subj = hdr->env->subject;
      if (flags & MUTT_FORMAT_TREE && !hdr->collapsed)
      {
        if (flags & MUTT_FORMAT_FORCESUBJ)
        {
          colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_SUBJECT);
          mutt_format_s(buf + colorlen, buflen - colorlen, "", NONULL(subj));
          add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
          snprintf(tmp, sizeof(tmp), "%s%s", hdr->tree, buf);
          mutt_format_s_tree(buf, buflen, prec, tmp);
        }
        else
          mutt_format_s_tree(buf, buflen, prec, hdr->tree);
      }
      else
      {
        colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_SUBJECT);
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, NONULL(subj));
        add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      }
    }
    break;

    case 'S':
      if (hdr->deleted)
        wch = get_nth_wchar(FlagChars, FlagCharDeleted);
      else if (hdr->attach_del)
        wch = get_nth_wchar(FlagChars, FlagCharDeletedAttach);
      else if (hdr->tagged)
        wch = get_nth_wchar(FlagChars, FlagCharTagged);
      else if (hdr->flagged)
        wch = get_nth_wchar(FlagChars, FlagCharImportant);
      else if (hdr->replied)
        wch = get_nth_wchar(FlagChars, FlagCharReplied);
      else if (hdr->read && (ctx && ctx->msgnotreadyet != hdr->msgno))
        wch = get_nth_wchar(FlagChars, FlagCharSEmpty);
      else if (hdr->old)
        wch = get_nth_wchar(FlagChars, FlagCharOld);
      else
        wch = get_nth_wchar(FlagChars, FlagCharNew);

      snprintf(tmp, sizeof(tmp), "%s", wch);
      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 't':
      tmp[0] = 0;
      if (!check_for_mailing_list(hdr->env->to, "To ", tmp, sizeof(tmp)) &&
          !check_for_mailing_list(hdr->env->cc, "Cc ", tmp, sizeof(tmp)))
      {
        if (hdr->env->to)
          snprintf(tmp, sizeof(tmp), "To %s", mutt_get_name(hdr->env->to));
        else if (hdr->env->cc)
          snprintf(tmp, sizeof(tmp), "Cc %s", mutt_get_name(hdr->env->cc));
      }
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'T':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt,
               (ToChars && ((i = user_is_recipient(hdr))) < ToChars->len) ?
                   ToChars->chars[i] :
                   " ");
      break;

    case 'u':
      if (hdr->env->from && hdr->env->from->mailbox)
      {
        mutt_str_strfcpy(tmp, mutt_addr_for_display(hdr->env->from), sizeof(tmp));
        p = strpbrk(tmp, "%@");
        if (p)
          *p = 0;
      }
      else
        tmp[0] = 0;
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'v':
      if (mutt_addr_is_user(hdr->env->from))
      {
        if (hdr->env->to)
          mutt_format_s(tmp, sizeof(tmp), prec, mutt_get_name(hdr->env->to));
        else if (hdr->env->cc)
          mutt_format_s(tmp, sizeof(tmp), prec, mutt_get_name(hdr->env->cc));
        else
          *tmp = 0;
      }
      else
        mutt_format_s(tmp, sizeof(tmp), prec, mutt_get_name(hdr->env->from));
      p = strpbrk(tmp, " %@");
      if (p)
        *p = 0;
      mutt_format_s(buf, buflen, prec, tmp);
      break;

    case 'W':
      if (!optional)
        mutt_format_s(buf, buflen, prec,
                      hdr->env->organization ? hdr->env->organization : "");
      else if (!hdr->env->organization)
        optional = 0;
      break;

#ifdef USE_NNTP
    case 'x':
      if (!optional)
        mutt_format_s(buf, buflen, prec,
                      hdr->env->x_comment_to ? hdr->env->x_comment_to : "");
      else if (!hdr->env->x_comment_to)
        optional = 0;
      break;
#endif

    case 'X':
    {
      int count = mutt_count_body_parts(ctx, hdr);

      /* The recursion allows messages without depth to return 0. */
      if (optional)
        optional = count != 0;

      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, count);
    }
    break;

    case 'y':
      if (optional)
        optional = hdr->env->x_label ? 1 : 0;

      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_LABEL);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec, NONULL(hdr->env->x_label));
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'Y':
      if (hdr->env->x_label)
      {
        i = 1; /* reduce reuse recycle */
        htmp = NULL;
        if (flags & MUTT_FORMAT_TREE && (hdr->thread->prev && hdr->thread->prev->message &&
                                         hdr->thread->prev->message->env->x_label))
        {
          htmp = hdr->thread->prev->message;
        }
        else if (flags & MUTT_FORMAT_TREE &&
                 (hdr->thread->parent && hdr->thread->parent->message &&
                  hdr->thread->parent->message->env->x_label))
        {
          htmp = hdr->thread->parent->message;
        }
        if (htmp && (mutt_str_strcasecmp(hdr->env->x_label, htmp->env->x_label) == 0))
          i = 0;
      }
      else
        i = 0;

      if (optional)
        optional = i;

      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_LABEL);
      if (i)
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, NONULL(hdr->env->x_label));
      else
        mutt_format_s(buf + colorlen, buflen - colorlen, prec, "");
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'z':
      if (src[0] == 's') /* status: deleted/new/old/replied */
      {
        char *ch = NULL;
        if (hdr->deleted)
          ch = get_nth_wchar(FlagChars, FlagCharDeleted);
        else if (hdr->attach_del)
          ch = get_nth_wchar(FlagChars, FlagCharDeletedAttach);
        else if (threads && thread_is_new(ctx, hdr))
          ch = get_nth_wchar(FlagChars, FlagCharNewThread);
        else if (threads && thread_is_old(ctx, hdr))
          ch = get_nth_wchar(FlagChars, FlagCharOldThread);
        else if (hdr->read && (ctx && (ctx->msgnotreadyet != hdr->msgno)))
        {
          if (hdr->replied)
            ch = get_nth_wchar(FlagChars, FlagCharReplied);
          else
            ch = get_nth_wchar(FlagChars, FlagCharZEmpty);
        }
        else
        {
          if (hdr->old)
            ch = get_nth_wchar(FlagChars, FlagCharOld);
          else
            ch = get_nth_wchar(FlagChars, FlagCharNew);
        }

        snprintf(tmp, sizeof(tmp), "%s", ch);
        src++;
      }
      else if (src[0] == 'c') /* crypto */
      {
        char *ch = NULL;
        if ((WithCrypto != 0) && (hdr->security & GOODSIGN))
          ch = "S";
        else if ((WithCrypto != 0) && (hdr->security & ENCRYPT))
          ch = "P";
        else if ((WithCrypto != 0) && (hdr->security & SIGN))
          ch = "s";
        else if (((WithCrypto & APPLICATION_PGP) != 0) && (hdr->security & PGPKEY))
          ch = "K";
        else
          ch = " ";

        snprintf(tmp, sizeof(tmp), "%s", ch);
        src++;
      }
      else if (src[0] == 't') /* tagged, flagged, recipient */
      {
        char *ch = NULL;
        if (hdr->tagged)
          ch = get_nth_wchar(FlagChars, FlagCharTagged);
        else if (hdr->flagged)
          ch = get_nth_wchar(FlagChars, FlagCharImportant);
        else
          ch = get_nth_wchar(ToChars, user_is_recipient(hdr));

        snprintf(tmp, sizeof(tmp), "%s", ch);
        src++;
      }
      else /* fallthrough */
        break;

      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'Z':
    {
      /* New/Old for threads; replied; New/Old for messages */
      char *first = NULL;
      if (threads && thread_is_new(ctx, hdr))
        first = get_nth_wchar(FlagChars, FlagCharNewThread);
      else if (threads && thread_is_old(ctx, hdr))
        first = get_nth_wchar(FlagChars, FlagCharOldThread);
      else if (hdr->read && (ctx && (ctx->msgnotreadyet != hdr->msgno)))
      {
        if (hdr->replied)
          first = get_nth_wchar(FlagChars, FlagCharReplied);
        else
          first = get_nth_wchar(FlagChars, FlagCharZEmpty);
      }
      else
      {
        if (hdr->old)
          first = get_nth_wchar(FlagChars, FlagCharOld);
        else
          first = get_nth_wchar(FlagChars, FlagCharNew);
      }

      /* Marked for deletion; deleted attachments; crypto */
      char *second = NULL;
      if (hdr->deleted)
        second = get_nth_wchar(FlagChars, FlagCharDeleted);
      else if (hdr->attach_del)
        second = get_nth_wchar(FlagChars, FlagCharDeletedAttach);
      else if ((WithCrypto != 0) && (hdr->security & GOODSIGN))
        second = "S";
      else if ((WithCrypto != 0) && (hdr->security & ENCRYPT))
        second = "P";
      else if ((WithCrypto != 0) && (hdr->security & SIGN))
        second = "s";
      else if (((WithCrypto & APPLICATION_PGP) != 0) && (hdr->security & PGPKEY))
        second = "K";
      else
        second = " ";

      /* Tagged, flagged and recipient flag */
      char *third = NULL;
      if (hdr->tagged)
        third = get_nth_wchar(FlagChars, FlagCharTagged);
      else if (hdr->flagged)
        third = get_nth_wchar(FlagChars, FlagCharImportant);
      else
        third = get_nth_wchar(ToChars, user_is_recipient(hdr));

      snprintf(tmp, sizeof(tmp), "%s%s%s", first, second, third);
    }

      colorlen = add_index_color(buf, buflen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s(buf + colorlen, buflen - colorlen, prec, tmp);
      add_index_color(buf + colorlen, buflen - colorlen, flags, MT_COLOR_INDEX);
      break;

    default:
      snprintf(buf, buflen, "%%%s%c", prec, op);
      break;
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, index_format_str,
                        (unsigned long) hfi, flags);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, index_format_str,
                        (unsigned long) hfi, flags);

  return src;
}

void mutt_make_string_flags(char *buf, size_t buflen, const char *s,
                            struct Context *ctx, struct Header *hdr, enum FormatFlag flags)
{
  struct HdrFormatInfo hfi;

  hfi.hdr = hdr;
  hfi.ctx = ctx;
  hfi.pager_progress = 0;

  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, s,
                      index_format_str, (unsigned long) &hfi, flags);
}

void mutt_make_string_info(char *buf, size_t buflen, int cols, const char *s,
                           struct HdrFormatInfo *hfi, enum FormatFlag flags)
{
  mutt_expando_format(buf, buflen, 0, cols, s, index_format_str, (unsigned long) hfi, flags);
}
