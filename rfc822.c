static char rcsid[]="$Id$";
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

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef TESTING
#include "mutt.h"
#else
#define safe_strdup strdup
#define safe_malloc malloc
#define SKIPWS(x) while(isspace(*x))x++
#define FREE(x) safe_free(x)
#define ISSPACE isspace
#define strfcpy(a,b,c) {if (c) {strncpy(a,b,c);a[c-1]=0;}}
#define STRING 128
#include "rfc822.h"
#endif

const char RFC822Specials[] = "@.,:;<>[]\\\"()";
#define is_special(x) strchr(RFC822Specials,x)

int RFC822Error = 0;

/* these must defined in the same order as the numerated errors given in rfc822.h */
const char *RFC822Errors[] = {
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
  if (*tokenlen < tokenmax)
    token[(*tokenlen)++] = '"';
  while (*s)
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    if (*s == '"')
      return (s + 1);
    if (*s == '\\')
    {
      if (!*++s)
	break;
    }
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
    if (ISSPACE (*s) || is_special (*s))
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
    SKIPWS (s);
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

  token[*tokenlen] = 0;
  addr->mailbox = safe_strdup (token);

  if (*commentlen && !addr->personal)
  {
    comment[*commentlen] = 0;
    addr->personal = safe_strdup (comment);
  }

  return s;
}

static const char *
parse_route_addr (const char *s,
		  char *comment, size_t *commentlen, size_t commentmax,
		  ADDRESS *addr)
{
  char token[STRING];
  size_t tokenlen = 0;

  SKIPWS (s);

  /* find the end of the route */
  if (*s == '@')
  {
    while (s && *s == '@')
    {
      if (tokenlen < sizeof (token) - 1)
	token[tokenlen++] = '@';
      s = parse_mailboxdomain (s + 1, ".\\[](", token,
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

  if (*s != '>' || !addr->mailbox)
  {
    RFC822Error = ERR_BAD_ROUTE_ADDR;
    return NULL;
  }

  s++;
  return s;
}

static const char *
parse_addr_spec (const char *s,
		 char *comment, size_t *commentlen, size_t commentmax,
		 ADDRESS *addr)
{
  char token[STRING];
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
    return;

  if (*last)
    (*last)->next = cur;
  else
    *top = cur;
  *last = cur;
}

ADDRESS *rfc822_parse_adrlist (ADDRESS *top, const char *s)
{
  const char *begin, *ps;
  char comment[STRING], phrase[STRING];
  size_t phraselen = 0, commentlen = 0;
  ADDRESS *cur, *last = NULL;
  
  RFC822Error = 0;

  last = top;
  while (last && last->next)
    last = last->next;

  SKIPWS (s);
  begin = s;
  while (*s)
  {
    if (*s == ',')
    {
      if (phraselen)
      {
	phrase[phraselen] = 0;
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	comment[commentlen] = 0;
	last->personal = safe_strdup (comment);
      }

#ifdef EXACT_ADDRESS
      if (last && !last->val)
	last->val = mutt_substrdup (begin, s);
#endif
      commentlen = 0;
      phraselen = 0;
      s++;
      begin = s;
      SKIPWS (begin);
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
    else if (*s == ':')
    {
      cur = rfc822_new_address ();
      phrase[phraselen] = 0;
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
      begin = s;
      SKIPWS (begin);
    }
    else if (*s == ';')
    {
      if (phraselen)
      {
	phrase[phraselen] = 0;
	add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
	comment[commentlen] = 0;
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
      begin = s;
      SKIPWS (begin);
    }
    else if (*s == '<')
    {
      phrase[phraselen] = 0;
      cur = rfc822_new_address ();
      if (phraselen)
      {
	if (cur->personal)
	  FREE (&cur->personal);
	/* if we get something like "Michael R. Elkins" remove the quotes */
	rfc822_dequote_comment (phrase);
	cur->personal = safe_strdup (phrase);
      }
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
      if (phraselen && phraselen < sizeof (phrase) - 1)
	phrase[phraselen++] = ' ';
      if ((ps = next_token (s, phrase, &phraselen, sizeof (phrase) - 1)) == NULL)
      {
	rfc822_free_address (&top);
	return NULL;
      }
      s = ps;
    }
    SKIPWS (s);
  }
  
  if (phraselen)
  {
    phrase[phraselen] = 0;
    comment[commentlen] = 0;
    add_addrspec (&top, &last, phrase, comment, &commentlen, sizeof (comment) - 1);
  }
  else if (commentlen && last && !last->personal)
  {
    comment[commentlen] = 0;
    last->personal = safe_strdup (comment);
  }
#ifdef EXACT_ADDRESS
  if (last)
    last->val = mutt_substrdup (begin, s);
#endif

  return top;
}

void rfc822_qualify (ADDRESS *addr, const char *host)
{
  char *p;

  for (; addr; addr = addr->next)
    if (!addr->group && addr->mailbox && strchr (addr->mailbox, '@') == NULL)
    {
      p = safe_malloc (strlen (addr->mailbox) + strlen (host) + 2);
      sprintf (p, "%s@%s", addr->mailbox, host);
      safe_free ((void **) &addr->mailbox);
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

void rfc822_write_address_single (char *buf, size_t buflen, ADDRESS *addr)
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
    len = strlen (pbuf);
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
	  if (!buflen)
	    goto done;
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
      len = strlen (pbuf);
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
    strfcpy (pbuf, addr->mailbox, buflen);
    len = strlen (pbuf);
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
void rfc822_write_address (char *buf, size_t buflen, ADDRESS *addr)
{
  char *pbuf = buf;
  size_t len = strlen (buf);
  
  buflen--; /* save room for the terminal nul */

  if (len > 0)
  {
    if (len > buflen)
      return; /* safety check for bogus arguments */

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
    rfc822_write_address_single (pbuf, buflen + 1, addr);

    /* this should be safe since we always have at least 1 char passed into
       the above call, which means `pbuf' should always be nul terminated */
    len = strlen (pbuf);
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
  return p;
}

/* this should be rfc822_cpy_adrlist */
ADDRESS *rfc822_cpy_adr (ADDRESS *addr)
{
  ADDRESS *top = NULL, *last = NULL;
  
  for (; addr; addr = addr->next)
  {
    if (last)
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
ADDRESS *rfc822_append (ADDRESS **a, ADDRESS *b)
{
  ADDRESS *tmp = *a;

  while (tmp && tmp->next)
    tmp = tmp->next;
  if (!b)
    return tmp;
  if (tmp)
    tmp->next = rfc822_cpy_adr (b);
  else
    tmp = *a = rfc822_cpy_adr (b);
  while (tmp && tmp->next)
    tmp = tmp->next;
  return tmp;
}

#ifdef TESTING
int safe_free (void **p)
{
  free(*p);
  *p = 0;
}

int main (int argc, char **argv)
{
  ADDRESS *list;
  char buf[256];
# if 0
  char *str = "michael, Michael Elkins <me@cs.hmc.edu>, testing a really complex address: this example <@contains.a.source.route@with.multiple.hosts:address@example.com>;, lothar@of.the.hillpeople (lothar)";
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
