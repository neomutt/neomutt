/**
 * @file
 * Manage IMAP BODYSTRUCTURE parsing
 *
 * @authors
 * Copyright (C) 2017 Mehdi Abaakouk <sileht@sileht.net>
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

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "globals.h"
#include "protos.h"

/**
 * body_struct_parse_value - Read one value from the server string
 * @param adata     Server data
 * @param s         String to read from
 * @param dest      Output buffer (will be malloc'd)
 * @param allow_nil Is the string "NIL" allowed?
 * @retval ptr Remaining string to be parsed
 *
 * @note Caller must free dest
 */
static char *body_struct_parse_value(struct ImapAccountData *adata, char *s, char **dest, bool allow_nil)
{
  char ctmp;
  char *flag_word = NULL;
  unsigned int bytes;

  if (*s == '{') // a literal
  {
    if (imap_get_literal_count(s, &bytes) == 0)
    {
      imap_read_literal_string(dest, adata, bytes);

      if (imap_cmd_step(adata) != IMAP_RES_CONTINUE)
        return NULL;

      s = adata->buf;
      SKIPWS(s);
      return s;
    }
    else
    {
      mutt_debug(1, "fail to parse literal count: %s\n", s);
      return NULL;
    }
  }
  else if (allow_nil && (mutt_str_strncasecmp("NIL", s, 3) == 0))
  {
    s += 3;
    SKIPWS(s);
    return s;
  }
  else if (*s == '"') // a string
  {
    s++;

    flag_word = s;

    while (*s && (*s != '"'))
    {
      if (*s == '\\') /* skip escaped char */
        s++;
      s++;
    }

    if (*s != '"')
      return NULL;
  }
  else // int
  {
    flag_word = s;
    while ((*s >= 48) && (*s <= 57))
      s++;
    if (!*s)
      return NULL;
  }
  ctmp = *s;
  *s = '\0';
  *dest = mutt_str_strdup(flag_word);
  *s = ctmp;
  if (*s == '"')
    s++;
  SKIPWS(s);
  return s;
}

/**
 * body_struct_skip_value - Skip the next value in the server string
 * @param adata     Server data
 * @param s         String to read from
 * @param allow_nil Is the string "NIL" allowed?
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_skip_value(struct ImapAccountData *adata, char *s, bool allow_nil)
{
  char *tmp = NULL;
  s = body_struct_parse_value(adata, s, &tmp, allow_nil);
  FREE(&tmp);
  return s;
}

/**
 * body_struct_parse_parameters - Turn a server string into parameters
 * @param adata  Server data
 * @param params List to store the Parameters
 * @param s      String to read from
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_parse_parameters(struct ImapAccountData *adata,
                                          struct ParameterList *params, char *s)
{
  if (mutt_str_strncasecmp("NIL", s, 3) == 0)
  {
    s += 3;
    SKIPWS(s);
    return s;
  }

  if (*s != '(')
  {
    mutt_debug(1, "missing param open parenthesis: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);

  while (*s && (*s != ')'))
  {
    struct Parameter *new = mutt_param_new();
    s = body_struct_parse_value(adata, s, &new->attribute, false);
    if (!s)
      return NULL;
    s = body_struct_parse_value(adata, s, &new->value, false);
    if (!s)
      return NULL;

    TAILQ_INSERT_TAIL(params, new, entries);
  }

  if (*s != ')')
  {
    mutt_debug(1, "missing param close parenthesis: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);
  return s;
}

/**
 * body_struct_skip_parameters - Skip parameters in the server string
 * @param adata Server data
 * @param s     String to read from
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_skip_parameters(struct ImapAccountData *adata, char *s)
{
  struct ParameterList params = TAILQ_HEAD_INITIALIZER(params);
  s = body_struct_parse_parameters(adata, &params, s);
  mutt_param_free(&params);
  return s;
}

/**
 * body_struct_parse_common_extension - Parse common parameter types
 * @param adata Server data
 * @param body  Email to attach Parameters to
 * @param s     String to read from
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_parse_common_extension(struct ImapAccountData *adata,
                                                struct Body *body, char *s)
{
  struct ParameterList params = TAILQ_HEAD_INITIALIZER(params);

  // body disposition
  if (mutt_str_strncasecmp("NIL", s, 3) == 0)
  {
    s += 3;
    SKIPWS(s);
  }
  else
  {
    if (*s != '(')
    {
      mutt_debug(1, "missing disposition open parenthesis: %s\n", s);
      return NULL;
    }
    s++;
    SKIPWS(s);

    char *tmp = NULL;
    s = body_struct_parse_value(adata, s, &tmp, false);
    if (!s)
    {
      mutt_debug(1, "fail to parse disposition type: %s\n", s);
      return NULL;
    }

    if (mutt_str_strcasecmp("inline", tmp) == 0)
      body->disposition = DISP_INLINE;
    else if (mutt_str_strcasecmp("form-data", tmp) == 0)
      body->disposition = DISP_FORM_DATA;
    else
      body->disposition = DISP_ATTACH;
    FREE(&tmp);

    s = body_struct_parse_parameters(adata, &params, s);
    if (!s)
    {
      mutt_debug(1, "fail to parse disposition params: %s\n", s);
      return NULL;
    }
    tmp = mutt_param_get(&params, "filename");
    if (tmp)
      body->filename = mutt_str_strdup(tmp);
    tmp = mutt_param_get(&params, "name");
    if (tmp)
      body->form_name = mutt_str_strdup(tmp);
    mutt_param_free(&params);

    if (*s != ')')
    {
      mutt_debug(1, "missing disposition close parenthesis: %s\n", s);
      return NULL;
    }
    s++;
    SKIPWS(s);
  }

  // body language (can be string or list of string)
  if (*s == '(')
  {
    s = body_struct_skip_parameters(adata, s);
    if (!s)
    {
      mutt_debug(1, "fail to parse language params: %s\n", s);
      return NULL;
    }
  }
  else
  {
    s = body_struct_skip_value(adata, s, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse language string: %s\n", s);
      return NULL;
    }
  }

  // body location uri
  s = body_struct_skip_value(adata, s, true);
  if (!s)
  {
    mutt_debug(1, "fail to parse location uri: %s\n", s);
    return NULL;
  }

  return s;
}

/**
 * body_struct_parse_addresses - Parse list of email addresses
 * @param adata Server data
 * @param s     String to read from
 * @param dest  Addresses will be saved here
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_parse_addresses(struct ImapAccountData *adata, char *s,
                                         struct AddressList *dest)
{
  if (mutt_str_strncasecmp("NIL", s, 3) == 0)
  {
    s += 3;
    SKIPWS(s);
    return s;
  }
  else if (*s != '(')
  {
    mutt_debug(1, "fail to parse open email parenthesis: %s\n", s);
    return NULL;
  }

  s++;
  SKIPWS(s);

  while (*s == '(')
  {
    s++;
    SKIPWS(s);

    struct Address *new = mutt_addr_new();

    s = body_struct_parse_value(adata, s, &new->personal, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse full name: %s\n", s);
      return NULL;
    }
    if (new->personal)
      rfc2047_decode(&new->personal);

    s = body_struct_skip_value(adata, s, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse source route: %s\n", s);
      return NULL;
    }
    s = body_struct_parse_value(adata, s, &new->mailbox, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse leftpart email: %s\n", s);
      return NULL;
    }
    if (new->mailbox)
      rfc2047_decode(&new->mailbox);
    char *tmp = NULL;
    s = body_struct_parse_value(adata, s, &tmp, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse rightpart email: %s\n", s);
      return NULL;
    }
    if (tmp)
    {
      rfc2047_decode(&tmp);
      mutt_str_append_item(&new->mailbox, tmp, '@');
    }

    /* TODO(sileht): I don't get it for now.  [RFC-2822] group syntax is
     * indicated by a special form of address structure in which the host name
     * field is NIL.  If the mailbox name field is also NIL, this is an end of
     * group marker (semi-colon in RFC 822 syntax).  If the mailbox name field
     * is non-NIL, this is a start of group marker, and the mailbox name field
     * holds the group name phrase.
     */

    if (*s != ')')
    {
      mutt_debug(1, "Unterminated email structure: %s\n", s);
      return NULL;
    }
    s++;
    SKIPWS(s);

    TAILQ_INSERT_TAIL(dest, new, entries);
  }

  if (*s != ')')
  {
    mutt_debug(1, "Unterminated email list structure: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);
  return s;
}

/**
 * body_struct_parse_envelope - Parse email header from the server string
 * @param adata Server data
 * @param body  Email to attach Parameters to
 * @param s     String to read from
 * @retval ptr Remaining string to be parsed
 */
static char *body_struct_parse_envelope(struct ImapAccountData *adata, struct Body *body, char *s)
{
  struct Envelope *e = mutt_env_new();
  body->email = email_new();
  body->email->env = e;

  if (*s != '(')
  {
    mutt_debug(1, "fail to parse open parenthesis: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);

  s = body_struct_skip_value(adata, s, true);
  if (!s)
  {
    mutt_debug(1, "fail to parse date: %s\n", s);
    return NULL;
  }

  // subject
  s = body_struct_parse_value(adata, s, &e->subject, true);
  if (!s)
  {
    mutt_debug(1, "fail to parse subject: %s\n", s);
    return NULL;
  }
  if (e->subject)
  {
    rfc2047_decode(&e->subject);

    regmatch_t pmatch[1];

    if (C_ReplyRegex && (regexec(C_ReplyRegex->regex, e->subject, 1, pmatch, 0) == 0))
      e->real_subj = e->subject + pmatch[0].rm_eo;
    else
      e->real_subj = e->subject;
  }

  mutt_debug(1, "XXXXXXXXXX SUBJECT%s\n", s);

  // emails
  s = body_struct_parse_addresses(adata, s, &e->from);
  if (!s)
  {
    mutt_debug(1, "fail to parse from: %s\n", s);
    return NULL;
  }
  s = body_struct_parse_addresses(adata, s, &e->sender);
  if (!s)
  {
    mutt_debug(1, "fail to parse sender: %s\n", s);
    return NULL;
  }
  s = body_struct_parse_addresses(adata, s, &e->reply_to);
  if (!s)
  {
    mutt_debug(1, "fail to parse reply_to: %s\n", s);
    return NULL;
  }
  s = body_struct_parse_addresses(adata, s, &e->to);
  if (!s)
  {
    mutt_debug(1, "fail to parse to: %s\n", s);
    return NULL;
  }
  s = body_struct_parse_addresses(adata, s, &e->cc);
  if (!s)
  {
    mutt_debug(1, "fail to parse cc: %s\n", s);
    return NULL;
  }
  s = body_struct_parse_addresses(adata, s, &e->bcc);
  if (!s)
  {
    mutt_debug(1, "fail to parse bcc: %s\n", s);
    return NULL;
  }

  // in-reply-to
  char *tmp = NULL;
  s = body_struct_parse_value(adata, s, &tmp, true);
  if (!s)
  {
    mutt_debug(1, "fail to parse in-reply-to: %s\n", s);
    return NULL;
  }
  if (tmp)
  {
    mutt_list_free(&e->in_reply_to);
    parse_references(&e->in_reply_to, tmp);
    FREE(&tmp);
  }

  // message-id
  s = body_struct_parse_value(adata, s, &e->message_id, true);
  if (!s)
  {
    mutt_debug(1, "fail to parse message-id: %s\n", s);
    return NULL;
  }

  if (*s != ')')
  {
    mutt_debug(1, "Unterminated BODYSTRUCTURE response: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);
  return s;
}

/**
 * body_struct_parse - Parse a BODYSTRUCTURE string
 * @param adata Server data
 * @param body  Email to attach Parameters to
 * @param s     String to read from
 * @retval ptr Remaining string to be parsed
 */
char *body_struct_parse(struct ImapAccountData *adata, struct Body *body, char *s)
{
  struct Body *new = NULL, *cur = NULL;

  if (*s != '(')
  {
    mutt_debug(1, "fail to parse open parenthesis: %s\n", s);
    return NULL;
  }
  s++;
  SKIPWS(s);

  body->encoding = ENC_7BIT; /* default from RFC1521 */
  body->type = TYPE_TEXT;
  body->disposition = DISP_INLINE;

  /* Enforce it to -1 length, until we can also calculate hdr_offset and offset
   * mutt_rfc822_read_header() will override it with the correct value. */
  body->length = -1;

  if (*s == '(') // multiparts
  {
    body->type = TYPE_MULTIPART;

    while (*s == '(')
    {
      new = mutt_body_new();
      s = body_struct_parse(adata, new, s);
      if (!s)
        return NULL; // Error have been already logged

      if (body->parts)
      {
        cur->next = new;
        cur = cur->next;
      }
      else
      {
        body->parts = new;
        cur = new;
      }
    }

    s = body_struct_parse_value(adata, s, &body->subtype, false);
    if (!s)
    {
      mutt_debug(1, "fail to parse multipart subtype: %s\n", s);
      return NULL;
    }

    if (*s && (*s != ')')) // optional extension data
    {
      s = body_struct_parse_parameters(adata, &body->parameter, s);
      if (!s)
      {
        mutt_debug(1, "fail to parse extension parameters: %s\n", s);
        return NULL;
      }
      s = body_struct_parse_common_extension(adata, body, s);
      if (!s)
        return NULL; // Error have been already logged
    }
  }
  else
  {
    char *type = NULL;
    s = body_struct_parse_value(adata, s, &type, false);
    if (!s)
    {
      mutt_debug(1, "fail to parse type: %s\n", s);
      return NULL;
    }
    body->type = mutt_check_mime_type(type);
    if (body->type == TYPE_OTHER)
      body->xtype = mutt_str_strdup(type);

    s = body_struct_parse_value(adata, s, &body->subtype, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse subtype: %s\n", s);
      return NULL;
    }
    s = body_struct_parse_parameters(adata, &body->parameter, s);
    if (!s)
    {
      mutt_debug(1, "fail to parse parameters: %s\n", s);
      return NULL;
    }

    s = body_struct_skip_value(adata, s, true);
    if (!s) // body_id
    {
      mutt_debug(1, "fail to parse body id: %s\n", s);
      return NULL;
    }
    s = body_struct_parse_value(adata, s, &body->description, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse description: %s\n", s);
      return NULL;
    }
    rfc2047_decode(&body->description);

    char *encoding = NULL;
    s = body_struct_parse_value(adata, s, &encoding, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse encoding: %s\n", s);
      return NULL;
    }
    if (encoding)
      body->encoding = mutt_check_encoding(encoding);

    s = body_struct_skip_value(adata, s, true);
    if (!s)
    {
      mutt_debug(1, "fail to parse length: %s\n", s);
      return NULL;
    }

    if ((body->type == TYPE_MESSAGE) &&
        mutt_str_strcasecmp(body->subtype, "RFC822") == 0)
    {
      s = body_struct_parse_envelope(adata, body, s);
      if (!s)
        return NULL; // Error have been already logged

      body->parts = mutt_body_new();
      s = body_struct_parse(adata, body->parts, s);
      if (!s)
        return NULL; // Error have been already logged

      // body line
      s = body_struct_skip_value(adata, s, true);
      if (!s)
      {
        mutt_debug(1, "fail to parse line number: %s\n", s);
        return NULL;
      }
    }
    else if (body->type == TYPE_TEXT)
    {
      // body line
      s = body_struct_skip_value(adata, s, true);
      if (!s)
      {
        mutt_debug(1, "fail to parse line number: %s\n", s);
        return NULL;
      }

      if (!mutt_param_get(&body->parameter, "charset"))
      {
        mutt_param_set(&body->parameter, "charset",
                       (C_AssumedCharset && *C_AssumedCharset) ? mutt_ch_get_default_charset() :
                                                             "us-ascii");
      }
    }

    if (!body->subtype)
    {
      /* Some older non-MIME mailers (i.e., mailtool, elm) have a content-type
       * field, so we can attempt to convert the type to Body here.
       */
      if (body->type == TYPE_TEXT)
        body->subtype = mutt_str_strdup("plain");
      else if (body->type == TYPE_AUDIO)
        body->subtype = mutt_str_strdup("basic");
      else if (body->type == TYPE_MESSAGE)
        body->subtype = mutt_str_strdup("rfc822");
      else if (body->type == TYPE_OTHER)
      {
        char buffer[128];

        body->type = TYPE_APPLICATION;
        snprintf(buffer, sizeof(buffer), "x-%s", s);
        body->subtype = mutt_str_strdup(buffer);
      }
      else
        body->subtype = mutt_str_strdup("x-unknown");
    }

    if (*s && (*s != ')')) // optionalue startxtension data
    {
      // body md5
      s = body_struct_skip_value(adata, s, true);
      if (!s)
      {
        mutt_debug(1, "fail to parse md5: %s\n", s);
        return NULL;
      }

      s = body_struct_parse_common_extension(adata, body, s);
      if (!s)
        return NULL; // Error have been already logged
    }
  }

  if (*s != ')')
  {
    mutt_debug(1, "Unterminated BODYSTRUCTURE response: %s\n", s);
    return NULL;
  }

  s++;
  SKIPWS(s);
  return s;
}
