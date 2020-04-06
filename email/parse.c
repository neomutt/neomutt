/**
 * @file
 * Miscellaneous email parsing routines
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page email_parse Miscellaneous email parsing routines
 *
 * Miscellaneous email parsing routines
 */

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "mutt.h"
#include "parse.h"
#include "body.h"
#include "email.h"
#include "envelope.h"
#include "from.h"
#include "globals.h"
#include "mime.h"
#include "mutt_globals.h"
#include "parameter.h"
#include "rfc2047.h"
#include "rfc2231.h"
#include "url.h"
#ifdef USE_AUTOCRYPT
#include "mutt_globals.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/* If the 'Content-Length' is bigger than 1GiB, then it's clearly wrong.
 * Cap the value to prevent overflow of Body.length */
#define CONTENT_TOO_BIG (1 << 30)

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
    const char *mailbox = TAILQ_FIRST(&lpenv->to)->mailbox;
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

  struct Buffer *buf = mutt_buffer_pool_get();
  /* allow_value_spaces, especially with autocrypt keydata, can result
   * in quite large parameter values.  avoid frequent reallocs by
   * pre-sizing */
  if (allow_value_spaces)
    mutt_buffer_alloc(buf, mutt_str_len(s));

  mutt_debug(LL_DEBUG2, "'%s'\n", s);

  while (*s)
  {
    mutt_buffer_reset(buf);

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
            if (C_AssumedCharset)
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
                mutt_buffer_addch(buf, *s);
              }
            }
            else
              mutt_buffer_addch(buf, *s);
          }
          if (*s)
            s++; /* skip over the " */
        }
        else
        {
          for (; *s && *s != ' ' && *s != ';'; s++)
            mutt_buffer_addch(buf, *s);
        }

        p = s;
      } while (allow_value_spaces && (*s == ' '));

      /* if the attribute token was missing, 'new' will be NULL */
      if (pnew)
      {
        pnew->value = mutt_buffer_strdup(buf);

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
  mutt_buffer_pool_release(&buf);
}

/**
 * parse_content_disposition - Parse a content disposition
 * @param s String to parse
 * @param ct Body to save the result
 *
 * e.g. parse a string "inline" and set #DISP_INLINE.
 */
static void parse_content_disposition(const char *s, struct Body *ct)
{
  struct ParameterList pl;
  TAILQ_INIT(&pl);

  if (mutt_istr_startswith(s, "inline"))
    ct->disposition = DISP_INLINE;
  else if (mutt_istr_startswith(s, "form-data"))
    ct->disposition = DISP_FORM_DATA;
  else
    ct->disposition = DISP_ATTACH;

  /* Check to see if a default filename was given */
  s = strchr(s, ';');
  if (s)
  {
    s = mutt_str_skip_email_wsp(s + 1);
    parse_parameters(&pl, s, false);
    s = mutt_param_get(&pl, "filename");
    if (s)
      mutt_str_replace(&ct->filename, s);
    s = mutt_param_get(&pl, "name");
    if (s)
      ct->form_name = mutt_str_dup(s);
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
 * @param s  Language string
 * @param ct Body of the email
 */
static void parse_content_language(const char *s, struct Body *ct)
{
  if (!s || !ct)
    return;

  mutt_debug(LL_DEBUG2, "RFC8255 >> Content-Language set to %s\n", s);
  ct->language = mutt_str_dup(s);
}

/**
 * mutt_matches_ignore - Does the string match the ignore list
 * @param s String to check
 * @retval true If string matches
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
 * @retval num MIME type, e.g. #TYPE_TEXT
 */
enum ContentType mutt_check_mime_type(const char *s)
{
  if (mutt_istr_equal("text", s))
    return TYPE_TEXT;
  if (mutt_istr_equal("multipart", s))
    return TYPE_MULTIPART;
#ifdef SUN_ATTACHMENT
  if (mutt_istr_equal("x-sun-attachment", s))
    return TYPE_MULTIPART;
#endif
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
 * @retval num Encoding type, e.g. #ENC_QUOTED_PRINTABLE
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
#ifdef SUN_ATTACHMENT
  if (mutt_istr_startswith(c, "uuencode"))
    return ENC_UUENCODED;
#endif
  return ENC_OTHER;
}

/**
 * mutt_parse_content_type - Parse a content type
 * @param s String to parse
 * @param ct Body to save the result
 *
 * e.g. parse a string "inline" and set #DISP_INLINE.
 */
void mutt_parse_content_type(const char *s, struct Body *ct)
{
  if (!s || !ct)
    return;

  FREE(&ct->subtype);
  mutt_param_free(&ct->parameter);

  /* First extract any existing parameters */
  char *pc = strchr(s, ';');
  if (pc)
  {
    *pc++ = 0;
    while (*pc && IS_SPACE(*pc))
      pc++;
    parse_parameters(&ct->parameter, pc, false);

    /* Some pre-RFC1521 gateways still use the "name=filename" convention,
     * but if a filename has already been set in the content-disposition,
     * let that take precedence, and don't set it here */
    pc = mutt_param_get(&ct->parameter, "name");
    if (pc && !ct->filename)
      ct->filename = mutt_str_dup(pc);

#ifdef SUN_ATTACHMENT
    /* this is deep and utter perversion */
    pc = mutt_param_get(&ct->parameter, "conversions");
    if (pc)
      ct->encoding = mutt_check_encoding(pc);
#endif
  }

  /* Now get the subtype */
  char *subtype = strchr(s, '/');
  if (subtype)
  {
    *subtype++ = '\0';
    for (pc = subtype; *pc && !IS_SPACE(*pc) && (*pc != ';'); pc++)
      ; // do nothing

    *pc = '\0';
    ct->subtype = mutt_str_dup(subtype);
  }

  /* Finally, get the major type */
  ct->type = mutt_check_mime_type(s);

#ifdef SUN_ATTACHMENT
  if (mutt_istr_equal("x-sun-attachment", s))
    ct->subtype = mutt_str_dup("x-sun-attachment");
#endif

  if (ct->type == TYPE_OTHER)
  {
    mutt_str_replace(&ct->xtype, s);
  }

  if (!ct->subtype)
  {
    /* Some older non-MIME mailers (i.e., mailtool, elm) have a content-type
     * field, so we can attempt to convert the type to Body here.  */
    if (ct->type == TYPE_TEXT)
      ct->subtype = mutt_str_dup("plain");
    else if (ct->type == TYPE_AUDIO)
      ct->subtype = mutt_str_dup("basic");
    else if (ct->type == TYPE_MESSAGE)
      ct->subtype = mutt_str_dup("rfc822");
    else if (ct->type == TYPE_OTHER)
    {
      char buf[128];

      ct->type = TYPE_APPLICATION;
      snprintf(buf, sizeof(buf), "x-%s", s);
      ct->subtype = mutt_str_dup(buf);
    }
    else
      ct->subtype = mutt_str_dup("x-unknown");
  }

  /* Default character set for text types. */
  if (ct->type == TYPE_TEXT)
  {
    pc = mutt_param_get(&ct->parameter, "charset");
    if (pc)
    {
      /* Microsoft Outlook seems to think it is necessary to repeat
       * charset=, strip it off not to confuse ourselves */
      if (mutt_istrn_equal(pc, "charset=", sizeof("charset=") - 1))
        mutt_param_set(&ct->parameter, "charset", pc + (sizeof("charset=") - 1));
    }
    else
    {
      mutt_param_set(&ct->parameter, "charset",
                     (C_AssumedCharset) ? (const char *) mutt_ch_get_default_charset() :
                                          "us-ascii");
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
 * mutt_rfc822_parse_line - Parse an email header
 * @param env       Envelope of the email
 * @param e         Email
 * @param line      Header field, e.g. 'to'
 * @param p         Header value, e.g. 'john@example.com'
 * @param user_hdrs If true, save into the Envelope's userhdrs
 * @param weed      If true, perform header weeding (filtering)
 * @param do_2047   If true, perform RFC2047 decoding of the field
 * @retval 1 If the field is recognised
 * @retval 0 If not
 *
 * Process a line from an email header.  Each line that is recognised is parsed
 * and the information put in the Envelope or Header.
 */
int mutt_rfc822_parse_line(struct Envelope *env, struct Email *e, char *line,
                           char *p, bool user_hdrs, bool weed, bool do_2047)
{
  if (!env || !line)
    return 0;

  bool matched = false;

  switch (tolower(line[0]))
  {
    case 'a':
      if (mutt_istr_equal(line + 1, "pparently-to"))
      {
        mutt_addrlist_parse(&env->to, p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "pparently-from"))
      {
        mutt_addrlist_parse(&env->from, p);
        matched = true;
      }
      break;

    case 'b':
      if (mutt_istr_equal(line + 1, "cc"))
      {
        mutt_addrlist_parse(&env->bcc, p);
        matched = true;
      }
      break;

    case 'c':
      if (mutt_istr_equal(line + 1, "c"))
      {
        mutt_addrlist_parse(&env->cc, p);
        matched = true;
      }
      else
      {
        size_t plen = mutt_istr_startswith(line + 1, "ontent-");
        if (plen != 0)
        {
          if (mutt_istr_equal(line + 1 + plen, "type"))
          {
            if (e)
              mutt_parse_content_type(p, e->content);
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "language"))
          {
            if (e)
              parse_content_language(p, e->content);
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "transfer-encoding"))
          {
            if (e)
              e->content->encoding = mutt_check_encoding(p);
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "length"))
          {
            if (e)
            {
              int rc = mutt_str_atol(p, (long *) &e->content->length);
              if ((rc < 0) || (e->content->length < 0))
                e->content->length = -1;
              if (e->content->length > CONTENT_TOO_BIG)
                e->content->length = CONTENT_TOO_BIG;
            }
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "description"))
          {
            if (e)
            {
              mutt_str_replace(&e->content->description, p);
              rfc2047_decode(&e->content->description);
            }
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "disposition"))
          {
            if (e)
              parse_content_disposition(p, e->content);
            matched = true;
          }
        }
      }
      break;

    case 'd':
      if (!mutt_istr_equal("ate", line + 1))
        break;

      mutt_str_replace(&env->date, p);
      if (e)
      {
        struct Tz tz;
        e->date_sent = mutt_date_parse_date(p, &tz);
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
      if (mutt_istr_equal("xpires", line + 1) && e &&
          (mutt_date_parse_date(p, NULL) < mutt_date_epoch()))
      {
        e->expired = true;
      }
      break;

    case 'f':
      if (mutt_istr_equal("rom", line + 1))
      {
        mutt_addrlist_parse(&env->from, p);
        matched = true;
      }
#ifdef USE_NNTP
      else if (mutt_istr_equal(line + 1, "ollowup-to"))
      {
        if (!env->followup_to)
        {
          mutt_str_remove_trailing_ws(p);
          env->followup_to = mutt_str_dup(mutt_str_skip_whitespace(p));
        }
        matched = true;
      }
#endif
      break;

    case 'i':
      if (!mutt_istr_equal(line + 1, "n-reply-to"))
        break;

      mutt_list_free(&env->in_reply_to);
      parse_references(&env->in_reply_to, p);
      matched = true;
      break;

    case 'l':
      if (mutt_istr_equal(line + 1, "ines"))
      {
        if (e)
        {
          /* HACK - neomutt has, for a very short time, produced negative
           * Lines header values.  Ignore them.  */
          if ((mutt_str_atoi(p, &e->lines) < 0) || (e->lines < 0))
            e->lines = 0;
        }

        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "ist-Post"))
      {
        /* RFC2369.  FIXME: We should ignore whitespace, but don't. */
        if (!mutt_strn_equal(p, "NO", 2))
        {
          char *beg = NULL, *end = NULL;
          for (beg = strchr(p, '<'); beg; beg = strchr(end, ','))
          {
            beg++;
            end = strchr(beg, '>');
            if (!end)
              break;

            char *mlist = mutt_strn_dup(beg, end - beg);
            /* Take the first mailto URL */
            if (url_check_scheme(mlist) == U_MAILTO)
            {
              FREE(&env->list_post);
              env->list_post = mlist;
              if (C_AutoSubscribe)
                mutt_auto_subscribe(env->list_post);

              break;
            }
            FREE(&mlist);
          }
        }
        matched = true;
      }
      break;

    case 'm':
      if (mutt_istr_equal(line + 1, "ime-version"))
      {
        if (e)
          e->mime = true;
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "essage-id"))
      {
        /* We add a new "Message-ID:" when building a message */
        FREE(&env->message_id);
        env->message_id = mutt_extract_message_id(p, NULL);
        matched = true;
      }
      else
      {
        size_t plen = mutt_istr_startswith(line + 1, "ail-");
        if (plen != 0)
        {
          if (mutt_istr_equal(line + 1 + plen, "reply-to"))
          {
            /* override the Reply-To: field */
            mutt_addrlist_clear(&env->reply_to);
            mutt_addrlist_parse(&env->reply_to, p);
            matched = true;
          }
          else if (mutt_istr_equal(line + 1 + plen, "followup-to"))
          {
            mutt_addrlist_parse(&env->mail_followup_to, p);
            matched = true;
          }
        }
      }
      break;

#ifdef USE_NNTP
    case 'n':
      if (mutt_istr_equal(line + 1, "ewsgroups"))
      {
        FREE(&env->newsgroups);
        mutt_str_remove_trailing_ws(p);
        env->newsgroups = mutt_str_dup(mutt_str_skip_whitespace(p));
        matched = true;
      }
      break;
#endif

    case 'o':
      /* field 'Organization:' saves only for pager! */
      if (mutt_istr_equal(line + 1, "rganization"))
      {
        if (!env->organization && !mutt_istr_equal(p, "unknown"))
          env->organization = mutt_str_dup(p);
      }
      break;

    case 'r':
      if (mutt_istr_equal(line + 1, "eferences"))
      {
        mutt_list_free(&env->references);
        parse_references(&env->references, p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "eply-to"))
      {
        mutt_addrlist_parse(&env->reply_to, p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "eturn-path"))
      {
        mutt_addrlist_parse(&env->return_path, p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "eceived"))
      {
        if (e && !e->received)
        {
          char *d = strrchr(p, ';');
          if (d)
          {
            d = mutt_str_skip_email_wsp(d + 1);
            e->received = mutt_date_parse_date(d, NULL);
          }
        }
      }
      break;

    case 's':
      if (mutt_istr_equal(line + 1, "ubject"))
      {
        if (!env->subject)
          env->subject = mutt_str_dup(p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "ender"))
      {
        mutt_addrlist_parse(&env->sender, p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "tatus"))
      {
        if (e)
        {
          while (*p)
          {
            switch (*p)
            {
              case 'O':
                e->old = C_MarkOld;
                break;
              case 'R':
                e->read = true;
                break;
              case 'r':
                e->replied = true;
                break;
            }
            p++;
          }
        }
        matched = true;
      }
      else if (e && (mutt_istr_equal("upersedes", line + 1) ||
                     mutt_istr_equal("upercedes", line + 1)))
      {
        FREE(&env->supersedes);
        env->supersedes = mutt_str_dup(p);
      }
      break;

    case 't':
      if (mutt_istr_equal(line + 1, "o"))
      {
        mutt_addrlist_parse(&env->to, p);
        matched = true;
      }
#ifdef USE_AUTOCRYPT
      else if (mutt_istr_equal(line + 1, "utocrypt"))
      {
        if (C_Autocrypt)
        {
          env->autocrypt = parse_autocrypt(env->autocrypt, p);
          matched = true;
        }
      }
      else if (mutt_istr_equal(line + 1, "utocrypt-gossip"))
      {
        if (C_Autocrypt)
        {
          env->autocrypt_gossip = parse_autocrypt(env->autocrypt_gossip, p);
          matched = true;
        }
      }
#endif
      break;

    case 'x':
      if (mutt_istr_equal(line + 1, "-status"))
      {
        if (e)
        {
          while (*p)
          {
            switch (*p)
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
            p++;
          }
        }
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "-label"))
      {
        FREE(&env->x_label);
        env->x_label = mutt_str_dup(p);
        matched = true;
      }
#ifdef USE_NNTP
      else if (mutt_istr_equal(line + 1, "-comment-to"))
      {
        if (!env->x_comment_to)
          env->x_comment_to = mutt_str_dup(p);
        matched = true;
      }
      else if (mutt_istr_equal(line + 1, "ref"))
      {
        if (!env->xref)
          env->xref = mutt_str_dup(p);
        matched = true;
      }
#endif
      else if (mutt_istr_equal(line + 1, "-original-to"))
      {
        mutt_addrlist_parse(&env->x_original_to, p);
        matched = true;
      }
      break;

    default:
      break;
  }

  /* Keep track of the user-defined headers */
  if (!matched && user_hdrs)
  {
    /* restore the original line */
    line[strlen(line)] = ':';

    if (!(weed && C_Weed && mutt_matches_ignore(line)))
    {
      struct ListNode *np = mutt_list_insert_tail(&env->userhdrs, mutt_str_dup(line));
      if (do_2047)
        rfc2047_decode(&np->data);
    }
  }

  return matched;
}

/**
 * mutt_rfc822_read_line - Read a header line from a file
 * @param fp      File to read from
 * @param line    Buffer to store the result
 * @param linelen Length of buffer
 * @retval ptr Line read from file
 *
 * Reads an arbitrarily long header field, and looks ahead for continuation
 * lines.  "line" must point to a dynamically allocated string; it is
 * increased if more space is required to fit the whole line.
 */
char *mutt_rfc822_read_line(FILE *fp, char *line, size_t *linelen)
{
  if (!fp || !line || !linelen)
    return NULL;

  char *buf = line;
  int ch;
  size_t offset = 0;

  while (true)
  {
    if (!fgets(buf, *linelen - offset, fp) || /* end of file or */
        (IS_SPACE(*line) && !offset))         /* end of headers */
    {
      *line = '\0';
      return line;
    }

    const size_t len = mutt_str_len(buf);
    if (len == 0)
      return line;

    buf += len - 1;
    if (*buf == '\n')
    {
      /* we did get a full line. remove trailing space */
      while (IS_SPACE(*buf))
      {
        *buf-- = '\0'; /* we can't come beyond line's beginning because
                        * it begins with a non-space */
      }

      /* check to see if the next line is a continuation line */
      ch = fgetc(fp);
      if ((ch != ' ') && (ch != '\t'))
      {
        ungetc(ch, fp);
        return line; /* next line is a separate header field or EOH */
      }

      /* eat tabs and spaces from the beginning of the continuation line */
      while (((ch = fgetc(fp)) == ' ') || (ch == '\t'))
        ; // do nothing

      ungetc(ch, fp);
      *++buf = ' '; /* string is still terminated because we removed
                       at least one whitespace char above */
    }

    buf++;
    offset = buf - line;
    if (*linelen < (offset + 256))
    {
      /* grow the buffer */
      *linelen += 256;
      mutt_mem_realloc(&line, *linelen);
      buf = line + offset;
    }
  }
  /* not reached */
}

/**
 * mutt_rfc822_read_header - parses an RFC822 header
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
  LOFF_T loc;
  size_t linelen = 1024;
  char *line = mutt_mem_malloc(linelen);
  char buf[linelen + 1];

  if (e)
  {
    if (!e->content)
    {
      e->content = mutt_body_new();

      /* set the defaults from RFC1521 */
      e->content->type = TYPE_TEXT;
      e->content->subtype = mutt_str_dup("plain");
      e->content->encoding = ENC_7BIT;
      e->content->length = -1;

      /* RFC2183 says this is arbitrary */
      e->content->disposition = DISP_INLINE;
    }
  }

  while ((loc = ftello(fp)) != -1)
  {
    line = mutt_rfc822_read_line(fp, line, &linelen);
    if (*line == '\0')
      break;
    p = strpbrk(line, ": \t");
    if (!p || (*p != ':'))
    {
      char return_path[1024];
      time_t t;

      /* some bogus MTAs will quote the original "From " line */
      if (mutt_str_startswith(line, ">From "))
        continue; /* just ignore */
      else if (is_from(line, return_path, sizeof(return_path), &t))
      {
        /* MH sometimes has the From_ line in the middle of the header! */
        if (e && !e->received)
          e->received = t - mutt_date_local_tz(t);
        continue;
      }

      fseeko(fp, loc, SEEK_SET);
      break; /* end of header */
    }

    *buf = '\0';

    if (mutt_replacelist_match(&SpamList, buf, sizeof(buf), line))
    {
      if (!mutt_regexlist_match(&NoSpamList, line))
      {
        /* if spam tag already exists, figure out how to amend it */
        if ((!mutt_buffer_is_empty(&env->spam)) && (*buf != '\0'))
        {
          /* If C_SpamSeparator defined, append with separator */
          if (C_SpamSeparator)
          {
            mutt_buffer_addstr(&env->spam, C_SpamSeparator);
            mutt_buffer_addstr(&env->spam, buf);
          }
          else /* overwrite */
          {
            mutt_buffer_reset(&env->spam);
            mutt_buffer_addstr(&env->spam, buf);
          }
        }

        /* spam tag is new, and match expr is non-empty; copy */
        else if (mutt_buffer_is_empty(&env->spam) && (*buf != '\0'))
        {
          mutt_buffer_addstr(&env->spam, buf);
        }

        /* match expr is empty; plug in null string if no existing tag */
        else if (mutt_buffer_is_empty(&env->spam))
        {
          mutt_buffer_addstr(&env->spam, "");
        }

        if (!mutt_buffer_is_empty(&env->spam))
          mutt_debug(LL_DEBUG5, "spam = %s\n", env->spam.data);
      }
    }

    *p = '\0';
    p = mutt_str_skip_email_wsp(p + 1);
    if (*p == '\0')
      continue; /* skip empty header fields */

    mutt_rfc822_parse_line(env, e, line, p, user_hdrs, weed, true);
  }

  FREE(&line);

  if (e)
  {
    e->content->hdr_offset = e->offset;
    e->content->offset = ftello(fp);

    rfc2047_decode_envelope(env);

    if (env->subject)
    {
      regmatch_t pmatch[1];

      if (mutt_regex_capture(C_ReplyRegex, env->subject, 1, pmatch))
      {
        env->real_subj = env->subject + pmatch[0].rm_eo;
      }
      else
        env->real_subj = env->subject;
    }

    if (e->received < 0)
    {
      mutt_debug(LL_DEBUG1, "resetting invalid received time to 0\n");
      e->received = 0;
    }

    /* check for missing or invalid date */
    if (e->date_sent <= 0)
    {
      mutt_debug(LL_DEBUG1,
                 "no date found, using received time from msg separator\n");
      e->date_sent = e->received;
    }

#ifdef USE_AUTOCRYPT
    if (C_Autocrypt)
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

  struct Body *p = mutt_body_new();
  struct Envelope *env = mutt_env_new();
  char *c = NULL;
  size_t linelen = 1024;
  char *line = mutt_mem_malloc(linelen);

  p->hdr_offset = ftello(fp);

  p->encoding = ENC_7BIT; /* default from RFC1521 */
  p->type = digest ? TYPE_MESSAGE : TYPE_TEXT;
  p->disposition = DISP_INLINE;

  while (*(line = mutt_rfc822_read_line(fp, line, &linelen)) != 0)
  {
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
        mutt_parse_content_type(c, p);
      else if (mutt_istr_equal("language", line + plen))
        parse_content_language(c, p);
      else if (mutt_istr_equal("transfer-encoding", line + plen))
        p->encoding = mutt_check_encoding(c);
      else if (mutt_istr_equal("disposition", line + plen))
        parse_content_disposition(c, p);
      else if (mutt_istr_equal("description", line + plen))
      {
        mutt_str_replace(&p->description, c);
        rfc2047_decode(&p->description);
      }
    }
#ifdef SUN_ATTACHMENT
    else if ((plen = mutt_istr_startswith(line, "x-sun-")))
    {
      if (mutt_istr_equal("data-type", line + plen))
        mutt_parse_content_type(c, p);
      else if (mutt_istr_equal("encoding-info", line + plen))
        p->encoding = mutt_check_encoding(c);
      else if (mutt_istr_equal("content-lines", line + plen))
        mutt_param_set(&p->parameter, "content-lines", c);
      else if (mutt_istr_equal("data-description", line + plen))
      {
        mutt_str_replace(&p->description, c);
        rfc2047_decode(&p->description);
      }
    }
#endif
    else
    {
      if (mutt_rfc822_parse_line(env, NULL, line, c, false, false, false))
        p->mime_headers = env;
    }
  }
  p->offset = ftello(fp); /* Mark the start of the real data */
  if ((p->type == TYPE_TEXT) && !p->subtype)
    p->subtype = mutt_str_dup("plain");
  else if ((p->type == TYPE_MESSAGE) && !p->subtype)
    p->subtype = mutt_str_dup("rfc822");

  FREE(&line);

  if (p->mime_headers)
    rfc2047_decode_envelope(p->mime_headers);
  else
    mutt_env_free(&env);

  return p;
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
 * mutt_parse_part - Parse a MIME part
 * @param fp File to read from
 * @param b  Body to store the results in
 */
void mutt_parse_part(FILE *fp, struct Body *b)
{
  if (!fp || !b)
    return;

  const char *bound = NULL;
  static unsigned short recurse_level = 0;

  if (recurse_level >= 100)
  {
    mutt_debug(LL_DEBUG1, "recurse level too deep. giving up.\n");
    return;
  }
  recurse_level++;

  switch (b->type)
  {
    case TYPE_MULTIPART:
#ifdef SUN_ATTACHMENT
      if (mutt_istr_equal(b->subtype, "x-sun-attachment"))
        bound = "--------";
      else
#endif
        bound = mutt_param_get(&b->parameter, "boundary");

      fseeko(fp, b->offset, SEEK_SET);
      b->parts = mutt_parse_multipart(fp, bound, b->offset + b->length,
                                      mutt_istr_equal("digest", b->subtype));
      break;

    case TYPE_MESSAGE:
      if (!b->subtype)
        break;

      fseeko(fp, b->offset, SEEK_SET);
      if (mutt_is_message_type(b->type, b->subtype))
        b->parts = mutt_rfc822_parse_message(fp, b);
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
 * mutt_parse_multipart - parse a multipart structure
 * @param fp       stream to read from
 * @param boundary body separator
 * @param end_off  length of the multipart body (used when the final
 *                 boundary is missing to avoid reading too far)
 * @param digest   true if reading a multipart/digest
 * @retval ptr New Body containing parsed structure
 */
struct Body *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, bool digest)
{
  if (!fp)
    return NULL;

  if (!boundary)
  {
    mutt_error(_("multipart message has no boundary parameter"));
    return NULL;
  }

  char buf[1024];
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
        for (size_t i = len - 1; IS_SPACE(buf[i]) && (i >= (blen + 2)); i--)
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

#ifdef SUN_ATTACHMENT
        if (mutt_param_get(&new_body->parameter, "content-lines"))
        {
          int lines = 0;
          if (mutt_str_atoi(
                  mutt_param_get(&new_body->parameter, "content-lines"), &lines) < 0)
            lines = 0;
          for (; lines > 0; lines--)
            if ((ftello(fp) >= end_off) || !fgets(buf, sizeof(buf), fp))
              break;
        }
#endif
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
      }
    }
  }

  /* in case of missing end boundary, set the length to something reasonable */
  if (last && (last->length == 0) && !final)
    last->length = end_off - last->offset;

  /* parse recursive MIME parts */
  for (last = head; last; last = last->next)
    mutt_parse_part(fp, last);

  return head;
}

/**
 * mutt_rfc822_parse_message - parse a Message/RFC822 body
 * @param fp     stream to read from
 * @param parent info about the message/rfc822 body part
 * @retval ptr New Body containing parsed message
 *
 * @note This assumes that 'parent->length' has been set!
 */
struct Body *mutt_rfc822_parse_message(FILE *fp, struct Body *parent)
{
  if (!fp || !parent)
    return NULL;

  parent->email = email_new();
  parent->email->offset = ftello(fp);
  parent->email->env = mutt_rfc822_read_header(fp, parent->email, false, false);
  struct Body *msg = parent->email->content;

  /* ignore the length given in the content-length since it could be wrong
   * and we already have the info to calculate the correct length */
  /* if (msg->length == -1) */
  msg->length = parent->length - (msg->offset - parent->offset);

  /* if body of this message is empty, we can end up with a negative length */
  if (msg->length < 0)
    msg->length = 0;

  mutt_parse_part(fp, msg);
  return msg;
}

/**
 * mutt_parse_mailto - Parse a mailto:// url
 * @param[in]  e    Envelope to fill
 * @param[out] body Body to
 * @param[in]  src  String to parse
 * @retval true  Success
 * @retval false Error
 */
bool mutt_parse_mailto(struct Envelope *e, char **body, const char *src)
{
  if (!e || !src)
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

  mutt_addrlist_parse(&e->to, url->path);

  struct UrlQuery *np;
  STAILQ_FOREACH(np, &url->query_strings, entries)
  {
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
    if (mutt_list_match(tag, &MailToAllow))
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

        mutt_str_asprintf(&scratch, "%s: %s", tag, value);
        scratch[taglen] = 0; /* overwrite the colon as mutt_rfc822_parse_line expects */
        value = mutt_str_skip_email_wsp(&scratch[taglen + 1]);
        mutt_rfc822_parse_line(e, NULL, scratch, value, true, false, true);
        FREE(&scratch);
      }
    }
  }

  /* RFC2047 decode after the RFC822 parsing */
  rfc2047_decode_envelope(e);

  url_free(&url);
  return true;
}
