/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 2016-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017-2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2019 Ian Zimmerman <itz@no-use.mooo.com>
 * Copyright (C) 2021 Christian Ludwig <ludwig@ma.tum.de>
 * Copyright (C) 2022 David Purton <dcpurton@marshwiggle.net>
 * Copyright (C) 2023 Steinar H Gunderson <steinar+neomutt@gunderson.no>
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
 * @page email_parse Email parsing code
 *
 * Miscellaneous email parsing routines
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "parse.h"
#include "body.h"
#include "email.h"
#include "envelope.h"
#include "from.h"
#include "globals.h"
#include "mime.h"
#include "parameter.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "url.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/* If the 'Content-Length' is bigger than 1GiB, then it's clearly wrong.
 * Cap the value to prevent overflow of Body.length */
#define CONTENT_TOO_BIG (1 << 30)

static void parse_part(FILE *fp, struct Body *b, int *counter);
static struct Body *rfc822_parse_message(FILE *fp, struct Body *parent, int *counter);
static struct Body *parse_multipart(FILE *fp, const char *boundary,
                                    LOFF_T end_off, bool digest, int *counter);

/**
 * mutt_filter_commandline_header_tag - Sanitise characters in a header tag
 * @param header String to sanitise
 */
void mutt_filter_commandline_header_tag(char *header)
{
  if (!header)
    return;

  for (; (*header != '\0'); header++)
  {
    if ((*header < 33) || (*header > 126) || (*header == ':'))
      *header = '?';
  }
}

/**
 * mutt_filter_commandline_header_value - Sanitise characters in a header value
 * @param header String to sanitise
 *
 * It might be preferable to use mutt_filter_unprintable() instead.
 * This filter is being lax, but preventing a header injection via an embedded
 * newline.
 */
void mutt_filter_commandline_header_value(char *header)
{
  if (!header)
    return;

  for (; (*header != '\0'); header++)
  {
    if ((*header == '\n') || (*header == '\r'))
      *header = ' ';
  }
}

/**
 * mutt_auto_subscribe - Check if user is subscribed to mailing list
 * @param mailto URL of mailing list subscribe
 */
void mutt_auto_subscribe(const char *mailto)
{
  if (!mailto)
    return;

  if (!AutoSubscribeCache)
    AutoSubscribeCache = mutt_hash_new(200, MUTT_HASH_STRCASECMP | MUTT_HASH_STRDUP_KEYS);

  if (mutt_hash_find(AutoSubscribeCache, mailto))
    return;

  mutt_hash_insert(AutoSubscribeCache, mailto, AutoSubscribeCache);

  struct Envelope *lpenv = mutt_env_new(); /* parsed envelope from the List-Post mailto: URL */

  if (mutt_parse_mailto(lpenv, NULL, mailto) && !TAILQ_EMPTY(&lpenv->to))
  {
    const char *mailbox = buf_string(TAILQ_FIRST(&lpenv->to)->mailbox);
    if (mailbox && !mutt_regexlist_match(&SubscribedLists, mailbox) &&
        !mutt_regexlist_match(&UnMailLists, mailbox) &&
        !mutt_regexlist_match(&UnSubscribedLists, mailbox))
    {
      /* mutt_regexlist_add() detects duplicates, so it is safe to
       * try to add here without any checks. */
      mutt_regexlist_add(&MailLists, mailbox, REG_ICASE, NULL);
      mutt_regexlist_add(&SubscribedLists, mailbox, REG_ICASE, NULL);
    }
  }

  mutt_env_free(&lpenv);
}

/**
 * parse_parameters - Parse a list of Parameters
 * @param pl                 Parameter list for the results
 * @param s                  String to parse
 * @param allow_value_spaces Allow values with spaces
 *
 * Autocrypt defines an irregular parameter format that doesn't follow the
 * rfc.  It splits keydata across multiple lines without parameter continuations.
 * The allow_value_spaces parameter allows parsing those values which
 * are split by spaces when unfolded.
 */
static void parse_parameters(struct ParameterList *pl, const char *s, bool allow_value_spaces)
{
  struct Parameter *pnew = NULL;
  const char *p = NULL;
  size_t i;

  struct Buffer *buf = buf_pool_get();
  /* allow_value_spaces, especially with autocrypt keydata, can result
   * in quite large parameter values.  avoid frequent reallocs by
   * pre-sizing */
  if (allow_value_spaces)
    buf_alloc(buf, mutt_str_len(s));

  mutt_debug(LL_DEBUG2, "'%s'\n", s);

  const bool assumed = !slist_is_empty(cc_assumed_charset());
  while (*s)
  {
    buf_reset(buf);

    p = strpbrk(s, "=;");
    if (!p)
    {
      mutt_debug(LL_DEBUG1, "malformed parameter: %s\n", s);
      goto bail;
    }

    /* if we hit a ; now the parameter has no value, just skip it */
    if (*p != ';')
    {
      i = p - s;
      /* remove whitespace from the end of the attribute name */
      while ((i > 0) && mutt_str_is_email_wsp(s[i - 1]))
        i--;

      /* the check for the missing parameter token is here so that we can skip
       * over any quoted value that may be present.  */
      if (i == 0)
      {
        mutt_debug(LL_DEBUG1, "missing attribute: %s\n", s);
        pnew = NULL;
      }
      else
      {
        pnew = mutt_param_new();
        pnew->attribute = mutt_strn_dup(s, i);
      }

      do
      {
        s = mutt_str_skip_email_wsp(p + 1); /* skip over the =, or space if we loop */

        if (*s == '"')
        {
          bool state_ascii = true;
          s++;
          for (; *s; s++)
          {
            if (assumed)
            {
              // As iso-2022-* has a character of '"' with non-ascii state, ignore it
              if (*s == 0x1b)
              {
                if ((s[1] == '(') && ((s[2] == 'B') || (s[2] == 'J')))
                  state_ascii = true;
                else
                  state_ascii = false;
              }
            }
            if (state_ascii && (*s == '"'))
              break;
            if (*s == '\\')
            {
              if (s[1])
              {
                s++;
                /* Quote the next character */
                buf_addch(buf, *s);
              }
            }
            else
            {
              buf_addch(buf, *s);
            }
          }
          if (*s)
            s++; /* skip over the " */
        }
        else
        {
          for (; *s && *s != ' ' && *s != ';'; s++)
            buf_addch(buf, *s);
        }

        p = s;
      } while (allow_value_spaces && (*s == ' '));

      /* if the attribute token was missing, 'new' will be NULL */
      if (pnew)
      {
        pnew->value = buf_strdup(buf);

        mutt_debug(LL_DEBUG2, "parse_parameter: '%s' = '%s'\n",
                   pnew->attribute ? pnew->attribute : "", pnew->value ? pnew->value : "");

        /* Add this parameter to the list */
        TAILQ_INSERT_HEAD(pl, pnew, entries);
      }
    }
    else
    {
      mutt_debug(LL_DEBUG1, "parameter with no value: %s\n", s);
      s = p;
    }

    /* Find the next parameter */
    if ((*s != ';') && !(s = strchr(s, ';')))
      break; /* no more parameters */

    do
    {
      /* Move past any leading whitespace. the +1 skips over the semicolon */
      s = mutt_str_skip_email_wsp(s + 1);
    } while (*s == ';'); /* skip empty parameters */
  }

bail:

  rfc2231_decode_parameters(pl);
  buf_pool_release(&buf);
}

/**
 * parse_content_disposition - Parse a content disposition
 * @param s String to parse
 * @param b Body to save the result
 *
 * e.g. parse a string "inline" and set #DISP_INLINE.
 */
static void parse_content_disposition(const char *s, struct Body *b)
{
  struct ParameterList pl = TAILQ_HEAD_INITIALIZER(pl);

  if (mutt_istr_startswith(s, "inline"))
    b->disposition = DISP_INLINE;
  else if (mutt_istr_startswith(s, "form-data"))
    b->disposition = DISP_FORM_DATA;
  else
    b->disposition = DISP_ATTACH;

  /* Check to see if a default filename was given */
  s = strchr(s, ';');
  if (s)
  {
    s = mutt_str_skip_email_wsp(s + 1);
    parse_parameters(&pl, s, false);
    s = mutt_param_get(&pl, "filename");
    if (s)
      mutt_str_replace(&b->filename, s);
    s = mutt_param_get(&pl, "name");
    if (s)
      mutt_str_replace(&b->form_name, s);
    mutt_param_free(&pl);
  }
}

/**
 * parse_references - Parse references from an email header
 * @param head List to receive the references
 * @param s    String to parse
 */
static void parse_references(struct ListHead *head, const char *s)
{
  if (!head)
    return;

  char *m = NULL;
  for (size_t off = 0; (m = mutt_extract_message_id(s, &off)); s += off)
  {
    mutt_list_insert_head(head, m);
  }
}

/**
 * parse_content_language - Read the content's language
 * @param s Language string
 * @param b Body of the email
 */
static void parse_content_language(const char *s, struct Body *b)
{
  if (!s || !b)
    return;

  mutt_debug(LL_DEBUG2, "RFC8255 >> Content-Language set to %s\n", s);
  mutt_str_replace(&b->language, s);
}

/**
 * mutt_matches_ignore - Does the string match the ignore list
 * @param s String to check
 * @retval true String matches
 *
 * Checks Ignore and UnIgnore using mutt_list_match
 */
bool mutt_matches_ignore(const char *s)
{
  return mutt_list_match(s, &Ignore) && !mutt_list_match(s, &UnIgnore);
}

/**
 * mutt_check_mime_type - Check a MIME type string
 * @param s String to check
 * @retval enum ContentType, e.g. #TYPE_TEXT
 */
enum ContentType mutt_check_mime_type(const char *s)
{
  if (mutt_istr_equal("text", s))
    return TYPE_TEXT;
  if (mutt_istr_equal("multipart", s))
    return TYPE_MULTIPART;
  if (mutt_istr_equal("x-sun-attachment", s))
    return TYPE_MULTIPART;
  if (mutt_istr_equal("application", s))
    return TYPE_APPLICATION;
  if (mutt_istr_equal("message", s))
    return TYPE_MESSAGE;
  if (mutt_istr_equal("image", s))
    return TYPE_IMAGE;
  if (mutt_istr_equal("audio", s))
    return TYPE_AUDIO;
  if (mutt_istr_equal("video", s))
    return TYPE_VIDEO;
  if (mutt_istr_equal("model", s))
    return TYPE_MODEL;
  if (mutt_istr_equal("*", s))
    return TYPE_ANY;
  if (mutt_istr_equal(".*", s))
    return TYPE_ANY;

  return TYPE_OTHER;
}

/**
 * mutt_extract_message_id - Find a message-id
 * @param[in]  s String to parse
 * @param[out] len Number of bytes of s parsed
 * @retval ptr  Message id found
 * @retval NULL No more message ids
 */
char *mutt_extract_message_id(const char *s, size_t *len)
{
  if (!s || (*s == '\0'))
    return NULL;

  char *decoded = mutt_str_dup(s);
  rfc2047_decode(&decoded);

  char *res = NULL;

  for (const char *p = decoded, *beg = NULL; *p; p++)
  {
    if (*p == '<')
    {
      beg = p;
      continue;
    }

    if (beg && (*p == '>'))
    {
      if (len)
        *len = p - decoded + 1;
      res = mutt_strn_dup(beg, (p + 1) - beg);
      break;
    }
  }

  FREE(&decoded);
  return res;
}

/**
 * mutt_check_encoding - Check the encoding type
 * @param c String to check
 * @retval enum ContentEncoding, e.g. #ENC_QUOTED_PRINTABLE
 */
int mutt_check_encoding(const char *c)
{
  if (mutt_istr_startswith(c, "7bit"))
    return ENC_7BIT;
  if (mutt_istr_startswith(c, "8bit"))
    return ENC_8BIT;
  if (mutt_istr_startswith(c, "binary"))
    return ENC_BINARY;
  if (mutt_istr_startswith(c, "quoted-printable"))
    return ENC_QUOTED_PRINTABLE;
  if (mutt_istr_startswith(c, "base64"))
    return ENC_BASE64;
  if (mutt_istr_startswith(c, "x-uuencode"))
    return ENC_UUENCODED;
  if (mutt_istr_startswith(c, "uuencode"))
    return ENC_UUENCODED;
  return ENC_OTHER;
}

/**
 * mutt_parse_content_type - Parse a content type
 * @param s String to parse
 * @param b Body to save the result
 *
 * e.g. parse a string "inline" and set #DISP_INLINE.
 */
void mutt_parse_content_type(const char *s, struct Body *b)
{
  if (!s || !b)
    return;

  FREE(&b->subtype);
  mutt_param_free(&b->parameter);

  /* First extract any existing parameters */
  char *pc = strchr(s, ';');
  if (pc)
  {
    *pc++ = 0;
    while (*pc && isspace(*pc))
      pc++;
    parse_parameters(&b->parameter, pc, false);

    /* Some pre-RFC1521 gateways still use the "name=filename" convention,
     * but if a filename has already been set in the content-disposition,
     * let that take precedence, and don't set it here */
    pc = mutt_param_get(&b->parameter, "name");
    if (pc && !b->filename)
      b->filename = mutt_str_dup(pc);

    /* this is deep and utter perversion */
    pc = mutt_param_get(&b->parameter, "conversions");
    if (pc)
      b->encoding = mutt_check_encoding(pc);
  }

  /* Now get the subtype */
  char *subtype = strchr(s, '/');
  if (subtype)
  {
    *subtype++ = '\0';
    for (pc = subtype; *pc && !isspace(*pc) && (*pc != ';'); pc++)
      ; // do nothing

    *pc = '\0';
    mutt_str_replace(&b->subtype, subtype);
  }

  /* Finally, get the major type */
  b->type = mutt_check_mime_type(s);

  if (mutt_istr_equal("x-sun-attachment", s))
    mutt_str_replace(&b->subtype, "x-sun-attachment");

  if (b->type == TYPE_OTHER)
  {
    mutt_str_replace(&b->xtype, s);
  }

  if (!b->subtype)
  {
    /* Some older non-MIME mailers (i.e., mailtool, elm) have a content-type
     * field, so we can attempt to convert the type to Body here.  */
    if (b->type == TYPE_TEXT)
    {
      b->subtype = mutt_str_dup("plain");
    }
    else if (b->type == TYPE_AUDIO)
    {
      b->subtype = mutt_str_dup("basic");
    }
    else if (b->type == TYPE_MESSAGE)
    {
      b->subtype = mutt_str_dup("rfc822");
    }
    else if (b->type == TYPE_OTHER)
    {
      char buf[128] = { 0 };

      b->type = TYPE_APPLICATION;
      snprintf(buf, sizeof(buf), "x-%s", s);
      b->subtype = mutt_str_dup(buf);
    }
    else
    {
      b->subtype = mutt_str_dup("x-unknown");
    }
  }

  /* Default character set for text types. */
  if (b->type == TYPE_TEXT)
  {
    pc = mutt_param_get(&b->parameter, "charset");
    if (pc)
    {
      /* Microsoft Outlook seems to think it is necessary to repeat
       * charset=, strip it off not to confuse ourselves */
      if (mutt_istrn_equal(pc, "charset=", sizeof("charset=") - 1))
        mutt_param_set(&b->parameter, "charset", pc + (sizeof("charset=") - 1));
    }
    else
    {
      mutt_param_set(&b->parameter, "charset",
                     mutt_ch_get_default_charset(cc_assumed_charset()));
    }
  }
}

#ifdef USE_AUTOCRYPT
/**
 * parse_autocrypt - Parse an Autocrypt header line
 * @param head Autocrypt header to insert before
 * @param s    Header string to parse
 * @retval ptr New AutocryptHeader inserted before head
 */
static struct AutocryptHeader *parse_autocrypt(struct AutocryptHeader *head, const char *s)
{
  struct AutocryptHeader *autocrypt = mutt_autocrypthdr_new();
  autocrypt->next = head;

  struct ParameterList pl = TAILQ_HEAD_INITIALIZER(pl);
  parse_parameters(&pl, s, true);
  if (TAILQ_EMPTY(&pl))
  {
    autocrypt->invalid = true;
    goto cleanup;
  }

  struct Parameter *p = NULL;
  TAILQ_FOREACH(p, &pl, entries)
  {
    if (mutt_istr_equal(p->attribute, "addr"))
    {
      if (autocrypt->addr)
      {
        autocrypt->invalid = true;
        goto cleanup;
      }
      autocrypt->addr = p->value;
      p->value = NULL;
    }
    else if (mutt_istr_equal(p->attribute, "prefer-encrypt"))
    {
      if (mutt_istr_equal(p->value, "mutual"))
        autocrypt->prefer_encrypt = true;
    }
    else if (mutt_istr_equal(p->attribute, "keydata"))
    {
      if (autocrypt->keydata)
      {
        autocrypt->invalid = true;
        goto cleanup;
      }
      autocrypt->keydata = p->value;
      p->value = NULL;
    }
    else if (p->attribute && (p->attribute[0] != '_'))
    {
      autocrypt->invalid = true;
      goto cleanup;
    }
  }

  /* Checking the addr against From, and for multiple valid headers
   * occurs later, after all the headers are parsed. */
  if (!autocrypt->addr || !autocrypt->keydata)
    autocrypt->invalid = true;

cleanup:
  mutt_param_free(&pl);
  return autocrypt;
}
#endif

/**
 * rfc2369_first_mailto - Extract the first mailto: URL from a RFC2369 list
 * @param body Body of the header
 * @retval ptr First mailto: URL found, or NULL if none was found
 */
static char *rfc2369_first_mailto(const char *body)
{
  for (const char *beg = body, *end = NULL; beg; beg = strchr(end, ','))
  {
    beg = strchr(beg, '<');
    if (!beg)
    {
      break;
    }
    beg++;
    end = strchr(beg, '>');
    if (!end)
    {
      break;
    }

    char *mlist = mutt_strn_dup(beg, end - beg);
    if (url_check_scheme(mlist) == U_MAILTO)
    {
      return mlist;
    }
    FREE(&mlist);
  }
  return NULL;
}

/**
 * mutt_rfc822_parse_line - Parse an email header
 * @param env       Envelope of the email
 * @param e         Email
 * @param name      Header field name, e.g. 'to'
 * @param name_len  Must be equivalent to strlen(name)
 * @param body      Header field body, e.g. 'john@example.com'
 * @param user_hdrs If true, save into the Envelope's userhdrs
 * @param weed      If true, perform header weeding (filtering)
 * @param do_2047   If true, perform RFC2047 decoding of the field
 * @retval 1 The field is recognised
 * @retval 0 The field is not recognised
 *
 * Process a line from an email header.  Each line that is recognised is parsed
 * and the information put in the Envelope or Header.
 */
int mutt_rfc822_parse_line(struct Envelope *env, struct Email *e,
                           const char *name, size_t name_len, const char *body,
                           bool user_hdrs, bool weed, bool do_2047)
{
  if (!env || !name)
    return 0;

  bool matched = false;

  switch (name[0] | 0x20)
  {
    case 'a':
      if ((name_len == 13) && eqi12(name + 1, "pparently-to"))
      {
        mutt_addrlist_parse(&env->to, body);
        matched = true;
      }
      else if ((name_len == 15) && eqi14(name + 1, "pparently-from"))
      {
        mutt_addrlist_parse(&env->from, body);
        matched = true;
      }
#ifdef USE_AUTOCRYPT
      else if ((name_len == 9) && eqi8(name + 1, "utocrypt"))
      {
        const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
        if (c_autocrypt)
        {
          env->autocrypt = parse_autocrypt(env->autocrypt, body);
          matched = true;
        }
      }
      else if ((name_len == 16) && eqi15(name + 1, "utocrypt-gossip"))
      {
        const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
        if (c_autocrypt)
        {
          env->autocrypt_gossip = parse_autocrypt(env->autocrypt_gossip, body);
          matched = true;
        }
      }
#endif
      break;

    case 'b':
      if ((name_len == 3) && eqi2(name + 1, "cc"))
      {
        mutt_addrlist_parse(&env->bcc, body);
        matched = true;
      }
      break;

    case 'c':
      if ((name_len == 2) && eqi1(name + 1, "c"))
      {
        mutt_addrlist_parse(&env->cc, body);
        matched = true;
      }
      else
      {
        if ((name_len >= 12) && eqi8(name, "content-"))
        {
          if ((name_len == 12) && eqi4(name + 8, "type"))
          {
            if (e)
              mutt_parse_content_type(body, e->body);
            matched = true;
          }
          else if ((name_len == 16) && eqi8(name + 8, "language"))
          {
            if (e)
              parse_content_language(body, e->body);
            matched = true;
          }
          else if ((name_len == 25) && eqi17(name + 8, "transfer-encoding"))
          {
            if (e)
              e->body->encoding = mutt_check_encoding(body);
            matched = true;
          }
          else if ((name_len == 14) && eqi8(name + 6, "t-length"))
          {
            if (e)
            {
              unsigned long len = 0;
              e->body->length = mutt_str_atoul(body, &len) ? MIN(len, CONTENT_TOO_BIG) : -1;
            }
            matched = true;
          }
          else if ((name_len == 19) && eqi11(name + 8, "description"))
          {
            if (e)
            {
              mutt_str_replace(&e->body->description, body);
              rfc2047_decode(&e->body->description);
            }
            matched = true;
          }
          else if ((name_len == 19) && eqi11(name + 8, "disposition"))
          {
            if (e)
              parse_content_disposition(body, e->body);
            matched = true;
          }
        }
      }
      break;

    case 'd':
      if ((name_len != 4) || !eqi4(name, "date"))
        break;

      mutt_str_replace(&env->date, body);
      if (e)
      {
        struct Tz tz = { 0 };
        // the caller will check e->date_sent for -1
        e->date_sent = mutt_date_parse_date(body, &tz);
        if (e->date_sent > 0)
        {
          e->zhours = tz.zhours;
          e->zminutes = tz.zminutes;
          e->zoccident = tz.zoccident;
        }
      }
      matched = true;
      break;

    case 'e':
      if ((name_len == 7) && eqi6(name + 1, "xpires") && e)
      {
        const time_t expired = mutt_date_parse_date(body, NULL);
        if ((expired != -1) && (expired < mutt_date_now()))
        {
          e->expired = true;
        }
      }
      break;

    case 'f':
      if ((name_len == 4) && eqi4(name, "from"))
      {
        mutt_addrlist_parse(&env->from, body);
        matched = true;
      }
      else if ((name_len == 11) && eqi10(name + 1, "ollowup-to"))
      {
        if (!env->followup_to)
        {
          env->followup_to = mutt_str_dup(mutt_str_skip_whitespace(body));
          mutt_str_remove_trailing_ws(env->followup_to);
        }
        matched = true;
      }
      break;

    case 'i':
      if ((name_len != 11) || !eqi10(name + 1, "n-reply-to"))
        break;

      mutt_list_free(&env->in_reply_to);
      char *body2 = mutt_str_dup(body); // Create a mutable copy
      mutt_filter_commandline_header_value(body2);
      parse_references(&env->in_reply_to, body2);
      FREE(&body2);
      matched = true;
      break;

    case 'l':
      if ((name_len == 5) && eqi4(name + 1, "ines"))
      {
        if (e)
        {
          unsigned int ui = 0; // we don't want a negative number of lines
          mutt_str_atoui(body, &ui);
          e->lines = ui;
        }

        matched = true;
      }
      else if ((name_len == 9) && eqi8(name + 1, "ist-post"))
      {
        /* RFC2369 */
        if (!mutt_strn_equal(mutt_str_skip_whitespace(body), "NO", 2))
        {
          char *mailto = rfc2369_first_mailto(body);
          if (mailto)
          {
            FREE(&env->list_post);
            env->list_post = mailto;
            const bool c_auto_subscribe = cs_subset_bool(NeoMutt->sub, "auto_subscribe");
            if (c_auto_subscribe)
              mutt_auto_subscribe(env->list_post);
          }
        }
        matched = true;
      }
      else if ((name_len == 14) && eqi13(name + 1, "ist-subscribe"))
      {
        /* RFC2369 */
        char *mailto = rfc2369_first_mailto(body);
        if (mailto)
        {
          FREE(&env->list_subscribe);
          env->list_subscribe = mailto;
        }
        matched = true;
      }
      else if ((name_len == 16) && eqi15(name + 1, "ist-unsubscribe"))
      {
        /* RFC2369 */
        char *mailto = rfc2369_first_mailto(body);
        if (mailto)
        {
          FREE(&env->list_unsubscribe);
          env->list_unsubscribe = mailto;
        }
        matched = true;
      }
      break;

    case 'm':
      if ((name_len == 12) && eqi11(name + 1, "ime-version"))
      {
        if (e)
          e->mime = true;
        matched = true;
      }
      else if ((name_len == 10) && eqi9(name + 1, "essage-id"))
      {
        /* We add a new "Message-ID:" when building a message */
        FREE(&env->message_id);
        env->message_id = mutt_extract_message_id(body, NULL);
        matched = true;
      }
      else
      {
        if ((name_len >= 13) && eqi4(name + 1, "ail-"))
        {
          if ((name_len == 13) && eqi8(name + 5, "reply-to"))
          {
            /* override the Reply-To: field */
            mutt_addrlist_clear(&env->reply_to);
            mutt_addrlist_parse(&env->reply_to, body);
            matched = true;
          }
          else if ((name_len == 16) && eqi11(name + 5, "followup-to"))
          {
            mutt_addrlist_parse(&env->mail_followup_to, body);
            matched = true;
          }
        }
      }
      break;

    case 'n':
      if ((name_len == 10) && eqi9(name + 1, "ewsgroups"))
      {
        FREE(&env->newsgroups);
        env->newsgroups = mutt_str_dup(mutt_str_skip_whitespace(body));
        mutt_str_remove_trailing_ws(env->newsgroups);
        matched = true;
      }
      break;

    case 'o':
      /* field 'Organization:' saves only for pager! */
      if ((name_len == 12) && eqi11(name + 1, "rganization"))
      {
        if (!env->organization && !mutt_istr_equal(body, "unknown"))
          env->organization = mutt_str_dup(body);
      }
      break;

    case 'r':
      if ((name_len == 10) && eqi9(name + 1, "eferences"))
      {
        mutt_list_free(&env->references);
        parse_references(&env->references, body);
        matched = true;
      }
      else if ((name_len == 8) && eqi8(name, "reply-to"))
      {
        mutt_addrlist_parse(&env->reply_to, body);
        matched = true;
      }
      else if ((name_len == 11) && eqi10(name + 1, "eturn-path"))
      {
        mutt_addrlist_parse(&env->return_path, body);
        matched = true;
      }
      else if ((name_len == 8) && eqi8(name, "received"))
      {
        if (e && (e->received == 0))
        {
          char *d = strrchr(body, ';');
          if (d)
          {
            d = mutt_str_skip_email_wsp(d + 1);
            // the caller will check e->received for -1
            e->received = mutt_date_parse_date(d, NULL);
          }
        }
      }
      break;

    case 's':
      if ((name_len == 7) && eqi6(name + 1, "ubject"))
      {
        if (!env->subject)
          mutt_env_set_subject(env, body);
        matched = true;
      }
      else if ((name_len == 6) && eqi5(name + 1, "ender"))
      {
        mutt_addrlist_parse(&env->sender, body);
        matched = true;
      }
      else if ((name_len == 6) && eqi5(name + 1, "tatus"))
      {
        if (e)
        {
          while (*body)
          {
            switch (*body)
            {
              case 'O':
              {
                e->old = true;
                break;
              }
              case 'R':
                e->read = true;
                break;
              case 'r':
                e->replied = true;
                break;
            }
            body++;
          }
        }
        matched = true;
      }
      else if (e && (name_len == 10) && eqi1(name + 1, "u") &&
               (eqi8(name + 2, "persedes") || eqi8(name + 2, "percedes")))
      {
        FREE(&env->supersedes);
        env->supersedes = mutt_str_dup(body);
      }
      break;

    case 't':
      if ((name_len == 2) && eqi1(name + 1, "o"))
      {
        mutt_addrlist_parse(&env->to, body);
        matched = true;
      }
      break;

    case 'x':
      if ((name_len == 8) && eqi8(name, "x-status"))
      {
        if (e)
        {
          while (*body)
          {
            switch (*body)
            {
              case 'A':
                e->replied = true;
                break;
              case 'D':
                e->deleted = true;
                break;
              case 'F':
                e->flagged = true;
                break;
              default:
                break;
            }
            body++;
          }
        }
        matched = true;
      }
      else if ((name_len == 7) && eqi6(name + 1, "-label"))
      {
        FREE(&env->x_label);
        env->x_label = mutt_str_dup(body);
        matched = true;
      }
      else if ((name_len == 12) && eqi11(name + 1, "-comment-to"))
      {
        if (!env->x_comment_to)
          env->x_comment_to = mutt_str_dup(body);
        matched = true;
      }
      else if ((name_len == 4) && eqi4(name, "xref"))
      {
        if (!env->xref)
          env->xref = mutt_str_dup(body);
        matched = true;
      }
      else if ((name_len == 13) && eqi12(name + 1, "-original-to"))
      {
        mutt_addrlist_parse(&env->x_original_to, body);
        matched = true;
      }
      break;

    default:
      break;
  }

  /* Keep track of the user-defined headers */
  if (!matched && user_hdrs)
  {
    const bool c_weed = cs_subset_bool(NeoMutt->sub, "weed");
    char *dup = NULL;
    mutt_str_asprintf(&dup, "%s: %s", name, body);

    if (!weed || !c_weed || !mutt_matches_ignore(dup))
    {
      struct ListNode *np = mutt_list_insert_tail(&env->userhdrs, dup);
      if (do_2047)
      {
        rfc2047_decode(&np->data);
      }
    }
    else
    {
      FREE(&dup);
    }
  }

  return matched;
}

/**
 * mutt_rfc822_read_line - Read a header line from a file
 * @param fp      File to read from
 * @param buf     Buffer to store the result
 * @retval num Number of bytes read from fp
 *
 * Reads an arbitrarily long header field, and looks ahead for continuation
 * lines.
 */
size_t mutt_rfc822_read_line(FILE *fp, struct Buffer *buf)
{
  if (!fp || !buf)
    return 0;

  size_t read = 0;
  char line[1024] = { 0 }; /* RFC2822 specifies a maximum line length of 998 */

  buf_reset(buf);
  while (true)
  {
    if (!fgets(line, sizeof(line), fp))
    {
      return 0;
    }

    const size_t linelen = mutt_str_len(line);
    if (linelen == 0)
    {
      break;
    }

    if (mutt_str_is_email_wsp(line[0]) && buf_is_empty(buf))
    {
      read = linelen;
      break;
    }

    read += linelen;

    size_t off = linelen - 1;
    if (line[off] == '\n')
    {
      /* We did get a full line: remove trailing space */
      do
      {
        line[off] = '\0';
      } while (off && mutt_str_is_email_wsp(line[--off]));

      /* check to see if the next line is a continuation line */
      int ch = fgetc(fp);
      if ((ch != ' ') && (ch != '\t'))
      {
        /* next line is a separate header field or EOH */
        ungetc(ch, fp);
        buf_addstr(buf, line);
        break;
      }
      read++;

      /* eat tabs and spaces from the beginning of the continuation line */
      while (((ch = fgetc(fp)) == ' ') || (ch == '\t'))
      {
        read++;
      }

      ungetc(ch, fp);
      line[off + 1] = ' '; /* string is still terminated because we removed
                              at least one whitespace char above */
    }

    buf_addstr(buf, line);
  }

  return read;
}

/**
 * mutt_rfc822_read_header - Parses an RFC822 header
 * @param fp        Stream to read from
 * @param e         Current Email (optional)
 * @param user_hdrs If set, store user headers
 *                  Used for recall-message and postpone modes
 * @param weed      If this parameter is set and the user has activated the
 *                  $weed option, honor the header weed list for user headers.
 *                  Used for recall-message
 * @retval ptr Newly allocated envelope structure
 *
 * Caller should free the Envelope using mutt_env_free().
 */
struct Envelope *mutt_rfc822_read_header(FILE *fp, struct Email *e, bool user_hdrs, bool weed)
{
  if (!fp)
    return NULL;

  struct Envelope *env = mutt_env_new();
  char *p = NULL;
  LOFF_T loc = e ? e->offset : ftello(fp);
  if (loc < 0)
  {
    mutt_debug(LL_DEBUG1, "ftello: %s (errno %d)\n", strerror(errno), errno);
    loc = 0;
  }

  struct Buffer *line = buf_pool_get();

  if (e)
  {
    if (!e->body)
    {
      e->body = mutt_body_new();

      /* set the defaults from RFC1521 */
      e->body->type = TYPE_TEXT;
      e->body->subtype = mutt_str_dup("plain");
      e->body->encoding = ENC_7BIT;
      e->body->length = -1;

      /* RFC2183 says this is arbitrary */
      e->body->disposition = DISP_INLINE;
    }
  }

  while (true)
  {
    LOFF_T line_start_loc = loc;
    size_t len = mutt_rfc822_read_line(fp, line);
    if (buf_is_empty(line))
    {
      break;
    }
    loc += len;
    const char *lines = buf_string(line);
    p = strpbrk(lines, ": \t");
    if (!p || (*p != ':'))
    {
      char return_path[1024] = { 0 };
      time_t t = 0;

      /* some bogus MTAs will quote the original "From " line */
      if (mutt_str_startswith(lines, ">From "))
      {
        continue; /* just ignore */
      }
      else if (is_from(lines, return_path, sizeof(return_path), &t))
      {
        /* MH sometimes has the From_ line in the middle of the header! */
        if (e && (e->received == 0))
          e->received = t - mutt_date_local_tz(t);
        continue;
      }

      /* We need to seek back to the start of the body. Note that we
       * keep track of loc ourselves, since calling ftello() incurs
       * a syscall, which can be expensive to do for every single line */
      (void) mutt_file_seek(fp, line_start_loc, SEEK_SET);
      break; /* end of header */
    }
    size_t name_len = p - lines;

    char buf[1024] = { 0 };
    if (mutt_replacelist_match(&SpamList, buf, sizeof(buf), lines))
    {
      if (!mutt_regexlist_match(&NoSpamList, lines))
      {
        /* if spam tag already exists, figure out how to amend it */
        if ((!buf_is_empty(&env->spam)) && (*buf != '\0'))
        {
          /* If `$spam_separator` defined, append with separator */
          const char *const c_spam_separator = cs_subset_string(NeoMutt->sub, "spam_separator");
          if (c_spam_separator)
          {
            buf_addstr(&env->spam, c_spam_separator);
            buf_addstr(&env->spam, buf);
          }
          else /* overwrite */
          {
            buf_reset(&env->spam);
            buf_addstr(&env->spam, buf);
          }
        }
        else if (buf_is_empty(&env->spam) && (*buf != '\0'))
        {
          /* spam tag is new, and match expr is non-empty; copy */
          buf_addstr(&env->spam, buf);
        }
        else if (buf_is_empty(&env->spam))
        {
          /* match expr is empty; plug in null string if no existing tag */
          buf_addstr(&env->spam, "");
        }

        if (!buf_is_empty(&env->spam))
          mutt_debug(LL_DEBUG5, "spam = %s\n", env->spam.data);
      }
    }

    *p = '\0';
    p = mutt_str_skip_email_wsp(p + 1);
    if (*p == '\0')
      continue; /* skip empty header fields */

    mutt_rfc822_parse_line(env, e, lines, name_len, p, user_hdrs, weed, true);
  }

  buf_pool_release(&line);

  if (e)
  {
    e->body->hdr_offset = e->offset;
    e->body->offset = ftello(fp);

    rfc2047_decode_envelope(env);

    if (e->received < 0)
    {
      mutt_debug(LL_DEBUG1, "resetting invalid received time to 0\n");
      e->received = 0;
    }

    /* check for missing or invalid date */
    if (e->date_sent <= 0)
    {
      mutt_debug(LL_DEBUG1, "no date found, using received time from msg separator\n");
      e->date_sent = e->received;
    }

#ifdef USE_AUTOCRYPT
    const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
    if (c_autocrypt)
    {
      mutt_autocrypt_process_autocrypt_header(e, env);
      /* No sense in taking up memory after the header is processed */
      mutt_autocrypthdr_free(&env->autocrypt);
    }
#endif
  }

  return env;
}

/**
 * mutt_read_mime_header - Parse a MIME header
 * @param fp      stream to read from
 * @param digest  true if reading subparts of a multipart/digest
 * @retval ptr New Body containing parsed structure
 */
struct Body *mutt_read_mime_header(FILE *fp, bool digest)
{
  if (!fp)
    return NULL;

  struct Body *b = mutt_body_new();
  struct Envelope *env = mutt_env_new();
  char *c = NULL;
  struct Buffer *buf = buf_pool_get();
  bool matched = false;

  b->hdr_offset = ftello(fp);

  b->encoding = ENC_7BIT; /* default from RFC1521 */
  b->type = digest ? TYPE_MESSAGE : TYPE_TEXT;
  b->disposition = DISP_INLINE;

  while (mutt_rfc822_read_line(fp, buf) != 0)
  {
    const char *line = buf_string(buf);
    /* Find the value of the current header */
    c = strchr(line, ':');
    if (c)
    {
      *c = '\0';
      c = mutt_str_skip_email_wsp(c + 1);
      if (*c == '\0')
      {
        mutt_debug(LL_DEBUG1, "skipping empty header field: %s\n", line);
        continue;
      }
    }
    else
    {
      mutt_debug(LL_DEBUG1, "bogus MIME header: %s\n", line);
      break;
    }

    size_t plen = mutt_istr_startswith(line, "content-");
    if (plen != 0)
    {
      if (mutt_istr_equal("type", line + plen))
      {
        mutt_parse_content_type(c, b);
      }
      else if (mutt_istr_equal("language", line + plen))
      {
        parse_content_language(c, b);
      }
      else if (mutt_istr_equal("transfer-encoding", line + plen))
      {
        b->encoding = mutt_check_encoding(c);
      }
      else if (mutt_istr_equal("disposition", line + plen))
      {
        parse_content_disposition(c, b);
      }
      else if (mutt_istr_equal("description", line + plen))
      {
        mutt_str_replace(&b->description, c);
        rfc2047_decode(&b->description);
      }
      else if (mutt_istr_equal("id", line + plen))
      {
        // strip <angle braces> from Content-ID: header
        char *id = c;
        int cid_len = mutt_str_len(c);
        if (cid_len > 2)
        {
          if (id[0] == '<')
          {
            id++;
            cid_len--;
          }
          if (id[cid_len - 1] == '>')
            id[cid_len - 1] = '\0';
        }
        mutt_str_replace(&b->content_id, id);
      }
    }
    else if ((plen = mutt_istr_startswith(line, "x-sun-")))
    {
      if (mutt_istr_equal("data-type", line + plen))
      {
        mutt_parse_content_type(c, b);
      }
      else if (mutt_istr_equal("encoding-info", line + plen))
      {
        b->encoding = mutt_check_encoding(c);
      }
      else if (mutt_istr_equal("content-lines", line + plen))
      {
        mutt_param_set(&b->parameter, "content-lines", c);
      }
      else if (mutt_istr_equal("data-description", line + plen))
      {
        mutt_str_replace(&b->description, c);
        rfc2047_decode(&b->description);
      }
    }
    else
    {
      if (mutt_rfc822_parse_line(env, NULL, line, strlen(line), c, false, false, false))
      {
        matched = true;
      }
    }
  }
  b->offset = ftello(fp); /* Mark the start of the real data */
  if ((b->type == TYPE_TEXT) && !b->subtype)
    b->subtype = mutt_str_dup("plain");
  else if ((b->type == TYPE_MESSAGE) && !b->subtype)
    b->subtype = mutt_str_dup("rfc822");

  buf_pool_release(&buf);

  if (matched)
  {
    b->mime_headers = env;
    rfc2047_decode_envelope(b->mime_headers);
  }
  else
  {
    mutt_env_free(&env);
  }

  return b;
}

/**
 * mutt_is_message_type - Determine if a mime type matches a message or not
 * @param type    Message type enum value
 * @param subtype Message subtype
 * @retval true  Type is message/news or message/rfc822
 * @retval false Otherwise
 */
bool mutt_is_message_type(int type, const char *subtype)
{
  if (type != TYPE_MESSAGE)
    return false;

  subtype = NONULL(subtype);
  return (mutt_istr_equal(subtype, "rfc822") ||
          mutt_istr_equal(subtype, "news") || mutt_istr_equal(subtype, "global"));
}

/**
 * parse_part - Parse a MIME part
 * @param fp      File to read from
 * @param b       Body to store the results in
 * @param counter Number of parts processed so far
 */
static void parse_part(FILE *fp, struct Body *b, int *counter)
{
  if (!fp || !b)
    return;

  const char *bound = NULL;
  static unsigned short recurse_level = 0;

  if (recurse_level >= MUTT_MIME_MAX_DEPTH)
  {
    mutt_debug(LL_DEBUG1, "recurse level too deep. giving up\n");
    return;
  }
  recurse_level++;

  switch (b->type)
  {
    case TYPE_MULTIPART:
      if (mutt_istr_equal(b->subtype, "x-sun-attachment"))
        bound = "--------";
      else
        bound = mutt_param_get(&b->parameter, "boundary");

      if (!mutt_file_seek(fp, b->offset, SEEK_SET))
      {
        goto bail;
      }
      b->parts = parse_multipart(fp, bound, b->offset + b->length,
                                 mutt_istr_equal("digest", b->subtype), counter);
      break;

    case TYPE_MESSAGE:
      if (!b->subtype)
        break;

      if (!mutt_file_seek(fp, b->offset, SEEK_SET))
      {
        goto bail;
      }
      if (mutt_is_message_type(b->type, b->subtype))
        b->parts = rfc822_parse_message(fp, b, counter);
      else if (mutt_istr_equal(b->subtype, "external-body"))
        b->parts = mutt_read_mime_header(fp, 0);
      else
        goto bail;
      break;

    default:
      goto bail;
  }

  /* try to recover from parsing error */
  if (!b->parts)
  {
    b->type = TYPE_TEXT;
    mutt_str_replace(&b->subtype, "plain");
  }
bail:
  recurse_level--;
}

/**
 * parse_multipart - Parse a multipart structure
 * @param fp       Stream to read from
 * @param boundary Body separator
 * @param end_off  Length of the multipart body (used when the final
 *                 boundary is missing to avoid reading too far)
 * @param digest   true if reading a multipart/digest
 * @param counter  Number of parts processed so far
 * @retval ptr New Body containing parsed structure
 */
static struct Body *parse_multipart(FILE *fp, const char *boundary,
                                    LOFF_T end_off, bool digest, int *counter)
{
  if (!fp)
    return NULL;

  if (!boundary)
  {
    mutt_error(_("multipart message has no boundary parameter"));
    return NULL;
  }

  char buf[1024] = { 0 };
  struct Body *head = NULL, *last = NULL, *new_body = NULL;
  bool final = false; /* did we see the ending boundary? */

  const size_t blen = mutt_str_len(boundary);
  while ((ftello(fp) < end_off) && fgets(buf, sizeof(buf), fp))
  {
    const size_t len = mutt_str_len(buf);

    const size_t crlf = ((len > 1) && (buf[len - 2] == '\r')) ? 1 : 0;

    if ((buf[0] == '-') && (buf[1] == '-') && mutt_str_startswith(buf + 2, boundary))
    {
      if (last)
      {
        last->length = ftello(fp) - last->offset - len - 1 - crlf;
        if (last->parts && (last->parts->length == 0))
          last->parts->length = ftello(fp) - last->parts->offset - len - 1 - crlf;
        /* if the body is empty, we can end up with a -1 length */
        if (last->length < 0)
          last->length = 0;
      }

      if (len > 0)
      {
        /* Remove any trailing whitespace, up to the length of the boundary */
        for (size_t i = len - 1; isspace(buf[i]) && (i >= (blen + 2)); i--)
          buf[i] = '\0';
      }

      /* Check for the end boundary */
      if (mutt_str_equal(buf + blen + 2, "--"))
      {
        final = true;
        break; /* done parsing */
      }
      else if (buf[2 + blen] == '\0')
      {
        new_body = mutt_read_mime_header(fp, digest);
        if (!new_body)
          break;

        if (mutt_param_get(&new_body->parameter, "content-lines"))
        {
          int lines = 0;
          mutt_str_atoi(mutt_param_get(&new_body->parameter, "content-lines"), &lines);
          for (; lines > 0; lines--)
            if ((ftello(fp) >= end_off) || !fgets(buf, sizeof(buf), fp))
              break;
        }

        /* Consistency checking - catch bad attachment end boundaries */
        if (new_body->offset > end_off)
        {
          mutt_body_free(&new_body);
          break;
        }
        if (head)
        {
          last->next = new_body;
          last = new_body;
        }
        else
        {
          last = new_body;
          head = new_body;
        }

        /* It seems more intuitive to add the counter increment to
         * parse_part(), but we want to stop the case where a multipart
         * contains thousands of tiny parts before the memory and data
         * structures are allocated.  */
        if (++(*counter) >= MUTT_MIME_MAX_PARTS)
          break;
      }
    }
  }

  /* in case of missing end boundary, set the length to something reasonable */
  if (last && (last->length == 0) && !final)
    last->length = end_off - last->offset;

  /* parse recursive MIME parts */
  for (last = head; last; last = last->next)
    parse_part(fp, last, counter);

  return head;
}

/**
 * rfc822_parse_message - Parse a Message/RFC822 body
 * @param fp      Stream to read from
 * @param parent  Info about the message/rfc822 body part
 * @param counter Number of parts processed so far
 * @retval ptr New Body containing parsed message
 *
 * @note This assumes that 'parent->length' has been set!
 */
static struct Body *rfc822_parse_message(FILE *fp, struct Body *parent, int *counter)
{
  if (!fp || !parent)
    return NULL;

  parent->email = email_new();
  parent->email->offset = ftello(fp);
  parent->email->env = mutt_rfc822_read_header(fp, parent->email, false, false);
  struct Body *msg = parent->email->body;

  /* ignore the length given in the content-length since it could be wrong
   * and we already have the info to calculate the correct length */
  /* if (msg->length == -1) */
  msg->length = parent->length - (msg->offset - parent->offset);

  /* if body of this message is empty, we can end up with a negative length */
  if (msg->length < 0)
    msg->length = 0;

  parse_part(fp, msg, counter);
  return msg;
}

/**
 * mailto_header_allowed - Is the string in the list
 * @param s String to match
 * @param h Head of the List
 * @retval true String matches a List item (or List contains "*")
 *
 * This is a very specific function.  It searches a List of strings looking for
 * a match.  If the list contains a string "*", then it match any input string.
 *
 * This is similar to mutt_list_match(), except that it
 * doesn't allow prefix matches.
 *
 * @note The case of the strings is ignored.
 */
static bool mailto_header_allowed(const char *s, struct ListHead *h)
{
  if (!h)
    return false;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    if ((*np->data == '*') || mutt_istr_equal(s, np->data))
      return true;
  }
  return false;
}

/**
 * mutt_parse_mailto - Parse a mailto:// url
 * @param[in]  env  Envelope to fill
 * @param[out] body Body to
 * @param[in]  src  String to parse
 * @retval true  Success
 * @retval false Error
 */
bool mutt_parse_mailto(struct Envelope *env, char **body, const char *src)
{
  if (!env || !src)
    return false;

  struct Url *url = url_parse(src);
  if (!url)
    return false;

  if (url->host)
  {
    /* this is not a path-only URL */
    url_free(&url);
    return false;
  }

  mutt_addrlist_parse(&env->to, url->path);

  struct UrlQuery *np;
  STAILQ_FOREACH(np, &url->query_strings, entries)
  {
    mutt_filter_commandline_header_tag(np->name);
    const char *tag = np->name;
    char *value = np->value;
    /* Determine if this header field is on the allowed list.  Since NeoMutt
     * interprets some header fields specially (such as
     * "Attach: ~/.gnupg/secring.gpg"), care must be taken to ensure that
     * only safe fields are allowed.
     *
     * RFC2368, "4. Unsafe headers"
     * The user agent interpreting a mailto URL SHOULD choose not to create
     * a message if any of the headers are considered dangerous; it may also
     * choose to create a message with only a subset of the headers given in
     * the URL.  */
    if (mailto_header_allowed(tag, &MailToAllow))
    {
      if (mutt_istr_equal(tag, "body"))
      {
        if (body)
          mutt_str_replace(body, value);
      }
      else
      {
        char *scratch = NULL;
        size_t taglen = mutt_str_len(tag);

        mutt_filter_commandline_header_value(value);
        mutt_str_asprintf(&scratch, "%s: %s", tag, value);
        scratch[taglen] = 0; /* overwrite the colon as mutt_rfc822_parse_line expects */
        value = mutt_str_skip_email_wsp(&scratch[taglen + 1]);
        mutt_rfc822_parse_line(env, NULL, scratch, taglen, value, true, false, true);
        FREE(&scratch);
      }
    }
  }

  /* RFC2047 decode after the RFC822 parsing */
  rfc2047_decode_envelope(env);

  url_free(&url);
  return true;
}

/**
 * mutt_parse_part - Parse a MIME part
 * @param fp File to read from
 * @param b  Body to store the results in
 */
void mutt_parse_part(FILE *fp, struct Body *b)
{
  int counter = 0;

  parse_part(fp, b, &counter);
}

/**
 * mutt_rfc822_parse_message - Parse a Message/RFC822 body
 * @param fp Stream to read from
 * @param b  Info about the message/rfc822 body part
 * @retval ptr New Body containing parsed message
 *
 * @note This assumes that 'b->length' has been set!
 */
struct Body *mutt_rfc822_parse_message(FILE *fp, struct Body *b)
{
  int counter = 0;

  return rfc822_parse_message(fp, b, &counter);
}

/**
 * mutt_parse_multipart - Parse a multipart structure
 * @param fp       Stream to read from
 * @param boundary Body separator
 * @param end_off  Length of the multipart body (used when the final
 *                 boundary is missing to avoid reading too far)
 * @param digest   true if reading a multipart/digest
 * @retval ptr New Body containing parsed structure
 */
struct Body *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, bool digest)
{
  int counter = 0;

  return parse_multipart(fp, boundary, end_off, digest, &counter);
}
