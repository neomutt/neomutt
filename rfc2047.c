/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

#include "mutt.h"
#include "mime.h"
#include "charset.h"
#include "rfc2047.h"

#include <ctype.h>
#include <string.h>

typedef void encode_t (char *, size_t, const unsigned char *, const char *);


static void q_encode_string (char *d, 
			     size_t dlen, 
			     const unsigned char *s,
			     const char *send_charset)
{
  char charset[SHORT_STRING];
  size_t cslen, wordlen;
  char *wptr = d;
  const unsigned char *t;
  int hibit = 0;

  for(t = s; *t && !hibit; t++)
    if(*t & 0x80) hibit = 1;

  if(hibit || !mutt_strcasecmp (send_charset, "iso-2022-jp"))
  {
    snprintf (charset, sizeof (charset), "=?%s?Q?",
	      mutt_strcasecmp ("us-ascii", send_charset) == 0 ? "unknown-8bit" : NONULL(send_charset));
  }
  else
    strfcpy(charset, "=?us-ascii?Q?", sizeof(charset));

  cslen = mutt_strlen (charset);

  strcpy (wptr, charset);
  wptr += cslen;
  wordlen = cslen;
  dlen -= cslen;

  dlen -= 3; /* save room for the word terminator */

  while (*s && dlen > 0)
  {
    if (wordlen >= 72)
    {
      if (dlen < 4 + cslen)
	break;

      strcpy (wptr, "?=\n ");
      wptr += 4;
      dlen -= 4;
      strcpy (wptr, charset);
      wptr += cslen;
      wordlen = cslen;
      dlen -= cslen;
    }

    if (*s == ' ')
    {
      *wptr++ = '_';
      wordlen++;
      dlen--;
    }
    else if ((*s & 0x80) || *s == '\t' || *s == '_' || strchr (MimeSpecials, *s))
    {
      if (wordlen >= 70)
      {
	if (dlen < 4 + cslen)
	  break;

	strcpy (wptr, "?=\n ");
	wptr += 4;
	dlen -= 4;

	strcpy (wptr, charset);
	wptr += cslen;
	wordlen = cslen;
	dlen -= cslen;
      }

      if (dlen < 3)
	break;
      sprintf (wptr, "=%02X", *s);
      wptr += 3;
      wordlen += 3;
      dlen -= 3;
    }
    else
    {
      *wptr++ = *s;
      wordlen++;
      dlen--;
    }
    s++;
  }

  strcpy (wptr, "?=");
}

static void b_encode_string (char *d, size_t dlen, 
			     const unsigned char *s, 
			     const char *send_charset)
{
  char charset[SHORT_STRING];
  char *wptr = d;
  int cslen;
  int wordlen;

  snprintf (charset, sizeof (charset), "=?%s?B?", NONULL(send_charset));
  cslen = mutt_strlen (charset);
  strcpy (wptr, charset);
  wptr += cslen;
  wordlen = cslen;
  dlen -= cslen;

  dlen -= 3; /* save room for the word terminator */

  while (*s && dlen >= 4)
  {
    if (wordlen >= 71)
    {
      if (dlen < 4 + cslen)
	break;

      strcpy (wptr, "?=\n ");
      wptr += 4;
      dlen -= 4;

      strcpy (wptr, charset);
      wptr += cslen;
      wordlen = cslen;
      dlen -= cslen;
    }

    *wptr++ = B64Chars[ (*s >> 2) & 0x3f ];
    *wptr++ = B64Chars[ ((*s & 0x3) << 4) | ((*(s+1) >> 4) & 0xf) ];
    s++;
    if (*s)
    {
      *wptr++ = B64Chars[ ((*s & 0xf) << 2) | ((*(s+1) >> 6) & 0x3) ];
      s++;
      if (*s)
      {
	*wptr++ = B64Chars[ *s & 0x3f ];
	s++;
      }
      else
	*wptr++ = '=';
    }
    else
    {
      *wptr++ = '=';
      *wptr++ = '=';
    }

    wordlen += 4;
    dlen -= 4;
  }

  strcpy (wptr, "?=");
}

void rfc2047_encode_string (char *d, size_t dlen, const unsigned char *s)
{
  int count = 0;
  int len;
  const unsigned char *p = s;
  encode_t *encoder;
  char send_charset[SHORT_STRING];
  unsigned char scratch[LONG_STRING]; 
  
  /* attention: this function will fail for
   * strings longer then LONG_STRING.  But lots
   * of code in mutt will anyway...
   */
  
  mutt_get_send_charset(send_charset, sizeof(send_charset), NULL, 0);
  
  /* First check to see if there are any 8-bit characters */
  for (; *p; p++)
  {
    if (*p & 0x80)
      count++;
    else if (*p == '=' && *(p+1) == '?')
    {
      count += 2;
      p++;
    }
  }
  if (!count)
  {
    strfcpy (d, (const char *)s, dlen);
    return;
  }

  if (mutt_strcasecmp("us-ascii", send_charset) == 0 ||
      mutt_strncasecmp("iso-8859", send_charset, 8) == 0)
    encoder = q_encode_string;
  else
  {
    /* figure out which encoding generates the most compact representation */
    len = mutt_strlen ((char *) s);
    if ((count * 2) + len <= (4 * len) / 3)
      encoder = q_encode_string;
    else
      encoder = b_encode_string;
  }

  /* Hack to pull the Re: and Fwd: out of the encoded word for better
     handling by agents which do not support RFC2047.  */
  if (!mutt_strncasecmp ("re: ", (char *) s, 4))
  {
    strncpy (d, (char *) s, 4);
    d += 4;
    dlen -= 4;
    s += 4;
  }
  else if (!mutt_strncasecmp ("fwd: ", (char *) s, 5))
  {
    strncpy (d, (char *) s, 5);
    d += 5;
    dlen -= 5;
    s += 5;
  }

  strfcpy((char *)scratch, (const char *) s, sizeof(scratch));
  if (*send_charset && mutt_strcasecmp("us-ascii", send_charset))
    mutt_convert_string ((char *)scratch, LONG_STRING, Charset, send_charset);
  
  (*encoder) (d, dlen, scratch, send_charset);
}

void rfc2047_encode_adrlist (ADDRESS *addr)
{
  ADDRESS *ptr = addr;
  char buffer[STRING];

  while (ptr)
  {
    if (ptr->personal)
    {
      rfc2047_encode_string (buffer, sizeof (buffer), (const unsigned char *)ptr->personal);
      mutt_str_replace (&ptr->personal, buffer);
    }
#ifdef EXACT_ADDRESS
    if (ptr->val)
    {
      rfc2047_encode_string (buffer, sizeof (buffer), (const unsigned char *)ptr->val);
      mutt_str_replace (&ptr->val, buffer);
    }
#endif
    ptr = ptr->next;
  }
}

static int rfc2047_decode_word (char *d, const char *s, size_t len)
{
  char *p = safe_strdup (s);
  char *pp = p;
  char *pd = d;
  char *t;
  int enc = 0, filter = 0, count = 0, c1, c2, c3, c4;
  char *charset = NULL;
  size_t olen = len;

  while ((pp = strtok (pp, "?")) != NULL)
  {
    count++;
    switch (count)
    {
      case 2:
	/* ignore language specification a la RFC 2231 */        
        if ((t = strchr (pp, '*')))
	  *t = '\0';
        charset = pp;
        filter = 1;
	break;
      case 3:
	if (toupper (*pp) == 'Q')
	  enc = ENCQUOTEDPRINTABLE;
	else if (toupper (*pp) == 'B')
	  enc = ENCBASE64;
	else
	  return (-1);
	break;
      case 4:
	if (enc == ENCQUOTEDPRINTABLE)
	{
	  while (*pp && len > 0)
	  {
	    if (*pp == '_')
	    {
	      *pd++ = ' ';
	      len--;
	    }
	    else if (*pp == '=')
	    {
	      if (pp[1] == 0 || pp[2] == 0)
		break;	/* something wrong */
	      *pd++ = (hexval(pp[1]) << 4) | hexval(pp[2]);
	      len--;
	      pp += 2;
	    }
	    else
	    {
	      *pd++ = *pp;
	      len--;
	    }
	    pp++;
	  }
	  *pd = 0;
	}
	else if (enc == ENCBASE64)
	{
	  while (*pp && len > 0)
	  {
	    if (pp[0] == '=' || pp[1] == 0 || pp[1] == '=')
	      break;  /* something wrong */
	    c1 = base64val(pp[0]);
	    c2 = base64val(pp[1]);
	    *pd++ = (c1 << 2) | ((c2 >> 4) & 0x3);
	    if (--len == 0) break;
	    
	    if (pp[2] == 0 || pp[2] == '=') break;

	    c3 = base64val(pp[2]);
	    *pd++ = ((c2 & 0xf) << 4) | ((c3 >> 2) & 0xf);
	    if (--len == 0)
	      break;

	    if (pp[3] == 0 || pp[3] == '=')
	      break;

	    c4 = base64val(pp[3]);
	    *pd++ = ((c3 & 0x3) << 6) | c4;
	    if (--len == 0)
	      break;

	    pp += 4;
	  }
	  *pd = 0;
	}
	break;
    }
    pp = 0;
  }
  
  if (filter)
    mutt_convert_string (d, olen, charset, Charset);
  safe_free ((void **) &p);
  return (0);
}

/* try to decode anything that looks like a valid RFC2047 encoded
 * header field, ignoring RFC822 parsing rules
 */
void rfc2047_decode (char *d, const char *s, size_t dlen)
{
  const char *p, *q;
  size_t n;
  int found_encoded = 0;
  int in_place = (d == s);
 
  dlen--; /* save room for the terminal nul */

  while (*s && dlen > 0)
  {
    if ((p = strstr (s, "=?")) == NULL ||
	(q = strchr (p + 2, '?')) == NULL ||
	(q = strchr (q + 1, '?')) == NULL ||
	(q = strstr (q + 1, "?=")) == NULL)
    {
      /* no encoded words */
      if (d != s)
	strfcpy (d, s, dlen + 1);
      return;
    }

    if (p != s)
    {
      n = (size_t) (p - s);
      /* ignore spaces between encoded words */
      if (!found_encoded || strspn (s, " \t\r\n") != n)
      {
	if (n > dlen)
	  n = dlen;
	if (d != s)
	  memcpy (d, s, n);
	d += n;
	dlen -= n;
      }
    }

    rfc2047_decode_word (d, p, in_place ? q + 2 - p : dlen);
    found_encoded = 1;
    s = q + 2;
    n = mutt_strlen (d);
    dlen -= n;
    d += n;
  }
  *d = 0;
}

void rfc2047_decode_adrlist (ADDRESS *a)
{
  while (a)
  {
    if (a->personal && strstr (a->personal, "=?") != NULL)
      rfc2047_decode (a->personal, a->personal, mutt_strlen (a->personal) + 1);
#ifdef EXACT_ADDRESS
    if (a->val && strstr (a->val, "=?") != NULL)
      rfc2047_decode (a->val, a->val, mutt_strlen (a->val) + 1);
#endif
    a = a->next;
  }
}
