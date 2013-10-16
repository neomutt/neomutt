/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2012 Michael R. Elkins <me@mutt.org>
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

#include <string.h>
#include <stdlib.h>

#ifndef TESTING
#include "mutt.h"
#else
#define safe_strdup strdup
#define safe_malloc malloc
#define FREE(x) safe_free(x)
#define strfcpy(a,b,c) {if (c) {strncpy(a,b,c);a[c-1]=0;}}
#define LONG_STRING 1024
#include "rfc822.h"
#endif

#include "mutt_idna.h"

#define terminate_string(a, b, c) do { if ((b) < (c)) a[(b)] = 0; else \
	a[(c)] = 0; } while (0)

#define terminate_buffer(a, b) terminate_string(a, b, sizeof (a) - 1)

const char RFC822Specials[] = "@.,:;<>[]\\\"()";
#define is_special(x) strchr(RFC822Specials,x)

int RFC822Error = 0;

/* these must defined in the same order as the numerated errors given in rfc822.h */
const char * const RFC822Errors[] = {
  "out of memory",
  "mismatched parenthesis",
  "mismatched quotes",
  "bad route in <>",
  "bad address in <>",
  "bad address spec"
};

void rfc822_dequote_comment (char *s)
{
  char *w = s;

  for (; *s; s++)
  {
    if (*s == '\\')
    {
      if (!*++s)
	break; /* error? */
      *w++ = *s;
    }
    else if (*s != '\"')
    {
      if (w != s)
	*w = *s;
      w++;
    }
  }
  *w = 0;
}

static void free_address (ADDRESS *a)
{
  FREE(&a->personal);
  FREE(&a->mailbox);
#ifdef EXACT_ADDRESS
  FREE(&a->val);
#endif
  FREE(&a);
}

int rfc822_remove_from_adrlist (ADDRESS **a, const char *mailbox)
{
  ADDRESS *p, *last = NULL, *t;
  int rv = -1;

  p = *a;
  last = NULL;
  while (p)
  {
    if (ascii_strcasecmp (mailbox, p->mailbox) == 0)
    {
      if (last)
	last->next = p->next;
      else
	(*a) = p->next;
      t = p;
      p = p->next;
      free_address (t);
      rv = 0;
    }
    else
    {
      last = p;
      p = p->next;
    }
  }

  return (rv);
}

void rfc822_free_address (ADDRESS **p)
{
  ADDRESS *t;

  while (*p)
  {
    t = *p;
    *p = (*p)->next;
#ifdef EXACT_ADDRESS
    FREE (&t->val);
#endif
    FREE (&t->personal);
    FREE (&t->mailbox);
    FREE (&t);
  }
}

static const char *
parse_comment (const char *s,
	       char *comment, size_t *commentlen, size_t commentmax)
{
  int level = 1;
  
  while (*s && level)
  {
    if (*s == '(')
      level++;
    else if (*s == ')')
    {
      if (--level == 0)
      {
	s++;
	break;
      }
    }
    else if (*s == '\\')
    {
      if (!*++s)
	break;
    }
    if (*commentlen < commentmax)
      comment[(*commentlen)++] = *s;
    s++;
  }
  if (level)
  {
    RFC822Error = ERR_MISMATCH_PAREN;
    return NULL;
  }
  return s;
}

static const char *
parse_quote (const char *s, char *token, size_t *tokenlen, size_t tokenmax)
{
  while (*s)
  {
    if (*tokenlen < tokenmax)
      token[*tokenlen] = *s;
    if (*s == '\\')
    {
      if (!*++s)
	break;

      if (*tokenlen < tokenmax)
	token[*tokenlen] = *s;
    }
    else if (*s == '"')
      return (s + 1);
    (*tokenlen)++;
    s++;
  }
  RFC822Error = ERR_MISMATCH_QUOTE;
  return NULL;
}

static const char *
next_token (const char *s, char *token, size_t *tokenlen, size_t tokenmax)
{
  if (*s == '(')
    return (parse_comment (s + 1, token, tokenlen, tokenmax));
  if (*s == '"')
    return (parse_quote (s + 1, token, tokenlen, tokenmax));
  if (is_special (*s))
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    return (s + 1);
  }
  while (*s)
  {
    if (is_email_wsp(*s) || is_special (*s))
      break;
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    s++;
  }
  return s;
}

static const char *
parse_mailboxdomain (const char *s, const char *nonspecial,
		     char *mailbox, size_t *mailboxlen, size_t mailboxmax,
		     char *comment, size_t *commentlen, size_t commentmax)
{
  const char *ps;

  while (*s)
  {
    s = skip_email_wsp(s);
    if (strchr (nonspecial, *s) == NULL && is_special (*s))
      return s;

    if (*s == '(')
    {
      if (*commentlen && *commentlen < commentmax)
	comment[(*commentlen)++] = ' ';
      ps = next_token (s, comment, commentlen, commentmax);
    }
    else
      ps = next_token (s, mailbox, mailboxlen, mailboxmax);
    if (!ps)
      return NULL;
    s = ps;
  }

  return s;
}

static const char *
parse_address (const char *s,
               char *token, size_t *tokenlen, size_t tokenmax,
	       char *comment, size_t *commentlen, size_t commentmax,
	       ADDRESS *addr)
{
  s = parse_mailboxdomain (s, ".\"(\\",
			   token, tokenlen, tokenmax,
			   comment, commentlen, commentmax);
  if (!s)
    return NULL;

  if (*s == '@')
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = '@';
    s = parse_mailboxdomain (s + 1, ".([]\\",
			     token, tokenlen, tokenmax,
			     comment, commentlen, commentmax);
    if (!s)
      return NULL;
  }

  terminate_string (token, *tokenlen, tokenmax);
  addr->mailbox = safe_strdup (token);

  if (*commentlen && !addr->personal)
  {
    terminate_string (comment, *commentlen, commentmax);
    addr->personal = safe_strdup (comment);
  }

  return s;
}

static const char *
parse_route_addr (const char *s,
		  char *comment, size_t *commentlen, size_t commentmax,
		  ADDRESS *addr)
{
  char token[LONG_STRING];
  size_t tokenlen = 0;

  s = skip_email_wsp(s);

  /* find the end of the route */
  if (*s == '@')
  {
    while (s && *s == '@')
    {
      if (tokenlen < sizeof (token) - 1)
	token[tokenlen++] = '@';
      s = parse_mailboxdomain (s + 1, ",.\\[](", token,
			       &tokenlen, sizeof (token) - 1,
			       comment, commentlen, commentmax);
    }
    if (!s || *s != ':')
    {
      RFC822Error = ERR_BAD_ROUTE;
      return NULL; /* invalid route */
    }

    if (tokenlen < sizeof (token) - 1)
      token[tokenlen++] = ':';
    s++;
  }

  if ((s = parse_address (s, token, &tokenlen, sizeof (token) - 1, comment, commentlen, commentmax, addr)) == NULL)
    return NULL;

  if (*s != '>')
  {
    RFC822Error = ERR_BAD_ROUTE_ADDR;
    return NULL;
  }

  if (!addr->mailbox)
    addr->mailbox = safe_strdup ("@");

  s++;
  return s;
}

static const char *
parse_addr_spec (const char *s,
		 char *comment, size_t *commentlen, size_t commentmax,
		 ADDRESS *addr)
{
  char token[LONG_STRING];
  size_t tokenlen = 0;

  s = parse_address (s, token, &tokenlen, sizeof (token) - 1, comment, commentlen, commentmax, addr);
  if (s && *s && *s != ',' && *s != ';')
  {
    RFC822Error = ERR_BAD_ADDR_SPEC;
    return NULL;
  }
  return s;
}

static void
add_addrspec (ADDRESS **top, ADDRESS **last, const char *phrase,
	      char *comment, size_t *commentlen, size_t commentmax)
{
  ADDRESS *cur = rfc822_new_address ();
  
  if (parse_addr_spec (phrase, comment, commentlen, commentmax, cur) == NULL)
  {
    rfc822_free_address (&cur);
    return;
  }

  if (*last)
    (*last)->next = cur;
  else
    *top = cur;
  *last = cur;
}

ADDRESS *rfc822_parse_adrlist (ADDRESS *top, const char *s)
{
  int ws_pending, nl;
#ifdef EXACT_ADDRESS
  const char *begin;
#endif
  const char *ps;
  char comment[LONG_STRING], phrase[LONG_STRING];
  size_t phraselen = 0, commentlen = 0;
  ADDRESS *cur, *last = NULL;
  
  RFC822Error = 0;

  last = top;
  while (last && last->next)
    last = last->next;

  ws_pending = is_email_wsp (*s);
  if ((nl = mutt_strlen (s)))
    nl = s[nl - 1] == '\n';
  
  s = skip_email_wsp(s);
#ifdef EXACT_ADDRESS
  begin = s;
#endif
  while (*s)
  {
    if (*s == ',')
    {
      if (phraselen)
      {
	terminate_buffer (phrase, phraselen);
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	terminate_buffer (comment, commentlen);
	last->personal = safe_strdup (comment);
      }

#ifdef EXACT_ADDRESS
      if (last && !last->val)
	last->val = mutt_substrdup (begin, s);
#endif
      commentlen = 0;
      phraselen = 0;
      s++;
#ifdef EXACT_ADDRESS
      begin = skip_email_wsp(s);
#endif
    }
    else if (*s == '(')
    {
      if (commentlen && commentlen < sizeof (comment) - 1)
	comment[commentlen++] = ' ';
      if ((ps = next_token (s, comment, &commentlen, sizeof (comment) - 1)) == NULL)
      {
	rfc822_free_address (&top);
	return NULL;
      }
      s = ps;
    }
    else if (*s == '"')
    {
      if (phraselen && phraselen < sizeof (phrase) - 1)
        phrase[phraselen++] = ' ';
      if ((ps = parse_quote (s + 1, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
        rfc822_free_address (&top);
        return NULL;
      }
      s = ps;
    }
    else if (*s == ':')
    {
      cur = rfc822_new_address ();
      terminate_buffer (phrase, phraselen);
      cur->mailbox = safe_strdup (phrase);
      cur->group = 1;

      if (last)
	last->next = cur;
      else
	top = cur;
      last = cur;

#ifdef EXACT_ADDRESS
      last->val = mutt_substrdup (begin, s);
#endif

      phraselen = 0;
      commentlen = 0;
      s++;
#ifdef EXACT_ADDRESS
      begin = skip_email_wsp(s);
#endif
    }
    else if (*s == ';')
    {
      if (phraselen)
      {
	terminate_buffer (phrase, phraselen);
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	terminate_buffer (comment, commentlen);
	last->personal = safe_strdup (comment);
      }
#ifdef EXACT_ADDRESS
      if (last && !last->val)
	last->val = mutt_substrdup (begin, s);
#endif

      /* add group terminator */
      cur = rfc822_new_address ();
      if (last)
      {
	last->next = cur;
	last = cur;
      }

      phraselen = 0;
      commentlen = 0;
      s++;
#ifdef EXACT_ADDRESS
      begin = skip_email_wsp(s);
#endif
    }
    else if (*s == '<')
    {
      terminate_buffer (phrase, phraselen);
      cur = rfc822_new_address ();
      if (phraselen)
	cur->personal = safe_strdup (phrase);
      if ((ps = parse_route_addr (s + 1, comment, &commentlen, sizeof (comment) - 1, cur)) == NULL)
      {
	rfc822_free_address (&top);
	rfc822_free_address (&cur);
	return NULL;
      }

      if (last)
	last->next = cur;
      else
	top = cur;
      last = cur;

      phraselen = 0;
      commentlen = 0;
      s = ps;
    }
    else
    {
      if (phraselen && phraselen < sizeof (phrase) - 1 && ws_pending)
	phrase[phraselen++] = ' ';
      if ((ps = next_token (s, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
	rfc822_free_address (&top);
	return NULL;
      }
      s = ps;
    }
    ws_pending = is_email_wsp(*s);
    s = skip_email_wsp(s);
  }
  
  if (phraselen)
  {
    terminate_buffer (phrase, phraselen);
    terminate_buffer (comment, commentlen);
    add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
  }
  else if (commentlen && last && !last->personal)
  {
    terminate_buffer (comment, commentlen);
    last->personal = safe_strdup (comment);
  }
#ifdef EXACT_ADDRESS
  if (last)
    last->val = mutt_substrdup (begin, s - nl < begin ? begin : s - nl);
#endif

  return top;
}

void rfc822_qualify (ADDRESS *addr, const char *host)
{
  char *p;

  for (; addr; addr = addr->next)
    if (!addr->group && addr->mailbox && strchr (addr->mailbox, '@') == NULL)
    {
      p = safe_malloc (mutt_strlen (addr->mailbox) + mutt_strlen (host) + 2);
      sprintf (p, "%s@%s", addr->mailbox, host);	/* __SPRINTF_CHECKED__ */
      FREE (&addr->mailbox);
      addr->mailbox = p;
    }
}

void
rfc822_cat (char *buf, size_t buflen, const char *value, const char *specials)
{
  if (strpbrk (value, specials))
  {
    char tmp[256], *pc = tmp;
    size_t tmplen = sizeof (tmp) - 3;

    *pc++ = '"';
    for (; *value && tmplen > 1; value++)
    {
      if (*value == '\\' || *value == '"')
      {
	*pc++ = '\\';
	tmplen--;
      }
      *pc++ = *value;
      tmplen--;
    }
    *pc++ = '"';
    *pc = 0;
    strfcpy (buf, tmp, buflen);
  }
  else
    strfcpy (buf, value, buflen);
}

void rfc822_write_address_single (char *buf, size_t buflen, ADDRESS *addr,
				  int display)
{
  size_t len;
  char *pbuf = buf;
  char *pc;
  
  if (!addr)
    return;

  buflen--; /* save room for the terminal nul */

#ifdef EXACT_ADDRESS
  if (addr->val)
  {
    if (!buflen)
      goto done;
    strfcpy (pbuf, addr->val, buflen);
    len = mutt_strlen (pbuf);
    pbuf += len;
    buflen -= len;
    if (addr->group)
    {
      if (!buflen)
	goto done;
      *pbuf++ = ':';
      buflen--;
      *pbuf = 0;
    }
    return;
  }
#endif

  if (addr->personal)
  {
    if (strpbrk (addr->personal, RFC822Specials))
    {
      if (!buflen)
	goto done;
      *pbuf++ = '"';
      buflen--;
      for (pc = addr->personal; *pc && buflen > 0; pc++)
      {
	if (*pc == '"' || *pc == '\\')
	{
	  *pbuf++ = '\\';
	  buflen--;
	}
	if (!buflen)
	  goto done;
	*pbuf++ = *pc;
	buflen--;
      }
      if (!buflen)
	goto done;
      *pbuf++ = '"';
      buflen--;
    }
    else
    {
      if (!buflen)
	goto done;
      strfcpy (pbuf, addr->personal, buflen);
      len = mutt_strlen (pbuf);
      pbuf += len;
      buflen -= len;
    }

    if (!buflen)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  if (addr->personal || (addr->mailbox && *addr->mailbox == '@'))
  {
    if (!buflen)
      goto done;
    *pbuf++ = '<';
    buflen--;
  }

  if (addr->mailbox)
  {
    if (!buflen)
      goto done;
    if (ascii_strcmp (addr->mailbox, "@") && !display)
    {
      strfcpy (pbuf, addr->mailbox, buflen);
      len = mutt_strlen (pbuf);
    }
    else if (ascii_strcmp (addr->mailbox, "@") && display)
    {
      strfcpy (pbuf, mutt_addr_for_display (addr), buflen);
      len = mutt_strlen (pbuf);
    }
    else
    {
      *pbuf = '\0';
      len = 0;
    }
    pbuf += len;
    buflen -= len;

    if (addr->personal || (addr->mailbox && *addr->mailbox == '@'))
    {
      if (!buflen)
	goto done;
      *pbuf++ = '>';
      buflen--;
    }

    if (addr->group)
    {
      if (!buflen)
	goto done;
      *pbuf++ = ':';
      buflen--;
      if (!buflen)
	goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }
  else
  {
    if (!buflen)
      goto done;
    *pbuf++ = ';';
    buflen--;
  }
done:
  /* no need to check for length here since we already save space at the
     beginning of this routine */
  *pbuf = 0;
}

/* note: it is assumed that `buf' is nul terminated! */
int rfc822_write_address (char *buf, size_t buflen, ADDRESS *addr, int display)
{
  char *pbuf = buf;
  size_t len = mutt_strlen (buf);
  
  buflen--; /* save room for the terminal nul */

  if (len > 0)
  {
    if (len > buflen)
      return pbuf - buf; /* safety check for bogus arguments */

    pbuf += len;
    buflen -= len;
    if (!buflen)
      goto done;
    *pbuf++ = ',';
    buflen--;
    if (!buflen)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  for (; addr && buflen > 0; addr = addr->next)
  {
    /* use buflen+1 here because we already saved space for the trailing
       nul char, and the subroutine can make use of it */
    rfc822_write_address_single (pbuf, buflen + 1, addr, display);

    /* this should be safe since we always have at least 1 char passed into
       the above call, which means `pbuf' should always be nul terminated */
    len = mutt_strlen (pbuf);
    pbuf += len;
    buflen -= len;

    /* if there is another address, and its not a group mailbox name or
       group terminator, add a comma to separate the addresses */
    if (addr->next && addr->next->mailbox && !addr->group)
    {
      if (!buflen)
	goto done;
      *pbuf++ = ',';
      buflen--;
      if (!buflen)
	goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }
done:
  *pbuf = 0;
  return pbuf - buf;
}

/* this should be rfc822_cpy_adr */
ADDRESS *rfc822_cpy_adr_real (ADDRESS *addr)
{
  ADDRESS *p = rfc822_new_address ();

#ifdef EXACT_ADDRESS
  p->val = safe_strdup (addr->val);
#endif
  p->personal = safe_strdup (addr->personal);
  p->mailbox = safe_strdup (addr->mailbox);
  p->group = addr->group;
  p->is_idn = addr->is_idn;
  p->idn_checked = addr->idn_checked;
  return p;
}

/* this should be rfc822_cpy_adrlist */
ADDRESS *rfc822_cpy_adr (ADDRESS *addr, int prune)
{
  ADDRESS *top = NULL, *last = NULL;
  
  for (; addr; addr = addr->next)
  {
    if (prune && addr->group && (!addr->next || !addr->next->mailbox))
    {
      /* ignore this element of the list */
    }
    else if (last)
    {
      last->next = rfc822_cpy_adr_real (addr);
      last = last->next;
    }
    else
      top = last = rfc822_cpy_adr_real (addr);
  }
  return top;
}

/* append list 'b' to list 'a' and return the last element in the new list */
ADDRESS *rfc822_append (ADDRESS **a, ADDRESS *b, int prune)
{
  ADDRESS *tmp = *a;

  while (tmp && tmp->next)
    tmp = tmp->next;
  if (!b)
    return tmp;
  if (tmp)
    tmp->next = rfc822_cpy_adr (b, prune);
  else
    tmp = *a = rfc822_cpy_adr (b, prune);
  while (tmp && tmp->next)
    tmp = tmp->next;
  return tmp;
}

/* incomplete. Only used to thwart the APOP MD5 attack (#2846). */
int rfc822_valid_msgid (const char *msgid)
{
  /* msg-id         = "<" addr-spec ">"
   * addr-spec      = local-part "@" domain
   * local-part     = word *("." word)
   * word           = atom / quoted-string
   * atom           = 1*<any CHAR except specials, SPACE and CTLs>
   * CHAR           = ( 0.-127. )
   * specials       = "(" / ")" / "<" / ">" / "@"
                    / "," / ";" / ":" / "\" / <">
		    / "." / "[" / "]"
   * SPACE          = ( 32. )
   * CTLS           = ( 0.-31., 127.)
   * quoted-string  = <"> *(qtext/quoted-pair) <">
   * qtext          = <any CHAR except <">, "\" and CR>
   * CR             = ( 13. )
   * quoted-pair    = "\" CHAR
   * domain         = sub-domain *("." sub-domain)
   * sub-domain     = domain-ref / domain-literal
   * domain-ref     = atom
   * domain-literal = "[" *(dtext / quoted-pair) "]"
   */

  unsigned int l, i;

  if (!msgid || !*msgid)
    return -1;

  l = mutt_strlen (msgid);
  if (l < 5) /* <atom@atom> */
    return -1;
  if (msgid[0] != '<' || msgid[l-1] != '>')
    return -1;
  if (!(strrchr (msgid, '@')))
    return -1;

  /* TODO: complete parser */
  for (i = 0; i < l; i++)
    if ((unsigned char)msgid[i] > 127)
      return -1;

  return 0;
}

#ifdef TESTING
int safe_free (void **p)	/* __SAFE_FREE_CHECKED__ */
{
  free(*p);		/* __MEM_CHECKED__ */
  *p = 0;
}

int main (int argc, char **argv)
{
  ADDRESS *list;
  char buf[256];
# if 0
  char *str = "michael, Michael Elkins <me@mutt.org>, testing a really complex address: this example <@contains.a.source.route,@with.multiple.hosts:address@example.com>;, lothar@of.the.hillpeople (lothar)";
# else
  char *str = "a b c ";
# endif
  
  list = rfc822_parse_adrlist (NULL, str);
  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), list);
  rfc822_free_address (&list);
  puts (buf);
  exit (0);
}
#endif
