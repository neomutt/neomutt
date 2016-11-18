/*
 * Copyright (C) 1996-2000,2002,2007 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "sort.h"
#include "charset.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

int mutt_is_mail_list (ADDRESS *addr)
{
  if (!mutt_match_rx_list (addr->mailbox, UnMailLists))
    return mutt_match_rx_list (addr->mailbox, MailLists);
  return 0;
}

int mutt_is_subscribed_list (ADDRESS *addr)
{
  if (!mutt_match_rx_list (addr->mailbox, UnMailLists)
      && !mutt_match_rx_list (addr->mailbox, UnSubscribedLists))
    return mutt_match_rx_list (addr->mailbox, SubscribedLists);
  return 0;
}

/* Search for a mailing list in the list of addresses pointed to by adr.
 * If one is found, print pfx and the name of the list into buf, then
 * return 1.  Otherwise, simply return 0.
 */
static int
check_for_mailing_list (ADDRESS *adr, char *pfx, char *buf, int buflen)
{
  for (; adr; adr = adr->next)
  {
    if (mutt_is_subscribed_list (adr))
    {
      if (pfx && buf && buflen)
	snprintf (buf, buflen, "%s%s", pfx, mutt_get_name (adr));
      return 1;
    }
  }
  return 0;
}

/* Search for a mailing list in the list of addresses pointed to by adr.
 * If one is found, print the address of the list into buf, then return 1.
 * Otherwise, simply return 0.
 */
static int
check_for_mailing_list_addr (ADDRESS *adr, char *buf, int buflen)
{
  for (; adr; adr = adr->next)
  {
    if (mutt_is_subscribed_list (adr))
    {
      if (buf && buflen)
	snprintf (buf, buflen, "%s", adr->mailbox);
      return 1;
    }
  }
  return 0;
}


static int first_mailing_list (char *buf, size_t buflen, ADDRESS *a)
{
  for (; a; a = a->next)
  {
    if (mutt_is_subscribed_list (a))
    {
      mutt_save_path (buf, buflen, a);
      return 1;
    }
  }
  return 0;
}

/**
 * Takes the color to embed, the buffer to manipulate and the buffer length as
 * arguments.
 * Returns the number of chars written.
 */
static size_t
add_index_color (char *buf, size_t buflen, format_flag flags, char color)
{
  int len;

  /* only add color markers if we are operating on main index entries. */
  if (!(flags & MUTT_FORMAT_INDEX))
    return 0;

  if (color == MT_COLOR_INDEX) { /* buf might be uninitialized other cases */
    len = mutt_strlen (buf);
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

static void make_from (ENVELOPE *hdr, char *buf, size_t len, int do_lists)
{
  int me;

  me = mutt_addr_is_user (hdr->from);

  if (do_lists || me)
  {
    if (check_for_mailing_list (hdr->to, "To ", buf, len))
      return;
    if (check_for_mailing_list (hdr->cc, "Cc ", buf, len))
      return;
  }

  if (me && hdr->to)
    snprintf (buf, len, "To %s", mutt_get_name (hdr->to));
  else if (me && hdr->cc)
    snprintf (buf, len, "Cc %s", mutt_get_name (hdr->cc));
  else if (me && hdr->bcc)
    snprintf (buf, len, "Bcc %s", mutt_get_name (hdr->bcc));
  else if (hdr->from)
    strfcpy (buf, mutt_get_name (hdr->from), len);
  else
    *buf = 0;
}

static void make_from_addr (ENVELOPE *hdr, char *buf, size_t len, int do_lists)
{
  int me;

  me = mutt_addr_is_user (hdr->from);

  if (do_lists || me)
  {
    if (check_for_mailing_list_addr (hdr->to, buf, len))
      return;
    if (check_for_mailing_list_addr (hdr->cc, buf, len))
      return;
  }

  if (me && hdr->to)
    snprintf (buf, len, "%s", hdr->to->mailbox);
  else if (me && hdr->cc)
    snprintf (buf, len, "%s", hdr->cc->mailbox);
  else if (hdr->from)
    strfcpy (buf, hdr->from->mailbox, len);
  else
    *buf = 0;
}

static int user_in_addr (ADDRESS *a)
{
  for (; a; a = a->next)
    if (mutt_addr_is_user (a))
      return 1;
  return 0;
}

/* Return values:
 * 0: user is not in list
 * 1: user is unique recipient
 * 2: user is in the TO list
 * 3: user is in the CC list
 * 4: user is originator
 * 5: sent to a subscribed mailinglist
 */
int mutt_user_is_recipient (HEADER *h)
{
  if (!h || !h->env)
    return 0;

  ENVELOPE *env = h->env;

  if(!h->recip_valid)
  {
    h->recip_valid = 1;
    
    if (mutt_addr_is_user (env->from))
      h->recipient = 4;
    else if (user_in_addr (env->to))
    {
      if (env->to->next || env->cc)
	h->recipient = 2; /* non-unique recipient */
      else
	h->recipient = 1; /* unique recipient */
    }
    else if (user_in_addr (env->cc))
      h->recipient = 3;
    else if (check_for_mailing_list (env->to, NULL, NULL, 0))
      h->recipient = 5;
    else if (check_for_mailing_list (env->cc, NULL, NULL, 0))
      h->recipient = 5;
    else
      h->recipient = 0;
  }
  
  return h->recipient;
}

/* %a = address of author
 * %A = reply-to address (if present; otherwise: address of author
 * %b = filename of the originating folder
 * %B = the list to which the letter was sent
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
 * %Z = status flags	*/

static const char *
hdr_format_str (char *dest,
		size_t destlen,
		size_t col,
                int cols,
		char op,
		const char *src,
		const char *prefix,
		const char *ifstring,
		const char *elsestring,
		unsigned long data,
		format_flag flags)
{
  struct hdr_format_info *hfi = (struct hdr_format_info *) data;
  HEADER *hdr, *htmp;
  CONTEXT *ctx;
  char fmt[SHORT_STRING], buf2[LONG_STRING], ch, *p;
  int do_locales, i;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  int threads = ((Sort & SORT_MASK) == SORT_THREADS);
  int is_index = (flags & MUTT_FORMAT_INDEX);
#define THREAD_NEW (threads && hdr->collapsed && hdr->num_hidden > 1 && mutt_thread_contains_unread (ctx, hdr) == 1)
#define THREAD_OLD (threads && hdr->collapsed && hdr->num_hidden > 1 && mutt_thread_contains_unread (ctx, hdr) == 2)
  size_t len;
  size_t colorlen;

  hdr = hfi->hdr;
  ctx = hfi->ctx;

  dest[0] = 0;
  switch (op)
  {
    case 'A':
      if(hdr->env->reply_to && hdr->env->reply_to->mailbox)
      {
        colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
        mutt_format_s (dest + colorlen, destlen - colorlen, prefix, mutt_addr_for_display (hdr->env->reply_to));
        add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
	break;
      }
      /* fall through if 'A' returns nothing */

    case 'a':
      colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
      if(hdr->env->from && hdr->env->from->mailbox)
        mutt_format_s (dest + colorlen, destlen - colorlen, prefix, mutt_addr_for_display (hdr->env->from));
      else
        mutt_format_s (dest + colorlen, destlen - colorlen, prefix, "");
      add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'B':
      if (!first_mailing_list (dest, destlen, hdr->env->to) &&
	  !first_mailing_list (dest, destlen, hdr->env->cc))
	dest[0] = 0;
      if (dest[0])
      {
	strfcpy (buf2, dest, sizeof(buf2));
	mutt_format_s (dest, destlen, prefix, buf2);
	break;
      }
      /* fall through if 'B' returns nothing */

    case 'b':
      if(ctx)
      {
	if ((p = strrchr (ctx->path, '/')))
	  strfcpy (dest, p + 1, destlen);
	else
	  strfcpy (dest, ctx->path, destlen);
      }
      else 
	strfcpy(dest, "(null)", destlen);
      strfcpy (buf2, dest, sizeof(buf2));
      mutt_format_s (dest, destlen, prefix, buf2);
      break;
    
    case 'c':
      colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_SIZE);
      mutt_pretty_size (buf2, sizeof (buf2), (long) hdr->content->length);
      mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'C':
      colorlen = add_index_color (fmt, sizeof (fmt), flags, MT_COLOR_INDEX_NUMBER);
      snprintf (fmt + colorlen, sizeof (fmt) - colorlen, "%%%sd", prefix);
      add_index_color (fmt + colorlen, sizeof (fmt) - colorlen, flags, MT_COLOR_INDEX);
      snprintf (dest, destlen, fmt, hdr->msgno + 1);
      break;

    case 'd':
    case 'D':
    case '{':
    case '[':
    case '(':
    case '<':

      /* preprocess $date_format to handle %Z */
      {
	const char *cp;
	struct tm *tm; 
	time_t T;
	int i = 0, invert = 0;

	if (optional && ((op == '[') || (op == '('))) {
	  char *is;
	  T = time (NULL);
	  tm = localtime (&T);
	  T -= (op == '(') ? hdr->received : hdr->date_sent;

	  is = (char *) prefix;
	  if (*is == '>') {
	    invert = 1;
	    is++;
	  }

	  while (*is && (*is != '?')) {
	    int t = strtol (is, &is, 10);
	    /* semi-broken (assuming 30 days in all months) */
	    switch (*(is++)) {
	      case 'y':
		if (t > 1) {
		  t--;
		  t *= (60 * 60 * 24 * 365);
		}
		t += ((tm->tm_mon  * 60 * 60 * 24 * 30) +
		      (tm->tm_mday * 60 * 60 * 24) +
		      (tm->tm_hour * 60 * 60) +
		      (tm->tm_min  * 60) +
		       tm->tm_sec);
		break;

	      case 'm':
		if (t > 1) {
		  t--;
		  t *= (60 * 60 * 24 * 30);
		}
		t += ((tm->tm_mday * 60 * 60 * 24) +
		      (tm->tm_hour * 60 * 60) +
		      (tm->tm_min  * 60) +
		      tm->tm_sec);
		break;

	      case 'w':
		if (t > 1) {
		  t--;
		  t *= (60 * 60 * 24 * 7);
		}
		t += ((tm->tm_wday * 60 * 60 * 24) +
		      (tm->tm_hour * 60 * 60) +
		      (tm->tm_min  * 60) +
		       tm->tm_sec);
		break;

	      case 'd':
		if (t > 1) {
		  t--;
		  t *= (60 * 60 * 24);
		}
		t += ((tm->tm_hour * 60 * 60) +
		      (tm->tm_min  * 60) +
		       tm->tm_sec);
		break;

	      case 'H':
		if (t > 1) {
		  t--;
		  t *= (60 * 60);
		}
		t += ((tm->tm_min * 60) +
		       tm->tm_sec);
		break;

	      case 'M':
		if (t > 1) {
		  t--;
		  t *= (60);
		}
		t += (tm->tm_sec);
		break;

	      default:
		break;
	    }
	    i += t;
	  }

	  if (i < 0)
	    i *= -1;

	  if (((T > i) || (T < (-1*i))) ^ invert)
	    optional = 0;
	  break;
	}

	p = dest;

	cp = (op == 'd' || op == 'D') ? (NONULL (DateFmt)) : src;
	if (*cp == '!')
	{
	  do_locales = 0;
	  cp++;
	}
	else
	  do_locales = 1;

	len = destlen - 1;
	while (len > 0 && (((op == 'd' || op == 'D') && *cp) ||
			   (op == '{' && *cp != '}') || 
			   (op == '[' && *cp != ']') ||
			   (op == '(' && *cp != ')') ||
			   (op == '<' && *cp != '>')))
	{
	  if (*cp == '%')
	  {
	    cp++;
	    if ((*cp == 'Z' || *cp == 'z') && (op == 'd' || op == '{'))
	    {
	      if (len >= 5)
	      {
		sprintf (p, "%c%02u%02u", hdr->zoccident ? '-' : '+',
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
	  tm = localtime (&hdr->date_sent);
	else if (op == '(')
	  tm = localtime (&hdr->received);
	else if (op == '<')
	{
	  T = time (NULL);
	  tm = localtime (&T);
	}
	else
	{
	  /* restore sender's time zone */
	  T = hdr->date_sent;
	  if (hdr->zoccident)
	    T -= (hdr->zhours * 3600 + hdr->zminutes * 60);
	  else
	    T += (hdr->zhours * 3600 + hdr->zminutes * 60);
	  tm = gmtime (&T);
	}

        if (!do_locales)
          setlocale (LC_TIME, "C");
        strftime (buf2, sizeof (buf2), dest, tm);
        if (!do_locales)
          setlocale (LC_TIME, "");

	colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_DATE);
	mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
	add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);

	if (len > 0 && op != 'd' && op != 'D') /* Skip ending op */
	  src = cp + 1;
      }
      break;

    case 'e':
      snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
      snprintf (dest, destlen, fmt, mutt_messages_in_thread(ctx, hdr, 1));
      break;

    case 'E':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, mutt_messages_in_thread(ctx, hdr, 0));
      }
      else if (mutt_messages_in_thread(ctx, hdr, 0) <= 1)
	optional = 0;
      break;

    case 'f':
      buf2[0] = 0;
      rfc822_write_address (buf2, sizeof (buf2), hdr->env->from, 1);
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'F':
      if (!optional)
      {
        colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
        make_from (hdr->env, buf2, sizeof (buf2), 0);
        mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
        add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (mutt_addr_is_user (hdr->env->from))
        optional = 0;
      break;

#ifdef USE_NOTMUCH
    case 'g':
      if (!optional)
      {
        colorlen = add_index_color(dest, destlen, flags, MT_COLOR_INDEX_TAGS);
        mutt_format_s (dest+colorlen, destlen-colorlen, prefix, nm_header_get_tags_transformed(hdr));
        add_index_color(dest+colorlen, destlen-colorlen, flags, MT_COLOR_INDEX);
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
         mutt_format_s (dest, destlen, prefix, NONULL (hdr->env->spam->data));
       else
         mutt_format_s (dest, destlen, prefix, "");

      break;

#ifdef USE_NNTP
    case 'q':
      mutt_format_s (dest, destlen, prefix, hdr->env->newsgroups ? hdr->env->newsgroups : "");
      break;
#endif

    case 'i':
      mutt_format_s (dest, destlen, prefix, hdr->env->message_id ? hdr->env->message_id : "<no.id>");
      break;

    case 'I':
      {
	int iflag = FALSE;
	int j = 0;

	for (i = 0; hdr->env->from && hdr->env->from->personal &&
		    hdr->env->from->personal[i] && (j < (SHORT_STRING - 1)); i++) {
	  if (isalpha ((int) hdr->env->from->personal[i])) {
	    if (!iflag) {
	      buf2[j++] = hdr->env->from->personal[i];
	      iflag = TRUE;
	    }
	  } else {
	    iflag = FALSE;
	  }
	}

	buf2[j] = '\0';
      }
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'l':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_SIZE);
	snprintf (dest + colorlen, destlen - colorlen, fmt, (int) hdr->lines);
	add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (hdr->lines <= 0)
        optional = 0;
      break;

    case 'L':
      if (!optional)
      {
	colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
	make_from (hdr->env, buf2, sizeof (buf2), 1);
	mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
	add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      else if (!check_for_mailing_list (hdr->env->to, NULL, NULL, 0) &&
	       !check_for_mailing_list (hdr->env->cc, NULL, NULL, 0))
      {
	optional = 0;
      }
      break;

    case 'm':
      if(ctx)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, ctx->msgcount);
      }
      else
	strfcpy(dest, "(null)", destlen);
      break;

    case 'n':
      colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_AUTHOR);
      mutt_format_s (dest + colorlen, destlen - colorlen, prefix, mutt_get_name (hdr->env->from));
      add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'N':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, hdr->score);
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
	make_from_addr (hdr->env, buf2, sizeof (buf2), 1);
	if (!option (OPTSAVEADDRESS) && (p = strpbrk (buf2, "%@")))
	  *p = 0;
	mutt_format_s (dest, destlen, prefix, buf2);
      }
      else if (!check_for_mailing_list_addr (hdr->env->to, NULL, 0) &&
	       !check_for_mailing_list_addr (hdr->env->cc, NULL, 0))
      {
	optional = 0;
      }
      break;

    case 'M':
      snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
      if (!optional)
      {
	colorlen = add_index_color (dest, destlen, flags,
				   MT_COLOR_INDEX_COLLAPSED);
	if (threads && is_index && hdr->collapsed && hdr->num_hidden > 1)
	{
	  snprintf (dest + colorlen, destlen - colorlen, fmt, hdr->num_hidden);
	  add_index_color (dest, destlen - colorlen, flags, MT_COLOR_INDEX);
	}
	else if (is_index && threads)
	{
	  mutt_format_s (dest + colorlen, destlen - colorlen, prefix, " ");
	  add_index_color (dest, destlen - colorlen, flags, MT_COLOR_INDEX);
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
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'R':
      buf2[0] = 0;
      rfc822_write_address(buf2, sizeof(buf2), hdr->env->cc, 1);
      if (optional && buf2[0] == '\0')
        optional = 0;
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 's':
      
      if (flags & MUTT_FORMAT_TREE && !hdr->collapsed)
      {
	if (flags & MUTT_FORMAT_FORCESUBJ)
	{
	  colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_SUBJECT);
	  mutt_format_s (dest + colorlen, destlen - colorlen, "", NONULL (hdr->env->subject));
	  add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
	  snprintf (buf2, sizeof (buf2), "%s%s", hdr->tree, dest);
	  mutt_format_s_tree (dest, destlen, prefix, buf2);
	}
	else
	  mutt_format_s_tree (dest, destlen, prefix, hdr->tree);
      }
      else
      {
	colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_SUBJECT);
	mutt_format_s (dest + colorlen, destlen - colorlen, prefix, NONULL (hdr->env->subject));
	add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }
      break;

    case 'S':
      if (hdr->deleted)
	ch = 'D';
      else if (hdr->attach_del)
	ch = 'd';
      else if (hdr->tagged)
	ch = '*';
      else if (hdr->flagged)
	ch = '!';
      else if (hdr->replied)
	ch = 'r';
      else if (hdr->read && (ctx && ctx->msgnotreadyet != hdr->msgno))
	ch = '-';
      else if (hdr->old)
	ch = 'O';
      else
	ch = 'N';

      /* FOO - this is probably unsafe, but we are not likely to have such
	 a short string passed into this routine */
      buf2[0] = ch;
      buf2[1] = 0;
      colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 't':
      buf2[0] = 0;
      if (!check_for_mailing_list (hdr->env->to, "To ", buf2, sizeof (buf2)) &&
	  !check_for_mailing_list (hdr->env->cc, "Cc ", buf2, sizeof (buf2)))
      {
	if (hdr->env->to)
	  snprintf (buf2, sizeof (buf2), "To %s", mutt_get_name (hdr->env->to));
	else if (hdr->env->cc)
	  snprintf (buf2, sizeof (buf2), "Cc %s", mutt_get_name (hdr->env->cc));
      }
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'T':
      snprintf (fmt, sizeof (fmt), "%%%sc", prefix);
      snprintf (dest, destlen, fmt,
		(Tochars && ((i = mutt_user_is_recipient (hdr))) < mutt_strlen (Tochars)) ? Tochars[i] : ' ');
      break;

    case 'u':
      if (hdr->env->from && hdr->env->from->mailbox)
      {
	strfcpy (buf2, mutt_addr_for_display (hdr->env->from), sizeof (buf2));
	if ((p = strpbrk (buf2, "%@")))
	  *p = 0;
      }
      else
	buf2[0] = 0;
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'v':
      if (mutt_addr_is_user (hdr->env->from)) 
      {
	if (hdr->env->to)
	  mutt_format_s (buf2, sizeof (buf2), prefix, mutt_get_name (hdr->env->to));
	else if (hdr->env->cc)
	  mutt_format_s (buf2, sizeof (buf2), prefix, mutt_get_name (hdr->env->cc));
	else
	  *buf2 = 0;
      }
      else
	mutt_format_s (buf2, sizeof (buf2), prefix, mutt_get_name (hdr->env->from));
      if ((p = strpbrk (buf2, " %@")))
	*p = 0;
      mutt_format_s (dest, destlen, prefix, buf2);
      break;

    case 'W':
      if (!optional)
	mutt_format_s (dest, destlen, prefix, hdr->env->organization ? hdr->env->organization : "");
      else if (!hdr->env->organization)
	optional = 0;
      break;

#ifdef USE_NNTP
    case 'x':
      if (!optional)
	mutt_format_s (dest, destlen, prefix, hdr->env->x_comment_to ? hdr->env->x_comment_to : "");
      else if (!hdr->env->x_comment_to)
	optional = 0;
      break;
#endif

    case 'Z':
    
      ch = ' ';

      if (WithCrypto && hdr->security & GOODSIGN)
        ch = 'S';
      else if (WithCrypto && hdr->security & ENCRYPT)
      	ch = 'P';
      else if (WithCrypto && hdr->security & SIGN)
        ch = 's';
      else if ((WithCrypto & APPLICATION_PGP) && hdr->security & PGPKEY)
        ch = 'K';

      snprintf (buf2, sizeof (buf2),
		"%c%c%c", (THREAD_NEW ? 'n' : (THREAD_OLD ? 'o' : 
		((hdr->read && (ctx && ctx->msgnotreadyet != hdr->msgno))
		? (hdr->replied ? 'r' : ' ') : (hdr->old ? 'O' : 'N')))),
		hdr->deleted ? 'D' : (hdr->attach_del ? 'd' : ch),
		hdr->tagged ? '*' :
		(hdr->flagged ? '!' :
		 (Tochars && ((i = mutt_user_is_recipient (hdr)) < mutt_strlen (Tochars)) ? Tochars[i] : ' ')));
      colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_FLAGS);
      mutt_format_s (dest + colorlen, destlen - colorlen, prefix, buf2);
      add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      break;

    case 'X':
      {
	int count = mutt_count_body_parts (ctx, hdr);

	/* The recursion allows messages without depth to return 0. */
	if (optional)
          optional = count != 0;

        snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
        snprintf (dest, destlen, fmt, count);
      }
      break;

     case 'y':
       if (optional)
	 optional = hdr->env->labels ? 1 : 0;

       colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_LABEL);
       mutt_format_s (dest + colorlen, destlen - colorlen, prefix, mutt_labels(NULL, 0, hdr->env, NULL));
       add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
       break;
 
    case 'Y':
      if (hdr->env->labels == NULL)
      {
        if (optional)
          optional = 0;
        mutt_format_s(dest, destlen, prefix, "");
        break;
      }
      else
      {
        char labels[HUGE_STRING];
        char labelstmp[HUGE_STRING];

        i = 1;  /* reduce reuse recycle */
        htmp = NULL;
        if ((flags & MUTT_FORMAT_TREE) &&
            hdr->thread->prev &&
            hdr->thread->prev->message &&
            hdr->thread->prev->message->env->labels)
          htmp = hdr->thread->prev->message;
        else if ((flags & MUTT_FORMAT_TREE) &&
                 hdr->thread->parent &&
                 hdr->thread->parent->message &&
                 hdr->thread->parent->message->env->labels)
          htmp = hdr->thread->parent->message;

        mutt_labels(labels, sizeof(labels), hdr->env, NULL);
        if (htmp)
        {
          mutt_labels(labelstmp, sizeof(labelstmp), htmp->env, NULL);
          if (htmp && mutt_strcasecmp (labels, labelstmp) == 0)
            i = 0;
        }

        if (optional)
	  optional = i;

        colorlen = add_index_color (dest, destlen, flags, MT_COLOR_INDEX_LABEL);
        if (i)
	  mutt_format_s (dest + colorlen, destlen - colorlen, prefix, labels);
        else
          mutt_format_s (dest + colorlen, destlen - colorlen, prefix, "");
        add_index_color (dest + colorlen, destlen - colorlen, flags, MT_COLOR_INDEX);
      }

      break;

#ifdef USE_NOTMUCH
    case 'G':
    {
      char *tag_transformed;
      char format[3];
      char *tag;

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
            mutt_format_s (dest+colorlen, destlen-colorlen, prefix,
                           (tag_transformed) ? tag_transformed : "");
            add_index_color(dest+colorlen, destlen-colorlen, flags, MT_COLOR_INDEX);
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
      snprintf (dest, destlen, "%%%s%c", prefix, op);
      break;
  }

  if (optional)
    mutt_FormatString (dest, destlen, col, cols, ifstring, hdr_format_str, (unsigned long) hfi, flags);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, col, cols, elsestring, hdr_format_str, (unsigned long) hfi, flags);

  return (src);
#undef THREAD_NEW
#undef THREAD_OLD
}

void
_mutt_make_string (char *dest, size_t destlen, const char *s, CONTEXT *ctx, HEADER *hdr, format_flag flags)
{
  struct hdr_format_info hfi;

  hfi.hdr = hdr;
  hfi.ctx = ctx;
  hfi.pager_progress = 0;

  mutt_FormatString (dest, destlen, 0, MuttIndexWindow->cols, s, hdr_format_str, (unsigned long) &hfi, flags);
}

void
mutt_make_string_info (char *dst, size_t dstlen, int cols, const char *s, struct hdr_format_info *hfi, format_flag flags)
{
  mutt_FormatString (dst, dstlen, 0, cols, s, hdr_format_str, (unsigned long) hfi, flags);
}
