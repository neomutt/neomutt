/*
 * Copyright (C) 2000 Thomas Roessler <roessler@guug.de>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/*
 * A simple URL parser.
 */

#include "mutt.h"
#include "mapping.h"
#include "url.h"

#include "mime.h"

#include <ctype.h>

static struct mapping_t UrlMap[] =
{
  { "file", 	U_FILE },
  { "imap", 	U_IMAP },
  { "imaps", 	U_IMAPS },
  { "pop",  	U_POP  },
  { "pops", 	U_POPS  },
  { "mailto",	U_MAILTO },
  { NULL,	U_UNKNOWN}
};


static void url_pct_decode (char *s)
{
  char *d;

  if (!s)
    return;
  
  for (d = s; *s; s++)
  {
    if (*s == '%' && s[1] && s[2] &&
	hexval (s[1]) >= 0 && hexval(s[2]) >= 0)
    {
      *d++ = (hexval (s[1]) << 4) | (hexval (s[2]));
      s += 2;
    }
    else
      *d++ = *s;
  }
  *d ='\0';
}

url_scheme_t url_check_scheme (const char *s)
{
  char sbuf[STRING];
  char *t;
  int i;
  
  if (!(t = strchr (s, ':')))
    return U_UNKNOWN;
  if ((t - s) + 1 >= sizeof (sbuf))
    return U_UNKNOWN;
  
  strfcpy (sbuf, s, t - s + 1);
  for (t = sbuf; *t; t++)
    *t = tolower (*t);

  if ((i = mutt_getvaluebyname (sbuf, UrlMap)) == -1)
    return U_UNKNOWN;
  else
    return (url_scheme_t) i;
}

int url_parse_file (char *d, const char *src, size_t dl)
{
  if (strncasecmp (src, "file:", 5))
    return -1;
  else if (!strncasecmp (src, "file://", 7))	/* we don't support remote files */
    return -1;
  else
    strfcpy (d, src + 5, dl);
  
  url_pct_decode (d);
  return 0;
}

static char *ciss_parse_userhost (ciss_url_t *ciss, char *src)
{
  char *t;
  char *p;
  char *path;

  ciss->user = NULL;
  ciss->pass = NULL;
  ciss->host = NULL;
  ciss->port = 0;

  if (strncmp (src, "//", 2))
    return src;
  
  src += 2;

  if ((path = strchr (src, '/')))
    *path++ = '\0';
  
  if ((t = strchr (src, '@')))
  {
    *t = '\0';
    if ((p = strchr (src, ':')))
    {
      *p = '\0';
      ciss->pass = safe_strdup (p + 1);
      url_pct_decode (ciss->pass);
    }
    ciss->user = safe_strdup (src);
    url_pct_decode (ciss->user);
    t++;
  }
  else
    t = src;
  
  if ((p = strchr (t, ':')))
  {
    *p++ = '\0';
    ciss->port = atoi (p);
  }
  else
    ciss->port = 0;
  
  ciss->host = safe_strdup (t);
  url_pct_decode (ciss->host);
  return path;
}

static void ciss_parse_path (ciss_url_t *ciss, char *src)
{
  ciss->path = safe_strdup (src);
  url_pct_decode (ciss->path);
}

int url_parse_ciss (ciss_url_t *ciss, const char *src)
{
  char *t, *tmp;
  
  if (!(t = strchr (src, ':')))
    return -1;
  
  tmp = safe_strdup (t + 1);

  t = ciss_parse_userhost (ciss, tmp);
  ciss_parse_path (ciss, t);
  
  safe_free ((void **) &tmp);

  return 0;
}

int url_parse_mailto (ENVELOPE *e, char **body, const char *src)
{
  char *t;
  char *tmp;
  char *headers;
  char *tag, *value;
  char scratch[HUGE_STRING];

  int taglen;

  LIST *last = NULL;
  
  if (!(t = strchr (src, ':')))
    return -1;
  
  tmp = safe_strdup (t + 1);
  if ((headers = strchr (tmp, '?')))
    *headers++ = '\0';

  url_pct_decode (tmp);
  e->to = rfc822_parse_adrlist (e->to, tmp);

  tag = headers ? strtok (headers, "&") : NULL;
  
  for (; tag; tag = strtok (NULL, "&"))
  {
    if ((value = strchr (tag, '=')))
      *value++ = '\0';
    if (!value || !*value)
      continue;

    url_pct_decode (tag);
    url_pct_decode (value);

    if (!strcasecmp (tag, "body"))
      mutt_str_replace (body, value);
    else 
    {
      taglen = strlen (tag);
      /* mutt_parse_rfc822_line makes some assumptions */
      snprintf (scratch, sizeof (scratch), "%s: %s", tag, value);
      scratch[taglen] = '\0';
      value = &scratch[taglen+1];
      SKIPWS (value);
      mutt_parse_rfc822_line (e, NULL, scratch, value, 1, 0, 0, &last, NULL);
    }
  }
  
  safe_free ((void **) &tmp);
  return 0;
}

