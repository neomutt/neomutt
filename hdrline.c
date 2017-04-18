/**
 * Copyright (C) 1996-2000,2002,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Ian Zimmerman <itz@primate.net>
 *
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
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "mutt.h"
#include "charset.h"
#include "mutt_crypt.h"
#include "mutt_curses.h"
#include "mutt_idna.h"
#include "sort.h"
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

enum
{
  /* Indexing into the Flagchars variable ($flag_chars) */
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

bool mutt_is_mail_list(ADDRESS *addr)
{
  if (!mutt_match_rx_list(addr->mailbox, UnMailLists))
    return mutt_match_rx_list(addr->mailbox, MailLists);
  return false;
}

bool mutt_is_subscribed_list(ADDRESS *addr)
{
  if (!mutt_match_rx_list(addr->mailbox, UnMailLists) &&
      !mutt_match_rx_list(addr->mailbox, UnSubscribedLists))
    return mutt_match_rx_list(addr->mailbox, SubscribedLists);
  return false;
}

/* Search for a mailing list in the list of addresses pointed to by adr.
 * If one is found, print pfx and the name of the list into buf, then
 * return 1.  Otherwise, simply return 0.
 */
static bool check_for_mailing_list(ADDRESS *adr, const char *pfx, char *buf, int buflen)
{
  for (; adr; adr = adr->next)
  {
    if (mutt_is_subscribed_list(adr))
    {
      if (pfx && buf && buflen)
        snprintf(buf, buflen, "%s%s", pfx, mutt_get_name(adr));
      return true;
    }
  }
  return false;
}

/* Search for a mailing list in the list of addresses pointed to by adr.
 * If one is found, print the address of the list into buf, then return 1.
 * Otherwise, simply return 0.
 */
static bool check_for_mailing_list_addr(ADDRESS *adr, char *buf, int buflen)
{
  for (; adr; adr = adr->next)
  {
    if (mutt_is_subscribed_list(adr))
    {
      if (buf && buflen)
        snprintf(buf, buflen, "%s", adr->mailbox);
      return true;
    }
  }
  return false;
}


static bool first_mailing_list(char *buf, size_t buflen, ADDRESS *a)
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
 * Takes the color to embed, the buffer to manipulate and the buffer length as
 * arguments.
 * Returns the number of chars written.
 */
static size_t add_index_color(char *buf, size_t buflen, format_flag flags, char color)
{
  int len;

  /* only add color markers if we are operating on main index entries. */
  if (!(flags & MUTT_FORMAT_INDEX))
    return 0;

  if (color == MT_COLOR_INDEX)
  { /* buf might be uninitialized other cases */
    len = mutt_strlen(buf);
    buf += len;
    buflen -= len;
  }

  if (buflen < 2)
    return 0;

  buf[0] = MUTT_SPECIAL_INDEX;
  buf[1] = color;
  buf[2] = '\0';

  return 2;
}

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
 * @table:  Multi-byte table
 * @index:  Select this character
 * @return: String pointer to the character
 *
 * Extract one multi-byte character from a string table.
 * If the index is invalid, then a space character will be returned.
 * If the character selected is '\n' (Ctrl-M), then "" will be returned.
 */
static char *get_nth_wchar(mbchar_table *table, int index)
{
  if (!table || !table->chars || (index < 0) || (index >= table->len))
    return " ";

  if (table->chars[index][0] == '\r')
    return "";

  return table->chars[index];
}

/**
 * make_from_prefix - Create a prefix for an author field
 * @disp:   Type of field
 * @return: Prefix string (do not free it)
 *
 * If $from_chars is set, pick an appropriate character from it.
 * If not, use the default prefix: "To", "Cc", etc
 */
static const char *make_from_prefix(enum FieldType disp)
{
  /* need 2 bytes at the end, one for the space, another for NUL */
  static char padded[8];
  static const char *long_prefixes[DISP_NUM] = {[DISP_TO] = "To ", [DISP_CC] = "Cc ",
                                                [DISP_BCC] = "Bcc ", [DISP_FROM] = ""};

  if (!Fromchars || !Fromchars->chars || (Fromchars->len == 0))
    return long_prefixes[disp];

  char *pchar = get_nth_wchar(Fromchars, disp);
  if (mutt_strlen(pchar) == 0)
      return "";

  snprintf(padded, sizeof(padded), "%s ", pchar);
  return padded;
}

/**
 * make_from - Generate a From: field (with optional prefix)
 * @env:      Envelope of the email
 * @buf:      Buffer to store the result
 * @len:      Size of the buffer
 * @do_lists: Should we check for mailing lists?
 *
 * Generate the %F or %L field in $index_format.
 * This is the author, or recipient of the email.
 *
 * The field can optionally be prefixed by a character from $from_chars.
 * If $from_chars is not set, the prefix will be, "To", "Cc", etc
 */
static void make_from(ENVELOPE *env, char *buf, size_t len, int do_lists)
{
  if (!env || !buf)
    return;

  bool me;
  enum FieldType disp;
  ADDRESS *name = NULL;

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

static void make_from_addr(ENVELOPE *hdr, char *buf, size_t len, int do_lists)
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
    strfcpy(buf, hdr->from->mailbox, len);
  else
    *buf = 0;
}

static bool user_in_addr(ADDRESS *a)
{
  for (; a; a = a->next)
    if (mutt_addr_is_user(a))
      return true;
  return false;
}

/* Return values:
 * 0: user is not in list
 * 1: user is unique recipient
 * 2: user is in the TO list
 * 3: user is in the CC list
 * 4: user is originator
 * 5: sent to a subscribed mailinglist
 */
static int user_is_recipient(HEADER *h)
{
  if (!h || !h->env)
    return 0;

  ENVELOPE *env = h->env;

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

/**
 * get_initials - Turn a name into initials
 * @name:   String to be converted
 * @buf:    Buffer for the result
 * @buflen: Size of the buffer
 * @return: 1 on Success, 0 on Failure
 *
 * Take a name, e.g. "John F. Kennedy" and reduce it to initials "JFK".
 * The function saves the first character from each word.  Words are delimited
 * by whitespace, or hyphens (so "Jean-Pierre" becomes "JP").
 */
static bool get_initials(const char *name, char *buf, int buflen)
{
  if (!name || !buf)
    return false;

  while (*name)
  {
    /* Char's length in bytes */
    int clen = mutt_charlen(name, NULL);
    if (clen < 1)
      return false;

    /* Ignore punctuation at the beginning of a word */
    if ((clen == 1) && ispunct(*name))
    {
      name++;
      continue;
    }

    if (clen >= buflen)
      return false;

    /* Copy one multibyte character */
    buflen -= clen;
    while (clen--)
      *buf++ = *name++;

    /* Skip to end-of-word */
    for (; *name; name += clen)
    {
      clen = mutt_charlen(name, NULL);
      if (clen < 1)
        return false;
      else if ((clen == 1) && (isspace(*name) || (*name == '-')))
        break;
    }

    /* Skip any whitespace, or hyphens */
    while (*name && (isspace(*name) || (*name == '-')))
      name++;
  }

  *buf = 0;
  return true;
}

static char *apply_subject_mods(ENVELOPE *env)
{
  if (env == NULL)
    return NULL;

  if (SubjectRxList == NULL)
    return env->subject;

  if (env->subject == NULL || *env->subject == '\0')
    return env->disp_subj = NULL;

  env->disp_subj = mutt_apply_replace(NULL, 0, env->subject, SubjectRxList);
  return env->disp_subj;
}


/* %a = address of author
 * %A = reply-to address (if present; otherwise: address of author
 * %b = filename of the originating folder
 * %B = the list to which the letter was sent, or else the folder name (%b).
 * %c = size of message in bytes
 * %C = current message number
 * %d = date and time of message using $date_format and sender's timezone
 * %D = date and time of message using $date_format and local timezone
 * %e = current message number in thread
 * %E = number of messages in current thread
 * %f = entire from line
 * %F = like %n, unless from self
 * %g = newsgroup name (if compiled with NNTP support)
 * %g = message labels (e.g. notmuch tags)
 * %i = message-id
 * %I = initials of author
 * %K = the list to which the letter was sent (if any; otherwise: empty)
 * %l = number of lines in the message
 * %L = like %F, except `lists' are displayed first
 * %m = number of messages in the mailbox
 * %n = name of author
 * %N = score
 * %O = like %L, except using address instead of name
 * %P = progress indicator for builtin pager
 * %r = comma separated list of To: recipients
 * %R = comma separated list of Cc: recipients
 * %s = subject
 * %S = short message status (e.g., N/O/D/!/r/-)
 * %t = `to:' field (recipients)
 * %T = $to_chars
 * %u = user (login) name of author
 * %v = first name of author, unless from self
 * %W = where user is (organization)
 * %x = `x-comment-to:' field (if present and compiled with NNTP support)
 * %X = number of MIME attachments
 * %y = `x-label:' field (if present)
 * %Y = `x-label:' field (if present, tree unfolded, and != parent's x-label)
 * %Z = status flags    */
static const char *hdr_format_str(char *dest, size_t destlen, size_t col, int cols,
                                  char op, const char *src, const char *prefix,
                                  const char *ifstring, const char *elsestring,
                                  unsigned long data, format_flag flags)
{
  struct hdr_format_info *hfi = (struct hdr_format_info *) data;
  HEADER *hdr = NULL, *htmp = NULL;
  CONTEXT *ctx = NULL;
  char fmt[SHORT_STRING], buf2[LONG_STRING], *p = NULL;
  char *wch = NULL;
  int do_locales, i;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  int threads = ((Sort & SORT_MASK) == SORT_THREADS);
  int is_index = (flags & MUTT_FORMAT_INDEX);
#define THREAD_NEW                                                             \
  (threads && hdr->collapsed && hdr->num_hidden > 1 &&                         \
   mutt_thread_contains_unread(ctx, hdr) == 1)
#define THREAD_OLD                                                             \
  (threads && hdr->collapsed && hdr->num_hidden > 1 &&                         \
   mutt_thread_contains_unread(ctx, hdr) == 2)
  size_t len;
  size_t colorlen;

  hdr = hfi->hdr;
  ctx = hfi->ctx;

  if (!hdr || !hdr->env)
    return src;
  dest[0] = 0;
  switch (op)
  {
    case 'A':
    case 'I':
      if (op == 'A')
      {
        if (hdr->env->reply_to && hdr->env->reply_to->mailbox)
        {
          colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
          mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                        mutt_addr_for_display(hdr->env->reply_to));
          add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
          break;
        }
      }
      else
      {
        if (get_initials(mutt_get_name(hdr->env->from), buf2, sizeof(buf2)))
        {
          colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
          mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
          add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
          break;
        }
      }
    /* fall through on failure */

    case 'a':
      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
      if (hdr->env->from && hdr->env->from->mailbox)
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                      mutt_addr_for_display(hdr->env->from));
      else
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, "");
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'B':
    case 'K':
      if (!first_mailing_list(dest, destlen, hdr->env->to) &&
          !first_mailing_list(dest, destlen, hdr->env->cc))
        dest[0] = 0;
      if (dest[0])
      {
        strfcpy(buf2, dest, sizeof(buf2));
        mutt_format_s(dest, destlen, prefix, buf2);
        break;
      }
      if (op == 'K')
      {
        if (optional)
          optional = 0;
        /* break if 'K' returns nothing */
        break;
      }
    /* fall through if 'B' returns nothing */

    case 'b':
      if (ctx)
      {
        if ((p = strrchr(ctx->path, '/')))
          strfcpy(dest, p + 1, destlen);
        else
          strfcpy(dest, ctx->path, destlen);
      }
      else
        strfcpy(dest, "(null)", destlen);
      strfcpy(buf2, dest, sizeof(buf2));
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'c':
      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_SIZE);
      mutt_pretty_size(buf2, sizeof(buf2), (long) hdr->content->length);
      mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'C':
      colorlen = add_index_color(fmt, sizeof(fmt), flags, MT_COLOR_INDEX_NUMBER);
      snprintf(fmt + colorlen, sizeof(fmt) - colorlen, "%%%sd", prefix);
      add_index_color(fmt + colorlen, sizeof(fmt) - colorlen, flags, MT_COLOR_INDEX);
      snprintf(dest, destlen, fmt, hdr->msgno + 1);
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
        int j = 0, invert = 0;

        if (optional && ((op == '[') || (op == '(')))
        {
          char *is = NULL;
          T = time(NULL);
          tm = localtime(&T);
          T -= (op == '(') ? hdr->received : hdr->date_sent;

          is = (char *) prefix;
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

        p = dest;

        cp = (op == 'd' || op == 'D') ? (NONULL(DateFmt)) : src;
        if (*cp == '!')
        {
          do_locales = 0;
          cp++;
        }
        else
          do_locales = 1;

        len = destlen - 1;
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
        strftime(buf2, sizeof(buf2), dest, tm);
        if (!do_locales)
          setlocale(LC_TIME, "");

        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_DATE);
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);

        if (len > 0 && op != 'd' && op != 'D') /* Skip ending op */
          src = cp + 1;
      }
      break;

    case 'e':
      snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
      snprintf(dest, destlen, fmt, mutt_messages_in_thread(ctx, hdr, 1));
      break;

    case 'E':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, mutt_messages_in_thread(ctx, hdr, 0));
      }
      else if (mutt_messages_in_thread(ctx, hdr, 0) <= 1)
        optional = 0;
      break;

    case 'f':
      buf2[0] = 0;
      rfc822_write_address(buf2, sizeof(buf2), hdr->env->from, 1);
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'F':
      if (!optional)
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
        make_from(hdr->env, buf2, sizeof(buf2), 0);
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (mutt_addr_is_user(hdr->env->from))
        optional = 0;
      break;

#ifdef USE_NOTMUCH
    case 'g':
      if (!optional)
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_TAGS);
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                      nm_header_get_tags_transformed(hdr));
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (!nm_header_get_tags_transformed(hdr))
        optional = 0;
      break;
#endif

    case 'H':
      /* (Hormel) spam score */
      if (optional)
        optional = hdr->env->spam ? 1 : 0;

      if (hdr->env->spam)
        mutt_format_s(dest, destlen, prefix, NONULL(hdr->env->spam->data));
      else
        mutt_format_s(dest, destlen, prefix, "");

      break;

#ifdef USE_NNTP
    case 'q':
      mutt_format_s(dest, destlen, prefix, hdr->env->newsgroups ? hdr->env->newsgroups : "");
      break;
#endif

    case 'i':
      mutt_format_s(dest, destlen, prefix,
                    hdr->env->message_id ? hdr->env->message_id : "<no.id>");
      break;

    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_SIZE);
        snprintf(dest + colorlen, destlen - colorlen, fmt, (int) hdr->lines);
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (hdr->lines <= 0)
        optional = 0;
      break;

    case 'L':
      if (!optional)
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
        make_from(hdr->env, buf2, sizeof(buf2), 1);
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
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
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, ctx->msgcount);
      }
      else
        strfcpy(dest, "(null)", destlen);
      break;

    case 'n':
      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
      mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                    mutt_get_name(hdr->env->from));
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'N':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(dest, destlen, fmt, hdr->score);
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
        make_from_addr(hdr->env, buf2, sizeof(buf2), 1);
        if (!option(OPTSAVEADDRESS) && (p = strpbrk(buf2, "%@")))
          *p = 0;
        mutt_format_s(dest, destlen, prefix, buf2);
      }
      else if (!check_for_mailing_list_addr(hdr->env->to, NULL, 0) &&
               !check_for_mailing_list_addr(hdr->env->cc, NULL, 0))
      {
        optional = 0;
      }
      break;

    case 'M':
      snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
      if (!optional)
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_COLLAPSED);
        if (threads && is_index && hdr->collapsed && hdr->num_hidden > 1)
        {
          snprintf(dest + colorlen, destlen - colorlen, fmt, hdr->num_hidden);
          add_index_color(dest, destlen - colorlen, flags, MT_COLOR_INDEX);
        }
        else if (is_index && threads)
        {
          mutt_format_s(dest + colorlen, destlen - colorlen, prefix, " ");
          add_index_color(dest, destlen - colorlen, flags, MT_COLOR_INDEX);
        }
        else
          *dest = '\0';
      }
      else
      {
        if (!(threads && is_index && hdr->collapsed && hdr->num_hidden > 1))
          optional = 0;
      }
      break;

    case 'P':
      strfcpy(dest, NONULL(hfi->pager_progress), destlen);
      break;

    case 'r':
      buf2[0] = 0;
      rfc822_write_address(buf2, sizeof(buf2), hdr->env->to, 1);
      if (optional && buf2[0] == '\0')
        optional = 0;
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'R':
      buf2[0] = 0;
      rfc822_write_address(buf2, sizeof(buf2), hdr->env->cc, 1);
      if (optional && buf2[0] == '\0')
        optional = 0;
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 's':
    {
      char *subj = NULL;
      if (hdr->env->disp_subj)
        subj = hdr->env->disp_subj;
      else if (SubjectRxList)
        subj = apply_subject_mods(hdr->env);
      else
        subj = hdr->env->subject;
      if (flags & MUTT_FORMAT_TREE && !hdr->collapsed)
      {
        if (flags & MUTT_FORMAT_FORCESUBJ)
        {
          colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_SUBJECT);
          mutt_format_s(dest + colorlen, destlen - colorlen, "", NONULL(subj));
          add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
          snprintf(buf2, sizeof(buf2), "%s%s", hdr->tree, dest);
          mutt_format_s_tree(dest, destlen, prefix, buf2);
        }
        else
          mutt_format_s_tree(dest, destlen, prefix, hdr->tree);
      }
      else
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_SUBJECT);
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, NONULL(subj));
        add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
    }
    break;

    case 'S':
      if (hdr->deleted)
        wch = get_nth_wchar(Flagchars, FlagCharDeleted);
      else if (hdr->attach_del)
        wch = get_nth_wchar(Flagchars, FlagCharDeletedAttach);
      else if (hdr->tagged)
        wch = get_nth_wchar(Flagchars, FlagCharTagged);
      else if (hdr->flagged)
        wch = get_nth_wchar(Flagchars, FlagCharImportant);
      else if (hdr->replied)
        wch = get_nth_wchar(Flagchars, FlagCharReplied);
      else if (hdr->read && (ctx && ctx->msgnotreadyet != hdr->msgno))
        wch = get_nth_wchar(Flagchars, FlagCharSEmpty);
      else if (hdr->old)
        wch = get_nth_wchar(Flagchars, FlagCharOld);
      else
        wch = get_nth_wchar(Flagchars, FlagCharNew);

      snprintf(buf2, sizeof(buf2), "%s", wch);
      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 't':
      buf2[0] = 0;
      if (!check_for_mailing_list(hdr->env->to, "To ", buf2, sizeof(buf2)) &&
          !check_for_mailing_list(hdr->env->cc, "Cc ", buf2, sizeof(buf2)))
      {
        if (hdr->env->to)
          snprintf(buf2, sizeof(buf2), "To %s", mutt_get_name(hdr->env->to));
        else if (hdr->env->cc)
          snprintf(buf2, sizeof(buf2), "Cc %s", mutt_get_name(hdr->env->cc));
      }
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'T':
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(dest, destlen, fmt,
               (Tochars && ((i = user_is_recipient(hdr))) < Tochars->len) ?
                   Tochars->chars[i] :
                   " ");
      break;

    case 'u':
      if (hdr->env->from && hdr->env->from->mailbox)
      {
        strfcpy(buf2, mutt_addr_for_display(hdr->env->from), sizeof(buf2));
        if ((p = strpbrk(buf2, "%@")))
          *p = 0;
      }
      else
        buf2[0] = 0;
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'v':
      if (mutt_addr_is_user(hdr->env->from))
      {
        if (hdr->env->to)
          mutt_format_s(buf2, sizeof(buf2), prefix, mutt_get_name(hdr->env->to));
        else if (hdr->env->cc)
          mutt_format_s(buf2, sizeof(buf2), prefix, mutt_get_name(hdr->env->cc));
        else
          *buf2 = 0;
      }
      else
        mutt_format_s(buf2, sizeof(buf2), prefix, mutt_get_name(hdr->env->from));
      if ((p = strpbrk(buf2, " %@")))
        *p = 0;
      mutt_format_s(dest, destlen, prefix, buf2);
      break;

    case 'W':
      if (!optional)
        mutt_format_s(dest, destlen, prefix,
                      hdr->env->organization ? hdr->env->organization : "");
      else if (!hdr->env->organization)
        optional = 0;
      break;

#ifdef USE_NNTP
    case 'x':
      if (!optional)
        mutt_format_s(dest, destlen, prefix,
                      hdr->env->x_comment_to ? hdr->env->x_comment_to : "");
      else if (!hdr->env->x_comment_to)
        optional = 0;
      break;
#endif

    case 'Z':
    {
      /* New/Old for threads; replied; New/Old for messages */
      char *first = NULL;
      if (THREAD_NEW)
        first = get_nth_wchar(Flagchars, FlagCharNewThread);
      else if (THREAD_OLD)
        first = get_nth_wchar(Flagchars, FlagCharOldThread);
      else if (hdr->read && (ctx && (ctx->msgnotreadyet != hdr->msgno)))
      {
        if (hdr->replied)
          first = get_nth_wchar(Flagchars, FlagCharReplied);
        else
          first = get_nth_wchar(Flagchars, FlagCharZEmpty);
      }
      else
      {
        if (hdr->old)
          first = get_nth_wchar(Flagchars, FlagCharOld);
        else
          first = get_nth_wchar(Flagchars, FlagCharNew);
      }

      /* Marked for deletion; deleted attachments; crypto */
      char *second = NULL;
      if (hdr->deleted)
        second = get_nth_wchar(Flagchars, FlagCharDeleted);
      else if (hdr->attach_del)
        second = get_nth_wchar(Flagchars, FlagCharDeletedAttach);
      else if (WithCrypto && (hdr->security & GOODSIGN))
        second = "S";
      else if (WithCrypto && (hdr->security & ENCRYPT))
        second = "P";
      else if (WithCrypto && (hdr->security & SIGN))
        second = "s";
      else if ((WithCrypto & APPLICATION_PGP) && (hdr->security & PGPKEY))
        second = "K";
      else
        second = " ";

      /* Tagged, flagged and recipient flag */
      char *third = NULL;
      if (hdr->tagged)
        third = get_nth_wchar(Flagchars, FlagCharTagged);
      else if (hdr->flagged)
        third = get_nth_wchar(Flagchars, FlagCharImportant);
      else
        third = get_nth_wchar(Tochars, user_is_recipient(hdr));

      snprintf(buf2, sizeof(buf2), "%s%s%s", first, second, third);
    }

      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s(dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'X':
    {
      int count = mutt_count_body_parts(ctx, hdr);

      /* The recursion allows messages without depth to return 0. */
      if (optional)
        optional = count != 0;

      snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
      snprintf(dest, destlen, fmt, count);
    }
    break;

    case 'y':
      if (optional)
        optional = hdr->env->x_label ? 1 : 0;

      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_LABEL);
      mutt_format_s(dest + colorlen, destlen - colorlen, prefix, NONULL(hdr->env->x_label));
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'Y':
      if (hdr->env->x_label)
      {
        i = 1; /* reduce reuse recycle */
        htmp = NULL;
        if (flags & MUTT_FORMAT_TREE && (hdr->thread->prev && hdr->thread->prev->message &&
                                         hdr->thread->prev->message->env->x_label))
          htmp = hdr->thread->prev->message;
        else if (flags & MUTT_FORMAT_TREE &&
                 (hdr->thread->parent && hdr->thread->parent->message &&
                  hdr->thread->parent->message->env->x_label))
          htmp = hdr->thread->parent->message;
        if (htmp && (mutt_strcasecmp(hdr->env->x_label, htmp->env->x_label) == 0))
          i = 0;
      }
      else
        i = 0;

      if (optional)
        optional = i;

      colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_LABEL);
      if (i)
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                      NONULL(hdr->env->x_label));
      else
        mutt_format_s(dest + colorlen, destlen - colorlen, prefix, "");
      add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);

      break;

#ifdef USE_NOTMUCH
    case 'G':
    {
      char *tag_transformed = NULL;
      char format[3];
      char *tag = NULL;

      if (!optional)
      {
        format[0] = op;
        format[1] = *src;
        format[2] = 0;

        tag = hash_find(TagFormats, format);
        if (tag != NULL)
        {
          tag_transformed = nm_header_get_tag_transformed(tag, hdr);

          colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_TAG);
          mutt_format_s(dest + colorlen, destlen - colorlen, prefix,
                        (tag_transformed) ? tag_transformed : "");
          add_index_color(dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
        }

        src++;
      }
      else
      {
        format[0] = op;
        format[1] = *prefix;
        format[2] = 0;

        tag = hash_find(TagFormats, format);
        if (tag != NULL)
          if (nm_header_get_tag_transformed(tag, hdr) == NULL)
            optional = 0;
      }

      break;
    }
#endif

    default:
      snprintf(dest, destlen, "%%%s%c", prefix, op);
      break;
  }

  if (optional)
    mutt_FormatString(dest, destlen, col, cols, ifstring, hdr_format_str,
                      (unsigned long) hfi, flags);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_FormatString(dest, destlen, col, cols, elsestring, hdr_format_str,
                      (unsigned long) hfi, flags);

  return src;
#undef THREAD_NEW
#undef THREAD_OLD
}

void _mutt_make_string(char *dest, size_t destlen, const char *s, CONTEXT *ctx,
                       HEADER *hdr, format_flag flags)
{
  struct hdr_format_info hfi;

  hfi.hdr = hdr;
  hfi.ctx = ctx;
  hfi.pager_progress = 0;

  mutt_FormatString(dest, destlen, 0, MuttIndexWindow->cols, s, hdr_format_str,
                    (unsigned long) &hfi, flags);
}

void mutt_make_string_info(char *dst, size_t dstlen, int cols, const char *s,
                           struct hdr_format_info *hfi, format_flag flags)
{
  mutt_FormatString(dst, dstlen, 0, cols, s, hdr_format_str, (unsigned long) hfi, flags);
}
