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
#include "mutt_regex.h"
#include "mailbox.h"
#include "mime.h"
#include "rfc2047.h"
#include "rfc2231.h"


#ifdef _PGPPATH
#include "pgp.h"
#endif /* _PGPPATH */



#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdlib.h>

/* Reads an arbitrarily long header field, and looks ahead for continuation
 * lines.  ``line'' must point to a dynamically allocated string; it is
 * increased if more space is required to fit the whole line.
 */
static char *read_rfc822_line (FILE *f, char *line, size_t *linelen)
{
  char *buf = line;
  char ch;
  size_t offset = 0;

  FOREVER
  {
    if (fgets (buf, *linelen - offset, f) == NULL ||	/* end of file or */
	(ISSPACE (*line) && !offset))			/* end of headers */ 
    {
      *line = 0;
      return (line);
    }

    buf += strlen (buf) - 1;
    if (*buf == '\n')
    {
      /* we did get a full line. remove trailing space */
      while (ISSPACE (*buf))
	*buf-- = 0;	/* we cannot come beyond line's beginning because
			 * it begins with a non-space */

      /* check to see if the next line is a continuation line */
      if ((ch = fgetc (f)) != ' ' && ch != '\t')
      {
	ungetc (ch, f);
	return (line); /* next line is a separate header field or EOH */
      }

      /* eat tabs and spaces from the beginning of the continuation line */
      while ((ch = fgetc (f)) == ' ' || ch == '\t')
	;
      ungetc (ch, f);
      *++buf = ' '; /* string is still terminated because we removed
		       at least one whitespace char above */
    }

    buf++;
    offset = buf - line;
    if (*linelen < offset + STRING)
    {
      /* grow the buffer */
      *linelen += STRING;
      safe_realloc ((void **) &line, *linelen);
      buf = line + offset;
    }
  }
  /* not reached */
}

static LIST *mutt_parse_references (char *s)
{
  LIST *t, *lst = NULL;

  while ((s = strtok (s, " \t")) != NULL)
  {
    /*
     * some mail clients add other garbage besides message-ids, so do a quick
     * check to make sure this looks like a valid message-id
     */
    if (*s == '<')
    {
      t = (LIST *)safe_malloc (sizeof (LIST));
      t->data = safe_strdup (s);
      t->next = lst;
      lst = t;
    }
    s = NULL;
  }

  return (lst);
}

int mutt_check_encoding (const char *c)
{
  if (mutt_strncasecmp ("7bit", c, sizeof ("7bit")-1) == 0)
    return (ENC7BIT);
  else if (mutt_strncasecmp ("8bit", c, sizeof ("8bit")-1) == 0)
    return (ENC8BIT);
  else if (mutt_strncasecmp ("binary", c, sizeof ("binary")-1) == 0)
    return (ENCBINARY);
  else if (mutt_strncasecmp ("quoted-printable", c, sizeof ("quoted-printable")-1) == 0)
    return (ENCQUOTEDPRINTABLE);
  else if (mutt_strncasecmp ("base64", c, sizeof("base64")-1) == 0)
    return (ENCBASE64);
  else if (mutt_strncasecmp ("x-uuencode", c, sizeof("x-uuencode")-1) == 0)
    return (ENCUUENCODED);
  else
    return (ENCOTHER);
}

static PARAMETER *parse_parameters (const char *s)
{
  PARAMETER *head = 0, *cur = 0, *new;
  char buffer[LONG_STRING];
  const char *p;
  size_t i;

  dprint (2, (debugfile, "parse_parameters: `%s'\n", s));
  
  while (*s)
  {
    if ((p = strpbrk (s, "=;")) == NULL)
    {
      dprint(1, (debugfile, "parse_parameters: malformed parameter: %s\n", s));
      rfc2231_decode_parameters (&head);
      return (head); /* just bail out now */
    }

    /* if we hit a ; now the parameter has no value, just skip it */
    if (*p != ';')
    {
      i = p - s;

      new = mutt_new_parameter ();

      new->attribute = safe_malloc (i + 1);
      memcpy (new->attribute, s, i);
      new->attribute[i] = 0;

      /* remove whitespace from the end of the attribute name */
      while (ISSPACE (new->attribute[--i]))
	new->attribute[i] = 0;

      s = p + 1; /* skip over the = */
      SKIPWS (s);

      if (*s == '"')
      {
	s++;
	for (i=0; *s && *s != '"' && i < sizeof (buffer) - 1; i++, s++)
	{
	  if (*s == '\\')
	  {
	    /* Quote the next character */
	    buffer[i] = s[1];
	    if (!*++s)
	      break;
	  }
	  else
	    buffer[i] = *s;
	}
	buffer[i] = 0;
	if (*s)
	  s++; /* skip over the " */
      }
      else
      {
	for (i=0; *s && *s != ' ' && *s != ';' && i < sizeof (buffer) - 1; i++, s++)
	  buffer[i] = *s;
	buffer[i] = 0;
      }

      new->value = safe_strdup (buffer);

      dprint (2, (debugfile, "parse_parameter: `%s' = `%s'\n", new->attribute, new->value));
      
      /* Add this parameter to the list */
      if (head)
      {
	cur->next = new;
	cur = cur->next;
      }
      else
	head = cur = new;
    }
    else
    {
      dprint (1, (debugfile, "parse_parameters(): parameter with no value: %s\n", s));
      s = p;
    }

    /* Find the next parameter */
    if (*s != ';' && (s = strchr (s, ';')) == NULL)
	break; /* no more parameters */

    do
    {
      s++;

      /* Move past any leading whitespace */
      SKIPWS (s);
    }
    while (*s == ';'); /* skip empty parameters */
  }    

  rfc2231_decode_parameters (&head);
  return (head);
}

int mutt_check_mime_type (const char *s)
{
  if (mutt_strcasecmp ("text", s) == 0)
    return TYPETEXT;
  else if (mutt_strcasecmp ("multipart", s) == 0)
    return TYPEMULTIPART;
  else if (mutt_strcasecmp ("application", s) == 0)
    return TYPEAPPLICATION;
  else if (mutt_strcasecmp ("message", s) == 0)
    return TYPEMESSAGE;
  else if (mutt_strcasecmp ("image", s) == 0)
    return TYPEIMAGE;
  else if (mutt_strcasecmp ("audio", s) == 0)
    return TYPEAUDIO;
  else if (mutt_strcasecmp ("video", s) == 0)
    return TYPEVIDEO;
  else if (mutt_strcasecmp ("model", s) == 0)
    return TYPEMODEL;
  else
    return TYPEOTHER;
}

void mutt_parse_content_type (char *s, BODY *ct)
{
  char *pc;
  char *subtype;

  safe_free((void **)&ct->subtype);
  mutt_free_parameter(&ct->parameter);

  /* First extract any existing parameters */
  if ((pc = strchr(s, ';')) != NULL)
  {
    *pc++ = 0;
    while (*pc && ISSPACE (*pc))
      pc++;
    ct->parameter = parse_parameters(pc);

    /* Some pre-RFC1521 gateways still use the "name=filename" convention */
    if ((pc = mutt_get_parameter("name", ct->parameter)) != 0)
      ct->filename = safe_strdup(pc);
  }
  
  /* Now get the subtype */
  if ((subtype = strchr(s, '/')))
  {
    *subtype++ = '\0';
    for(pc = subtype; *pc && !ISSPACE(*pc) && *pc != ';'; pc++)
      ;
    *pc = '\0';
    ct->subtype = safe_strdup (subtype);
  }

  /* Finally, get the major type */
  ct->type = mutt_check_mime_type (s);

  if (ct->type == TYPEOTHER)
  {
    ct->xtype = safe_strdup (s);
  }

  if (ct->subtype == NULL)
  {
    /* Some older non-MIME mailers (i.e., mailtool, elm) have a content-type
     * field, so we can attempt to convert the type to BODY here.
     */
    if (ct->type == TYPETEXT)
      ct->subtype = safe_strdup ("plain");
    else if (ct->type == TYPEAUDIO)
      ct->subtype = safe_strdup ("basic");
    else if (ct->type == TYPEMESSAGE)
      ct->subtype = safe_strdup ("rfc822");
    else if (ct->type == TYPEOTHER)
    {
      char buffer[SHORT_STRING];

      ct->type = TYPEAPPLICATION;
      snprintf (buffer, sizeof (buffer), "x-%s", s);
      ct->subtype = safe_strdup (buffer);
    }
    else
      ct->subtype = safe_strdup ("x-unknown");
  }

  /* Default character set for text types. */
  if (ct->type == TYPETEXT)
  {
    if (!(pc = mutt_get_parameter ("charset", ct->parameter)))
      mutt_set_parameter ("charset", "us-ascii", &ct->parameter);
  }

}

static void parse_content_disposition (char *s, BODY *ct)
{
  PARAMETER *parms;

  if (!mutt_strncasecmp ("attach", s, 6))
    ct->disposition = DISPATTACH;
  else if (!mutt_strncasecmp ("form-data", s, 9))
    ct->disposition = DISPFORMDATA;
  else
    ct->disposition = DISPINLINE;

  /* Check to see if a default filename was given */
  if ((s = strchr (s, ';')) != NULL)
  {
    s++;
    SKIPWS (s);
    if ((s = mutt_get_parameter ("filename", (parms = parse_parameters (s)))) != 0)
    {
      /* free() here because the content-type parsing routine might
       * have allocated space if a "name=filename" parameter was
       * specified.
       */
      safe_free ((void **) &ct->filename);
      ct->filename = safe_strdup (s); 
    }
    if ((s = mutt_get_parameter ("name", parms)) != 0)
      ct->form_name = safe_strdup (s);
    mutt_free_parameter (&parms);
  }
}

/* args:
 *	fp	stream to read from
 *
 *	digest	1 if reading subparts of a multipart/digest, 0
 *		otherwise
 */

BODY *mutt_read_mime_header (FILE *fp, int digest)
{
  BODY *p = mutt_new_body();
  char *c;
  char *line = safe_malloc (LONG_STRING);
  size_t linelen = LONG_STRING;
  
  p->hdr_offset = ftell(fp);

  p->encoding = ENC7BIT; /* default from RFC1521 */
  p->type = digest ? TYPEMESSAGE : TYPETEXT;

  while (*(line = read_rfc822_line (fp, line, &linelen)) != 0)
  {
    /* Find the value of the current header */
    if ((c = strchr (line, ':')))
    {
      *c = 0;
      c++;
      SKIPWS (c);
      if (!*c)
      {
	dprint (1, (debugfile, "mutt_read_mime_header(): skipping empty header field: %s\n", line));
	continue;
      }
    }
    else
    {
      dprint (1, (debugfile, "read_mime_header: bogus MIME header: %s\n", line));
      break;
    }

    if (!mutt_strncasecmp ("content-", line, 8))
    {
      if (!mutt_strcasecmp ("type", line + 8))
	mutt_parse_content_type (c, p);
      else if (!mutt_strcasecmp ("transfer-encoding", line + 8))
	p->encoding = mutt_check_encoding (c);
      else if (!mutt_strcasecmp ("disposition", line + 8))
	parse_content_disposition (c, p);
      else if (!mutt_strcasecmp ("description", line + 8))
      {
	safe_free ((void **) &p->description);
	p->description = safe_strdup (c);
	rfc2047_decode (p->description, p->description, mutt_strlen (p->description) + 1);
      }
    }
  }
  p->offset = ftell (fp); /* Mark the start of the real data */
  if (p->type == TYPETEXT && !p->subtype)
    p->subtype = safe_strdup ("plain");
  else if (p->type == TYPEMESSAGE && !p->subtype)
    p->subtype = safe_strdup ("rfc822");

  FREE (&line);

  return (p);
}

void mutt_parse_part (FILE *fp, BODY *b)
{
  switch (b->type)
  {
    case TYPEMULTIPART:
      fseek (fp, b->offset, SEEK_SET);
      b->parts =  mutt_parse_multipart (fp, mutt_get_parameter ("boundary", b->parameter), 
					b->offset + b->length,
					mutt_strcasecmp ("digest", b->subtype) == 0);
      break;

    case TYPEMESSAGE:
      if (b->subtype)
      {
	fseek (fp, b->offset, SEEK_SET);
	if (mutt_is_message_type(b->type, b->subtype))
	  b->parts = mutt_parse_messageRFC822 (fp, b);
	else if (mutt_strcasecmp (b->subtype, "external-body") == 0)
	  b->parts = mutt_read_mime_header (fp, 0);
	else
	  return;
      }
      break;

    default:
      return;
  }

  /* try to recover from parsing error */
  if (!b->parts)
  {
    b->type = TYPETEXT;
    safe_free ((void **) &b->subtype);
    b->subtype = safe_strdup ("plain");
  }
}

/* parse a MESSAGE/RFC822 body
 *
 * args:
 *	fp		stream to read from
 *
 *	parent		structure which contains info about the message/rfc822
 *			body part
 *
 * NOTE: this assumes that `parent->length' has been set!
 */

BODY *mutt_parse_messageRFC822 (FILE *fp, BODY *parent)
{
  BODY *msg;

  parent->hdr = mutt_new_header ();
  parent->hdr->offset = ftell (fp);
  parent->hdr->env = mutt_read_rfc822_header (fp, parent->hdr, 0);
  msg = parent->hdr->content;

  /* ignore the length given in the content-length since it could be wrong
     and we already have the info to calculate the correct length */
  /* if (msg->length == -1) */
  msg->length = parent->length - (msg->offset - parent->offset);

  /* if body of this message is empty, we can end up with a negative length */
  if (msg->length < 0)
    msg->length = 0;

  mutt_parse_part(fp, msg);
  return (msg);
}

/* parse a multipart structure
 *
 * args:
 *	fp		stream to read from
 *
 *	boundary	body separator
 *
 *	end_off		length of the multipart body (used when the final
 *			boundary is missing to avoid reading too far)
 *
 *	digest		1 if reading a multipart/digest, 0 otherwise
 */

BODY *mutt_parse_multipart (FILE *fp, const char *boundary, long end_off, int digest)
{
  int blen, len, crlf = 0;
  char buffer[LONG_STRING];
  BODY *head = 0, *last = 0, *new = 0;
  int i;
  int final = 0; /* did we see the ending boundary? */

  if (!boundary)
  {
    mutt_error _("multipart message has no boundary parameter!");
    return (NULL);
  }

  blen = mutt_strlen (boundary);
  while (ftell (fp) < end_off && fgets (buffer, LONG_STRING, fp) != NULL)
  {
    len = mutt_strlen (buffer);

    crlf =  (len > 1 && buffer[len - 2] == '\r') ? 1 : 0;

    if (buffer[0] == '-' && buffer[1] == '-' &&
	mutt_strncmp (buffer + 2, boundary, blen) == 0)
    {
      if (last)
      {
	last->length = ftell (fp) - last->offset - len - 1 - crlf;
	if (last->parts && last->parts->length == 0)
	  last->parts->length = ftell (fp) - last->parts->offset - len - 1 - crlf;
	/* if the body is empty, we can end up with a -1 length */
	if (last->length < 0)
	  last->length = 0;
      }

      /* Remove any trailing whitespace, up to the length of the boundary */
      for (i = len - 1; ISSPACE (buffer[i]) && i >= blen + 2; i--)
        buffer[i] = 0;

      /* Check for the end boundary */
      if (mutt_strcmp (buffer + blen + 2, "--") == 0)
      {
	final = 1;
	break; /* done parsing */
      }
      else if (buffer[2 + blen] == 0)
      {
	new = mutt_read_mime_header (fp, digest);
	
	/*
	 * Consistency checking - catch
	 * bad attachment end boundaries
	 */
	
	if(new->offset > end_off)
	{
	  mutt_free_body(&new);
	  break;
	}
	if (head)
	{
	  last->next = new;
	  last = new;
	}
	else
	  last = head = new;
      }
    }
  }

  /* in case of missing end boundary, set the length to something reasonable */
  if (last && last->length == 0 && !final)
    last->length = end_off - last->offset;

  /* parse recursive MIME parts */
  for(last = head; last; last = last->next)
    mutt_parse_part(fp, last);
  
  return (head);
}

static const char *uncomment_timezone (char *buf, size_t buflen, const char *tz)
{
  char *p;
  size_t len;

  if (*tz != '(')
    return tz; /* no need to do anything */
  tz++;
  SKIPWS (tz);
  if ((p = strpbrk (tz, " )")) == NULL)
    return tz;
  len = p - tz;
  if (len > buflen - 1)
    len = buflen - 1;
  memcpy (buf, tz, len);
  buf[len] = 0;
  return buf;
}

static struct tz_t
{
  char tzname[5];
  unsigned char zhours;
  unsigned char zminutes;
  unsigned char zoccident; /* west of UTC? */
}
TimeZones[] =
{
  { "aat",   1,  0, 1 }, /* Atlantic Africa Time */
  { "adt",   4,  0, 0 }, /* Arabia DST */
  { "ast",   3,  0, 0 }, /* Arabia */
/*{ "ast",   4,  0, 1 },*/ /* Atlantic */
  { "bst",   1,  0, 0 }, /* British DST */
  { "cat",   1,  0, 0 }, /* Central Africa */
  { "cdt",   5,  0, 1 },
  { "cest",  2,  0, 0 }, /* Central Europe DST */
  { "cet",   1,  0, 0 }, /* Central Europe */
  { "cst",   6,  0, 1 },
/*{ "cst",   8,  0, 0 },*/ /* China */
/*{ "cst",   9, 30, 0 },*/ /* Australian Central Standard Time */
  { "eat",   3,  0, 0 }, /* East Africa */
  { "edt",   4,  0, 1 },
  { "eest",  3,  0, 0 }, /* Eastern Europe DST */
  { "eet",   2,  0, 0 }, /* Eastern Europe */
  { "egst",  0,  0, 0 }, /* Eastern Greenland DST */
  { "egt",   1,  0, 1 }, /* Eastern Greenland */
  { "est",   5,  0, 1 },
  { "gmt",   0,  0, 0 },
  { "gst",   4,  0, 0 }, /* Presian Gulf */
  { "hkt",   8,  0, 0 }, /* Hong Kong */
  { "ict",   7,  0, 0 }, /* Indochina */
  { "idt",   3,  0, 0 }, /* Israel DST */
  { "ist",   2,  0, 0 }, /* Israel */
/*{ "ist",   5, 30, 0 },*/ /* India */
  { "jst",   9,  0, 0 }, /* Japan */
  { "kst",   9,  0, 0 }, /* Korea */
  { "mdt",   6,  0, 1 },
  { "met",   1,  0, 0 }, /* this is now officially CET */
  { "msd",   4,  0, 0 }, /* Moscow DST */
  { "msk",   3,  0, 0 }, /* Moscow */
  { "mst",   7,  0, 1 },
  { "nzdt", 13,  0, 0 }, /* New Zealand DST */
  { "nzst", 12,  0, 0 }, /* New Zealand */
  { "pdt",   7,  0, 1 },
  { "pst",   8,  0, 1 },
  { "sat",   2,  0, 0 }, /* South Africa */
  { "smt",   4,  0, 0 }, /* Seychelles */
  { "sst",  11,  0, 1 }, /* Samoa */
/*{ "sst",   8,  0, 0 },*/ /* Singapore */
  { "utc",   0,  0, 0 },
  { "wat",   0,  0, 0 }, /* West Africa */
  { "west",  1,  0, 0 }, /* Western Europe DST */
  { "wet",   0,  0, 0 }, /* Western Europe */
  { "wgst",  2,  0, 1 }, /* Western Greenland DST */
  { "wgt",   3,  0, 1 }, /* Western Greenland */
  { "wst",   8,  0, 0 }, /* Western Australia */
};

/* parses a date string in RFC822 format:
 *
 * Date: [ weekday , ] day-of-month month year hour:minute:second timezone
 *
 * This routine assumes that `h' has been initialized to 0.  the `timezone'
 * field is optional, defaulting to +0000 if missing.
 */
time_t mutt_parse_date (const char *s, HEADER *h)
{
  int count = 0;
  char *t;
  int hour, min, sec;
  struct tm tm;
  int i;
  int tz_offset = 0;
  int zhours = 0;
  int zminutes = 0;
  int zoccident = 0;
  const char *ptz;
  char tzstr[SHORT_STRING];
  char scratch[SHORT_STRING];

  /* Don't modify our argument. Fixed-size buffer is ok here since
   * the date format imposes a natural limit. 
   */

  strfcpy (scratch, s, sizeof (scratch));
  
  /* kill the day of the week, if it exists. */
  if ((t = strchr (scratch, ',')))
    t++;
  else
    t = scratch;
  SKIPWS (t);

  memset (&tm, 0, sizeof (tm));

  while ((t = strtok (t, " \t")) != NULL)
  {
    switch (count)
    {
      case 0: /* day of the month */
	if (!isdigit ((unsigned char) *t))
	  return (-1);
	tm.tm_mday = atoi (t);
	if (tm.tm_mday > 31)
	  return (-1);
	break;

      case 1: /* month of the year */
	if ((i = mutt_check_month (t)) < 0)
	  return (-1);
	tm.tm_mon = i;
	break;

      case 2: /* year */
	tm.tm_year = atoi (t);
	if (tm.tm_year >= 1900)
	  tm.tm_year -= 1900;
	break;

      case 3: /* time of day */
	if (sscanf (t, "%d:%d:%d", &hour, &min, &sec) == 3)
	  ;
	else if (sscanf (t, "%d:%d", &hour, &min) == 2)
	  sec = 0;
	else
	{
	  dprint(1, (debugfile, "parse_date: could not process time format: %s\n", t));
	  return(-1);
	}
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;
	break;

      case 4: /* timezone */
	/* sometimes we see things like (MST) or (-0700) so attempt to
	 * compensate by uncommenting the string if non-RFC822 compliant
	 */
	ptz = uncomment_timezone (tzstr, sizeof (tzstr), t);

	if (*ptz == '+' || *ptz == '-')
	{
	  if (ptz[1] && ptz[2] && ptz[3] && ptz[4]
	      && isdigit ((unsigned char) ptz[1]) && isdigit ((unsigned char) ptz[2])
	      && isdigit ((unsigned char) ptz[3]) && isdigit ((unsigned char) ptz[4]))
	  {
	    zhours = (ptz[1] - '0') * 10 + (ptz[2] - '0');
	    zminutes = (ptz[3] - '0') * 10 + (ptz[4] - '0');

	    if (ptz[0] == '-')
	      zoccident = 1;
	  }
	}
	else
	{
	  struct tz_t *tz;

	  tz = bsearch (ptz, TimeZones, sizeof TimeZones/sizeof (struct tz_t),
			sizeof (struct tz_t),
			(int (*)(const void *, const void *)) strcasecmp
			/* This is safe to do: A pointer to a struct equals
			 * a pointer to its first element*/);

	  if (tz)
	  {
	    zhours = tz->zhours;
	    zminutes = tz->zminutes;
	    zoccident = tz->zoccident;
	  }

	  /* ad hoc support for the European MET (now officially CET) TZ */
	  if (mutt_strcasecmp (t, "MET") == 0)
	  {
	    if ((t = strtok (NULL, " \t")) != NULL)
	    {
	      if (!mutt_strcasecmp (t, "DST"))
		zhours++;
	    }
	  }
	}
	tz_offset = zhours * 3600 + zminutes * 60;
	if (!zoccident)
	  tz_offset = -tz_offset;
	break;
    }
    count++;
    t = 0;
  }

  if (count < 4) /* don't check for missing timezone */
  {
    dprint(1,(debugfile, "parse_date(): error parsing date format, using received time\n"));
    return (-1);
  }

  if (h)
  {
    h->zhours = zhours;
    h->zminutes = zminutes;
    h->zoccident = zoccident;
  }

  return (mutt_mktime (&tm, 0) + tz_offset);
}

/* extract the first substring that looks like a message-id */
static char *extract_message_id (const char *s)
{
  const char *p;
  char *r;
  size_t l;

  if ((s = strchr (s, '<')) == NULL || (p = strchr (s, '>')) == NULL)
    return (NULL);
  l = (size_t)(p - s) + 1;
  r = safe_malloc (l + 1);
  memcpy (r, s, l);
  r[l] = 0;
  return (r);
}

void mutt_parse_mime_message (CONTEXT *ctx, HEADER *cur)
{
  MESSAGE *msg;

  if (cur->content->type != TYPEMESSAGE && cur->content->type != TYPEMULTIPART)
    return; /* nothing to do */

  if (cur->content->parts)
    return; /* The message was parsed earlier. */

  if ((msg = mx_open_message (ctx, cur->msgno)))
  {
    mutt_parse_part (msg->fp, cur->content);


#ifdef _PGPPATH
    cur->pgp = pgp_query (cur->content);
#endif /* _PGPPATH */


    mx_close_message (&msg);
  }
}

/* mutt_read_rfc822_header() -- parses a RFC822 header
 *
 * Args:
 *
 * f		stream to read from
 *
 * hdr		header structure of current message (optional).
 * 
 * user_hdrs	If set, store user headers.  Used for edit-message and 
 * 		postpone modes.
 * 
 */
ENVELOPE *mutt_read_rfc822_header (FILE *f, HEADER *hdr, short user_hdrs)
{
  ENVELOPE *e = mutt_new_envelope();
  LIST *last = NULL;
  char *line = safe_malloc (LONG_STRING);
  char *p;
  char in_reply_to[STRING];
  long loc;
  int matched;
  size_t linelen = LONG_STRING;

  in_reply_to[0] = 0;

  if (hdr)
  {
    if (hdr->content == NULL)
    {
      hdr->content = mutt_new_body ();

      /* set the defaults from RFC1521 */
      hdr->content->type = TYPETEXT;
      hdr->content->subtype = safe_strdup ("plain");
      hdr->content->encoding = ENC7BIT;
      hdr->content->length = -1;
    }
  }

  loc = ftell (f);
  while (*(line = read_rfc822_line (f, line, &linelen)) != 0)
  {
    matched = 0;

    if ((p = strpbrk (line, ": \t")) == NULL || *p != ':')
    {
      char return_path[LONG_STRING];
      time_t t;

      /* some bogus MTAs will quote the original "From " line */
      if (mutt_strncmp (">From ", line, 6) == 0)
      {
	loc = ftell (f);
	continue; /* just ignore */
      }
      else if ((t = is_from (line, return_path, sizeof (return_path))))
      {
	/* MH somtimes has the From_ line in the middle of the header! */
	if (hdr && !hdr->received)
	  hdr->received = t - mutt_local_tz (t);
	loc = ftell (f);
	continue;
      }

      fseek (f, loc, 0);
      break; /* end of header */
    }

    *p = 0;
    p++;
    SKIPWS (p);
    if (!*p)
      continue; /* skip empty header fields */

    switch (tolower (line[0]))
    {
      case 'a':
	if (mutt_strcasecmp (line+1, "pparently-to") == 0)
	{
	  e->to = rfc822_parse_adrlist (e->to, p);
	  matched = 1;
	}
	else if (mutt_strcasecmp (line+1, "pparently-from") == 0)
	{
	  e->from = rfc822_parse_adrlist (e->from, p);
	  matched = 1;
	}
	break;

      case 'b':
	if (mutt_strcasecmp (line+1, "cc") == 0)
	{
	  e->bcc = rfc822_parse_adrlist (e->bcc, p);
	  matched = 1;
	}
	break;

      case 'c':
	if (mutt_strcasecmp (line+1, "c") == 0)
	{
	  e->cc = rfc822_parse_adrlist (e->cc, p);
	  matched = 1;
	}
	else if (mutt_strncasecmp (line + 1, "ontent-", 7) == 0)
	{
	  if (mutt_strcasecmp (line+8, "type") == 0)
	  {
	    if (hdr)
	      mutt_parse_content_type (p, hdr->content);
	    matched = 1;
	  }
	  else if (mutt_strcasecmp (line+8, "transfer-encoding") == 0)
	  {
	    if (hdr)
	      hdr->content->encoding = mutt_check_encoding (p);
	    matched = 1;
	  }
	  else if (mutt_strcasecmp (line+8, "length") == 0)
	  {
	    if (hdr)
	      hdr->content->length = atoi (p);
	    matched = 1;
	  }
	  else if (mutt_strcasecmp (line+8, "description") == 0)
	  {
	    if (hdr)
	    {
	      safe_free ((void **) &hdr->content->description);
	      hdr->content->description = safe_strdup (p);
	      rfc2047_decode (hdr->content->description,
			      hdr->content->description,
			      mutt_strlen (hdr->content->description) + 1);
	    }
	    matched = 1;
	  }
	  else if (mutt_strcasecmp (line+8, "disposition") == 0)
	  {
	    if (hdr)
	      parse_content_disposition (p, hdr->content);
	    matched = 1;
	  }
	}
	break;

      case 'd':
	if (!mutt_strcasecmp ("ate", line + 1))
	{
	  safe_free((void **)&e->date);
	  e->date = safe_strdup(p);
	  if (hdr)
	    hdr->date_sent = mutt_parse_date (p, hdr);
	  matched = 1;
	}
	break;

      case 'e':
	if (!mutt_strcasecmp ("xpires", line + 1) &&
	    hdr && mutt_parse_date (p, NULL) < time (NULL))
	  hdr->expired = 1;
	break;

      case 'f':
	if (!mutt_strcasecmp ("rom", line + 1))
	{
	  e->from = rfc822_parse_adrlist (e->from, p);
	  matched = 1;
	}
	break;

      case 'i':
	if (!mutt_strcasecmp (line+1, "n-reply-to"))
	{
	  if (hdr)
	  {
	    strfcpy (in_reply_to, p, sizeof (in_reply_to));
	    rfc2047_decode (in_reply_to, in_reply_to,
			    sizeof (in_reply_to));
	  }
	}
	break;

      case 'l':
	if (!mutt_strcasecmp (line + 1, "ines"))
	{
	  if (hdr)
	    hdr->lines = atoi (p);
	  matched = 1;
	}
	break;

      case 'm':
	if (!mutt_strcasecmp (line + 1, "ime-version"))
	{
	  if (hdr)
	    hdr->mime = 1;
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "essage-id"))
	{
	  /* We add a new "Message-Id:" when building a message */
	  safe_free ((void **) &e->message_id);
	  e->message_id = extract_message_id (p);
	  matched = 1;
	}
	else if (!mutt_strncasecmp (line + 1, "ail-", 4))
	{
	  if (!mutt_strcasecmp (line + 5, "reply-to"))
	  {
	    /* override the Reply-To: field */
	    rfc822_free_address (&e->reply_to);
	    e->reply_to = rfc822_parse_adrlist (e->reply_to, p);
	    matched = 1;
	  }
	  else if (!mutt_strcasecmp (line + 5, "followup-to"))
	  {
	    e->mail_followup_to = rfc822_parse_adrlist (e->mail_followup_to, p);
	    matched = 1;
	  }
	}
	break;

      case 'r':
	if (!mutt_strcasecmp (line + 1, "eferences"))
	{
	  mutt_free_list (&e->references);
	  e->references = mutt_parse_references (p);
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "eply-to"))
	{
	  e->reply_to = rfc822_parse_adrlist (e->reply_to, p);
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "eturn-path"))
	{
	  e->return_path = rfc822_parse_adrlist (e->return_path, p);
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "eceived"))
	{
	  if (hdr && !hdr->received)
	  {
	    char *d = strchr (p, ';');

	    if (d)
	      hdr->received = mutt_parse_date (d + 1, NULL);
	  }
	}
	break;

      case 's':
	if (!mutt_strcasecmp (line + 1, "ubject"))
	{
	  if (!e->subject)
	    e->subject = safe_strdup (p);
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "ender"))
	{
	  e->sender = rfc822_parse_adrlist (e->sender, p);
	  matched = 1;
	}
	else if (!mutt_strcasecmp (line + 1, "tatus"))
	{
	  if (hdr)
	  {
	    while (*p)
	    {
	      switch(*p)
	      {
		case 'r':
		  hdr->replied = 1;
		  break;
		case 'O':
		  if (option (OPTMARKOLD))
		    hdr->old = 1;
		  break;
		case 'R':
		  hdr->read = 1;
		  break;
	      }
	      p++;
	    }
	  }
	  matched = 1;
	}
	else if ((!mutt_strcasecmp ("upersedes", line + 1) ||
	          !mutt_strcasecmp ("upercedes", line + 1)) && hdr)
	  e->supersedes = safe_strdup (p);
	break;

      case 't':
	if (mutt_strcasecmp (line+1, "o") == 0)
	{
	  e->to = rfc822_parse_adrlist (e->to, p);
	  matched = 1;
	}
	break;

      case 'x':
	if (mutt_strcasecmp (line+1, "-status") == 0)
	{
	  if (hdr)
	  {
	    while (*p)
	    {
	      switch (*p)
	      {
		case 'A':
		  hdr->replied = 1;
		  break;
		case 'D':
		  hdr->deleted = 1;
		  break;
		case 'F':
		  hdr->flagged = 1;
		  break;
		default:
		  break;
	      }
	      p++;
	    }
	  }
	  matched = 1;
	}
	    
      default:
	break;
    }

     /* Keep track of the user-defined headers */
    if (!matched && user_hdrs)
    {
      if (last)
      {
	last->next = mutt_new_list ();
	last = last->next;
      }
      else
	last = e->userhdrs = mutt_new_list ();
      line[strlen (line)] = ':';
      last->data = safe_strdup (line);
    }

    loc = ftell (f);
  }

  FREE (&line);

  if (hdr)
  {
    hdr->content->hdr_offset = hdr->offset;
    hdr->content->offset = ftell (f);

    /* if an in-reply-to was given, check to see if it is in the references
     * list already.  if not, add it so we can do a better job of threading.
     */
    if (in_reply_to[0] && (p = extract_message_id (in_reply_to)) != NULL)
    {
      if (!e->references ||
	  (e->references && mutt_strcmp (e->references->data, p) != 0))
      {
	LIST *tmp = mutt_new_list ();

	tmp->data = p;
	tmp->next = e->references;
	e->references = tmp;
      }
      else
	safe_free ((void **) &p);
    }

    /* do RFC2047 decoding */
    rfc2047_decode_adrlist (e->from);
    rfc2047_decode_adrlist (e->to);
    rfc2047_decode_adrlist (e->cc);
    rfc2047_decode_adrlist (e->reply_to);
    rfc2047_decode_adrlist (e->mail_followup_to);
    rfc2047_decode_adrlist (e->return_path);
    rfc2047_decode_adrlist (e->sender);

    if (e->subject)
    {
      regmatch_t pmatch[1];

      rfc2047_decode (e->subject, e->subject, mutt_strlen (e->subject) + 1);

      if (regexec (ReplyRegexp.rx, e->subject, 1, pmatch, 0) == 0)
	e->real_subj = e->subject + pmatch[0].rm_eo;
      else
	e->real_subj = e->subject;
    }

    /* check for missing or invalid date */
    if (hdr->date_sent <= 0)
    {
      dprint(1,(debugfile,"read_rfc822_header(): no date found, using received time from msg separator\n"));
      hdr->date_sent = hdr->received;
    }
  }

  return (e);
}

ADDRESS *mutt_parse_adrlist (ADDRESS *p, const char *s)
{
  const char *q;

  /* check for a simple whitespace separated list of addresses */
  if ((q = strpbrk (s, "\"<>():;,\\")) == NULL)
  {
    char tmp[HUGE_STRING];
    char *r;

    strfcpy (tmp, s, sizeof (tmp));
    r = tmp;
    while ((r = strtok (r, " \t")) != NULL)
    {
      p = rfc822_parse_adrlist (p, r);
      r = NULL;
    }
  }
  else
    p = rfc822_parse_adrlist (p, s);
  
  return p;
}
