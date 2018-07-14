/**
 * @file
 * Parse and identify different URL schemes
 *
 * @authors
 * Copyright (C) 2000-2002,2004 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "email/email.h"
#include "mutt.h"
#include "globals.h"
#include "parse.h"

int url_parse_mailto(struct Envelope *e, char **body, const char *src)
{
  char *p = NULL;
  char *tag = NULL, *value = NULL;

  int rc = -1;

  char *t = strchr(src, ':');
  if (!t)
    return -1;

  /* copy string for safe use of strtok() */
  char *tmp = mutt_str_strdup(t + 1);
  if (!tmp)
    return -1;

  char *headers = strchr(tmp, '?');
  if (headers)
    *headers++ = '\0';

  if (url_pct_decode(tmp) < 0)
    goto out;

  e->to = mutt_addr_parse_list(e->to, tmp);

  tag = headers ? strtok_r(headers, "&", &p) : NULL;

  for (; tag; tag = strtok_r(NULL, "&", &p))
  {
    value = strchr(tag, '=');
    if (value)
      *value++ = '\0';
    if (!value || !*value)
      continue;

    if (url_pct_decode(tag) < 0)
      goto out;
    if (url_pct_decode(value) < 0)
      goto out;

    /* Determine if this header field is on the allowed list.  Since NeoMutt
     * interprets some header fields specially (such as
     * "Attach: ~/.gnupg/secring.gpg"), care must be taken to ensure that
     * only safe fields are allowed.
     *
     * RFC2368, "4. Unsafe headers"
     * The user agent interpreting a mailto URL SHOULD choose not to create
     * a message if any of the headers are considered dangerous; it may also
     * choose to create a message with only a subset of the headers given in
     * the URL.
     */
    if (mutt_list_match(tag, &MailToAllow))
    {
      if (mutt_str_strcasecmp(tag, "body") == 0)
      {
        if (body)
          mutt_str_replace(body, value);
      }
      else
      {
        char *scratch = NULL;
        size_t taglen = mutt_str_strlen(tag);

        safe_asprintf(&scratch, "%s: %s", tag, value);
        scratch[taglen] = 0; /* overwrite the colon as mutt_rfc822_parse_line expects */
        value = mutt_str_skip_email_wsp(&scratch[taglen + 1]);
        mutt_rfc822_parse_line(e, NULL, scratch, value, 1, 0, 1);
        FREE(&scratch);
      }
    }
  }

  /* RFC2047 decode after the RFC822 parsing */
  rfc2047_decode_addrlist(e->from);
  rfc2047_decode_addrlist(e->to);
  rfc2047_decode_addrlist(e->cc);
  rfc2047_decode_addrlist(e->bcc);
  rfc2047_decode_addrlist(e->reply_to);
  rfc2047_decode_addrlist(e->mail_followup_to);
  rfc2047_decode_addrlist(e->return_path);
  rfc2047_decode_addrlist(e->sender);
  rfc2047_decode(&e->x_label);
  rfc2047_decode(&e->subject);

  rc = 0;

out:
  FREE(&tmp);
  return rc;
}
