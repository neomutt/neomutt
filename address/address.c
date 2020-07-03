/**
 * @file
 * Representation of an email address
 *
 * @authors
 * Copyright (C) 1996-2000,2011-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "address.h"
#include "idna2.h"

/**
 * AddressSpecials - Characters with special meaning for email addresses
 */
const char AddressSpecials[] = "@.,:;<>[]\\\"()";

/**
 * is_special - Is this character special to an email address?
 * @param ch Character
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
    if ((*s == '\0'))
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
  addr->mailbox = mutt_str_dup(token);

  if (*commentlen && !addr->personal)
  {
    terminate_string(comment, *commentlen, commentmax);
    addr->personal = mutt_str_dup(comment);
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
    addr->mailbox = mutt_str_dup("@");

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
 * @retval bool True if an address was successfully parsed and added
 */
static bool add_addrspec(struct AddressList *al, const char *phrase,
                         char *comment, size_t *commentlen, size_t commentmax)
{
  struct Address *cur = mutt_addr_new();

  if (!parse_addr_spec(phrase, comment, commentlen, commentmax, cur))
  {
    mutt_addr_free(&cur);
    return false;
  }

  mutt_addrlist_append(al, cur);
  return true;
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
 * mutt_addr_create - Create and populate a new Address
 * @param[in] personal The personal name for the Address (can be NULL)
 * @param[in] mailbox The mailbox for the Address (can be NULL)
 * @retval ptr Newly allocated Address
 * @note The personal and mailbox values, if not NULL, are going to be copied
 * into the newly allocated Address.
 */
struct Address *mutt_addr_create(const char *personal, const char *mailbox)
{
  struct Address *a = mutt_addr_new();
  a->personal = mutt_str_dup(personal);
  a->mailbox = mutt_str_dup(mailbox);
  return a;
}

/**
 * mutt_addrlist_remove - Remove an Address from a list
 * @param[in,out] al      AddressList
 * @param[in]     mailbox Email address to match
 * @retval  0 Success
 * @retval -1 Error, or email not found
 */
int mutt_addrlist_remove(struct AddressList *al, const char *mailbox)
{
  if (!al)
    return -1;

  if (!mailbox)
    return 0;

  int rc = -1;
  struct Address *a = NULL, *tmp = NULL;
  TAILQ_FOREACH_SAFE(a, al, entries, tmp)
  {
    if (mutt_istr_equal(mailbox, a->mailbox))
    {
      TAILQ_REMOVE(al, a, entries);
      mutt_addr_free(&a);
      rc = 0;
    }
  }

  return rc;
}

/**
 * mutt_addr_free - Free a single Address
 * @param[out] ptr Address to free
 */
void mutt_addr_free(struct Address **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Address *a = *ptr;

  FREE(&a->personal);
  FREE(&a->mailbox);
  FREE(ptr);
}

/**
 * mutt_addrlist_parse - Parse a list of email addresses
 * @param al AddressList to append addresses
 * @param s  String to parse
 * @retval num Number of parsed addresses
 */
int mutt_addrlist_parse(struct AddressList *al, const char *s)
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
      case ';':
      case ',':
        if (phraselen != 0)
        {
          terminate_buffer(phrase, phraselen);
          if (add_addrspec(al, phrase, comment, &commentlen, sizeof(comment) - 1))
          {
            parsed++;
          }
        }
        else if (commentlen != 0)
        {
          struct Address *last = TAILQ_LAST(al, AddressList);
          if (last && !last->personal)
          {
            terminate_buffer(comment, commentlen);
            last->personal = mutt_str_dup(comment);
          }
        }

        if (*s == ';')
        {
          /* add group terminator */
          mutt_addrlist_append(al, mutt_addr_new());
        }

        phraselen = 0;
        commentlen = 0;
        s++;
        break;

      case '(':
        if ((commentlen != 0) && (commentlen < (sizeof(comment) - 1)))
          comment[commentlen++] = ' ';
        s = next_token(s, comment, &commentlen, sizeof(comment) - 1);
        if (!s)
        {
          mutt_addrlist_clear(al);
          return 0;
        }
        break;

      case '"':
        if ((phraselen != 0) && (phraselen < (sizeof(phrase) - 1)))
          phrase[phraselen++] = ' ';
        s = parse_quote(s + 1, phrase, &phraselen, sizeof(phrase) - 1);
        if (!s)
        {
          mutt_addrlist_clear(al);
          return 0;
        }
        break;

      case ':':
      {
        struct Address *a = mutt_addr_new();
        terminate_buffer(phrase, phraselen);
        a->mailbox = mutt_str_dup(phrase);
        a->group = true;
        mutt_addrlist_append(al, a);
        phraselen = 0;
        commentlen = 0;
        s++;
        break;
      }

      case '<':
      {
        struct Address *a = mutt_addr_new();
        terminate_buffer(phrase, phraselen);
        a->personal = mutt_str_dup(phrase);
        s = parse_route_addr(s + 1, comment, &commentlen, sizeof(comment) - 1, a);
        if (!s)
        {
          mutt_addrlist_clear(al);
          mutt_addr_free(&a);
          return 0;
        }
        mutt_addrlist_append(al, a);
        phraselen = 0;
        commentlen = 0;
        parsed++;
        break;
      }

      default:
        if ((phraselen != 0) && (phraselen < (sizeof(phrase) - 1)) && ws_pending)
          phrase[phraselen++] = ' ';
        if (*s == '\\')
        {
          s++;
          if (*s && (phraselen < (sizeof(phrase) - 1)))
          {
            phrase[phraselen++] = *s;
            s++;
          }
        }
        s = next_token(s, phrase, &phraselen, sizeof(phrase) - 1);
        if (!s)
        {
          mutt_addrlist_clear(al);
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
    if (add_addrspec(al, phrase, comment, &commentlen, sizeof(comment) - 1))
    {
      parsed++;
    }
  }
  else if (commentlen != 0)
  {
    struct Address *last = TAILQ_LAST(al, AddressList);
    if (last && !last->personal)
    {
      terminate_buffer(comment, commentlen);
      last->personal = mutt_str_dup(comment);
    }
  }

  return parsed;
}

/**
 * mutt_addrlist_parse2 - Parse a list of email addresses
 * @param al Add to this List of Addresses
 * @param s String to parse
 * @retval num Number of parsed addresses
 *
 * Simple email addresses (without any personal name or grouping) can be
 * separated by whitespace or commas.
 */
int mutt_addrlist_parse2(struct AddressList *al, const char *s)
{
  if (!s || (*s == '\0'))
    return 0;

  int parsed = 0;

  /* check for a simple whitespace separated list of addresses */
  if (!strpbrk(s, "\"<>():;,\\"))
  {
    char *copy = mutt_str_dup(s);
    char *r = copy;
    while ((r = strtok(r, " \t")))
    {
      parsed += mutt_addrlist_parse(al, r);
      r = NULL;
    }
    FREE(&copy);
  }
  else
    parsed = mutt_addrlist_parse(al, s);

  return parsed;
}

/**
 * mutt_addrlist_qualify - Expand local names in an Address list using a hostname
 * @param al   Address list
 * @param host Hostname
 *
 * Any addresses containing a bare name will be expanded using the hostname.
 * e.g. "john", "example.com" -> 'john@example.com'. This function has no
 * effect if host is NULL or the empty string.
 */
void mutt_addrlist_qualify(struct AddressList *al, const char *host)
{
  if (!al || !host || (*host == '\0'))
    return;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (!a->group && a->mailbox && !strchr(a->mailbox, '@'))
    {
      char *p = mutt_mem_malloc(mutt_str_len(a->mailbox) + mutt_str_len(host) + 2);
      sprintf(p, "%s@%s", a->mailbox, host);
      FREE(&a->mailbox);
      a->mailbox = p;
    }
  }
}

/**
 * mutt_addr_cat - Copy a string and wrap it in quotes if it contains special characters
 * @param buf      Buffer for the result
 * @param buflen   Length of the result buffer
 * @param value    String to copy
 * @param specials Characters to lookup
 *
 * This function copies the string in the "value" parameter in the buffer
 * pointed to by "buf" parameter. If the input string contains any of the
 * characters specified in the "specials" parameters, the output string is
 * wrapped in double quoted. Additionally, any backslashes or quotes inside the
 * input string are backslash-escaped.
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
    mutt_str_copy(buf, tmp, buflen);
  }
  else
    mutt_str_copy(buf, value, buflen);
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

  p->personal = mutt_str_dup(addr->personal);
  p->mailbox = mutt_str_dup(addr->mailbox);
  p->group = addr->group;
  p->is_intl = addr->is_intl;
  p->intl_checked = addr->intl_checked;
  return p;
}

/**
 * mutt_addrlist_copy - Copy a list of addresses into another list
 * @param dst   Destination Address list
 * @param src   Source Address list
 * @param prune Skip groups if there are more addresses
 */
void mutt_addrlist_copy(struct AddressList *dst, const struct AddressList *src, bool prune)
{
  if (!dst || !src)
    return;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, src, entries)
  {
    struct Address *next = TAILQ_NEXT(a, entries);
    if (prune && a->group && (!next || !next->mailbox))
    {
      /* ignore this element of the list */
    }
    else
    {
      mutt_addrlist_append(dst, mutt_addr_copy(a));
    }
  }
}

/**
 * mutt_addr_valid_msgid - Is this a valid Message ID?
 * @param msgid Message ID
 * @retval true It is a valid message ID
 *
 * Incomplete. Only used to thwart the APOP MD5 attack
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

  if (!msgid || (*msgid == '\0'))
    return false;

  size_t l = mutt_str_len(msgid);
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
 * mutt_addrlist_equal - Compare two Address lists for equality
 * @param ala First Address
 * @param alb Second Address
 * @retval true Address lists are strictly identical
 */
bool mutt_addrlist_equal(const struct AddressList *ala, const struct AddressList *alb)
{
  if (!ala || !alb)
  {
    return !(ala || alb);
  }

  struct Address *ana = TAILQ_FIRST(ala);
  struct Address *anb = TAILQ_FIRST(alb);

  while (ana && anb)
  {
    if (!mutt_str_equal(ana->mailbox, anb->mailbox) ||
        !mutt_str_equal(ana->personal, anb->personal))
    {
      break;
    }

    ana = TAILQ_NEXT(ana, entries);
    anb = TAILQ_NEXT(anb, entries);
  }

  return !(ana || anb);
}

/**
 * mutt_addrlist_count_recips - Count the number of Addresses with valid recipients
 * @param al Address list
 * @retval num Number of valid Addresses
 *
 * An Address has a recipient if the mailbox is set and is not a group
 */
int mutt_addrlist_count_recips(const struct AddressList *al)
{
  if (!al)
    return 0;

  int c = 0;
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    c += (a->mailbox && !a->group);
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
  if (!mutt_istr_equal(a->mailbox, b->mailbox))
    return false;
  return true;
}

/**
 * mutt_addrlist_search - Search for an e-mail address in a list
 * @param haystack Address List
 * @param needle   Address containing the search email
 * @retval true If the Address is in the list
 */
bool mutt_addrlist_search(const struct AddressList *haystack, const struct Address *needle)
{
  if (!needle || !haystack)
    return false;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, haystack, entries)
  {
    if (mutt_addr_cmp(needle, a))
      return true;
  }
  return false;
}

/**
 * addr_is_intl - Does the Address have IDN components
 * @param a Address to check
 * @retval true Address contains IDN components
 */
static bool addr_is_intl(const struct Address *a)
{
  if (!a)
    return false;
  return a->intl_checked && a->is_intl;
}

/**
 * addr_is_local - Does the Address have NO IDN components
 * @param a Address to check
 * @retval true Address contains NO IDN components
 */
static bool addr_is_local(const struct Address *a)
{
  if (!a)
    return false;
  return a->intl_checked && !a->is_intl;
}

/**
 * addr_mbox_to_udomain - Split a mailbox name into user and domain
 * @param[in]  mbox   Mailbox name to split
 * @param[out] user   User
 * @param[out] domain Domain
 * @retval 0  Success
 * @retval -1 Error
 *
 * @warning The caller must free user and domain
 */
static int addr_mbox_to_udomain(const char *mbox, char **user, char **domain)
{
  if (!mbox || !user || !domain)
    return -1;

  char *ptr = strchr(mbox, '@');

  /* Fail if '@' is missing, at the start, or at the end */
  if (!ptr || (ptr == mbox) || (ptr[1] == '\0'))
    return -1;

  *user = mutt_strn_dup(mbox, ptr - mbox);
  *domain = mutt_str_dup(ptr + 1);

  return 0;
}

/**
 * addr_set_intl - Mark an Address as having IDN components
 * @param a            Address to modify
 * @param intl_mailbox Email address with IDN components
 */
static void addr_set_intl(struct Address *a, char *intl_mailbox)
{
  if (!a)
    return;

  FREE(&a->mailbox);
  a->mailbox = intl_mailbox;
  a->intl_checked = true;
  a->is_intl = true;
}

/**
 * addr_set_local - Mark an Address as having NO IDN components
 * @param a             Address
 * @param local_mailbox Email address with NO IDN components
 */
static void addr_set_local(struct Address *a, char *local_mailbox)
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

  if (!a->mailbox || addr_is_local(a))
    return a->mailbox;

  if (addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
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
 * @retval num    Length of the string written to buf
 *
 * If 'display' is set, then it doesn't matter if the transformation isn't
 * reversible.
 */
size_t mutt_addr_write(char *buf, size_t buflen, struct Address *addr, bool display)
{
  if (!buf || (buflen == 0) || !addr)
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
      len = mutt_str_copy(pbuf, addr->personal, buflen + 1);
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
    if (mutt_str_equal(addr->mailbox, "@"))
    {
      *pbuf = '\0';
    }
    else
    {
      const char *a = display ? mutt_addr_for_display(addr) : addr->mailbox;
      len = mutt_str_copy(pbuf, a, buflen + 1);
      pbuf += len;
      buflen -= len;
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
 * mutt_addrlist_write - Write an Address to a buffer
 * @param al      AddressList to display
 * @param buf     Buffer for the Address
 * @param buflen  Length of the buffer
 * @param display This address will be displayed to the user
 * @retval num    Length of the string written to buf
 *
 * If 'display' is set, then it doesn't matter if the transformation isn't
 * reversible.
 *
 * @note It is assumed that `buf` is nul terminated!
 */
size_t mutt_addrlist_write(const struct AddressList *al, char *buf, size_t buflen, bool display)
{
  if (!buf || (buflen == 0) || !al)
    return 0;

  size_t len = mutt_str_len(buf);
  if (len >= buflen)
  {
    return 0;
  }

  char *pbuf = buf + len;
  buflen -= len;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (len > 0)
    {
      if (buflen > 1)
      {
        *pbuf++ = ',';
        buflen--;
      }
      if (buflen > 1)
      {
        *pbuf++ = ' ';
        buflen--;
      }
    }

    if (buflen == 1)
    {
      break;
    }

    len = mutt_addr_write(pbuf, buflen, a, display);
    pbuf += len;
    buflen -= len;
  }

  *pbuf = '\0';
  return pbuf - buf;
}

/**
 * mutt_addrlist_write_list - Write Addresses to a List
 * @param al   AddressList to write
 * @param list List for the Addresses
 * @retval num Number of addresses written
 */
size_t mutt_addrlist_write_list(const struct AddressList *al, struct ListHead *list)
{
  if (!al || !list)
    return 0;

  char addr[256];
  size_t count = 0;
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    mutt_addr_write(addr, sizeof(addr), a, true);
    mutt_list_insert_tail(list, strdup(addr));
    count++;
  }

  return count;
}

/**
 * mutt_addrlist_write_file - Wrapper for mutt_write_address()
 * @param al      Address list
 * @param fp      File to write to
 * @param linelen Line length to use
 * @param display True if these addresses will be displayed to the user
 *
 * So we can handle very large recipient lists without needing a huge temporary
 * buffer in memory
 */
void mutt_addrlist_write_file(const struct AddressList *al, FILE *fp, int linelen, bool display)
{
  char buf[1024];
  int count = 0;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    buf[0] = '\0';
    mutt_addr_write(buf, sizeof(buf), a, display);
    size_t len = mutt_str_len(buf);
    if (count && (linelen + len > 74))
    {
      fputs("\n\t", fp);
      linelen = len + 8; /* tab is usually about 8 spaces... */
    }
    else
    {
      if (count && a->mailbox)
      {
        fputc(' ', fp);
        linelen++;
      }
      linelen += len;
    }
    fputs(buf, fp);
    struct Address *next = TAILQ_NEXT(a, entries);
    if (!a->group && next && next->mailbox)
    {
      linelen++;
      fputc(',', fp);
    }
    count++;
  }
  fputc('\n', fp);
}

/**
 * mutt_addr_to_intl - Convert an Address to Punycode
 * @param a Address to convert
 * @retval bool True on success, false otherwise
 */
bool mutt_addr_to_intl(struct Address *a)
{
  if (!a || !a->mailbox || addr_is_intl(a))
    return true;

  char *user = NULL;
  char *domain = NULL;
  if (addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
    return true;

  char *intl_mailbox = mutt_idna_local_to_intl(user, domain);

  FREE(&user);
  FREE(&domain);

  if (!intl_mailbox)
    return false;

  addr_set_intl(a, intl_mailbox);
  return true;
}

/**
 * mutt_addrlist_to_intl - Convert an Address list to Punycode
 * @param[in]  al  Address list to modify
 * @param[out] err Pointer for failed addresses
 * @retval 0  Success, all addresses converted
 * @retval -1 Error, err will be set to the failed address
 */
int mutt_addrlist_to_intl(struct AddressList *al, char **err)
{
  if (!al)
    return 0;

  int rc = 0;

  if (err)
    *err = NULL;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (!a->mailbox || addr_is_intl(a))
      continue;

    char *user = NULL;
    char *domain = NULL;
    if (addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
      continue;

    char *intl_mailbox = mutt_idna_local_to_intl(user, domain);

    FREE(&user);
    FREE(&domain);

    if (!intl_mailbox)
    {
      rc = -1;
      if (err && !*err)
        *err = mutt_str_dup(a->mailbox);
      continue;
    }

    addr_set_intl(a, intl_mailbox);
  }

  return rc;
}

/**
 * mutt_addr_to_local - Convert an Address from Punycode
 * @param a Address to convert
 * @retval bool True on success, false otherwise
 */
bool mutt_addr_to_local(struct Address *a)
{
  if (!a->mailbox)
  {
    return false;
  }

  if (addr_is_local(a))
  {
    return true;
  }

  char *user = NULL;
  char *domain = NULL;
  if (addr_mbox_to_udomain(a->mailbox, &user, &domain) == -1)
  {
    return false;
  }

  char *local_mailbox = mutt_idna_intl_to_local(user, domain, 0);
  FREE(&user);
  FREE(&domain);

  if (!local_mailbox)
  {
    return false;
  }

  addr_set_local(a, local_mailbox);
  return true;
}

/**
 * mutt_addrlist_to_local - Convert an Address list from Punycode
 * @param al Address list to modify
 * @retval 0 Always
 */
int mutt_addrlist_to_local(struct AddressList *al)
{
  if (!al)
    return 0;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    mutt_addr_to_local(a);
  }
  return 0;
}

/**
 * mutt_addrlist_dedupe - Remove duplicate addresses
 * @param al Address list to de-dupe
 *
 * Given a list of addresses, return a list of unique addresses
 */
void mutt_addrlist_dedupe(struct AddressList *al)
{
  if (!al)
    return;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (a->mailbox)
    {
      struct Address *a2 = TAILQ_NEXT(a, entries);
      struct Address *tmp = NULL;

      if (a2)
      {
        TAILQ_FOREACH_FROM_SAFE(a2, al, entries, tmp)
        {
          if (a2->mailbox && mutt_istr_equal(a->mailbox, a2->mailbox))
          {
            mutt_debug(LL_DEBUG2, "Removing %s\n", a2->mailbox);
            TAILQ_REMOVE(al, a2, entries);
            mutt_addr_free(&a2);
          }
        }
      }
    }
  }
}

/**
 * mutt_addrlist_remove_xrefs - Remove cross-references
 * @param a Reference AddressList
 * @param b AddressLis to trim
 *
 * Remove addresses from "b" which are contained in "a"
 */
void mutt_addrlist_remove_xrefs(const struct AddressList *a, struct AddressList *b)
{
  if (!a || !b)
    return;

  struct Address *aa = NULL, *ab = NULL, *tmp = NULL;

  TAILQ_FOREACH_SAFE(ab, b, entries, tmp)
  {
    TAILQ_FOREACH(aa, a, entries)
    {
      if (mutt_addr_cmp(aa, ab))
      {
        TAILQ_REMOVE(b, ab, entries);
        mutt_addr_free(&ab);
        break;
      }
    }
  }
}

/**
 * mutt_addrlist_clear - Unlink and free all Address in an AddressList
 * @param al AddressList
 *
 * @note After this call, the AddressList is reinitialized and ready for reuse.
 */
void mutt_addrlist_clear(struct AddressList *al)
{
  if (!al)
    return;

  struct Address *a = TAILQ_FIRST(al), *next = NULL;
  while (a)
  {
    next = TAILQ_NEXT(a, entries);
    mutt_addr_free(&a);
    a = next;
  }
  TAILQ_INIT(al);
}

/**
 * mutt_addrlist_append - Append an Address to an AddressList
 * @param al AddressList
 * @param a  Address
 */
void mutt_addrlist_append(struct AddressList *al, struct Address *a)
{
  if (al && a)
    TAILQ_INSERT_TAIL(al, a, entries);
}

/**
 * mutt_addrlist_prepend - Prepend an Address to an AddressList
 * @param al AddressList
 * @param a  Address
 */
void mutt_addrlist_prepend(struct AddressList *al, struct Address *a)
{
  if (al && a)
    TAILQ_INSERT_HEAD(al, a, entries);
}

/**
 * mutt_addr_uses_unicode - Does this address use Unicode character
 * @param str Address string to check
 * @retval true If the string uses 8-bit characters
 */
bool mutt_addr_uses_unicode(const char *str)
{
  if (!str)
    return false;

  while (*str)
  {
    if ((unsigned char) *str & (1 << 7))
      return true;
    str++;
  }

  return false;
}

/**
 * mutt_addrlist_uses_unicode - Do any of a list of addresses use Unicode characters
 * @param al Address list to check
 * @retval true If any use 8-bit characters
 */
bool mutt_addrlist_uses_unicode(const struct AddressList *al)
{
  if (!al)
  {
    return false;
  }

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    if (a->mailbox && !a->group && mutt_addr_uses_unicode(a->mailbox))
      return true;
  }
  return false;
}
