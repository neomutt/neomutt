/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "mutt_curses.h"



#ifdef _PGPPATH
#include "pgp.h"
#endif



#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

int mutt_is_mail_list (ADDRESS *addr)
{
  LIST *p;

  if (addr->mailbox)
  {
    for (p = MailLists; p; p = p->next)
      if (strncasecmp (addr->mailbox, p->data, strlen (p->data)) == 0)
	return 1;
  }
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
    if (mutt_is_mail_list (adr))
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
    if (mutt_is_mail_list (adr))
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
    if (mutt_is_mail_list (a))
    {
      mutt_save_path (buf, buflen, a);
      return 1;
    }
  }
  return 0;
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

int mutt_user_is_recipient (ADDRESS *a)
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
 */
static int user_is_recipient (ENVELOPE *hdr)
{
  if (mutt_addr_is_user (hdr->from))
    return 4;

  if (mutt_user_is_recipient (hdr->to))
  {
    if (hdr->to->next || hdr->cc)
      return 2; /* non-unique recipient */
    else
      return 1; /* unique recipient */
  }

  if (mutt_user_is_recipient (hdr->cc))
    return 3;

  return (0);
}

/* %a = address of author
 * %b = filename of the originating folder
 * %B = the list to which the letter was sent
 * %c = size of message in bytes
 * %C = current message number
 * %d = date and time of message (using strftime)
 * %f = entire from line
 * %F = like %n, unless from self
 * %i = message-id
 * %l = number of lines in the message
 * %L = like %F, except `lists' are displayed first
 * %m = number of messages in the mailbox
 * %n = name of author
 * %N = score
 * %O = like %L, except using address instead of name
 * %s = subject
 * %S = short message status (e.g., N/O/D/!/r/-)
 * %t = `to:' field (recipients)
 * %T = $to_chars
 * %u = user (login) name of author
 * %Z = status flags	*/

struct hdr_format_info
{
  CONTEXT *ctx;
  HEADER *hdr;
};

static const char *
hdr_format_str (char *dest,
		size_t destlen,
		char op,
		const char *src,
		const char *prefix,
		const char *ifstring,
		const char *elsestring,
		unsigned long data,
		format_flag flags)
{
  struct hdr_format_info *hfi = (struct hdr_format_info *) data;
  HEADER *hdr;
  CONTEXT *ctx;
  char fmt[SHORT_STRING], buf2[SHORT_STRING], ch, *p;
  int do_locales, i;
  int optional = (flags & M_FORMAT_OPTIONAL);
  size_t len;

  hdr = hfi->hdr;
  ctx = hfi->ctx;

  dest[0] = 0;
  switch (op)
  {
    case 'a':
      if(hdr->env->from && hdr->env->from->mailbox)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, hdr->env->from->mailbox);
      }
      else
        dest[0] = '\0';
      break;

    case 'B':
      if (!first_mailing_list (dest, destlen, hdr->env->to) &&
	  !first_mailing_list (dest, destlen, hdr->env->cc))
	dest[0] = 0;
      break;

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
      break;
    
    case 'c':
      mutt_pretty_size (buf2, sizeof (buf2), (long) hdr->content->length);
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, buf2);
      break;

    case 'C':
      snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
      snprintf (dest, destlen, fmt, hdr->msgno + 1);
      break;

    case 'd':
    case '{':
    case '[':
    case '(':
    case '<':

      /* preprocess $date_format to handle %Z */
      {
	const char *cp;
	struct tm *tm; 
	time_t T;

	p = dest;

	cp = (op == 'd') ? (NONULL (DateFmt)) : src;
	if (*cp == '!')
	{
	  do_locales = 0;
	  cp++;
	}
	else
	  do_locales = 1;

	len = destlen - 1;
	while (len > 0 && ((op == 'd' && *cp) ||
			   (op == '{' && *cp != '}') || 
			   (op == '[' && *cp != ']') ||
			   (op == '(' && *cp != ')') ||
			   (op == '<' && *cp != '>')))
	{
	  if (*cp == '%')
	  {
	    cp++;
	    if (*cp == 'Z' && (op == 'd' || op == '{'))
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

	if (do_locales && Locale)
	  setlocale (LC_TIME, Locale);

	if (op == '[')
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

	strftime (buf2, sizeof (buf2), dest, tm);

	if (do_locales)
	  setlocale (LC_TIME, "C");

	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, buf2);
	if (len > 0 && op != 'd')
	  src = cp + 1;
      }
      break;

    case 'f':
      buf2[0] = 0;
      rfc822_write_address (buf2, sizeof (buf2), hdr->env->from);
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, buf2);
      break;

    case 'F':
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      make_from (hdr->env, buf2, sizeof (buf2), 0);
      snprintf (dest, destlen, fmt, buf2);
      break;

    case 'i':
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, hdr->env->message_id ? hdr->env->message_id : "<no.id>");
      break;

    case 'l':
      snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
      snprintf (dest, destlen, fmt, (int) hdr->lines);
      break;

    case 'L':
      if (!optional)
      {
	make_from (hdr->env, buf2, sizeof (buf2), 1);
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, buf2);
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
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, mutt_get_name (hdr->env->from));
      break;

    case 'N':
      snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
      snprintf (dest, destlen, fmt, hdr->score);
      break;

    case 'O':
      if (!optional)
      {
	make_from_addr (hdr->env, buf2, sizeof (buf2), 1);
	if (!option (OPTSAVEADDRESS) && (p = strpbrk (buf2, "%@")))
	  *p = 0;
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, buf2);
      }
      else if (!check_for_mailing_list_addr (hdr->env->to, NULL, 0) &&
	       !check_for_mailing_list_addr (hdr->env->cc, NULL, 0))
      {
	optional = 0;
      }
      break;

    case 's':
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      if (flags & M_FORMAT_TREE)
      {
	if (flags & M_FORMAT_FORCESUBJ)
	{
	  snprintf (buf2, sizeof (buf2), "%s%s", hdr->tree,
		    hdr->env->subject ? hdr->env->subject : "");
	  snprintf (dest, destlen, fmt, buf2);
	}
	else
	  snprintf (dest, destlen, fmt, hdr->tree);
      }
      else
      {
	snprintf (dest, destlen, fmt, hdr->env->subject ? hdr->env->subject : "");
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
      *dest = ch;
      *(dest + 1) = 0;
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
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, buf2);
      break;

    case 'T':
      snprintf (fmt, sizeof (fmt), "%%%sc", prefix);
      snprintf (dest, destlen, fmt,
		(Tochars && ((i = user_is_recipient (hdr->env))) < strlen (Tochars)) ? Tochars[i] : ' ');
      break;

    case 'u':
      if (hdr->env->from && hdr->env->from->mailbox)
      {
	strfcpy (buf2, hdr->env->from->mailbox, sizeof (buf2));
	if ((p = strpbrk (buf2, "%@")))
	  *p = 0;
      }
      else
	buf2[0] = 0;
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (dest, destlen, fmt, buf2);
      break;

    case 'Z':
      if (hdr->mailcap)
	ch = 'M';



#ifdef _PGPPATH
      else if (hdr->pgp & PGPENCRYPT)
      	ch = 'P';
      else if (hdr->pgp & PGPSIGN)
        ch = 'S';
      else if (hdr->pgp & PGPKEY)
        ch = 'K';
#endif



      else
	ch = ' ';
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (buf2, sizeof (buf2),
		"%c%c%c",
		(hdr->read && (ctx && ctx->msgnotreadyet != hdr->msgno))
		? (hdr->replied ? 'r' : ' ') : (hdr->old ? 'O' : 'N'),
		hdr->deleted ? 'D' : (hdr->attach_del ? 'd' : ch),
		hdr->tagged ? '*' :
		(hdr->flagged ? '!' :
		 (Tochars && ((i = user_is_recipient (hdr->env)) < strlen (Tochars)) ? Tochars[user_is_recipient (hdr->env)] : ' ')));
      snprintf (dest, destlen, fmt, buf2);
      break;

    default:
      snprintf (dest, destlen, "%%%s%c", prefix, op);
      break;
  }

  if (optional)
    mutt_FormatString (dest, destlen, ifstring, hdr_format_str, (unsigned long) hfi, flags);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, elsestring, hdr_format_str, (unsigned long) hfi, flags);

  return (src);
}

void
_mutt_make_string (char *dest, size_t destlen, const char *s, CONTEXT *ctx, HEADER *hdr, format_flag flags)
{
  struct hdr_format_info hfi;

  hfi.hdr = hdr;
  hfi.ctx = ctx;

  mutt_FormatString (dest, destlen, s, hdr_format_str, (unsigned long) &hfi, flags);
}
