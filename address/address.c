/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 1996-2000,2011-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

/**
 * @page addr_address Representation of an email address
 *
 * Representation of an email address
 */

#include "config.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "address.h"
#include "idna2.h"

/**
 * AddressSpecials - Characters with special meaning for email addresses
 */
const char AddressSpecials[] = "@.,:;<>[]\\\"()";

/**
 * is_special - Is this character special to an email address?
 */
#define is_special(ch) strchr(AddressSpecials, ch)

/**
 * AddressError - An out-of-band error code
 *
 * Many of the Address functions set this variable on error.
 * Its values are defined in #AddressError.
 * Text for the errors can be looked up using #AddressErrors.
 */
int AddressError = 0;

/**
 * AddressErrors - Messages for the error codes in #AddressError
 *
 * These must defined in the same order as enum AddressError.
 */
const char *const AddressErrors[] = {
  "out of memory",   "mismatched parentheses", "mismatched quotes",
  "bad route in <>", "bad address in <>",      "bad address spec",
};

/**
 * parse_comment - Extract a comment (parenthesised string)
 * @param[in]  s          String, just after the opening parenthesis
 * @param[out] comment    Buffer to store parenthesised string
 * @param[out] commentlen Length of parenthesised string
 * @param[in]  commentmax Length of buffer
 * @retval ptr  First character after parenthesised string
 * @retval NULL Error
 */
static const char *parse_comment(const char *s, char *comment, size_t *commentlen, size_t commentmax)
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
  if (level != 0)
  {
    AddressError = ADDR_ERR_MISMATCH_PAREN;
    return NULL;
  }
  return s;
}

/**
 * parse_quote - Extract a quoted string
 * @param[in]  s        String, just after the opening quote mark
 * @param[out] token    Buffer to store quoted string
 * @param[out] tokenlen Length of quoted string
 * @param[in]  tokenmax Length of buffer
 * @retval ptr  First character after quoted string
 * @retval NULL Error
 */
static const char *parse_quote(const char *s, char *token, size_t *tokenlen, size_t tokenmax)
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
      return s + 1;
    (*tokenlen)++;
    s++;
  }
  AddressError = ADDR_ERR_MISMATCH_QUOTE;
  return NULL;
}

/**
 * next_token - Find the next word, skipping quoted and parenthesised text
 * @param[in]  s        String to search
 * @param[out] token    Buffer for the token
 * @param[out] tokenlen Length of the next token
 * @param[in]  tokenmax Length of the buffer
 * @retval ptr First character after the next token
 */
static const char *next_token(const char *s, char *token, size_t *tokenlen, size_t tokenmax)
{
  if (*s == '(')
    return parse_comment(s + 1, token, tokenlen, tokenmax);
  if (*s == '"')
    return parse_quote(s + 1, token, tokenlen, tokenmax);
  if (*s && is_special(*s))
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    return s + 1;
  }
  while (*s)
  {
    if (mutt_str_is_email_wsp(*s) || is_special(*s))
      break;
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    s++;
  }
  return s;
}

/**
 * parse_mailboxdomain - Extract part of an email address (and a comment)
 * @param[in]  s          String to parse
 * @param[in]  nonspecial Specific characters that are valid
 * @param[out] mailbox    Buffer for email address
 * @param[out] mailboxlen Length of saved email address
 * @param[in]  mailboxmax Length of mailbox buffer
 * @param[out] comment    Buffer for comment
 * @param[out] commentlen Length of saved comment
 * @param[in]  commentmax Length of comment buffer
 * @retval ptr First character after the email address part
 *
 * This will be called twice to parse an email address, first for the mailbox
 * name, then for the domain name.  Each part can also have a comment in `()`.
 * The comment can be at the start or end of the mailbox or domain.
 *
 * Examples:
 * - "john.doe@example.com"
 * - "john.doe(comment)@example.com"
 * - "john.doe@example.com(comment)"
 *
 * The first call will return "john.doe" with optional comment, "comment".
 * The second call will return "example.com" with optional comment, "comment".
 */
static const char *parse_mailboxdomain(const char *s, const char *nonspecial,
                                       char *mailbox, size_t *mailboxlen,
                                       size_t mailboxmax, char *comment,
                                       size_t *commentlen, size_t commentmax)
{
  const char *ps = NULL;

  while (*s)
  {
    s = mutt_str_skip_email_wsp(s);
    if (!*s)
      return s;

    if (!strchr(nonspecial, *s) && is_special(*s))
      return s;

    if (*s == '(')
    {
      if (*commentlen && (*commentlen < commentmax))
        comment[(*commentlen)++] = ' ';
      ps = next_token(s, comment, commentlen, commentmax);
    }
    else
      ps = next_token(s, mailbox, mailboxlen, mailboxmax);
    if (!ps)
      return NULL;
    s = ps;
  }

  return s;
}

/**
 * parse_address - Extract an email address
 * @param[in]  s          String, just after the opening `<`
 * @param[out] token      Buffer for the email address
 * @param[out] tokenlen   Length of the email address
 * @param[in]  tokenmax   Length of the email address buffer
 * @param[out] comment    Buffer for any comments
 * @param[out] commentlen Length of any comments
 * @param[in]  commentmax Length of the comment buffer
 * @param[in]  addr       Address to store the results
 * @retval ptr  The closing `>` of the email address
 * @retval NULL Error
 */
static const char *parse_address(const char *s, char *token, size_t *tokenlen,
                                 size_t tokenmax, char *comment, size_t *commentlen,
                                 size_t commentmax, struct Address *addr)
{
  s = parse_mailboxdomain(s, ".\"(\\", token, tokenlen, tokenmax, comment,
                          commentlen, commentmax);
  if (!s)
    return NULL;

  if (*s == '@')
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = '@';
    s = parse_mailboxdomain(s + 1, ".([]\\", token, tokenlen, tokenmax, comment,
                            commentlen, commentmax);
    if (!s)
      return NULL;
  }

  terminate_string(token, *tokenlen, tokenmax);
  addr->mailbox = mutt_str_strdup(token);

  if (*commentlen && !addr->personal)
  {
    terminate_string(comment, *commentlen, commentmax);
    addr->personal = mutt_str_strdup(comment);
  }

  return s;
}

/**
 * parse_route_addr - Parse an email addresses
 * @param[in]  s          String, just after the opening `<`
 * @param[out] comment    Buffer for any comments
 * @param[out] commentlen Length of any comments
 * @param[in]  commentmax Length of the comments buffer
 * @param[in]  addr       Address to store the details
 * @retval ptr First character after the email address
 */
static const char *parse_route_addr(const char *s, char *comment, size_t *commentlen,
                                    size_t commentmax, struct Address *addr)
{
  char token[1024];
  size_t tokenlen = 0;

  s = mutt_str_skip_email_wsp(s);

  /* find the end of the route */
  if (*s == '@')
  {
    while (s && (*s == '@'))
    {
      if (tokenlen < (sizeof(token) - 1))
        token[tokenlen++] = '@';
      s = parse_mailboxdomain(s + 1, ",.\\[](", token, &tokenlen,
                              sizeof(token) - 1, comment, commentlen, commentmax);
    }
    if (!s || (*s != ':'))
    {
      AddressError = ADDR_ERR_BAD_ROUTE;
      return NULL; /* invalid route */
    }

    if (tokenlen < (sizeof(token) - 1))
      token[tokenlen++] = ':';
    s++;
  }

  s = parse_address(s, token, &tokenlen, sizeof(token) - 1, comment, commentlen,
                    commentmax, addr);
  if (!s)
    return NULL;

  if (*s != '>')
  {
    AddressError = ADDR_ERR_BAD_ROUTE_ADDR;
    return NULL;
  }

  if (!addr->mailbox)
    addr->mailbox = mutt_str_strdup("@");

  s++;
  return s;
}

/**
 * parse_addr_spec - Parse an email address
 * @param[in]  s          String to parse
 * @param[out] comment    Buffer for any comments
 * @param[out] commentlen Length of any comments
 * @param[in]  commentmax Length of the comments buffer
 * @param[in]  addr       Address to fill in
 * @retval ptr First character after the email address
 */
static const char *parse_addr_spec(const char *s, char *comment, size_t *commentlen,
                                   size_t commentmax, struct Address *addr)
{
  char token[1024];
  size_t tokenlen = 0;

  s = parse_address(s, token, &tokenlen, sizeof(token) - 1, comment, commentlen,
                    commentmax, addr);
  if (s && *s && (*s != ',') && (*s != ';'))
  {
    AddressError = ADDR_ERR_BAD_ADDR_SPEC;
    return NULL;
  }
  return s;
}

/**
 * add_addrspec - Parse an email address and add an Address to a list
 * @param[out] al         Address list
 * @param[in]  phrase     String to parse
 * @param[out] comment    Buffer for any comments
 * @param[out] commentlen Length of any comments
 * @param[in]  commentmax Length of the comments buffer
 */
static void add_addrspec(struct AddressList *al, const char *phrase,
                         char *comment, size_t *commentlen, size_t commentmax)
{
  struct Address *cur = mutt_addr_new();

  if (!parse_addr_spec(phrase, comment, commentlen, commentmax, cur))
  {
    mutt_addr_free(&cur);
    return;
  }

  mutt_addresslist_append(al, cur);
}

/**
 * mutt_addr_new - Create a new Address
 * @retval ptr Newly allocated Address
 *
 * Free the result with mutt_addr_free()
 */
struct Address *mutt_addr_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Address));
}

/**
 * mutt_addresslist_remove - Remove an Address from a list
 * @param[in, out] al AddressList
 * @param[in]  mailbox Email address to match
 * @retval  0 Success
 * @retval -1 Error, or email not found
 */
int mutt_addresslist_remove(struct AddressList *al, const char *mailbox)
{
  if (!al)
    return -1;

  if (!mailbox)
    return 0;

  int rc = -1;
  struct AddressNode *an, *tmp;
  TAILQ_FOREACH_SAFE(an, al, entries, tmp)
  {
    if (mutt_str_strcasecmp(mailbox, an->addr->mailbox) == 0)
    {
      mutt_addresslist_free_one(al, an);
      rc = 0;
    }
  }

  return rc;
}

/**
 * mutt_addr_free - Free a single Addresses
 * @param[out] a Address to free
 */
void mutt_addr_free(struct Address **a)
{
  if (!a || !*a)
    return;
  FREE(&(*a)->personal);
  FREE(&(*a)->mailbox);
  FREE(a);
}

/**
 * mutt_addresslist_free - Free an AddressList
 */
void mutt_addresslist_free(struct AddressList **al)
{
  if (!al)
    return;
  mutt_addresslist_free_all(*al);
  FREE(al);
}

/**
 * mutt_addresslist_parse - Parse a list of email addresses
 * @param al AddressList to append addresses
 * @param s  String to parse
 * @retval Number of parsed addressess
 */
int mutt_addresslist_parse(struct AddressList *al, const char *s)
{
  if (!s)
    return 0;

  int parsed = 0;
  char comment[1024], phrase[1024];
  size_t phraselen = 0, commentlen = 0;
  AddressError = 0;

  bool ws_pending = mutt_str_is_email_wsp(*s);

  s = mutt_str_skip_email_wsp(s);
  while (*s)
  {
    switch (*s)
    {
      case ',':
        if (phraselen != 0)
        {
          terminate_buffer(phrase, phraselen);
          add_addrspec(al, phrase, comment, &commentlen, sizeof(comment) - 1);
        }
        else if (commentlen != 0)
        {
          struct AddressNode *last = TAILQ_LAST(al, AddressList);
          if (last && last->addr && !last->addr->personal)
          {
            terminate_buffer(comment, commentlen);
            last->addr->personal = mutt_str_strdup(comment);
          }
        }

        commentlen = 0;
        phraselen = 0;
        s++;
        break;

      case '(':
        if ((commentlen != 0) && (commentlen < (sizeof(comment) - 1)))
          comment[commentlen++] = ' ';
        s = next_token(s, comment, &commentlen, sizeof(comment) - 1);
        if (!s)
        {
          mutt_addresslist_free_all(al);
          return 0;
        }
        break;

      case '"':
        if ((phraselen != 0) && (phraselen < (sizeof(phrase) - 1)))
          phrase[phraselen++] = ' ';
        s = parse_quote(s + 1, phrase, &phraselen, sizeof(phrase) - 1);
        if (!s)
        {
          mutt_addresslist_free_all(al);
          return 0;
        }
        break;

      case ':':
      {
        struct Address *a = mutt_addr_new();
        terminate_buffer(phrase, phraselen);
        a->mailbox = mutt_str_strdup(phrase);
        a->group = 1;
        mutt_addresslist_append(al, a);
        phraselen = 0;
        commentlen = 0;
        s++;
        parsed++;
        break;
      }

      case ';':
        if (phraselen != 0)
        {
          terminate_buffer(phrase, phraselen);
          add_addrspec(al, phrase, comment, &commentlen, sizeof(comment) - 1);
        }
        else if (commentlen != 0)
        {
          struct AddressNode *last = TAILQ_LAST(al, AddressList);
          if (last && last->addr && !last->addr->personal)
          {
            terminate_buffer(comment, commentlen);
            last->addr->personal = mutt_str_strdup(comment);
          }
        }

        /* add group terminator */
        mutt_addresslist_append(al, mutt_addr_new());

        phraselen = 0;
        commentlen = 0;
        s++;
        break;

      case '<':
      {
        struct Address *a = mutt_addr_new();
        terminate_buffer(phrase, phraselen);
        a->personal = mutt_str_strdup(phrase);
        s = parse_route_addr(s + 1, comment, &commentlen, sizeof(comment) - 1, a);
        if (!s)
        {
          mutt_addresslist_free_all(al);
          mutt_addr_free(&a);
          return 0;
        }
        mutt_addresslist_append(al, a);
        phraselen = 0;
        commentlen = 0;
        parsed++;
        break;
      }

      default:
        if ((phraselen != 0) && (phraselen < (sizeof(phrase) - 1)) && ws_pending)
          phrase[phraselen++] = ' ';
        s = next_token(s, phrase, &phraselen, sizeof(phrase) - 1);
        if (!s)
        {
          mutt_addresslist_free_all(al);
          return 0;
        }
        break;
    } // switch (*s)

    ws_pending = mutt_str_is_email_wsp(*s);
    s = mutt_str_skip_email_wsp(s);
  } // while (*s)

  if (phraselen != 0)
  {
    terminate_buffer(phrase, phraselen);
    terminate_buffer(comment, commentlen);
    add_addrspec(al, phrase, comment, &commentlen, sizeof(comment) - 1);
  }
  else if (commentlen != 0)
  {
    struct AddressNode *last = TAILQ_LAST(al, AddressList);
    if (last && last->addr && !last->addr->personal)
    {
      terminate_buffer(comment, commentlen);
      last->addr->personal = mutt_str_strdup(comment);
    }
  }

  return parsed;
}

/**
 * mutt_addresslist_parse - Parse a list of email addresses
 * @param al Add to this List of Addresses
 * @param s String to parse
 * @retval int Number of parsed addresses
 *
 * The email addresses can be separated by whitespace or commas.
 */
int mutt_addresslist_parse2(struct AddressList *al, const char *s)
{
  if (!s)
    return 0;

  int parsed = 0;

  /* check for a simple whitespace separated list of addresses */
  const char *q = strpbrk(s, "\"<>():;,\\");
  if (!q)
  {
    struct Buffer *tmp = mutt_buffer_alloc(1024);
    mutt_buffer_strcpy(tmp, s);
    char *r = tmp->data;
    while ((r = strtok(r, " \t")))
    {
      parsed += mutt_addresslist_parse(al, r);
      r = NULL;
    }
    mutt_buffer_free(&tmp);
  }
  else
    parsed = mutt_addresslist_parse(al, s);

  return parsed;
}

/**
 * mutt_addresslist_qualify - Expand local names in an Address list using a hostname
 * @param al Address list
 * @param host Hostname
 *
 * Any addresses containing a bare name will be expanded using the hostname.
 * e.g. "john", "example.com" -> 'john@example.com'. This function has no
 * effect if host is NULL or the empty string.
 */
void mutt_addresslist_qualify(struct AddressList *al, const char *host)
{
  if (!al || !host || !*host)
    return;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, al, entries)
  {
    struct Address *addr = an->addr;
    if (!addr->group && addr->mailbox && !strchr(addr->mailbox, '@'))
    {
      char *p = mutt_mem_malloc(mutt_str_strlen(addr->mailbox) + mutt_str_strlen(host) + 2);
      sprintf(p, "%s@%s", addr->mailbox, host);
      FREE(&addr->mailbox);
      addr->mailbox = p;
    }
  }
}

/**
 * mutt_addr_cat - Copy a string and escape the specified characters
 * @param buf      Buffer for the result
 * @param buflen   Length of the result buffer
 * @param value    String to copy
 * @param specials Characters to be escaped
 */
void mutt_addr_cat(char *buf, size_t buflen, const char *value, const char *specials)
{
  if (!buf || !value || !specials)
    return;

  if (strpbrk(value, specials))
  {
    char tmp[256];
    char *pc = tmp;
    size_t tmplen = sizeof(tmp) - 3;

    *pc++ = '"';
    for (; *value && (tmplen > 1); value++)
    {
      if ((*value == '\\') || (*value == '"'))
      {
        *pc++ = '\\';
        tmplen--;
      }
      *pc++ = *value;
      tmplen--;
    }
    *pc++ = '"';
    *pc = '\0';
    mutt_str_strfcpy(buf, tmp, buflen);
  }
  else
    mutt_str_strfcpy(buf, value, buflen);
}

/**
 * mutt_addr_copy - Copy the real address
 * @param addr Address to copy
 * @retval ptr New Address
 */
struct Address *mutt_addr_copy(const struct Address *addr)
{
  if (!addr)
    return NULL;

  struct Address *p = mutt_addr_new();

  p->personal = mutt_str_strdup(addr->personal);
  p->mailbox = mutt_str_strdup(addr->mailbox);
  p->group = addr->group;
  p->is_intl = addr->is_intl;
  p->intl_checked = addr->intl_checked;
  return p;
}

/**
 * mutt_addresslist_copy - Copy a list of addresses into another list
 * @param dst   Destination Address list
 * @param src   Source Address list
 * @param prune Skip groups if there are more addresses
 */
void mutt_addresslist_copy(struct AddressList *dst, const struct AddressList *src, bool prune)
{
  if (!dst || !src)
    return;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, src, entries)
  {
    struct Address *addr = an->addr;
    struct AddressNode *next = TAILQ_NEXT(an, entries);
    if (prune && addr->group && (!next || !next->addr->mailbox))
    {
      /* ignore this element of the list */
    }
    else
    {
      mutt_addresslist_append(dst, mutt_addr_copy(addr));
    }
  }
}

/**
 * mutt_addr_valid_msgid - Is this a valid Message ID?
 * @param msgid Message ID
 * @retval true It is a valid message ID
 *
 * Incomplete. Only used to thwart the APOP MD5 attack (#2846).
 */
bool mutt_addr_valid_msgid(const char *msgid)
{
  /* msg-id         = "<" addr-spec ">"
   * addr-spec      = local-part "@" domain
   * local-part     = word *("." word)
   * word           = atom / quoted-string
   * atom           = 1*<any CHAR except specials, SPACE and CTLs>
   * CHAR           = ( 0.-127. )
   * specials       = "(" / ")" / "<" / ">" / "@"
   *                / "," / ";" / ":" / "\" / <">
   *                / "." / "[" / "]"
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

  if (!msgid || !*msgid)
    return false;

  size_t l = mutt_str_strlen(msgid);
  if (l < 5) /* <atom@atom> */
    return false;
  if ((msgid[0] != '<') || (msgid[l - 1] != '>'))
    return false;
  if (!(strrchr(msgid, '@')))
    return false;

  /* TODO: complete parser */
  for (size_t i = 0; i < l; i++)
    if ((unsigned char) msgid[i] > 127)
      return false;

  return true;
}

/**
 * mutt_addresslist_equal - Compare two Address lists for equality
 * @param a First Address
 * @param b Second Address
 * @retval true Address lists are strictly identical
 */
bool mutt_addresslist_equal(const struct AddressList *ala, const struct AddressList *alb)
{
  if (!ala || !alb)
  {
    return !(ala || alb);
  }

  struct AddressNode *ana = TAILQ_FIRST(ala);
  struct AddressNode *anb = TAILQ_FIRST(alb);

  while (ana && anb)
  {
    if ((mutt_str_strcmp(ana->addr->mailbox, anb->addr->mailbox) != 0) ||
        (mutt_str_strcmp(ana->addr->personal, anb->addr->personal) != 0))
    {
      break;
    }

    ana = TAILQ_NEXT(ana, entries);
    anb = TAILQ_NEXT(anb, entries);
  }

  return !(ana || anb);
}

/**
 * mutt_addresslist_has_recips - Count the number of Addresses with valid recipients
 * @param al Address list
 * @retval num Number of valid Addresses
 *
 * An Address has a recipient if the mailbox or group is set.
 */
int mutt_addresslist_has_recips(const struct AddressList *al)
{
  if (!al)
    return 0;

  int c = 0;
  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, al, entries)
  {
    if (!an->addr->mailbox || an->addr->group)
      continue;
    c++;
  }
  return c;
}

/**
 * mutt_addr_cmp - Compare two e-mail addresses
 * @param a Address 1
 * @param b Address 2
 * @retval true if they are equivalent
 */
bool mutt_addr_cmp(const struct Address *a, const struct Address *b)
{
  if (!a || !b)
    return false;
  if (!a->mailbox || !b->mailbox)
    return false;
  if (mutt_str_strcasecmp(a->mailbox, b->mailbox) != 0)
    return false;
  return true;
}

/**
 * mutt_addresslist_search - Search for an e-mail address in a list
 * @param a   Address containing the search email
 * @param lst Address List
 * @retval true If the Address is in the list
 */
bool mutt_addresslist_search(const struct Address *needle, const struct AddressList *haystack)
{
  if (!needle || !haystack)
    return false;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, haystack, entries)
  {
    if (mutt_addr_cmp(needle, an->addr))
      return true;
  }
  return false;
}

/**
 * mutt_addr_is_intl - Does the Address have IDN components
 * @param a Address to check
 * @retval true Address contains IDN components
 */
static bool mutt_addr_is_intl(const struct Address *a)
{
  if (!a)
    return false;
  return a->intl_checked && a->is_intl;
}

/**
 * mutt_addr_is_local - Does the Address have NO IDN components
 * @param a Address to check
 * @retval true Address contains NO IDN components
 */
static bool mutt_addr_is_local(const struct Address *a)
{
  if (!a)
    return false;
  return a->intl_checked && !a->is_intl;
}

/**
 * mutt_addr_mbox_to_udomain - Split a mailbox name into user and domain
 * @param[in]  mbox   Mailbox name to split
 * @param[out] user   User
 * @param[out] domain Domain
 * @retval 0  Success
 * @retval -1 Error
 *
 * @warning The caller must free user and domain
 */
static int mutt_addr_mbox_to_udomain(const char *mbox, char **user, char **domain)
{
  if (!mbox || !user || !domain)
    return -1;

  char *ptr = strchr(mbox, '@');

  /* Fail if '@' is missing, at the start, or at the end */
  if (!ptr || (ptr == mbox) || (ptr[1] == '\0'))
    return -1;

  *user = mutt_str_substr_dup(mbox, ptr);
  *domain = mutt_str_strdup(ptr + 1);

  return 0;
}

/**
 * mutt_addr_set_intl - Mark an Address as having IDN components
 * @param a            Address to modify
 * @param intl_mailbox Email address with IDN components
 */
static void mutt_addr_set_intl(struct Address *a, char *intl_mailbox)
{
  if (!a)
    return;

  FREE(&a->mailbox);
  a->mailbox = intl_mailbox;
  a->intl_checked = true;
  a->is_intl = true;
}

/**
 * mutt_addr_set_local - Mark an Address as having NO IDN components
 * @param a             Address
 * @param local_mailbox Email address with NO IDN components
 */
static void mutt_addr_set_local(struct Address *a, char *local_mailbox)
{
  if (!a)
    return;

  FREE(&a->mailbox);
  a->mailbox = local_mailbox;
  a->intl_checked = true;
  a->is_intl = false;
}

/**
 * mutt_addr_for_display - Convert an Address for display purposes
 * @param a Address to convert
 * @retval ptr Address to display
 *
 * @warning This function may return a static pointer.  It must not be freed by
 * the caller.  Later calls may overwrite the returned pointer.
 */
const char *mutt_addr_for_display(const struct Address *a)
{
  if (!a)
    return NULL;

  char *user = NULL, *domain = NULL;
  static char *buf = NULL;

  if (!a->mailbox || mutt_addr_is_local(a))
    return a->mailbox;

  if (mutt_addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
    return a->mailbox;

  char *local_mailbox = mutt_idna_intl_to_local(user, domain, MI_MAY_BE_IRREVERSIBLE);

  FREE(&user);
  FREE(&domain);

  if (!local_mailbox)
    return a->mailbox;

  mutt_str_replace(&buf, local_mailbox);
  FREE(&local_mailbox);

  return buf;
}

/**
 * mutt_addr_write - Write a single Address to a buffer
 * @param buf     Buffer for the Address
 * @param buflen  Length of the buffer
 * @param addr    Address to display
 * @param display This address will be displayed to the user
 * @retval n      Number of characters written to buf
 *
 * If 'display' is set, then it doesn't matter if the transformation isn't
 * reversible.
 */
size_t mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display)
{
  if (!buf || !addr)
    return 0;

  size_t len;
  char *pbuf = buf;
  char *pc = NULL;

  buflen--; /* save room for the terminal nul */

  if (addr->personal)
  {
    if (strpbrk(addr->personal, AddressSpecials))
    {
      if (buflen == 0)
        goto done;
      *pbuf++ = '"';
      buflen--;
      for (pc = addr->personal; *pc && (buflen > 0); pc++)
      {
        if ((*pc == '"') || (*pc == '\\'))
        {
          *pbuf++ = '\\';
          buflen--;
        }
        if (buflen == 0)
          goto done;
        *pbuf++ = *pc;
        buflen--;
      }
      if (buflen == 0)
        goto done;
      *pbuf++ = '"';
      buflen--;
    }
    else
    {
      if (buflen == 0)
        goto done;
      mutt_str_strfcpy(pbuf, addr->personal, buflen);
      len = mutt_str_strlen(pbuf);
      pbuf += len;
      buflen -= len;
    }

    if (buflen == 0)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  if (addr->personal || (addr->mailbox && (*addr->mailbox == '@')))
  {
    if (buflen == 0)
      goto done;
    *pbuf++ = '<';
    buflen--;
  }

  if (addr->mailbox)
  {
    if (buflen == 0)
      goto done;
    if (mutt_str_strcmp(addr->mailbox, "@") != 0)
    {
      const char *a = display ? mutt_addr_for_display(addr) : addr->mailbox;
      mutt_str_strfcpy(pbuf, a, buflen);
      len = mutt_str_strlen(pbuf);
      pbuf += len;
      buflen -= len;
    }
    else
    {
      *pbuf = '\0';
    }

    if (addr->personal || (addr->mailbox && (*addr->mailbox == '@')))
    {
      if (buflen == 0)
        goto done;
      *pbuf++ = '>';
      buflen--;
    }

    if (addr->group)
    {
      if (buflen == 0)
        goto done;
      *pbuf++ = ':';
      buflen--;
      if (buflen == 0)
        goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }
  else
  {
    if (buflen == 0)
      goto done;
    *pbuf++ = ';';
  }
done:
  /* no need to check for length here since we already save space at the
   * beginning of this routine */
  *pbuf = '\0';

  return pbuf - buf;
}

/**
 * mutt_addresslist_write - Write an Address to a buffer
 * @param buf     Buffer for the Address
 * @param buflen  Length of the buffer
 * @param al      AddressList to display
 * @param display This address will be displayed to the user
 * @retval num Bytes written to the buffer
 *
 * If 'display' is set, then it doesn't matter if the transformation isn't
 * reversible.
 *
 * @note It is assumed that `buf` is nul terminated!
 */
size_t mutt_addresslist_write(char *buf, size_t buflen,
                              const struct AddressList *al, bool display)
{
  if (!buf || !al)
    return 0;

  char *pbuf = buf;
  size_t len = mutt_str_strlen(buf);

  buflen--; /* save room for the terminal nul */

  if (len > 0)
  {
    if (len > buflen)
      return 0; /* safety check for bogus arguments */

    pbuf += len;
    buflen -= len;
    if (buflen == 0)
      goto done;
    *pbuf++ = ',';
    buflen--;
    if (buflen == 0)
      goto done;
    *pbuf++ = ' ';
    buflen--;
  }

  struct AddressNode *np = NULL;
  TAILQ_FOREACH(np, al, entries)
  {
    if (buflen == 0)
      break;

    /* use buflen+1 here because we already saved space for the trailing
     * nul char, and the subroutine can make use of it */
    mutt_addr_write(pbuf, buflen + 1, np->addr, display);

    /* this should be safe since we always have at least 1 char passed into
     * the above call, which means 'pbuf' should always be nul terminated */
    len = mutt_str_strlen(pbuf);
    pbuf += len;
    buflen -= len;

    /* if there is another address, and it's not a group mailbox name or
     * group terminator, add a comma to separate the addresses */
    struct AddressNode *next = TAILQ_NEXT(np, entries);
    if (next && next->addr->mailbox && !next->addr->group)
    {
      if (buflen == 0)
        goto done;
      *pbuf++ = ',';
      buflen--;
      if (buflen == 0)
        goto done;
      *pbuf++ = ' ';
      buflen--;
    }
  }

done:
  *pbuf = '\0';
  return pbuf - buf;
}

/**
 * mutt_addrlist_to_intl - Convert an Address list to Punycode
 * @param[in]  a   Address list to modify
 * @param[out] err Pointer for failed addresses
 * @retval 0  Success, all addresses converted
 * @retval -1 Error, err will be set to the failed address
 */
int mutt_addresslist_to_intl(struct AddressList *al, char **err)
{
  char *user = NULL, *domain = NULL;
  char *intl_mailbox = NULL;
  int rc = 0;

  if (err)
    *err = NULL;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, al, entries)
  {
    if (!an->addr->mailbox || mutt_addr_is_intl(an->addr))
      continue;

    if (mutt_addr_mbox_to_udomain(an->addr->mailbox, &user, &domain) == -1)
      continue;

    intl_mailbox = mutt_idna_local_to_intl(user, domain);

    FREE(&user);
    FREE(&domain);

    if (!intl_mailbox)
    {
      rc = -1;
      if (err && !*err)
        *err = mutt_str_strdup(an->addr->mailbox);
      continue;
    }

    mutt_addr_set_intl(an->addr, intl_mailbox);
  }

  return rc;
}

int mutt_addrlist_to_intl(struct Address *a, char **err)
{
  struct AddressList *al = mutt_addr_to_addresslist(a);
  int rc = mutt_addresslist_to_intl(al, err);
  a = mutt_addresslist_to_addr(al);
  FREE(&al);
  return rc;
}

/**
 * mutt_addrlist_to_local - Convert an Address list from Punycode
 * @param a Address list to modify
 * @retval 0 Always
 */
int mutt_addresslist_to_local(struct AddressList *al)
{
  char *user = NULL, *domain = NULL;
  char *local_mailbox = NULL;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, al, entries)
  {
    if (!an->addr->mailbox || mutt_addr_is_local(an->addr))
      continue;

    if (mutt_addr_mbox_to_udomain(an->addr->mailbox, &user, &domain) == -1)
      continue;

    local_mailbox = mutt_idna_intl_to_local(user, domain, 0);

    FREE(&user);
    FREE(&domain);

    if (local_mailbox)
      mutt_addr_set_local(an->addr, local_mailbox);
  }
  return 0;
}

int mutt_addrlist_to_local(struct Address *a)
{
  struct AddressList *al = mutt_addr_to_addresslist(a);
  mutt_addresslist_to_local(al);
  mutt_addresslist_to_addr(al);
  FREE(&al);
  return 0;
}

/**
 * mutt_addresslist_dedupe - Remove duplicate addresses
 * @param addr Address list to de-dupe
 * @retval ptr Updated Address list
 *
 * Given a list of addresses, return a list of unique addresses
 */
void mutt_addresslist_dedupe(struct AddressList *al)
{
  if (!al)
    return;

  struct AddressNode *an = NULL;
  TAILQ_FOREACH(an, al, entries)
  {
    if (an->addr->mailbox)
    {
      struct AddressNode *an2 = TAILQ_NEXT(an, entries), *tmp;
      if (an2)
      {
        TAILQ_FOREACH_FROM_SAFE(an2, al, entries, tmp)
        {
          if (an2->addr->mailbox &&
              (mutt_str_strcasecmp(an->addr->mailbox, an2->addr->mailbox) == 0))
          {
            mutt_debug(LL_DEBUG2, "Removing %s\n", an2->addr->mailbox);
            mutt_addresslist_free_one(al, an2);
          }
        }
      }
    }
  }
}

/**
 * mutt_addresslist_remove_xrefs - Remove cross-references
 * @param a Reference AddressList
 * @param b AddressLis to trim
 *
 * Remove addresses from "b" which are contained in "a"
 */
void mutt_addresslist_remove_xrefs(const struct AddressList *a, struct AddressList *b)
{
  if (!a || !b)
    return;

  struct AddressNode *ana, *anb, *tmp;

  TAILQ_FOREACH_SAFE(anb, b, entries, tmp)
  {
    TAILQ_FOREACH(ana, a, entries)
    {
      if (mutt_addr_cmp(ana->addr, anb->addr))
      {
        mutt_addresslist_free_one(b, anb);
        break;
      }
    }
  }
}

/**
 * mutt_addresslist_new - Create a new AddressList
 * @return a newly allocated AddressList
 */
struct AddressList *mutt_addresslist_new(void)
{
  struct AddressList *al = mutt_mem_calloc(1, sizeof(struct AddressList));
  TAILQ_INIT(al);
  return al;
}

/**
 * mutt_addresslist_append - Append an address to an AddressList
 * @param al AddressList
 * @param a  Address
 */
void mutt_addresslist_append(struct AddressList *al, struct Address *a)
{
  if (!al || !a)
    return;

  struct AddressNode *anode = mutt_mem_calloc(1, sizeof(struct AddressNode));
  anode->addr = a;
  TAILQ_INSERT_TAIL(al, anode, entries);
}

/**
 * mutt_addresslist_prepend - Prepend an address to an AddressList
 * @param al AddressList
 * @param a  Address
 */
void mutt_addresslist_prepend(struct AddressList *al, struct Address *a)
{
  struct AddressNode *anode = mutt_mem_calloc(1, sizeof(struct AddressNode));
  anode->addr = a;
  TAILQ_INSERT_HEAD(al, anode, entries);
}

/**
 * mutt_addr_to_addresslist - Convert a raw list of Address into an AddressList
 * @param a Raw list of Address
 * @retval ptr AddressList
 *
 * The Address members are shared between the input argument and the return
 * value.
 */
struct AddressList *mutt_addr_to_addresslist(struct Address *a)
{
  struct AddressList *al = mutt_addresslist_new();
  while (a)
  {
    struct Address *next = a->next;
    a->next = NULL; /* canary */
    mutt_addresslist_append(al, a);
    a = next;
  }
  return al;
}

/**
 * mutt_addresslist_to_addr - Convert an AddressList into a raw list of Address
 * @param a Reference list an AddressList 
 * @retval ptr Raw list of Address
 *
 * The Address members are shared between the input argument and the return
 * value.
 */
struct Address *mutt_addresslist_to_addr(struct AddressList *al)
{
  struct Address *a = NULL;
  struct AddressNode *an, *tmp;
  TAILQ_FOREACH_REVERSE_SAFE(an, al, AddressList, entries, tmp)
  {
    assert(an->addr->next == NULL); /* canary */
    an->addr->next = a;
    a = an->addr;
    TAILQ_REMOVE(al, an, entries);
    FREE(&an);
  }
  return a;
}

/**
 * mutt_addresslist_free_one - Unlinks an AddressNode from an AddressList and
 * frees the referenced Address
 * @param al AddressList
 * @param an AddressNode
 */
void mutt_addresslist_free_one(struct AddressList *al, struct AddressNode *an)
{
  TAILQ_REMOVE(al, an, entries);
  mutt_addr_free(&an->addr);
  FREE(&an);
}

/**
 * mutt_addresslist_free_one - Unlinks all AddressNodes from an AddressList,
 * frees the referenced Addresses and reinitialize the AddressList.
 * @param al AddressList
 */
void mutt_addresslist_free_all(struct AddressList *al)
{
  if (!al)
    return;

  struct AddressNode *np = TAILQ_FIRST(al), *next = NULL;
  while (np)
  {
    next = TAILQ_NEXT(np, entries);
    mutt_addr_free(&np->addr);
    FREE(&np);
    np = next;
  }
  TAILQ_INIT(al);
}

/**
 * mutt_addresslist_clear - Unlinks all AddressNodes from an AddressList
 * and reinitialize the AddressList.
 * @param al AddressList
 */
void mutt_addresslist_clear(struct AddressList *al)
{
  struct AddressNode *np = TAILQ_FIRST(al), *next = NULL;
  while (np)
  {
    next = TAILQ_NEXT(np, entries);
    FREE(&np);
    np = next;
  }
  TAILQ_INIT(al);
}

struct Address *mutt_addresslist_first(const struct AddressList *al)
{
  return (al && TAILQ_FIRST(al)) ? TAILQ_FIRST(al)->addr : NULL;
}
