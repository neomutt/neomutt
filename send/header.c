/**
 * @file
 * Write a MIME Email Header to a file
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page send_header Write a MIME Email Header to a file
 *
 * Write a MIME Email Header to a file
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "gui/lib.h"
#include "header.h"
#include "mutt_globals.h"
#include "options.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/**
 * The next array/enum pair is used to to keep track of user headers that
 * override pre-defined headers NeoMutt would emit. Keep the array sorted and
 * in sync with the enum.
 */
static const char *const userhdrs_override_headers[] = {
  "content-type:",
  "user-agent:",
};

/**
 * enum UserHdrsOverrideIdx - Headers that the user may override
 */
enum UserHdrsOverrideIdx
{
  USERHDRS_OVERRIDE_CONTENT_TYPE, ///< Override the "Content-Type"
  USERHDRS_OVERRIDE_USER_AGENT,   ///< Override the "User-Agent"
};

/**
 * struct UserHdrsOverride - Which headers have been overridden
 */
struct UserHdrsOverride
{
  bool is_overridden[mutt_array_size(userhdrs_override_headers)];
};

/**
 * print_val - Add pieces to an email header, wrapping where necessary
 * @param fp      File to write to
 * @param pfx     Prefix for headers
 * @param value   Text to be added
 * @param chflags Flags, see #CopyHeaderFlags
 * @param col     Column that this text starts at
 * @retval  0 Success
 * @retval -1 Failure
 */
static int print_val(FILE *fp, const char *pfx, const char *value,
                     CopyHeaderFlags chflags, size_t col)
{
  while (value && (value[0] != '\0'))
  {
    if (fputc(*value, fp) == EOF)
      return -1;
    /* corner-case: break words longer than 998 chars by force,
     * mandated by RFC5322 */
    if (!(chflags & CH_DISPLAY) && (++col >= 998))
    {
      if (fputs("\n ", fp) < 0)
        return -1;
      col = 1;
    }
    if (*value == '\n')
    {
      if ((value[1] != '\0') && pfx && (pfx[0] != '\0') && (fputs(pfx, fp) == EOF))
        return -1;
      /* for display, turn folding spaces into folding tabs */
      if ((chflags & CH_DISPLAY) && ((value[1] == ' ') || (value[1] == '\t')))
      {
        value++;
        while ((value[0] != '\0') && ((value[0] == ' ') || (value[0] == '\t')))
          value++;
        if (fputc('\t', fp) == EOF)
          return -1;
        continue;
      }
    }
    value++;
  }
  return 0;
}

/**
 * fold_one_header - Fold one header line
 * @param fp      File to write to
 * @param tag     Header key, e.g. "From"
 * @param value   Header value
 * @param vlen    Length of the header value string
 * @param pfx     Prefix for header
 * @param wraplen Column to wrap at
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int fold_one_header(FILE *fp, const char *tag, const char *value, size_t vlen,
                           const char *pfx, int wraplen, CopyHeaderFlags chflags)
{
  if (!value || (*value == '\0') || !vlen)
    return 0;

  const char *p = value;
  char buf[8192] = { 0 };
  int first = 1, col = 0, l = 0;
  const bool display = (chflags & CH_DISPLAY);

  mutt_debug(LL_DEBUG5, "pfx=[%s], tag=[%s], flags=%d value=[%.*s]\n", pfx, tag,
             chflags, ((value[vlen - 1] == '\n') ? vlen - 1 : vlen), value);

  if (tag && *tag && (fprintf(fp, "%s%s: ", NONULL(pfx), tag) < 0))
    return -1;
  col = mutt_str_len(tag) + ((tag && (tag[0] != '\0')) ? 2 : 0) + mutt_str_len(pfx);

  while (p && (p[0] != '\0'))
  {
    int fold = 0;

    /* find the next word and place it in 'buf'. it may start with
     * whitespace we can fold before */
    const char *next = mutt_str_find_word(p);
    l = MIN(sizeof(buf) - 1, next - p);
    memcpy(buf, p, l);
    buf[l] = '\0';

    /* determine width: character cells for display, bytes for sending
     * (we get pure ascii only) */
    const int w = mutt_mb_width(buf, col, display);
    const int enc = mutt_str_startswith(buf, "=?");

    mutt_debug(LL_DEBUG5, "word=[%s], col=%d, w=%d, next=[0x0%x]\n",
               (buf[0] == '\n' ? "\\n" : buf), col, w, *next);

    /* insert a folding \n before the current word's lwsp except for
     * header name, first word on a line (word longer than wrap width)
     * and encoded words */
    if (!first && !enc && col && ((col + w) >= wraplen))
    {
      col = mutt_str_len(pfx);
      fold = 1;
      if (fprintf(fp, "\n%s", NONULL(pfx)) <= 0)
        return -1;
    }

    /* print the actual word; for display, ignore leading ws for word
     * and fold with tab for readability */
    if (display && fold)
    {
      char *pc = buf;
      while ((pc[0] != '\0') && ((pc[0] == ' ') || (pc[0] == '\t')))
      {
        pc++;
        col--;
      }
      if (fputc('\t', fp) == EOF)
        return -1;
      if (print_val(fp, pfx, pc, chflags, col) < 0)
        return -1;
      col += 8;
    }
    else if (print_val(fp, pfx, buf, chflags, col) < 0)
      return -1;
    col += w;

    /* if the current word ends in \n, ignore all its trailing spaces
     * and reset column; this prevents us from putting only spaces (or
     * even none) on a line if the trailing spaces are located at our
     * current line width
     * XXX this covers ASCII space only, for display we probably
     * want something like iswspace() here */
    const char *sp = next;
    while ((sp[0] != '\0') && ((sp[0] == ' ') || (sp[0] == '\t')))
      sp++;
    if (sp[0] == '\n')
    {
      if (sp[1] == '\0')
        break;
      next = sp;
      col = 0;
    }

    p = next;
    first = 0;
  }

  /* if we have printed something but didn't \n-terminate it, do it
   * except the last word we printed ended in \n already */
  if (col && ((l == 0) || (buf[l - 1] != '\n')))
    if (putc('\n', fp) == EOF)
      return -1;

  return 0;
}

/**
 * unfold_header - Unfold a wrapped email header
 * @param s String to process
 * @retval ptr Unfolded string
 *
 * @note The string is altered in-place
 */
static char *unfold_header(char *s)
{
  char *p = s;
  char *q = s;

  while (p && (p[0] != '\0'))
  {
    /* remove CRLF prior to FWSP, turn \t into ' ' */
    if ((p[0] == '\r') && (p[1] == '\n') && ((p[2] == ' ') || (p[2] == '\t')))
    {
      *q++ = ' ';
      p += 3;
      continue;
    }
    /* remove LF prior to FWSP, turn \t into ' ' */
    else if ((p[0] == '\n') && ((p[1] == ' ') || (p[1] == '\t')))
    {
      *q++ = ' ';
      p += 2;
      continue;
    }
    *q++ = *p++;
  }
  if (q)
    q[0] = '\0';

  return s;
}

/**
 * userhdrs_override_cmp - Compare a user-defined header with an element of the userhdrs_override_headers list
 * @param a Pointer to the string containing the user-defined header
 * @param b Pointer to an element of the userhdrs_override_headers list
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int userhdrs_override_cmp(const void *a, const void *b)
{
  const char *ca = a;
  const char *cb = *(const char **) b;
  return mutt_istrn_cmp(ca, cb, strlen(cb));
}

/**
 * write_one_header - Write out one header line
 * @param fp      File to write to
 * @param pfxw    Width of prefix string
 * @param max     Max width
 * @param wraplen Column to wrap at
 * @param pfx     Prefix for header
 * @param start   Start of header line
 * @param end     End of header line
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int write_one_header(FILE *fp, int pfxw, int max, int wraplen, const char *pfx,
                            const char *start, const char *end, CopyHeaderFlags chflags)
{
  const char *t = strchr(start, ':');
  if (!t || (t > end))
  {
    mutt_debug(LL_DEBUG1, "#2 warning: header not in 'key: value' format!\n");
    return 0;
  }

  const size_t vallen = end - start;
  const bool short_enough = (pfxw + max <= wraplen);

  mutt_debug((short_enough ? LL_DEBUG2 : LL_DEBUG5), "buf[%s%.*s] %s, max width = %d %s %d\n",
             NONULL(pfx), vallen - 1 /* skip newline */, start,
             (short_enough ? "short enough" : "too long"), max,
             (short_enough ? "<=" : ">"), wraplen);

  int rc = 0;
  const char *valbuf = NULL, *tagbuf = NULL;
  const bool is_from = (vallen > 5) && mutt_istr_startswith(start, "from ");

  /* only pass through folding machinery if necessary for sending,
   * never wrap From_ headers on sending */
  if (!(chflags & CH_DISPLAY) && (short_enough || is_from))
  {
    if (pfx && *pfx)
    {
      if (fputs(pfx, fp) == EOF)
      {
        return -1;
      }
    }

    valbuf = mutt_strn_dup(start, end - start);
    rc = print_val(fp, pfx, valbuf, chflags, mutt_str_len(pfx));
  }
  else
  {
    if (!is_from)
    {
      tagbuf = mutt_strn_dup(start, t - start);
      /* skip over the colon separating the header field name and value */
      t++;

      /* skip over any leading whitespace (WSP, as defined in RFC5322)
       * NOTE: mutt_str_skip_email_wsp() does the wrong thing here.
       *       See tickets 3609 and 3716. */
      while ((*t == ' ') || (*t == '\t'))
        t++;
    }
    const char *s = is_from ? start : t;
    valbuf = mutt_strn_dup(s, end - s);
    rc = fold_one_header(fp, tagbuf, valbuf, end - s, pfx, wraplen, chflags);
  }

  FREE(&tagbuf);
  FREE(&valbuf);
  return rc;
}

/**
 * write_userhdrs - Write user-defined headers and keep track of the interesting ones
 * @param fp       FILE pointer where to write the headers
 * @param userhdrs List of headers to write
 * @param privacy  Omit headers that could identify the user
 * @param sub      Config Subset
 * @retval obj UserHdrsOverride struct containing a bitmask of which unique headers were written
 */
static struct UserHdrsOverride write_userhdrs(FILE *fp, const struct ListHead *userhdrs,
                                              bool privacy, struct ConfigSubset *sub)
{
  struct UserHdrsOverride overrides = { { 0 } };

  struct ListNode *tmp = NULL;
  STAILQ_FOREACH(tmp, userhdrs, entries)
  {
    char *const colon = strchr(tmp->data, ':');
    if (!colon)
    {
      continue;
    }

    const char *const value = mutt_str_skip_email_wsp(colon + 1);
    if (*value == '\0')
    {
      continue; /* don't emit empty fields. */
    }

    /* check whether the current user-header is an override */
    size_t curr_override = (size_t) -1;
    const char *const *idx = bsearch(tmp->data, userhdrs_override_headers,
                                     mutt_array_size(userhdrs_override_headers),
                                     sizeof(char *), userhdrs_override_cmp);
    if (idx != NULL)
    {
      curr_override = idx - userhdrs_override_headers;
      overrides.is_overridden[curr_override] = true;
    }

    if (privacy && (curr_override == USERHDRS_OVERRIDE_USER_AGENT))
    {
      continue;
    }

    *colon = '\0';
    mutt_write_one_header(fp, tmp->data, value, NULL, 0, CH_NO_FLAGS, sub);
    *colon = ':';
  }

  return overrides;
}

/**
 * mutt_write_one_header - Write one header line to a file
 * @param fp      File to write to
 * @param tag     Header key, e.g. "From"
 * @param value   Header value
 * @param pfx     Prefix for header
 * @param wraplen Column to wrap at
 * @param chflags Flags, see #CopyHeaderFlags
 * @param sub     Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 *
 * split several headers into individual ones and call write_one_header
 * for each one
 */
int mutt_write_one_header(FILE *fp, const char *tag, const char *value,
                          const char *pfx, int wraplen, CopyHeaderFlags chflags,
                          struct ConfigSubset *sub)
{
  char *last = NULL, *line = NULL;
  int max = 0, w, rc = -1;
  int pfxw = mutt_strwidth(pfx);
  char *v = mutt_str_dup(value);
  bool display = (chflags & CH_DISPLAY);

  const bool c_weed = cs_subset_bool(sub, "weed");
  if (!display || c_weed)
    v = unfold_header(v);

  /* when not displaying, use sane wrap value */
  if (!display)
  {
    const short c_wrap_headers = cs_subset_number(sub, "wrap_headers");
    if ((c_wrap_headers < 78) || (c_wrap_headers > 998))
      wraplen = 78;
    else
      wraplen = c_wrap_headers;
  }
  else if (wraplen <= 0)
    wraplen = 78;

  const size_t vlen = mutt_str_len(v);
  if (tag)
  {
    /* if header is short enough, simply print it */
    if (!display && (mutt_strwidth(tag) + 2 + pfxw + mutt_strnwidth(v, vlen) <= wraplen))
    {
      mutt_debug(LL_DEBUG5, "buf[%s%s: %s] is short enough\n", NONULL(pfx), tag, v);
      if (fprintf(fp, "%s%s: %s\n", NONULL(pfx), tag, v) <= 0)
        goto out;
      rc = 0;
      goto out;
    }
    else
    {
      rc = fold_one_header(fp, tag, v, vlen, pfx, wraplen, chflags);
      goto out;
    }
  }

  char *p = v;
  last = v;
  line = v;
  while (p && *p)
  {
    p = strchr(p, '\n');

    /* find maximum line width in current header */
    if (p)
      *p = '\0';
    w = mutt_mb_width(line, 0, display);
    if (w > max)
      max = w;
    if (p)
      *p = '\n';

    if (!p)
      break;

    line = ++p;
    if ((*p != ' ') && (*p != '\t'))
    {
      if (write_one_header(fp, pfxw, max, wraplen, pfx, last, p, chflags) < 0)
        goto out;
      last = p;
      max = 0;
    }
  }

  if (last && *last)
    if (write_one_header(fp, pfxw, max, wraplen, pfx, last, p, chflags) < 0)
      goto out;

  rc = 0;

out:
  FREE(&v);
  return rc;
}

/**
 * mutt_write_references - Add the message references to a list
 * @param r    String List of references
 * @param fp   File to write to
 * @param trim Trim the list to at most this many items
 * @param sub  Config Subset
 *
 * Write the list in reverse because they are stored in reverse order when
 * parsed to speed up threading.
 */
void mutt_write_references(const struct ListHead *r, FILE *fp, size_t trim,
                           struct ConfigSubset *sub)
{
  struct ListNode *np = NULL;
  size_t length = 0;

  STAILQ_FOREACH(np, r, entries)
  {
    if (++length == trim)
      break;
  }

  struct ListNode **ref = mutt_mem_calloc(length, sizeof(struct ListNode *));

  // store in reverse order
  size_t tmp = length;
  STAILQ_FOREACH(np, r, entries)
  {
    ref[--tmp] = np;
    if (tmp == 0)
      break;
  }

  for (size_t i = 0; i < length; i++)
  {
    fputc(' ', fp);
    fputs(ref[i]->data, fp);
    if (i != length - 1)
      fputc('\n', fp);
  }

  FREE(&ref);
}

/**
 * mutt_rfc822_write_header - Write out one RFC822 header line
 * @param fp      File to write to
 * @param env     Envelope of email
 * @param attach  Attachment
 * @param mode    Mode, see #MuttWriteHeaderMode
 * @param privacy If true, remove headers that might identify the user
 * @param hide_protected_subject If true, replace subject header
 * @param sub     Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 *
 * @note All RFC2047 encoding should be done outside of this routine, except
 *       for the "real name."  This will allow this routine to be used more than
 *       once, if necessary.
 *
 * Likewise, all IDN processing should happen outside of this routine.
 *
 * privacy true => will omit any headers which may identify the user.
 *               Output generated is suitable for being sent through
 *               anonymous remailer chains.
 *
 * hide_protected_subject: replaces the Subject header with
 * $crypt_protected_headers_subject in NORMAL or POSTPONE mode.
 */
int mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach,
                             enum MuttWriteHeaderMode mode, bool privacy,
                             bool hide_protected_subject, struct ConfigSubset *sub)
{
  char buf[1024];

  if (((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_FCC)) && !privacy)
  {
    struct Buffer *date = mutt_buffer_pool_get();
    mutt_date_make_date(date);
    fprintf(fp, "Date: %s\n", mutt_b2s(date));
    mutt_buffer_pool_release(&date);
  }

  /* UseFrom is not consulted here so that we can still write a From:
   * field if the user sets it with the 'my_hdr' command */
  if (!TAILQ_EMPTY(&env->from) && !privacy)
  {
    buf[0] = '\0';
    mutt_addrlist_write(&env->from, buf, sizeof(buf), false);
    fprintf(fp, "From: %s\n", buf);
  }

  if (!TAILQ_EMPTY(&env->sender) && !privacy)
  {
    buf[0] = '\0';
    mutt_addrlist_write(&env->sender, buf, sizeof(buf), false);
    fprintf(fp, "Sender: %s\n", buf);
  }

  if (!TAILQ_EMPTY(&env->to))
  {
    fputs("To: ", fp);
    mutt_addrlist_write_file(&env->to, fp, 4, false);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("To:\n", fp);

  if (!TAILQ_EMPTY(&env->cc))
  {
    fputs("Cc: ", fp);
    mutt_addrlist_write_file(&env->cc, fp, 4, false);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("Cc:\n", fp);

  if (!TAILQ_EMPTY(&env->bcc))
  {
    const bool c_write_bcc = cs_subset_bool(sub, "write_bcc");

    if ((mode == MUTT_WRITE_HEADER_POSTPONE) ||
        (mode == MUTT_WRITE_HEADER_EDITHDRS) || (mode == MUTT_WRITE_HEADER_FCC) ||
        ((mode == MUTT_WRITE_HEADER_NORMAL) && c_write_bcc))
    {
      fputs("Bcc: ", fp);
      mutt_addrlist_write_file(&env->bcc, fp, 5, false);
    }
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
      fputs("Bcc:\n", fp);

#ifdef USE_NNTP
  if (env->newsgroups)
    fprintf(fp, "Newsgroups: %s\n", env->newsgroups);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend)
    fputs("Newsgroups:\n", fp);

  if (env->followup_to)
    fprintf(fp, "Followup-To: %s\n", env->followup_to);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend)
    fputs("Followup-To:\n", fp);

  const bool c_x_comment_to = cs_subset_bool(sub, "x_comment_to");
  if (env->x_comment_to)
    fprintf(fp, "X-Comment-To: %s\n", env->x_comment_to);
  else if ((mode == MUTT_WRITE_HEADER_EDITHDRS) && OptNewsSend && c_x_comment_to)
    fputs("X-Comment-To:\n", fp);
#endif

  if (env->subject)
  {
    if (hide_protected_subject &&
        ((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_FCC) ||
         (mode == MUTT_WRITE_HEADER_POSTPONE)))
    {
      const char *c_crypt_protected_headers_subject =
          cs_subset_string(sub, "crypt_protected_headers_subject");
      mutt_write_one_header(fp, "Subject", c_crypt_protected_headers_subject,
                            NULL, 0, CH_NO_FLAGS, sub);
    }
    else
      mutt_write_one_header(fp, "Subject", env->subject, NULL, 0, CH_NO_FLAGS, sub);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
    fputs("Subject:\n", fp);

  /* save message id if the user has set it */
  if (env->message_id && !privacy)
    fprintf(fp, "Message-ID: %s\n", env->message_id);

  if (!TAILQ_EMPTY(&env->reply_to))
  {
    fputs("Reply-To: ", fp);
    mutt_addrlist_write_file(&env->reply_to, fp, 10, false);
  }
  else if (mode == MUTT_WRITE_HEADER_EDITHDRS)
    fputs("Reply-To:\n", fp);

  if (!TAILQ_EMPTY(&env->mail_followup_to))
  {
#ifdef USE_NNTP
    if (!OptNewsSend)
#endif
    {
      fputs("Mail-Followup-To: ", fp);
      mutt_addrlist_write_file(&env->mail_followup_to, fp, 18, false);
    }
  }

  /* Add any user defined headers */
  struct UserHdrsOverride userhdrs_overrides =
      write_userhdrs(fp, &env->userhdrs, privacy, sub);

  if ((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_FCC) ||
      (mode == MUTT_WRITE_HEADER_POSTPONE) || (mode == MUTT_WRITE_HEADER_MIME))
  {
    if (!STAILQ_EMPTY(&env->references))
    {
      fputs("References:", fp);
      mutt_write_references(&env->references, fp, 10, sub);
      fputc('\n', fp);
    }

    /* Add the MIME headers */
    if (!userhdrs_overrides.is_overridden[USERHDRS_OVERRIDE_CONTENT_TYPE])
    {
      fputs("MIME-Version: 1.0\n", fp);
      mutt_write_mime_header(attach, fp, sub);
    }
  }

  if (!STAILQ_EMPTY(&env->in_reply_to))
  {
    fputs("In-Reply-To:", fp);
    mutt_write_references(&env->in_reply_to, fp, 0, sub);
    fputc('\n', fp);
  }

#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(sub, "autocrypt");
  if (c_autocrypt)
  {
    if (mode == MUTT_WRITE_HEADER_NORMAL || mode == MUTT_WRITE_HEADER_FCC)
      mutt_autocrypt_write_autocrypt_header(env, fp);
    if (mode == MUTT_WRITE_HEADER_MIME)
      mutt_autocrypt_write_gossip_headers(env, fp);
  }
#endif

  const bool c_user_agent = cs_subset_bool(sub, "user_agent");
  if (((mode == MUTT_WRITE_HEADER_NORMAL) || (mode == MUTT_WRITE_HEADER_FCC)) && !privacy &&
      c_user_agent && !userhdrs_overrides.is_overridden[USERHDRS_OVERRIDE_USER_AGENT])
  {
    /* Add a vanity header */
    fprintf(fp, "User-Agent: NeoMutt/%s%s\n", PACKAGE_VERSION, GitVer);
  }

  return (ferror(fp) == 0) ? 0 : -1;
}

/**
 * mutt_write_mime_header - Create a MIME header
 * @param a   Body part
 * @param fp  File to write to
 * @param sub Config Subset
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_write_mime_header(struct Body *a, FILE *fp, struct ConfigSubset *sub)
{
  if (!a || !fp)
    return -1;

  int len;
  int tmplen;
  char buf[256] = { 0 };

  fprintf(fp, "Content-Type: %s/%s", TYPE(a), a->subtype);

  if (!TAILQ_EMPTY(&a->parameter))
  {
    len = 25 + mutt_str_len(a->subtype); /* approximate len. of content-type */

    struct Parameter *np = NULL;
    TAILQ_FOREACH(np, &a->parameter, entries)
    {
      if (!np->attribute || !np->value)
        continue;

      struct ParameterList pl_conts = TAILQ_HEAD_INITIALIZER(pl_conts);
      rfc2231_encode_string(&pl_conts, np->attribute, np->value);
      struct Parameter *cont = NULL;
      TAILQ_FOREACH(cont, &pl_conts, entries)
      {
        fputc(';', fp);

        buf[0] = 0;
        mutt_addr_cat(buf, sizeof(buf), cont->value, MimeSpecials);

        /* Dirty hack to make messages readable by Outlook Express
         * for the Mac: force quotes around the boundary parameter
         * even when they aren't needed.  */
        if (mutt_istr_equal(cont->attribute, "boundary") && mutt_str_equal(buf, cont->value))
          snprintf(buf, sizeof(buf), "\"%s\"", cont->value);

        tmplen = mutt_str_len(buf) + mutt_str_len(cont->attribute) + 1;
        if (len + tmplen + 2 > 76)
        {
          fputs("\n\t", fp);
          len = tmplen + 1;
        }
        else
        {
          fputc(' ', fp);
          len += tmplen + 1;
        }

        fprintf(fp, "%s=%s", cont->attribute, buf);
      }

      mutt_param_free(&pl_conts);
    }
  }

  fputc('\n', fp);

  if (a->language)
    fprintf(fp, "Content-Language: %s\n", a->language);

  if (a->description)
    fprintf(fp, "Content-Description: %s\n", a->description);

  if (a->disposition != DISP_NONE)
  {
    const char *dispstr[] = { "inline", "attachment", "form-data" };

    if (a->disposition < sizeof(dispstr) / sizeof(char *))
    {
      fprintf(fp, "Content-Disposition: %s", dispstr[a->disposition]);
      len = 21 + mutt_str_len(dispstr[a->disposition]);

      if (a->use_disp && (a->disposition != DISP_INLINE))
      {
        char *fn = a->d_filename;
        if (!fn)
          fn = a->filename;

        if (fn)
        {
          /* Strip off the leading path... */
          char *t = strrchr(fn, '/');
          if (t)
            t++;
          else
            t = fn;

          struct ParameterList pl_conts = TAILQ_HEAD_INITIALIZER(pl_conts);
          rfc2231_encode_string(&pl_conts, "filename", t);
          struct Parameter *cont = NULL;
          TAILQ_FOREACH(cont, &pl_conts, entries)
          {
            fputc(';', fp);
            buf[0] = 0;
            mutt_addr_cat(buf, sizeof(buf), cont->value, MimeSpecials);

            tmplen = mutt_str_len(buf) + mutt_str_len(cont->attribute) + 1;
            if (len + tmplen + 2 > 76)
            {
              fputs("\n\t", fp);
              len = tmplen + 1;
            }
            else
            {
              fputc(' ', fp);
              len += tmplen + 1;
            }

            fprintf(fp, "%s=%s", cont->attribute, buf);
          }

          mutt_param_free(&pl_conts);
        }
      }

      fputc('\n', fp);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n", a->disposition);
    }
  }

  if (a->encoding != ENC_7BIT)
    fprintf(fp, "Content-Transfer-Encoding: %s\n", ENCODING(a->encoding));

  const bool c_crypt_protected_headers_write =
      cs_subset_bool(sub, "crypt_protected_headers_write");
  bool autocrypt = false;
#ifdef USE_AUTOCRYPT
  autocrypt = cs_subset_bool(sub, "autocrypt");
#endif

  if ((c_crypt_protected_headers_write || autocrypt) && a->mime_headers)
  {
    mutt_rfc822_write_header(fp, a->mime_headers, NULL, MUTT_WRITE_HEADER_MIME,
                             false, false, sub);
  }

  /* Do NOT add the terminator here!!! */
  return ferror(fp) ? -1 : 0;
}
