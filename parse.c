/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 1996-2000,2012-2013 Michael R. Elkins <me@mutt.org>
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
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "body.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "url.h"

struct Context;

/**
 * mutt_read_rfc822_line - Read a header line from a file
 *
 * Reads an arbitrarily long header field, and looks ahead for continuation
 * lines.  ``line'' must point to a dynamically allocated string; it is
 * increased if more space is required to fit the whole line.
 */
char *mutt_read_rfc822_line(FILE *f, char *line, size_t *linelen)
{
  char *buf = line;
  int ch;
  size_t offset = 0;

  while (true)
  {
    if (fgets(buf, *linelen - offset, f) == NULL || /* end of file or */
        (ISSPACE(*line) && !offset))                /* end of headers */
    {
      *line = 0;
      return line;
    }

    const size_t len = mutt_str_strlen(buf);
    if (!len)
      return line;

    buf += len - 1;
    if (*buf == '\n')
    {
      /* we did get a full line. remove trailing space */
      while (ISSPACE(*buf))
        *buf-- = 0; /* we cannot come beyond line's beginning because
                         * it begins with a non-space */

      /* check to see if the next line is a continuation line */
      ch = fgetc(f);
      if ((ch != ' ') && (ch != '\t'))
      {
        ungetc(ch, f);
        return line; /* next line is a separate header field or EOH */
      }

      /* eat tabs and spaces from the beginning of the continuation line */
      while ((ch = fgetc(f)) == ' ' || ch == '\t')
        ;
      ungetc(ch, f);
      *++buf = ' '; /* string is still terminated because we removed
                       at least one whitespace char above */
    }

    buf++;
    offset = buf - line;
    if (*linelen < offset + STRING)
    {
      /* grow the buffer */
      *linelen += STRING;
      mutt_mem_realloc(&line, *linelen);
      buf = line + offset;
    }
  }
  /* not reached */
}

static void parse_references(struct ListHead *head, char *s)
{
  char *m = NULL;
  const char *sp = NULL;

  while ((m = mutt_extract_message_id(s, &sp)))
  {
    mutt_list_insert_head(head, m);
    s = NULL;
  }
}

int mutt_check_encoding(const char *c)
{
  if (mutt_str_strncasecmp("7bit", c, sizeof("7bit") - 1) == 0)
    return ENC7BIT;
  else if (mutt_str_strncasecmp("8bit", c, sizeof("8bit") - 1) == 0)
    return ENC8BIT;
  else if (mutt_str_strncasecmp("binary", c, sizeof("binary") - 1) == 0)
    return ENCBINARY;
  else if (mutt_str_strncasecmp("quoted-printable", c, sizeof("quoted-printable") - 1) == 0)
  {
    return ENCQUOTEDPRINTABLE;
  }
  else if (mutt_str_strncasecmp("base64", c, sizeof("base64") - 1) == 0)
    return ENCBASE64;
  else if (mutt_str_strncasecmp("x-uuencode", c, sizeof("x-uuencode") - 1) == 0)
    return ENCUUENCODED;
#ifdef SUN_ATTACHMENT
  else if (mutt_str_strncasecmp("uuencode", c, sizeof("uuencode") - 1) == 0)
    return ENCUUENCODED;
#endif
  else
    return ENCOTHER;
}

static void parse_parameters(struct ParameterList *param, const char *s)
{
  struct Parameter *new = NULL;
  char buffer[LONG_STRING];
  const char *p = NULL;
  size_t i;

  mutt_debug(2, "'%s'\n", s);

  while (*s)
  {
    p = strpbrk(s, "=;");
    if (!p)
    {
      mutt_debug(1, "malformed parameter: %s\n", s);
      goto bail;
    }

    /* if we hit a ; now the parameter has no value, just skip it */
    if (*p != ';')
    {
      i = p - s;
      /* remove whitespace from the end of the attribute name */
      while (i > 0 && mutt_str_is_email_wsp(s[i - 1]))
        i--;

      /* the check for the missing parameter token is here so that we can skip
       * over any quoted value that may be present.
       */
      if (i == 0)
      {
        mutt_debug(1, "missing attribute: %s\n", s);
        new = NULL;
      }
      else
      {
        new = mutt_param_new();
        new->attribute = mutt_str_substr_dup(s, s + i);
      }

      s = mutt_str_skip_email_wsp(p + 1); /* skip over the = */

      if (*s == '"')
      {
        bool state_ascii = true;
        s++;
        for (i = 0; *s && i < sizeof(buffer) - 1; i++, s++)
        {
          if (AssumedCharset && *AssumedCharset)
          {
            /* As iso-2022-* has a character of '"' with non-ascii state,
             * ignore it. */
            if (*s == 0x1b && i < sizeof(buffer) - 2)
            {
              if (s[1] == '(' && (s[2] == 'B' || s[2] == 'J'))
                state_ascii = true;
              else
                state_ascii = false;
            }
          }
          if (state_ascii && *s == '"')
            break;
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
        for (i = 0; *s && *s != ' ' && *s != ';' && i < sizeof(buffer) - 1; i++, s++)
          buffer[i] = *s;
        buffer[i] = 0;
      }

      /* if the attribute token was missing, 'new' will be NULL */
      if (new)
      {
        new->value = mutt_str_strdup(buffer);

        mutt_debug(2, "parse_parameter: '%s' = `%s'\n",
                   new->attribute ? new->attribute : "", new->value ? new->value : "");

        /* Add this parameter to the list */
        TAILQ_INSERT_HEAD(param, new, entries);
      }
    }
    else
    {
      mutt_debug(1, "parameter with no value: %s\n", s);
      s = p;
    }

    /* Find the next parameter */
    if (*s != ';' && (s = strchr(s, ';')) == NULL)
      break; /* no more parameters */

    do
    {
      /* Move past any leading whitespace. the +1 skips over the semicolon */
      s = mutt_str_skip_email_wsp(s + 1);
    } while (*s == ';'); /* skip empty parameters */
  }

bail:

  rfc2231_decode_parameters(param);
}

int mutt_check_mime_type(const char *s)
{
  if (mutt_str_strcasecmp("text", s) == 0)
    return TYPETEXT;
  else if (mutt_str_strcasecmp("multipart", s) == 0)
    return TYPEMULTIPART;
#ifdef SUN_ATTACHMENT
  else if (mutt_str_strcasecmp("x-sun-attachment", s) == 0)
    return TYPEMULTIPART;
#endif
  else if (mutt_str_strcasecmp("application", s) == 0)
    return TYPEAPPLICATION;
  else if (mutt_str_strcasecmp("message", s) == 0)
    return TYPEMESSAGE;
  else if (mutt_str_strcasecmp("image", s) == 0)
    return TYPEIMAGE;
  else if (mutt_str_strcasecmp("audio", s) == 0)
    return TYPEAUDIO;
  else if (mutt_str_strcasecmp("video", s) == 0)
    return TYPEVIDEO;
  else if (mutt_str_strcasecmp("model", s) == 0)
    return TYPEMODEL;
  else if (mutt_str_strcasecmp("*", s) == 0)
    return TYPEANY;
  else if (mutt_str_strcasecmp(".*", s) == 0)
    return TYPEANY;
  else
    return TYPEOTHER;
}

void mutt_parse_content_type(char *s, struct Body *ct)
{
  char *pc = NULL;
  char *subtype = NULL;

  FREE(&ct->subtype);
  mutt_param_free(&ct->parameter);

  /* First extract any existing parameters */
  pc = strchr(s, ';');
  if (pc)
  {
    *pc++ = 0;
    while (*pc && ISSPACE(*pc))
      pc++;
    parse_parameters(&ct->parameter, pc);

    /* Some pre-RFC1521 gateways still use the "name=filename" convention,
     * but if a filename has already been set in the content-disposition,
     * let that take precedence, and don't set it here */
    pc = mutt_param_get(&ct->parameter, "name");
    if (pc && !ct->filename)
      ct->filename = mutt_str_strdup(pc);

#ifdef SUN_ATTACHMENT
    /* this is deep and utter perversion */
    pc = mutt_param_get(&ct->parameter, "conversions");
    if (pc)
      ct->encoding = mutt_check_encoding(pc);
#endif
  }

  /* Now get the subtype */
  subtype = strchr(s, '/');
  if (subtype)
  {
    *subtype++ = '\0';
    for (pc = subtype; *pc && !ISSPACE(*pc) && *pc != ';'; pc++)
      ;
    *pc = '\0';
    ct->subtype = mutt_str_strdup(subtype);
  }

  /* Finally, get the major type */
  ct->type = mutt_check_mime_type(s);

#ifdef SUN_ATTACHMENT
  if (mutt_str_strcasecmp("x-sun-attachment", s) == 0)
    ct->subtype = mutt_str_strdup("x-sun-attachment");
#endif

  if (ct->type == TYPEOTHER)
  {
    ct->xtype = mutt_str_strdup(s);
  }

  if (!ct->subtype)
  {
    /* Some older non-MIME mailers (i.e., mailtool, elm) have a content-type
     * field, so we can attempt to convert the type to Body here.
     */
    if (ct->type == TYPETEXT)
      ct->subtype = mutt_str_strdup("plain");
    else if (ct->type == TYPEAUDIO)
      ct->subtype = mutt_str_strdup("basic");
    else if (ct->type == TYPEMESSAGE)
      ct->subtype = mutt_str_strdup("rfc822");
    else if (ct->type == TYPEOTHER)
    {
      char buffer[SHORT_STRING];

      ct->type = TYPEAPPLICATION;
      snprintf(buffer, sizeof(buffer), "x-%s", s);
      ct->subtype = mutt_str_strdup(buffer);
    }
    else
      ct->subtype = mutt_str_strdup("x-unknown");
  }

  /* Default character set for text types. */
  if (ct->type == TYPETEXT)
  {
    pc = mutt_param_get(&ct->parameter, "charset");
    if (!pc)
      mutt_param_set(&ct->parameter, "charset",
                     (AssumedCharset && *AssumedCharset) ?
                         (const char *) mutt_ch_get_default_charset() :
                         "us-ascii");
  }
}

static void parse_content_disposition(const char *s, struct Body *ct)
{
  struct ParameterList parms;
  TAILQ_INIT(&parms);

  if (mutt_str_strncasecmp("inline", s, 6) == 0)
    ct->disposition = DISPINLINE;
  else if (mutt_str_strncasecmp("form-data", s, 9) == 0)
    ct->disposition = DISPFORMDATA;
  else
    ct->disposition = DISPATTACH;

  /* Check to see if a default filename was given */
  s = strchr(s, ';');
  if (s)
  {
    s = mutt_str_skip_email_wsp(s + 1);
    parse_parameters(&parms, s);
    s = mutt_param_get(&parms, "filename");
    if (s)
      mutt_str_replace(&ct->filename, s);
    s = mutt_param_get(&parms, "name");
    if (s)
      ct->form_name = mutt_str_strdup(s);
    mutt_param_free(&parms);
  }
}

/**
 * mutt_read_mime_header - Parse a MIME header
 * @param fp      stream to read from
 * @param digest  1 if reading subparts of a multipart/digest, 0 otherwise
 */
struct Body *mutt_read_mime_header(FILE *fp, int digest)
{
  struct Body *p = mutt_new_body();
  char *c = NULL;
  char *line = mutt_mem_malloc(LONG_STRING);
  size_t linelen = LONG_STRING;

  p->hdr_offset = ftello(fp);

  p->encoding = ENC7BIT; /* default from RFC1521 */
  p->type = digest ? TYPEMESSAGE : TYPETEXT;
  p->disposition = DISPINLINE;

  while (*(line = mutt_read_rfc822_line(fp, line, &linelen)) != 0)
  {
    /* Find the value of the current header */
    c = strchr(line, ':');
    if (c)
    {
      *c = 0;
      c = mutt_str_skip_email_wsp(c + 1);
      if (!*c)
      {
        mutt_debug(1, "skipping empty header field: %s\n", line);
        continue;
      }
    }
    else
    {
      mutt_debug(1, "bogus MIME header: %s\n", line);
      break;
    }

    if (mutt_str_strncasecmp("content-", line, 8) == 0)
    {
      if (mutt_str_strcasecmp("type", line + 8) == 0)
        mutt_parse_content_type(c, p);
      else if (mutt_str_strcasecmp("transfer-encoding", line + 8) == 0)
        p->encoding = mutt_check_encoding(c);
      else if (mutt_str_strcasecmp("disposition", line + 8) == 0)
        parse_content_disposition(c, p);
      else if (mutt_str_strcasecmp("description", line + 8) == 0)
      {
        mutt_str_replace(&p->description, c);
        mutt_rfc2047_decode(&p->description);
      }
    }
#ifdef SUN_ATTACHMENT
    else if (mutt_str_strncasecmp("x-sun-", line, 6) == 0)
    {
      if (mutt_str_strcasecmp("data-type", line + 6) == 0)
        mutt_parse_content_type(c, p);
      else if (mutt_str_strcasecmp("encoding-info", line + 6) == 0)
        p->encoding = mutt_check_encoding(c);
      else if (mutt_str_strcasecmp("content-lines", line + 6) == 0)
        mutt_param_set(&p->parameter, "content-lines", c);
      else if (mutt_str_strcasecmp("data-description", line + 6) == 0)
      {
        mutt_str_replace(&p->description, c);
        mutt_rfc2047_decode(&p->description);
      }
    }
#endif
  }
  p->offset = ftello(fp); /* Mark the start of the real data */
  if (p->type == TYPETEXT && !p->subtype)
    p->subtype = mutt_str_strdup("plain");
  else if (p->type == TYPEMESSAGE && !p->subtype)
    p->subtype = mutt_str_strdup("rfc822");

  FREE(&line);

  return p;
}

void mutt_parse_part(FILE *fp, struct Body *b)
{
  char *bound = NULL;

  switch (b->type)
  {
    case TYPEMULTIPART:
#ifdef SUN_ATTACHMENT
      if (mutt_str_strcasecmp(b->subtype, "x-sun-attachment") == 0)
        bound = "--------";
      else
#endif
        bound = mutt_param_get(&b->parameter, "boundary");

      fseeko(fp, b->offset, SEEK_SET);
      b->parts = mutt_parse_multipart(fp, bound, b->offset + b->length,
                                      (mutt_str_strcasecmp("digest", b->subtype) == 0));
      break;

    case TYPEMESSAGE:
      if (b->subtype)
      {
        fseeko(fp, b->offset, SEEK_SET);
        if (mutt_is_message_type(b->type, b->subtype))
          b->parts = mutt_parse_message_rfc822(fp, b);
        else if (mutt_str_strcasecmp(b->subtype, "external-body") == 0)
          b->parts = mutt_read_mime_header(fp, 0);
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
    mutt_str_replace(&b->subtype, "plain");
  }
}

/**
 * mutt_parse_message_rfc822 - parse a Message/RFC822 body
 * @param fp     stream to read from
 * @param parent info about the message/rfc822 body part
 *
 * NOTE: this assumes that `parent->length' has been set!
 */
struct Body *mutt_parse_message_rfc822(FILE *fp, struct Body *parent)
{
  struct Body *msg = NULL;

  parent->hdr = mutt_new_header();
  parent->hdr->offset = ftello(fp);
  parent->hdr->env = mutt_read_rfc822_header(fp, parent->hdr, 0, 0);
  msg = parent->hdr->content;

  /* ignore the length given in the content-length since it could be wrong
     and we already have the info to calculate the correct length */
  /* if (msg->length == -1) */
  msg->length = parent->length - (msg->offset - parent->offset);

  /* if body of this message is empty, we can end up with a negative length */
  if (msg->length < 0)
    msg->length = 0;

  mutt_parse_part(fp, msg);
  return msg;
}

/**
 * mutt_parse_multipart - parse a multipart structure
 * @param fp       stream to read from
 * @param boundary body separator
 * @param end_off  length of the multipart body (used when the final
 *                 boundary is missing to avoid reading too far)
 * @param digest   1 if reading a multipart/digest, 0 otherwise
 */
struct Body *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, int digest)
{
  char buffer[LONG_STRING];
  struct Body *head = NULL, *last = NULL, *new = NULL;
  bool final = false; /* did we see the ending boundary? */

  if (!boundary)
  {
    mutt_error(_("multipart message has no boundary parameter!"));
    return NULL;
  }

  const size_t blen = mutt_str_strlen(boundary);
  while (ftello(fp) < end_off && fgets(buffer, LONG_STRING, fp) != NULL)
  {
    const size_t len = mutt_str_strlen(buffer);

    const size_t crlf = (len > 1 && buffer[len - 2] == '\r') ? 1 : 0;

    if (buffer[0] == '-' && buffer[1] == '-' &&
        (mutt_str_strncmp(buffer + 2, boundary, blen) == 0))
    {
      if (last)
      {
        last->length = ftello(fp) - last->offset - len - 1 - crlf;
        if (last->parts && last->parts->length == 0)
          last->parts->length = ftello(fp) - last->parts->offset - len - 1 - crlf;
        /* if the body is empty, we can end up with a -1 length */
        if (last->length < 0)
          last->length = 0;
      }

      if (len > 0)
      {
        /* Remove any trailing whitespace, up to the length of the boundary */
        for (size_t i = len - 1; ISSPACE(buffer[i]) && i >= blen + 2; i--)
          buffer[i] = 0;
      }

      /* Check for the end boundary */
      if (mutt_str_strcmp(buffer + blen + 2, "--") == 0)
      {
        final = true;
        break; /* done parsing */
      }
      else if (buffer[2 + blen] == 0)
      {
        new = mutt_read_mime_header(fp, digest);

#ifdef SUN_ATTACHMENT
        if (mutt_param_get(&new->parameter, "content-lines"))
        {
          int lines;
          if (mutt_str_atoi(mutt_param_get(&new->parameter, "content-lines"), &lines) < 0)
            lines = 0;
          for (; lines; lines--)
            if (ftello(fp) >= end_off || fgets(buffer, LONG_STRING, fp) == NULL)
              break;
        }
#endif

        /*
         * Consistency checking - catch
         * bad attachment end boundaries
         */

        if (new->offset > end_off)
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
  for (last = head; last; last = last->next)
    mutt_parse_part(fp, last);

  return head;
}

/**
 * mutt_extract_message_id - Find a message-id
 *
 * extract the first substring that looks like a message-id.
 * call back with NULL for more (like strtok).
 */
char *mutt_extract_message_id(const char *s, const char **saveptr)
{
  const char *o = NULL, *onull = NULL, *p = NULL;
  char *ret = NULL;

  if (s)
    p = s;
  else if (saveptr)
    p = *saveptr;
  else
    return NULL;

  for (s = NULL, o = NULL, onull = NULL; (p = strpbrk(p, "<> \t;")) != NULL; ++p)
  {
    if (*p == '<')
    {
      s = p;
      o = onull = NULL;
      continue;
    }

    if (!s)
      continue;

    if (*p == '>')
    {
      size_t olen = onull - o, slen = p - s + 1;
      ret = mutt_mem_malloc(olen + slen + 1);
      if (o)
        memcpy(ret, o, olen);
      memcpy(ret + olen, s, slen);
      ret[olen + slen] = '\0';
      if (saveptr)
        *saveptr = p + 1; /* next call starts after '>' */
      return ret;
    }

    /* some idiotic clients break their message-ids between lines */
    if (s == p)
    {
      /* step past another whitespace */
      s = p + 1;
    }
    else if (o)
    {
      /* more than two lines, give up */
      s = o = onull = NULL;
    }
    else
    {
      /* remember the first line, start looking for the second */
      o = s;
      onull = p;
      s = p + 1;
    }
  }

  return NULL;
}

void mutt_parse_mime_message(struct Context *ctx, struct Header *cur)
{
  struct Message *msg = NULL;

  do
  {
    if (cur->content->type != TYPEMESSAGE && cur->content->type != TYPEMULTIPART)
      break; /* nothing to do */

    if (cur->content->parts)
      break; /* The message was parsed earlier. */

    msg = mx_open_message(ctx, cur->msgno);
    if (msg)
    {
      mutt_parse_part(msg->fp, cur->content);

      if (WithCrypto)
        cur->security = crypt_query(cur->content);

      mx_close_message(ctx, &msg);
    }
  } while (0);

  cur->attach_valid = false;
}

int mutt_parse_rfc822_line(struct Envelope *e, struct Header *hdr, char *line,
                           char *p, short user_hdrs, short weed, short do_2047)
{
  int matched = 0;

  switch (tolower(line[0]))
  {
    case 'a':
      if (mutt_str_strcasecmp(line + 1, "pparently-to") == 0)
      {
        e->to = mutt_addr_parse_list(e->to, p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "pparently-from") == 0)
      {
        e->from = mutt_addr_parse_list(e->from, p);
        matched = 1;
      }
      break;

    case 'b':
      if (mutt_str_strcasecmp(line + 1, "cc") == 0)
      {
        e->bcc = mutt_addr_parse_list(e->bcc, p);
        matched = 1;
      }
      break;

    case 'c':
      if (mutt_str_strcasecmp(line + 1, "c") == 0)
      {
        e->cc = mutt_addr_parse_list(e->cc, p);
        matched = 1;
      }
      else if (mutt_str_strncasecmp(line + 1, "ontent-", 7) == 0)
      {
        if (mutt_str_strcasecmp(line + 8, "type") == 0)
        {
          if (hdr)
            mutt_parse_content_type(p, hdr->content);
          matched = 1;
        }
        else if (mutt_str_strcasecmp(line + 8, "transfer-encoding") == 0)
        {
          if (hdr)
            hdr->content->encoding = mutt_check_encoding(p);
          matched = 1;
        }
        else if (mutt_str_strcasecmp(line + 8, "length") == 0)
        {
          if (hdr)
          {
            hdr->content->length = atol(p);
            if (hdr->content->length < 0)
              hdr->content->length = -1;
          }
          matched = 1;
        }
        else if (mutt_str_strcasecmp(line + 8, "description") == 0)
        {
          if (hdr)
          {
            mutt_str_replace(&hdr->content->description, p);
            mutt_rfc2047_decode(&hdr->content->description);
          }
          matched = 1;
        }
        else if (mutt_str_strcasecmp(line + 8, "disposition") == 0)
        {
          if (hdr)
            parse_content_disposition(p, hdr->content);
          matched = 1;
        }
      }
      break;

    case 'd':
      if (mutt_str_strcasecmp("ate", line + 1) == 0)
      {
        mutt_str_replace(&e->date, p);
        if (hdr)
        {
          struct Tz tz;
          hdr->date_sent = mutt_date_parse_date(p, &tz);
          if (hdr->date_sent > 0)
          {
            hdr->zhours = tz.zhours;
            hdr->zminutes = tz.zminutes;
            hdr->zoccident = tz.zoccident;
          }
        }
        matched = 1;
      }
      break;

    case 'e':
      if ((mutt_str_strcasecmp("xpires", line + 1) == 0) && hdr &&
          mutt_date_parse_date(p, NULL) < time(NULL))
      {
        hdr->expired = true;
      }
      break;

    case 'f':
      if (mutt_str_strcasecmp("rom", line + 1) == 0)
      {
        e->from = mutt_addr_parse_list(e->from, p);
        matched = 1;
      }
#ifdef USE_NNTP
      else if (mutt_str_strcasecmp(line + 1, "ollowup-to") == 0)
      {
        if (!e->followup_to)
        {
          mutt_str_remove_trailing_ws(p);
          e->followup_to = mutt_str_strdup(mutt_str_skip_whitespace(p));
        }
        matched = 1;
      }
#endif
      break;

    case 'i':
      if (mutt_str_strcasecmp(line + 1, "n-reply-to") == 0)
      {
        mutt_list_free(&e->in_reply_to);
        parse_references(&e->in_reply_to, p);
        matched = 1;
      }
      break;

    case 'l':
      if (mutt_str_strcasecmp(line + 1, "ines") == 0)
      {
        if (hdr)
        {
          /*
         * HACK - neomutt has, for a very short time, produced negative
         * Lines header values.  Ignore them.
         */
          if (mutt_str_atoi(p, &hdr->lines) < 0 || hdr->lines < 0)
            hdr->lines = 0;
        }

        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "ist-Post") == 0)
      {
        /* RFC2369.  FIXME: We should ignore whitespace, but don't. */
        if (strncmp(p, "NO", 2) != 0)
        {
          char *beg = NULL, *end = NULL;
          for (beg = strchr(p, '<'); beg; beg = strchr(end, ','))
          {
            beg++;
            end = strchr(beg, '>');
            if (!end)
              break;

            /* Take the first mailto URL */
            if (url_check_scheme(beg) == U_MAILTO)
            {
              FREE(&e->list_post);
              e->list_post = mutt_str_substr_dup(beg, end);
              break;
            }
          }
        }
        matched = 1;
      }
      break;

    case 'm':
      if (mutt_str_strcasecmp(line + 1, "ime-version") == 0)
      {
        if (hdr)
          hdr->mime = true;
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "essage-id") == 0)
      {
        /* We add a new "Message-ID:" when building a message */
        FREE(&e->message_id);
        e->message_id = mutt_extract_message_id(p, NULL);
        matched = 1;
      }
      else if (mutt_str_strncasecmp(line + 1, "ail-", 4) == 0)
      {
        if (mutt_str_strcasecmp(line + 5, "reply-to") == 0)
        {
          /* override the Reply-To: field */
          mutt_addr_free(&e->reply_to);
          e->reply_to = mutt_addr_parse_list(e->reply_to, p);
          matched = 1;
        }
        else if (mutt_str_strcasecmp(line + 5, "followup-to") == 0)
        {
          e->mail_followup_to = mutt_addr_parse_list(e->mail_followup_to, p);
          matched = 1;
        }
      }
      break;

#ifdef USE_NNTP
    case 'n':
      if (mutt_str_strcasecmp(line + 1, "ewsgroups") == 0)
      {
        FREE(&e->newsgroups);
        mutt_str_remove_trailing_ws(p);
        e->newsgroups = mutt_str_strdup(mutt_str_skip_whitespace(p));
        matched = 1;
      }
      break;
#endif

    case 'o':
      /* field `Organization:' saves only for pager! */
      if (mutt_str_strcasecmp(line + 1, "rganization") == 0)
      {
        if (!e->organization && (mutt_str_strcasecmp(p, "unknown") != 0))
          e->organization = mutt_str_strdup(p);
      }
      break;

    case 'r':
      if (mutt_str_strcasecmp(line + 1, "eferences") == 0)
      {
        mutt_list_free(&e->references);
        parse_references(&e->references, p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "eply-to") == 0)
      {
        e->reply_to = mutt_addr_parse_list(e->reply_to, p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "eturn-path") == 0)
      {
        e->return_path = mutt_addr_parse_list(e->return_path, p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "eceived") == 0)
      {
        if (hdr && !hdr->received)
        {
          char *d = strrchr(p, ';');

          if (d)
            hdr->received = mutt_date_parse_date(d + 1, NULL);
        }
      }
      break;

    case 's':
      if (mutt_str_strcasecmp(line + 1, "ubject") == 0)
      {
        if (!e->subject)
          e->subject = mutt_str_strdup(p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "ender") == 0)
      {
        e->sender = mutt_addr_parse_list(e->sender, p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "tatus") == 0)
      {
        if (hdr)
        {
          while (*p)
          {
            switch (*p)
            {
              case 'r':
                hdr->replied = true;
                break;
              case 'O':
                hdr->old = MarkOld ? true : false;
                break;
              case 'R':
                hdr->read = true;
                break;
            }
            p++;
          }
        }
        matched = 1;
      }
      else if (((mutt_str_strcasecmp("upersedes", line + 1) == 0) ||
                (mutt_str_strcasecmp("upercedes", line + 1) == 0)) &&
               hdr)
      {
        FREE(&e->supersedes);
        e->supersedes = mutt_str_strdup(p);
      }
      break;

    case 't':
      if (mutt_str_strcasecmp(line + 1, "o") == 0)
      {
        e->to = mutt_addr_parse_list(e->to, p);
        matched = 1;
      }
      break;

    case 'x':
      if (mutt_str_strcasecmp(line + 1, "-status") == 0)
      {
        if (hdr)
        {
          while (*p)
          {
            switch (*p)
            {
              case 'A':
                hdr->replied = true;
                break;
              case 'D':
                hdr->deleted = true;
                break;
              case 'F':
                hdr->flagged = true;
                break;
              default:
                break;
            }
            p++;
          }
        }
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "-label") == 0)
      {
        FREE(&e->x_label);
        e->x_label = mutt_str_strdup(p);
        matched = 1;
      }
#ifdef USE_NNTP
      else if (mutt_str_strcasecmp(line + 1, "-comment-to") == 0)
      {
        if (!e->x_comment_to)
          e->x_comment_to = mutt_str_strdup(p);
        matched = 1;
      }
      else if (mutt_str_strcasecmp(line + 1, "ref") == 0)
      {
        if (!e->xref)
          e->xref = mutt_str_strdup(p);
        matched = 1;
      }
#endif
      else if (mutt_str_strcasecmp(line + 1, "-original-to") == 0)
      {
        e->x_original_to = mutt_addr_parse_list(e->x_original_to, p);
        matched = 1;
      }

    default:
      break;
  }

  /* Keep track of the user-defined headers */
  if (!matched && user_hdrs)
  {
    /* restore the original line */
    line[strlen(line)] = ':';

    if (!(weed && Weed && mutt_matches_ignore(line)))
    {
      struct ListNode *np = mutt_list_insert_tail(&e->userhdrs, mutt_str_strdup(line));
      if (do_2047)
        mutt_rfc2047_decode(&np->data);
    }
  }

  return matched;
}

/**
 * mutt_read_rfc822_header - parses an RFC822 header
 * @param f         Stream to read from
 * @param hdr       Header structure of current message (optional)
 * @param user_hdrs If set, store user headers
 *                  Used for recall-message and postpone modes
 * @param weed      If this parameter is set and the user has activated the
 *                  $weed option, honor the header weed list for user headers.
 *                  Used for recall-message
 * @retval ptr Newly allocated envelope structure
 *
 * Caller should free the Envelope using mutt_env_free().
 */
struct Envelope *mutt_read_rfc822_header(FILE *f, struct Header *hdr,
                                         short user_hdrs, short weed)
{
  struct Envelope *e = mutt_env_new();
  char *line = mutt_mem_malloc(LONG_STRING);
  char *p = NULL;
  LOFF_T loc;
  size_t linelen = LONG_STRING;
  char buf[LONG_STRING + 1];

  if (hdr)
  {
    if (!hdr->content)
    {
      hdr->content = mutt_new_body();

      /* set the defaults from RFC1521 */
      hdr->content->type = TYPETEXT;
      hdr->content->subtype = mutt_str_strdup("plain");
      hdr->content->encoding = ENC7BIT;
      hdr->content->length = -1;

      /* RFC2183 says this is arbitrary */
      hdr->content->disposition = DISPINLINE;
    }
  }

  while ((loc = ftello(f)) != -1)
  {
    line = mutt_read_rfc822_line(f, line, &linelen);
    if (*line == '\0')
      break;
    p = strpbrk(line, ": \t");
    if (!p || (*p != ':'))
    {
      char return_path[LONG_STRING];
      time_t t;

      /* some bogus MTAs will quote the original "From " line */
      if (mutt_str_strncmp(">From ", line, 6) == 0)
        continue; /* just ignore */
      else if (is_from(line, return_path, sizeof(return_path), &t))
      {
        /* MH sometimes has the From_ line in the middle of the header! */
        if (hdr && !hdr->received)
          hdr->received = t - mutt_date_local_tz(t);
        continue;
      }

      fseeko(f, loc, SEEK_SET);
      break; /* end of header */
    }

    *buf = '\0';

    if (mutt_replacelist_match(SpamList, buf, sizeof(buf), line))
    {
      if (!mutt_regexlist_match(NoSpamList, line))
      {
        /* if spam tag already exists, figure out how to amend it */
        if (e->spam && *buf)
        {
          /* If SpamSeparator defined, append with separator */
          if (SpamSeparator)
          {
            mutt_buffer_addstr(e->spam, SpamSeparator);
            mutt_buffer_addstr(e->spam, buf);
          }

          /* else overwrite */
          else
          {
            e->spam->dptr = e->spam->data;
            *e->spam->dptr = '\0';
            mutt_buffer_addstr(e->spam, buf);
          }
        }

        /* spam tag is new, and match expr is non-empty; copy */
        else if (!e->spam && *buf)
        {
          e->spam = mutt_buffer_from(buf);
        }

        /* match expr is empty; plug in null string if no existing tag */
        else if (!e->spam)
        {
          e->spam = mutt_buffer_from("");
        }

        if (e->spam && e->spam->data)
          mutt_debug(5, "spam = %s\n", e->spam->data);
      }
    }

    *p = 0;
    p = mutt_str_skip_email_wsp(p + 1);
    if (!*p)
      continue; /* skip empty header fields */

    mutt_parse_rfc822_line(e, hdr, line, p, user_hdrs, weed, 1);
  }

  FREE(&line);

  if (hdr)
  {
    hdr->content->hdr_offset = hdr->offset;
    hdr->content->offset = ftello(f);

    /* do RFC2047 decoding */
    rfc2047_decode_addrlist(e->from);
    rfc2047_decode_addrlist(e->to);
    rfc2047_decode_addrlist(e->cc);
    rfc2047_decode_addrlist(e->bcc);
    rfc2047_decode_addrlist(e->reply_to);
    rfc2047_decode_addrlist(e->mail_followup_to);
    rfc2047_decode_addrlist(e->return_path);
    rfc2047_decode_addrlist(e->sender);
    rfc2047_decode_addrlist(e->x_original_to);

    if (e->subject)
    {
      regmatch_t pmatch[1];

      mutt_rfc2047_decode(&e->subject);

      if (ReplyRegex && ReplyRegex->regex &&
          (regexec(ReplyRegex->regex, e->subject, 1, pmatch, 0) == 0))
        e->real_subj = e->subject + pmatch[0].rm_eo;
      else
        e->real_subj = e->subject;
    }

    if (hdr->received < 0)
    {
      mutt_debug(1, "resetting invalid received time to 0\n");
      hdr->received = 0;
    }

    /* check for missing or invalid date */
    if (hdr->date_sent <= 0)
    {
      mutt_debug(1, "no date found, using received time from msg separator\n");
      hdr->date_sent = hdr->received;
    }
  }

  return e;
}

/**
 * count_body_parts_check - Compares mime types to the ok and except lists
 */
static bool count_body_parts_check(struct ListHead *checklist, struct Body *b, bool dflt)
{
  struct AttachMatch *a = NULL;

  /* If list is null, use default behavior. */
  if (!checklist || STAILQ_EMPTY(checklist))
  {
    return false;
  }

  struct ListNode *np;
  STAILQ_FOREACH(np, checklist, entries)
  {
    a = (struct AttachMatch *) np->data;
    mutt_debug(5, "%s %d/%s ?? %s/%s [%d]... ", dflt ? "[OK]   " : "[EXCL] ", b->type,
               b->subtype ? b->subtype : "*", a->major, a->minor, a->major_int);
    if ((a->major_int == TYPEANY || a->major_int == b->type) &&
        (!b->subtype || !regexec(&a->minor_regex, b->subtype, 0, NULL, 0)))
    {
      mutt_debug(5, "yes\n");
      return true;
    }
    else
    {
      mutt_debug(5, "no\n");
    }
  }

  return false;
}

static int count_body_parts(struct Body *body, int flags)
{
  int count = 0;

  if (!body)
    return 0;

  for (struct Body *bp = body; bp != NULL; bp = bp->next)
  {
    /* Initial disposition is to count and not to recurse this part. */
    bool shallcount = true; /* default */
    bool shallrecurse = false;

    mutt_debug(5, "desc=\"%s\"; fn=\"%s\", type=\"%d/%s\"\n",
               bp->description ? bp->description : ("none"),
               bp->filename ? bp->filename : bp->d_filename ? bp->d_filename : "(none)",
               bp->type, bp->subtype ? bp->subtype : "*");

    if (bp->type == TYPEMESSAGE)
    {
      shallrecurse = true;

      /* If it's an external body pointer, don't recurse it. */
      if (mutt_str_strcasecmp(bp->subtype, "external-body") == 0)
        shallrecurse = false;

      /* Don't count containers if they're top-level. */
      if (flags & MUTT_PARTS_TOPLEVEL)
        shallcount = false; // top-level message/*
    }
    else if (bp->type == TYPEMULTIPART)
    {
      /* Always recurse multiparts, except multipart/alternative. */
      shallrecurse = true;
      if (mutt_str_strcasecmp(bp->subtype, "alternative") == 0)
        shallrecurse = false;

      /* Don't count containers if they're top-level. */
      if (flags & MUTT_PARTS_TOPLEVEL)
        shallcount = false; /* top-level multipart */
    }

    if (bp->disposition == DISPINLINE && bp->type != TYPEMULTIPART &&
        bp->type != TYPEMESSAGE && bp == body)
    {
      shallcount = false; /* ignore fundamental inlines */
    }

    /* If this body isn't scheduled for enumeration already, don't bother
     * profiling it further.
     */
    if (shallcount)
    {
      /* Turn off shallcount if message type is not in ok list,
       * or if it is in except list. Check is done separately for
       * inlines vs. attachments.
       */

      if (bp->disposition == DISPATTACH)
      {
        if (!count_body_parts_check(&AttachAllow, bp, true))
          shallcount = false; /* attach not allowed */
        if (count_body_parts_check(&AttachExclude, bp, false))
          shallcount = false; /* attach excluded */
      }
      else
      {
        if (!count_body_parts_check(&InlineAllow, bp, true))
          shallcount = false; /* inline not allowed */
        if (count_body_parts_check(&InlineExclude, bp, false))
          shallcount = false; /* excluded */
      }
    }

    if (shallcount)
      count++;
    bp->attach_qualifies = shallcount ? true : false;

    mutt_debug(5, "%p shallcount = %d\n", (void *) bp, shallcount);

    if (shallrecurse)
    {
      mutt_debug(5, "%p pre count = %d\n", (void *) bp, count);
      bp->attach_count = count_body_parts(bp->parts, flags & ~MUTT_PARTS_TOPLEVEL);
      count += bp->attach_count;
      mutt_debug(5, "%p post count = %d\n", (void *) bp, count);
    }
  }

  mutt_debug(5, "return %d\n", count < 0 ? 0 : count);
  return count < 0 ? 0 : count;
}

int mutt_count_body_parts(struct Context *ctx, struct Header *hdr)
{
  bool keep_parts = false;

  if (hdr->attach_valid)
    return hdr->attach_total;

  if (hdr->content->parts)
    keep_parts = true;
  else
    mutt_parse_mime_message(ctx, hdr);

  if (!STAILQ_EMPTY(&AttachAllow) || !STAILQ_EMPTY(&AttachExclude) ||
      !STAILQ_EMPTY(&InlineAllow) || !STAILQ_EMPTY(&InlineExclude))
  {
    hdr->attach_total = count_body_parts(hdr->content, MUTT_PARTS_TOPLEVEL);
  }
  else
    hdr->attach_total = 0;

  hdr->attach_valid = true;

  if (!keep_parts)
    mutt_free_body(&hdr->content->parts);

  return hdr->attach_total;
}
