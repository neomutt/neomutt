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
 * @page address Representation of an email address
 *
 * Representation of an email address
 *
 * | Data             | Description
 * | :--------------- | :--------------------------------------------------
 * | #AddressError    | An out-of-band error code
 * | #AddressErrors   | Messages for the error codes in #AddressError
 * | #AddressSpecials | Characters with special meaning for email addresses
 *
 * | Function                     | Description
 * | :--------------------------- | :---------------------------------------------------------
 * | mutt_addr_append()           | Append one list of addresses onto another
 * | mutt_addr_cat()              | Copy a string and escape the specified characters
 * | mutt_addr_cmp()              | Compare two e-mail addresses
 * | mutt_addr_cmp_strict()       | Strictly compare two Address lists
 * | mutt_addr_copy()             | Copy the real address
 * | mutt_addr_copy_list()        | Copy a list of addresses
 * | mutt_addr_free()             | Free a list of Addresses
 * | mutt_addr_has_recips()       | Count the number of Addresses with valid recipients
 * | mutt_addr_new()              | Create a new Address
 * | mutt_addr_parse_list()       | Parse a list of email addresses
 * | mutt_addr_parse_list2()      | Parse a list of email addresses
 * | mutt_addr_qualify()          | Expand local names in an Address list using a hostname
 * | mutt_addr_remove_from_list() | Remove an Address from a list
 * | mutt_addr_search()           | Search for an e-mail address in a list
 * | mutt_addr_valid_msgid()      | Is this a valid Message ID?
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "address.h"
#include "mutt_idna.h"

/**
 * AddressSpecials - Characters with special meaning for email addresses
 */
const char AddressSpecials[] = "@.,:;<>[]\\\"()";

/**
 * is_special - Is this character special to an email address?
 */
#define is_special(x) strchr(AddressSpecials, x)

/**
 * free_address - Free a single Address
 * @param a Address to free
 *
 * @note This doesn't alter the links if the Address is in a list.
 */
static void free_address(struct Address **a)
{
  if (!a || !*a)
    return;
  FREE(&(*a)->personal);
  FREE(&(*a)->mailbox);
  FREE(&(*a));
}

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
  if (level)
  {
    AddressError = ERR_MISMATCH_PAREN;
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
      return (s + 1);
    (*tokenlen)++;
    s++;
  }
  AddressError = ERR_MISMATCH_QUOTE;
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
    return (parse_comment(s + 1, token, tokenlen, tokenmax));
  if (*s == '"')
    return (parse_quote(s + 1, token, tokenlen, tokenmax));
  if (*s && is_special(*s))
  {
    if (*tokenlen < tokenmax)
      token[(*tokenlen)++] = *s;
    return (s + 1);
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

    if (strchr(nonspecial, *s) == NULL && is_special(*s))
      return s;

    if (*s == '(')
    {
      if (*commentlen && *commentlen < commentmax)
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
  char token[LONG_STRING];
  size_t tokenlen = 0;

  s = mutt_str_skip_email_wsp(s);

  /* find the end of the route */
  if (*s == '@')
  {
    while (s && *s == '@')
    {
      if (tokenlen < sizeof(token) - 1)
        token[tokenlen++] = '@';
      s = parse_mailboxdomain(s + 1, ",.\\[](", token, &tokenlen,
                              sizeof(token) - 1, comment, commentlen, commentmax);
    }
    if (!s || *s != ':')
    {
      AddressError = ERR_BAD_ROUTE;
      return NULL; /* invalid route */
    }

    if (tokenlen < sizeof(token) - 1)
      token[tokenlen++] = ':';
    s++;
  }

  if ((s = parse_address(s, token, &tokenlen, sizeof(token) - 1, comment,
                         commentlen, commentmax, addr)) == NULL)
    return NULL;

  if (*s != '>')
  {
    AddressError = ERR_BAD_ROUTE_ADDR;
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
  char token[LONG_STRING];
  size_t tokenlen = 0;

  s = parse_address(s, token, &tokenlen, sizeof(token) - 1, comment, commentlen,
                    commentmax, addr);
  if (s && *s && *s != ',' && *s != ';')
  {
    AddressError = ERR_BAD_ADDR_SPEC;
    return NULL;
  }
  return s;
}

/**
 * add_addrspec - Parse an email address and add an Address to a list
 * @param[in]  top        Top of Address list
 * @param[in]  last       End of Address list
 * @param[in]  phrase     String to parse
 * @param[out] comment    Buffer for any comments
 * @param[out] commentlen Length of any comments
 * @param[in]  commentmax Length of the comments buffer
 */
static void add_addrspec(struct Address **top, struct Address **last, const char *phrase,
                         char *comment, size_t *commentlen, size_t commentmax)
{
  struct Address *cur = mutt_addr_new();

  if (parse_addr_spec(phrase, comment, commentlen, commentmax, cur) == NULL)
  {
    mutt_addr_free(&cur);
    return;
  }

  if (*last)
    (*last)->next = cur;
  else
    *top = cur;
  *last = cur;
}

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
  "out of memory",   "mismatched parenthesis", "mismatched quotes",
  "bad route in <>", "bad address in <>",      "bad address spec",
};

/**
 * mutt_addr_new - Create a new Address
 * @retval ptr Newly allocated Address
 *
 * Free the result with free_address() or mutt_addr_free()
 */
struct Address *mutt_addr_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Address));
}

/**
 * mutt_addr_remove_from_list - Remove an Address from a list
 * @param a       Address list
 * @param mailbox Email address to match
 * @retval  0 Success
 * @retval -1 Error, or email not found
 */
int mutt_addr_remove_from_list(struct Address **a, const char *mailbox)
{
  struct Address *p = NULL, *last = NULL, *t = NULL;
  int rc = -1;

  p = *a;
  last = NULL;
  while (p)
  {
    if (mutt_str_strcasecmp(mailbox, p->mailbox) == 0)
    {
      if (last)
        last->next = p->next;
      else
        (*a) = p->next;
      t = p;
      p = p->next;
      free_address(&t);
      rc = 0;
    }
    else
    {
      last = p;
      p = p->next;
    }
  }

  return rc;
}

/**
 * mutt_addr_free - Free a list of Addresses
 * @param p Top of the list
 */
void mutt_addr_free(struct Address **p)
{
  struct Address *t = NULL;

  while (*p)
  {
    t = *p;
    *p = (*p)->next;
    free_address(&t);
  }
}

/**
 * mutt_addr_parse_list - Parse a list of email addresses
 * @param top List to append addresses
 * @param s   String to parse
 * @retval ptr  Top of the address list
 * @retval NULL Error
 */
struct Address *mutt_addr_parse_list(struct Address *top, const char *s)
{
  int ws_pending, nl;
  const char *ps = NULL;
  char comment[LONG_STRING], phrase[LONG_STRING];
  size_t phraselen = 0, commentlen = 0;
  struct Address *cur = NULL, *last = NULL;

  AddressError = 0;

  last = top;
  while (last && last->next)
    last = last->next;

  ws_pending = mutt_str_is_email_wsp(*s);
  if ((nl = mutt_str_strlen(s)))
    nl = s[nl - 1] == '\n';

  s = mutt_str_skip_email_wsp(s);
  while (*s)
  {
    if (*s == ',')
    {
      if (phraselen)
      {
        terminate_buffer(phrase, phraselen);
        add_addrspec(&top, &last, phrase, comment, &commentlen, sizeof(comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
        terminate_buffer(comment, commentlen);
        last->personal = mutt_str_strdup(comment);
      }

      commentlen = 0;
      phraselen = 0;
      s++;
    }
    else if (*s == '(')
    {
      if (commentlen && commentlen < sizeof(comment) - 1)
        comment[commentlen++] = ' ';
      ps = next_token(s, comment, &commentlen, sizeof(comment) - 1);
      if (!ps)
      {
        mutt_addr_free(&top);
        return NULL;
      }
      s = ps;
    }
    else if (*s == '"')
    {
      if (phraselen && phraselen < sizeof(phrase) - 1)
        phrase[phraselen++] = ' ';
      ps = parse_quote(s + 1, phrase, &phraselen, sizeof(phrase) - 1);
      if (!ps)
      {
        mutt_addr_free(&top);
        return NULL;
      }
      s = ps;
    }
    else if (*s == ':')
    {
      cur = mutt_addr_new();
      terminate_buffer(phrase, phraselen);
      cur->mailbox = mutt_str_strdup(phrase);
      cur->group = 1;

      if (last)
        last->next = cur;
      else
        top = cur;
      last = cur;

      phraselen = 0;
      commentlen = 0;
      s++;
    }
    else if (*s == ';')
    {
      if (phraselen)
      {
        terminate_buffer(phrase, phraselen);
        add_addrspec(&top, &last, phrase, comment, &commentlen, sizeof(comment) - 1);
      }
      else if (commentlen && last && !last->personal)
      {
        terminate_buffer(comment, commentlen);
        last->personal = mutt_str_strdup(comment);
      }

      /* add group terminator */
      cur = mutt_addr_new();
      if (last)
      {
        last->next = cur;
        last = cur;
      }

      phraselen = 0;
      commentlen = 0;
      s++;
    }
    else if (*s == '<')
    {
      terminate_buffer(phrase, phraselen);
      cur = mutt_addr_new();
      if (phraselen)
        cur->personal = mutt_str_strdup(phrase);
      ps = parse_route_addr(s + 1, comment, &commentlen, sizeof(comment) - 1, cur);
      if (!ps)
      {
        mutt_addr_free(&top);
        mutt_addr_free(&cur);
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
      if (phraselen && phraselen < sizeof(phrase) - 1 && ws_pending)
        phrase[phraselen++] = ' ';
      ps = next_token(s, phrase, &phraselen, sizeof(phrase) - 1);
      if (!ps)
      {
        mutt_addr_free(&top);
        return NULL;
      }
      s = ps;
    }
    ws_pending = mutt_str_is_email_wsp(*s);
    s = mutt_str_skip_email_wsp(s);
  }

  if (phraselen)
  {
    terminate_buffer(phrase, phraselen);
    terminate_buffer(comment, commentlen);
    add_addrspec(&top, &last, phrase, comment, &commentlen, sizeof(comment) - 1);
  }
  else if (commentlen && last && !last->personal)
  {
    terminate_buffer(comment, commentlen);
    last->personal = mutt_str_strdup(comment);
  }

  return top;
}

/**
 * mutt_addr_parse_list2 - Parse a list of email addresses
 * @param p Add to this List of Addresses
 * @param s String to parse
 * @retval ptr Head of the list of addresses
 *
 * The email addresses can be separated by whitespace or commas.
 */
struct Address *mutt_addr_parse_list2(struct Address *p, const char *s)
{
  const char *q = NULL;

  /* check for a simple whitespace separated list of addresses */
  q = strpbrk(s, "\"<>():;,\\");
  if (!q)
  {
    char tmp[HUGE_STRING];
    char *r = NULL;

    mutt_str_strfcpy(tmp, s, sizeof(tmp));
    r = tmp;
    while ((r = strtok(r, " \t")) != NULL)
    {
      p = mutt_addr_parse_list(p, r);
      r = NULL;
    }
  }
  else
    p = mutt_addr_parse_list(p, s);

  return p;
}

/**
 * mutt_addr_qualify - Expand local names in an Address list using a hostname
 * @param addr Address list
 * @param host Hostname
 *
 * Any addresses containing a bare name will be expanded using the hostname.
 * e.g. "john", "example.com" -> 'john@example.com'.
 */
void mutt_addr_qualify(struct Address *addr, const char *host)
{
  char *p = NULL;

  for (; addr; addr = addr->next)
  {
    if (!addr->group && addr->mailbox && strchr(addr->mailbox, '@') == NULL)
    {
      p = mutt_mem_malloc(mutt_str_strlen(addr->mailbox) + mutt_str_strlen(host) + 2);
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
  if (strpbrk(value, specials))
  {
    char tmp[256], *pc = tmp;
    size_t tmplen = sizeof(tmp) - 3;

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
struct Address *mutt_addr_copy(struct Address *addr)
{
  struct Address *p = mutt_addr_new();

  p->personal = mutt_str_strdup(addr->personal);
  p->mailbox = mutt_str_strdup(addr->mailbox);
  p->group = addr->group;
  p->is_intl = addr->is_intl;
  p->intl_checked = addr->intl_checked;
  return p;
}

/**
 * mutt_addr_copy_list - Copy a list of addresses
 * @param addr  Address list
 * @param prune Skip groups if there are more addresses
 * @retval ptr New Address list
 */
struct Address *mutt_addr_copy_list(struct Address *addr, bool prune)
{
  struct Address *top = NULL, *last = NULL;

  for (; addr; addr = addr->next)
  {
    if (prune && addr->group && (!addr->next || !addr->next->mailbox))
    {
      /* ignore this element of the list */
    }
    else if (last)
    {
      last->next = mutt_addr_copy(addr);
      last = last->next;
    }
    else
      top = last = mutt_addr_copy(addr);
  }
  return top;
}

/**
 * mutt_addr_append - Append one list of addresses onto another
 * @param a     Destination Address list
 * @param b     Source Address list
 * @param prune Skip groups if there are more addresses
 * @retval ptr Last Address in the combined list
 *
 * Append the Source onto the end of the Destination Address list.
 */
struct Address *mutt_addr_append(struct Address **a, struct Address *b, bool prune)
{
  struct Address *tmp = *a;

  while (tmp && tmp->next)
    tmp = tmp->next;
  if (!b)
    return tmp;
  if (tmp)
    tmp->next = mutt_addr_copy_list(b, prune);
  else
    tmp = *a = mutt_addr_copy_list(b, prune);
  while (tmp && tmp->next)
    tmp = tmp->next;
  return tmp;
}

/**
 * mutt_addr_valid_msgid - Is this a valid Message ID?
 * @param msgid Message ID
 * @retval bool True if it is valid
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

  size_t l;

  if (!msgid || !*msgid)
    return false;

  l = mutt_str_strlen(msgid);
  if (l < 5) /* <atom@atom> */
    return false;
  if (msgid[0] != '<' || msgid[l - 1] != '>')
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
 * mutt_addr_cmp_strict - Strictly compare two Address lists
 * @param a First Address
 * @param b Second Address
 * @retval true Address lists are strictly identical
 */
bool mutt_addr_cmp_strict(const struct Address *a, const struct Address *b)
{
  while (a && b)
  {
    if ((mutt_str_strcmp(a->mailbox, b->mailbox) != 0) ||
        (mutt_str_strcmp(a->personal, b->personal) != 0))
    {
      return false;
    }

    a = a->next;
    b = b->next;
  }
  if (a || b)
    return false;

  return true;
}

/**
 * mutt_addr_has_recips - Count the number of Addresses with valid recipients
 * @param a Address list
 * @retval num Number of valid Addresses
 *
 * An Address has a recipient if the mailbox or group is set.
 */
int mutt_addr_has_recips(struct Address *a)
{
  int c = 0;

  for (; a; a = a->next)
  {
    if (!a->mailbox || a->group)
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
bool mutt_addr_cmp(struct Address *a, struct Address *b)
{
  if (!a->mailbox || !b->mailbox)
    return false;
  if (mutt_str_strcasecmp(a->mailbox, b->mailbox) != 0)
    return false;
  return true;
}

/**
 * mutt_addr_search - Search for an e-mail address in a list
 * @param a   Address containing the search email
 * @param lst Address List
 * @retval true If the Address is in the list
 */
bool mutt_addr_search(struct Address *a, struct Address *lst)
{
  for (; lst; lst = lst->next)
  {
    if (mutt_addr_cmp(a, lst))
      return true;
  }
  return false;
}
