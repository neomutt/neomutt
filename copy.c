/**
 * @file
 * Duplicate the structure of an entire email
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2014 Michael R. Elkins <me@mutt.org>
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
 * @page copy Duplicate the structure of an entire email
 *
 * Duplicate the structure of an entire email
 */

#include "config.h"
#include <ctype.h>
#include <inttypes.h> // IWYU pragma: keep
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "copy.h"
#include "context.h"
#include "handler.h"
#include "hdrline.h"
#include "mutt_globals.h"
#include "mx.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef USE_NOTMUCH
#include "muttlib.h"
#include "notmuch/lib.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

static int address_header_decode(char **h);
static int copy_delete_attach(struct Body *b, FILE *fp_in, FILE *fp_out,
                              const char *quoted_date);

/**
 * mutt_copy_hdr - Copy header from one file to another
 * @param fp_in     FILE pointer to read from
 * @param fp_out    FILE pointer to write to
 * @param off_start Offset to start from
 * @param off_end   Offset to finish at
 * @param chflags   Flags, see #CopyHeaderFlags
 * @param prefix    Prefix for quoting headers
 * @param wraplen   Width to wrap at (when chflags & CH_DISPLAY)
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Ok, the only reason for not merging this with mutt_copy_header() below is to
 * avoid creating a Email structure in message_handler().  Also, this one will
 * wrap headers much more aggressively than the other one.
 */
int mutt_copy_hdr(FILE *fp_in, FILE *fp_out, LOFF_T off_start, LOFF_T off_end,
                  CopyHeaderFlags chflags, const char *prefix, int wraplen)
{
  bool from = false;
  bool this_is_from = false;
  bool ignore = false;
  char buf[1024]; /* should be long enough to get most fields in one pass */
  char *nl = NULL;
  char **headers = NULL;
  int hdr_count;
  int x;
  char *this_one = NULL;
  size_t this_one_len = 0;
  int error;

  if (off_start < 0)
    return -1;

  if (ftello(fp_in) != off_start)
    if (fseeko(fp_in, off_start, SEEK_SET) < 0)
      return -1;

  buf[0] = '\n';
  buf[1] = '\0';

  if ((chflags & (CH_REORDER | CH_WEED | CH_MIME | CH_DECODE | CH_PREFIX | CH_WEED_DELIVERED)) == 0)
  {
    /* Without these flags to complicate things
     * we can do a more efficient line to line copying */
    while (ftello(fp_in) < off_end)
    {
      nl = strchr(buf, '\n');

      if (!fgets(buf, sizeof(buf), fp_in))
        break;

      /* Is it the beginning of a header? */
      if (nl && (buf[0] != ' ') && (buf[0] != '\t'))
      {
        ignore = true;
        if (!from && mutt_str_startswith(buf, "From "))
        {
          if ((chflags & CH_FROM) == 0)
            continue;
          from = true;
        }
        else if ((chflags & CH_NOQFROM) && mutt_istr_startswith(buf, ">From "))
          continue;
        else if ((buf[0] == '\n') || ((buf[0] == '\r') && (buf[1] == '\n')))
          break; /* end of header */

        if ((chflags & (CH_UPDATE | CH_XMIT | CH_NOSTATUS)) &&
            (mutt_istr_startswith(buf, "Status:") || mutt_istr_startswith(buf, "X-Status:")))
        {
          continue;
        }
        if ((chflags & (CH_UPDATE_LEN | CH_XMIT | CH_NOLEN)) &&
            (mutt_istr_startswith(buf, "Content-Length:") ||
             mutt_istr_startswith(buf, "Lines:")))
        {
          continue;
        }
        if ((chflags & CH_UPDATE_REFS) &&
            mutt_istr_startswith(buf, "References:"))
          continue;
        if ((chflags & CH_UPDATE_IRT) &&
            mutt_istr_startswith(buf, "In-Reply-To:"))
          continue;
        if (chflags & CH_UPDATE_LABEL && mutt_istr_startswith(buf, "X-Label:"))
          continue;
        if ((chflags & CH_UPDATE_SUBJECT) &&
            mutt_istr_startswith(buf, "Subject:"))
          continue;

        ignore = false;
      }

      if (!ignore && (fputs(buf, fp_out) == EOF))
        return -1;
    }
    return 0;
  }

  hdr_count = 1;
  x = 0;
  error = false;

  /* We are going to read and collect the headers in an array
   * so we are able to do re-ordering.
   * First count the number of entries in the array */
  if (chflags & CH_REORDER)
  {
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &HeaderOrderList, entries)
    {
      mutt_debug(LL_DEBUG3, "Reorder list: %s\n", np->data);
      hdr_count++;
    }
  }

  mutt_debug(LL_DEBUG1, "WEED is %sset\n", (chflags & CH_WEED) ? "" : "not ");

  headers = mutt_mem_calloc(hdr_count, sizeof(char *));

  /* Read all the headers into the array */
  while (ftello(fp_in) < off_end)
  {
    nl = strchr(buf, '\n');

    /* Read a line */
    if (!fgets(buf, sizeof(buf), fp_in))
      break;

    /* Is it the beginning of a header? */
    if (nl && (buf[0] != ' ') && (buf[0] != '\t'))
    {
      /* Do we have anything pending? */
      if (this_one)
      {
        if (chflags & CH_DECODE)
        {
          if (address_header_decode(&this_one) == 0)
            rfc2047_decode(&this_one);
          this_one_len = mutt_str_len(this_one);

          /* Convert CRLF line endings to LF */
          if ((this_one_len > 2) && (this_one[this_one_len - 2] == '\r') &&
              (this_one[this_one_len - 1] == '\n'))
          {
            this_one[this_one_len - 2] = '\n';
            this_one[this_one_len - 1] = '\0';
          }
        }

        if (!headers[x])
          headers[x] = this_one;
        else
        {
          int hlen = mutt_str_len(headers[x]);

          mutt_mem_realloc(&headers[x], hlen + this_one_len + sizeof(char));
          strcat(headers[x] + hlen, this_one);
          FREE(&this_one);
        }

        this_one = NULL;
      }

      ignore = true;
      this_is_from = false;
      if (!from && mutt_str_startswith(buf, "From "))
      {
        if ((chflags & CH_FROM) == 0)
          continue;
        this_is_from = true;
        from = true;
      }
      else if ((buf[0] == '\n') || ((buf[0] == '\r') && (buf[1] == '\n')))
        break; /* end of header */

      /* note: CH_FROM takes precedence over header weeding. */
      if (!((chflags & CH_FROM) && (chflags & CH_FORCE_FROM) && this_is_from) &&
          (chflags & CH_WEED) && mutt_matches_ignore(buf))
      {
        continue;
      }
      if ((chflags & CH_WEED_DELIVERED) &&
          mutt_istr_startswith(buf, "Delivered-To:"))
      {
        continue;
      }
      if ((chflags & (CH_UPDATE | CH_XMIT | CH_NOSTATUS)) &&
          (mutt_istr_startswith(buf, "Status:") ||
           mutt_istr_startswith(buf, "X-Status:")))
      {
        continue;
      }
      if ((chflags & (CH_UPDATE_LEN | CH_XMIT | CH_NOLEN)) &&
          (mutt_istr_startswith(buf, "Content-Length:") || mutt_istr_startswith(buf, "Lines:")))
      {
        continue;
      }
      if ((chflags & CH_MIME))
      {
        if (mutt_istr_startswith(buf, "mime-version:"))
        {
          continue;
        }
        size_t plen = mutt_istr_startswith(buf, "content-");
        if ((plen != 0) &&
            (mutt_istr_startswith(buf + plen, "transfer-encoding:") ||
             mutt_istr_startswith(buf + plen, "type:")))
        {
          continue;
        }
      }
      if ((chflags & CH_UPDATE_REFS) &&
          mutt_istr_startswith(buf, "References:"))
        continue;
      if ((chflags & CH_UPDATE_IRT) &&
          mutt_istr_startswith(buf, "In-Reply-To:"))
        continue;
      if ((chflags & CH_UPDATE_LABEL) && mutt_istr_startswith(buf, "X-Label:"))
        continue;
      if ((chflags & CH_UPDATE_SUBJECT) &&
          mutt_istr_startswith(buf, "Subject:"))
        continue;

      /* Find x -- the array entry where this header is to be saved */
      if (chflags & CH_REORDER)
      {
        struct ListNode *np = NULL;
        x = 0;
        STAILQ_FOREACH(np, &HeaderOrderList, entries)
        {
          ++x;
          if (mutt_istr_startswith(buf, np->data))
          {
            mutt_debug(LL_DEBUG2, "Reorder: %s matches %s", np->data, buf);
            break;
          }
        }
      }

      ignore = false;
    } /* If beginning of header */

    if (!ignore)
    {
      mutt_debug(LL_DEBUG2, "Reorder: x = %d; hdr_count = %d\n", x, hdr_count);
      if (this_one)
      {
        size_t blen = mutt_str_len(buf);

        mutt_mem_realloc(&this_one, this_one_len + blen + sizeof(char));
        strcat(this_one + this_one_len, buf);
        this_one_len += blen;
      }
      else
      {
        this_one = mutt_str_dup(buf);
        this_one_len = mutt_str_len(this_one);
      }
    }
  } /* while (ftello (fp_in) < off_end) */

  /* Do we have anything pending?  -- XXX, same code as in above in the loop. */
  if (this_one)
  {
    if (chflags & CH_DECODE)
    {
      if (address_header_decode(&this_one) == 0)
        rfc2047_decode(&this_one);
      this_one_len = mutt_str_len(this_one);
    }

    if (!headers[x])
      headers[x] = this_one;
    else
    {
      int hlen = mutt_str_len(headers[x]);

      mutt_mem_realloc(&headers[x], hlen + this_one_len + sizeof(char));
      strcat(headers[x] + hlen, this_one);
      FREE(&this_one);
    }

    this_one = NULL;
  }

  /* Now output the headers in order */
  for (x = 0; x < hdr_count; x++)
  {
    if (headers[x])
    {
      /* We couldn't do the prefixing when reading because RFC2047
       * decoding may have concatenated lines.  */
      if (chflags & (CH_DECODE | CH_PREFIX))
      {
        const char *pre = (chflags & CH_PREFIX) ? prefix : NULL;
        wraplen = mutt_window_wrap_cols(wraplen, C_Wrap);

        if (mutt_write_one_header(fp_out, 0, headers[x], pre, wraplen, chflags,
                                  NeoMutt->sub) == -1)
        {
          error = true;
          break;
        }
      }
      else
      {
        if (fputs(headers[x], fp_out) == EOF)
        {
          error = true;
          break;
        }
      }
    }
  }

  /* Free in a separate loop to be sure that all headers are freed
   * in case of error. */
  for (x = 0; x < hdr_count; x++)
    FREE(&headers[x]);
  FREE(&headers);

  if (error)
    return -1;
  return 0;
}

/**
 * mutt_copy_header - Copy Email header
 * @param fp_in    FILE pointer to read from
 * @param e        Email
 * @param fp_out   FILE pointer to write to
 * @param chflags  See #CopyHeaderFlags
 * @param prefix   Prefix for quoting headers (if #CH_PREFIX is set)
 * @param wraplen  Width to wrap at (when chflags & CH_DISPLAY)
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_copy_header(FILE *fp_in, struct Email *e, FILE *fp_out,
                     CopyHeaderFlags chflags, const char *prefix, int wraplen)
{
  char *temp_hdr = NULL;

  if (e->env)
  {
    chflags |= ((e->env->changed & MUTT_ENV_CHANGED_IRT) ? CH_UPDATE_IRT : 0) |
               ((e->env->changed & MUTT_ENV_CHANGED_REFS) ? CH_UPDATE_REFS : 0) |
               ((e->env->changed & MUTT_ENV_CHANGED_XLABEL) ? CH_UPDATE_LABEL : 0) |
               ((e->env->changed & MUTT_ENV_CHANGED_SUBJECT) ? CH_UPDATE_SUBJECT : 0);
  }

  if (mutt_copy_hdr(fp_in, fp_out, e->offset, e->content->offset, chflags,
                    prefix, wraplen) == -1)
    return -1;

  if (chflags & CH_TXTPLAIN)
  {
    char chsbuf[128];
    char buf[128];
    fputs("MIME-Version: 1.0\n", fp_out);
    fputs("Content-Transfer-Encoding: 8bit\n", fp_out);
    fputs("Content-Type: text/plain; charset=", fp_out);
    mutt_ch_canonical_charset(chsbuf, sizeof(chsbuf), C_Charset ? C_Charset : "us-ascii");
    mutt_addr_cat(buf, sizeof(buf), chsbuf, MimeSpecials);
    fputs(buf, fp_out);
    fputc('\n', fp_out);
  }

  if ((chflags & CH_UPDATE_IRT) && !STAILQ_EMPTY(&e->env->in_reply_to))
  {
    fputs("In-Reply-To:", fp_out);
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &e->env->in_reply_to, entries)
    {
      fputc(' ', fp_out);
      fputs(np->data, fp_out);
    }
    fputc('\n', fp_out);
  }

  if ((chflags & CH_UPDATE_REFS) && !STAILQ_EMPTY(&e->env->references))
  {
    fputs("References:", fp_out);
    mutt_write_references(&e->env->references, fp_out, 0, NeoMutt->sub);
    fputc('\n', fp_out);
  }

  if ((chflags & CH_UPDATE) && ((chflags & CH_NOSTATUS) == 0))
  {
    if (e->old || e->read)
    {
      fputs("Status: ", fp_out);
      if (e->read)
        fputs("RO", fp_out);
      else if (e->old)
        fputc('O', fp_out);
      fputc('\n', fp_out);
    }

    if (e->flagged || e->replied)
    {
      fputs("X-Status: ", fp_out);
      if (e->replied)
        fputc('A', fp_out);
      if (e->flagged)
        fputc('F', fp_out);
      fputc('\n', fp_out);
    }
  }

  if (chflags & CH_UPDATE_LEN && ((chflags & CH_NOLEN) == 0))
  {
    fprintf(fp_out, "Content-Length: " OFF_T_FMT "\n", e->content->length);
    if ((e->lines != 0) || (e->content->length == 0))
      fprintf(fp_out, "Lines: %d\n", e->lines);
  }

#ifdef USE_NOTMUCH
  if (chflags & CH_VIRTUAL)
  {
    /* Add some fake headers based on notmuch data */
    char *folder = nm_email_get_folder(e);
    if (folder && !(C_Weed && mutt_matches_ignore("folder")))
    {
      char buf[1024];
      mutt_str_copy(buf, folder, sizeof(buf));
      mutt_pretty_mailbox(buf, sizeof(buf));

      fputs("Folder: ", fp_out);
      fputs(buf, fp_out);
      fputc('\n', fp_out);
    }
  }
#endif
  char *tags = driver_tags_get(&e->tags);
  if (tags && !(C_Weed && mutt_matches_ignore("tags")))
  {
    fputs("Tags: ", fp_out);
    fputs(tags, fp_out);
    fputc('\n', fp_out);
  }
  FREE(&tags);

  if ((chflags & CH_UPDATE_LABEL) && e->env->x_label)
  {
    temp_hdr = e->env->x_label;
    /* env->x_label isn't currently stored with direct references elsewhere.
     * Context->label_hash strdups the keys.  But to be safe, encode a copy */
    if (!(chflags & CH_DECODE))
    {
      temp_hdr = mutt_str_dup(temp_hdr);
      rfc2047_encode(&temp_hdr, NULL, sizeof("X-Label:"), C_SendCharset);
    }
    if (mutt_write_one_header(
            fp_out, "X-Label", temp_hdr, (chflags & CH_PREFIX) ? prefix : 0,
            mutt_window_wrap_cols(wraplen, C_Wrap), chflags, NeoMutt->sub) == -1)
    {
      return -1;
    }
    if (!(chflags & CH_DECODE))
      FREE(&temp_hdr);
  }

  if ((chflags & CH_UPDATE_SUBJECT) && e->env->subject)
  {
    temp_hdr = e->env->subject;
    /* env->subject is directly referenced in Context->subj_hash, so we
     * have to be careful not to encode (and thus free) that memory. */
    if (!(chflags & CH_DECODE))
    {
      temp_hdr = mutt_str_dup(temp_hdr);
      rfc2047_encode(&temp_hdr, NULL, sizeof("Subject:"), C_SendCharset);
    }
    if (mutt_write_one_header(
            fp_out, "Subject", temp_hdr, (chflags & CH_PREFIX) ? prefix : 0,
            mutt_window_wrap_cols(wraplen, C_Wrap), chflags, NeoMutt->sub) == -1)
    {
      return -1;
    }
    if (!(chflags & CH_DECODE))
      FREE(&temp_hdr);
  }

  if ((chflags & CH_NONEWLINE) == 0)
  {
    if (chflags & CH_PREFIX)
      fputs(prefix, fp_out);
    fputc('\n', fp_out); /* add header terminator */
  }

  if (ferror(fp_out) || feof(fp_out))
    return -1;

  return 0;
}

/**
 * count_delete_lines - Count lines to be deleted in this email body
 * @param fp      FILE pointer to read from
 * @param b       Email Body
 * @param length  Number of bytes to be deleted
 * @param datelen Length of the date
 * @retval num Number of lines to be deleted
 *
 * Count the number of lines and bytes to be deleted in this body
 */
static int count_delete_lines(FILE *fp, struct Body *b, LOFF_T *length, size_t datelen)
{
  int dellines = 0;

  if (b->deleted)
  {
    fseeko(fp, b->offset, SEEK_SET);
    for (long l = b->length; l; l--)
    {
      const int ch = getc(fp);
      if (ch == EOF)
        break;
      if (ch == '\n')
        dellines++;
    }
    /* 3 and 89 come from the added header of three lines in
     * copy_delete_attach().  89 is the size of the header(including
     * the newlines, tabs, and a single digit length), not including
     * the date length. */
    dellines -= 3;
    *length -= b->length - (89 + datelen);
    /* Count the number of digits exceeding the first one to write the size */
    for (long l = 10; b->length >= l; l *= 10)
      (*length)++;
  }
  else
  {
    for (b = b->parts; b; b = b->next)
      dellines += count_delete_lines(fp, b, length, datelen);
  }
  return dellines;
}

/**
 * mutt_copy_message_fp - make a copy of a message from a FILE pointer
 * @param fp_out  Where to write output
 * @param fp_in   Where to get input
 * @param e       Email being copied
 * @param cmflags Flags, see #CopyMessageFlags
 * @param chflags Flags, see #CopyHeaderFlags
 * @param wraplen Width to wrap at (when chflags & CH_DISPLAY)
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_copy_message_fp(FILE *fp_out, FILE *fp_in, struct Email *e,
                         CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen)
{
  struct Body *body = e->content;
  char prefix[128];
  LOFF_T new_offset = -1;
  int rc = 0;

  if (cmflags & MUTT_CM_PREFIX)
  {
    if (C_TextFlowed)
      mutt_str_copy(prefix, ">", sizeof(prefix));
    else
    {
      mutt_make_string(prefix, sizeof(prefix), wraplen, NONULL(C_IndentString),
                       Context, Context->mailbox, e);
    }
  }

  if ((cmflags & MUTT_CM_NOHEADER) == 0)
  {
    if (cmflags & MUTT_CM_PREFIX)
      chflags |= CH_PREFIX;
    else if (e->attach_del && (chflags & CH_UPDATE_LEN))
    {
      int new_lines;
      int rc_attach_del = -1;
      LOFF_T new_length = body->length;
      struct Buffer *quoted_date = NULL;

      quoted_date = mutt_buffer_pool_get();
      mutt_buffer_addch(quoted_date, '"');
      mutt_date_make_date(quoted_date);
      mutt_buffer_addch(quoted_date, '"');

      /* Count the number of lines and bytes to be deleted */
      fseeko(fp_in, body->offset, SEEK_SET);
      new_lines = e->lines - count_delete_lines(fp_in, body, &new_length,
                                                mutt_buffer_len(quoted_date));

      /* Copy the headers */
      if (mutt_copy_header(fp_in, e, fp_out, chflags | CH_NOLEN | CH_NONEWLINE, NULL, wraplen))
        goto attach_del_cleanup;
      fprintf(fp_out, "Content-Length: " OFF_T_FMT "\n", new_length);
      if (new_lines <= 0)
        new_lines = 0;
      else
        fprintf(fp_out, "Lines: %d\n", new_lines);

      putc('\n', fp_out);
      if (ferror(fp_out) || feof(fp_out))
        goto attach_del_cleanup;
      new_offset = ftello(fp_out);

      /* Copy the body */
      if (fseeko(fp_in, body->offset, SEEK_SET) < 0)
        goto attach_del_cleanup;
      if (copy_delete_attach(body, fp_in, fp_out, mutt_b2s(quoted_date)))
        goto attach_del_cleanup;

      mutt_buffer_pool_release(&quoted_date);

      LOFF_T fail = ((ftello(fp_out) - new_offset) - new_length);
      if (fail)
      {
        mutt_error(ngettext("The length calculation was wrong by %ld byte",
                            "The length calculation was wrong by %ld bytes", fail),
                   fail);
        new_length += fail;
      }

      /* Update original message if we are sync'ing a mailfolder */
      if (cmflags & MUTT_CM_UPDATE)
      {
        e->attach_del = false;
        e->lines = new_lines;
        body->offset = new_offset;

        /* update the total size of the mailbox to reflect this deletion */
        Context->mailbox->size -= body->length - new_length;
        /* if the message is visible, update the visible size of the mailbox as well.  */
        if (Context->mailbox->v2r[e->msgno] != -1)
          Context->vsize -= body->length - new_length;

        body->length = new_length;
        mutt_body_free(&body->parts);
      }

      rc_attach_del = 0;

    attach_del_cleanup:
      mutt_buffer_pool_release(&quoted_date);
      return rc_attach_del;
    }

    if (mutt_copy_header(fp_in, e, fp_out, chflags,
                         (chflags & CH_PREFIX) ? prefix : NULL, wraplen) == -1)
    {
      return -1;
    }

    new_offset = ftello(fp_out);
  }

  if (cmflags & MUTT_CM_DECODE)
  {
    /* now make a text/plain version of the message */
    struct State s = { 0 };
    s.fp_in = fp_in;
    s.fp_out = fp_out;
    if (cmflags & MUTT_CM_PREFIX)
      s.prefix = prefix;
    if (cmflags & MUTT_CM_DISPLAY)
    {
      s.flags |= MUTT_DISPLAY;
      s.wraplen = wraplen;
    }
    if (cmflags & MUTT_CM_PRINTING)
      s.flags |= MUTT_PRINTING;
    if (cmflags & MUTT_CM_WEED)
      s.flags |= MUTT_WEED;
    if (cmflags & MUTT_CM_CHARCONV)
      s.flags |= MUTT_CHARCONV;
    if (cmflags & MUTT_CM_REPLYING)
      s.flags |= MUTT_REPLYING;

    if ((WithCrypto != 0) && cmflags & MUTT_CM_VERIFY)
      s.flags |= MUTT_VERIFY;

    rc = mutt_body_handler(body, &s);
  }
  else if ((WithCrypto != 0) && (cmflags & MUTT_CM_DECODE_CRYPT) && (e->security & SEC_ENCRYPT))
  {
    struct Body *cur = NULL;
    FILE *fp = NULL;

    if (((WithCrypto & APPLICATION_PGP) != 0) && (cmflags & MUTT_CM_DECODE_PGP) &&
        (e->security & APPLICATION_PGP) && (e->content->type == TYPE_MULTIPART))
    {
      if (crypt_pgp_decrypt_mime(fp_in, &fp, e->content, &cur))
        return -1;
      fputs("MIME-Version: 1.0\n", fp_out);
    }

    if (((WithCrypto & APPLICATION_SMIME) != 0) && (cmflags & MUTT_CM_DECODE_SMIME) &&
        (e->security & APPLICATION_SMIME) && (e->content->type == TYPE_APPLICATION))
    {
      if (crypt_smime_decrypt_mime(fp_in, &fp, e->content, &cur))
        return -1;
    }

    if (!cur)
    {
      mutt_error(_("No decryption engine available for message"));
      return -1;
    }

    mutt_write_mime_header(cur, fp_out, NeoMutt->sub);
    fputc('\n', fp_out);

    if (fseeko(fp, cur->offset, SEEK_SET) < 0)
      return -1;
    if (mutt_file_copy_bytes(fp, fp_out, cur->length) == -1)
    {
      mutt_file_fclose(&fp);
      mutt_body_free(&cur);
      return -1;
    }
    mutt_body_free(&cur);
    mutt_file_fclose(&fp);
  }
  else
  {
    if (fseeko(fp_in, body->offset, SEEK_SET) < 0)
      return -1;
    if (cmflags & MUTT_CM_PREFIX)
    {
      int c;
      size_t bytes = body->length;

      fputs(prefix, fp_out);

      while (((c = fgetc(fp_in)) != EOF) && bytes--)
      {
        fputc(c, fp_out);
        if (c == '\n')
        {
          fputs(prefix, fp_out);
        }
      }
    }
    else if (mutt_file_copy_bytes(fp_in, fp_out, body->length) == -1)
      return -1;
  }

  if ((cmflags & MUTT_CM_UPDATE) && ((cmflags & MUTT_CM_NOHEADER) == 0) &&
      (new_offset != -1))
  {
    body->offset = new_offset;
    mutt_body_free(&body->parts);
  }

  return rc;
}

/**
 * mutt_copy_message - Copy a message from a Mailbox
 * @param fp_out  FILE pointer to write to
 * @param m       Source mailbox
 * @param e       Email
 * @param cmflags Flags, see #CopyMessageFlags
 * @param chflags Flags, see #CopyHeaderFlags
 * @param wraplen Width to wrap at (when chflags & CH_DISPLAY)
 * @retval  0 Success
 * @retval -1 Failure
 *
 * should be made to return -1 on fatal errors, and 1 on non-fatal errors
 * like partial decode, where it is worth displaying as much as possible
 */
int mutt_copy_message(FILE *fp_out, struct Mailbox *m, struct Email *e,
                      CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen)
{
  struct Message *msg = mx_msg_open(m, e->msgno);
  if (!msg)
    return -1;
  if (!e->content)
    return -1;
  int rc = mutt_copy_message_fp(fp_out, msg->fp, e, cmflags, chflags, wraplen);
  if ((rc == 0) && (ferror(fp_out) || feof(fp_out)))
  {
    mutt_debug(LL_DEBUG1, "failed to detect EOF!\n");
    rc = -1;
  }
  mx_msg_close(m, &msg);
  return rc;
}

/**
 * append_message - appends a copy of the given message to a mailbox
 * @param dest    destination mailbox
 * @param fp_in    where to get input
 * @param src     source mailbox
 * @param e       Email being copied
 * @param cmflags Flags, see #CopyMessageFlags
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Error
 */
static int append_message(struct Mailbox *dest, FILE *fp_in, struct Mailbox *src,
                          struct Email *e, CopyMessageFlags cmflags, CopyHeaderFlags chflags)
{
  char buf[256];
  struct Message *msg = NULL;
  int rc;

  if (fseeko(fp_in, e->offset, SEEK_SET) < 0)
    return -1;
  if (!fgets(buf, sizeof(buf), fp_in))
    return -1;

  msg = mx_msg_open_new(dest, e, is_from(buf, NULL, 0, NULL) ? MUTT_MSG_NO_FLAGS : MUTT_ADD_FROM);
  if (!msg)
    return -1;
  if ((dest->type == MUTT_MBOX) || (dest->type == MUTT_MMDF))
    chflags |= CH_FROM | CH_FORCE_FROM;
  chflags |= ((dest->type == MUTT_MAILDIR) ? CH_NOSTATUS : CH_UPDATE);
  rc = mutt_copy_message_fp(msg->fp, fp_in, e, cmflags, chflags, 0);
  if (mx_msg_commit(dest, msg) != 0)
    rc = -1;

#ifdef USE_NOTMUCH
  if (msg->committed_path && (dest->type == MUTT_MAILDIR) && (src->type == MUTT_NOTMUCH))
    nm_update_filename(src, NULL, msg->committed_path, e);
#endif

  mx_msg_close(dest, &msg);
  return rc;
}

/**
 * mutt_append_message - Append a message
 * @param dest    Destination Mailbox
 * @param src     Source Mailbox
 * @param e       Email
 * @param cmflags Flags, see #CopyMessageFlags
 * @param chflags Flags, see #CopyHeaderFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_append_message(struct Mailbox *dest, struct Mailbox *src, struct Email *e,
                        CopyMessageFlags cmflags, CopyHeaderFlags chflags)
{
  struct Message *msg = mx_msg_open(src, e->msgno);
  if (!msg)
    return -1;
  int rc = append_message(dest, msg->fp, src, e, cmflags, chflags);
  mx_msg_close(src, &msg);
  return rc;
}

/**
 * copy_delete_attach - Copy a message, deleting marked attachments
 * @param b           Email Body
 * @param fp_in       FILE pointer to read from
 * @param fp_out      FILE pointer to write to
 * @param quoted_date Date stamp
 * @retval  0 Success
 * @retval -1 Failure
 *
 * This function copies a message body, while deleting _in_the_copy_
 * any attachments which are marked for deletion.
 * Nothing is changed in the original message -- this is left to the caller.
 */
static int copy_delete_attach(struct Body *b, FILE *fp_in, FILE *fp_out, const char *quoted_date)
{
  struct Body *part = NULL;

  for (part = b->parts; part; part = part->next)
  {
    if (part->deleted || part->parts)
    {
      /* Copy till start of this part */
      if (mutt_file_copy_bytes(fp_in, fp_out, part->hdr_offset - ftello(fp_in)))
        return -1;

      if (part->deleted)
      {
        /* If this is modified, count_delete_lines() needs to be changed too */
        fprintf(
            fp_out,
            "Content-Type: message/external-body; access-type=x-mutt-deleted;\n"
            "\texpiration=%s; length=" OFF_T_FMT "\n"
            "\n",
            quoted_date, part->length);
        if (ferror(fp_out))
          return -1;

        /* Copy the original mime headers */
        if (mutt_file_copy_bytes(fp_in, fp_out, part->offset - ftello(fp_in)))
          return -1;

        /* Skip the deleted body */
        fseeko(fp_in, part->offset + part->length, SEEK_SET);
      }
      else
      {
        if (copy_delete_attach(part, fp_in, fp_out, quoted_date))
          return -1;
      }
    }
  }

  /* Copy the last parts */
  if (mutt_file_copy_bytes(fp_in, fp_out, b->offset + b->length - ftello(fp_in)))
    return -1;

  return 0;
}

/**
 * format_address_header - Write address headers to a buffer
 * @param[out] h  Array of header strings
 * @param[in]  al AddressList
 *
 * This function is the equivalent of mutt_addrlist_write_file(), but writes to
 * a buffer instead of writing to a stream.  mutt_addrlist_write_file could be
 * re-used if we wouldn't store all the decoded headers in a huge array, first.
 *
 * TODO fix that.
 */
static void format_address_header(char **h, struct AddressList *al)
{
  char buf[8192];
  char cbuf[256];
  char c2buf[256];
  char *p = NULL;
  size_t linelen, buflen, plen;

  linelen = mutt_str_len(*h);
  plen = linelen;
  buflen = linelen + 3;

  mutt_mem_realloc(h, buflen);
  struct Address *first = TAILQ_FIRST(al);
  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    *buf = '\0';
    *cbuf = '\0';
    *c2buf = '\0';
    const size_t l = mutt_addr_write(buf, sizeof(buf), a, false);

    if (a != first && (linelen + l > 74))
    {
      strcpy(cbuf, "\n\t");
      linelen = l + 8;
    }
    else
    {
      if (a->mailbox)
      {
        strcpy(cbuf, " ");
        linelen++;
      }
      linelen += l;
    }
    if (!a->group && TAILQ_NEXT(a, entries) && TAILQ_NEXT(a, entries)->mailbox)
    {
      linelen++;
      buflen++;
      strcpy(c2buf, ",");
    }

    const size_t cbuflen = mutt_str_len(cbuf);
    const size_t c2buflen = mutt_str_len(c2buf);
    buflen += l + cbuflen + c2buflen;
    mutt_mem_realloc(h, buflen);
    p = *h;
    strcat(p + plen, cbuf);
    plen += cbuflen;
    strcat(p + plen, buf);
    plen += l;
    strcat(p + plen, c2buf);
    plen += c2buflen;
  }

  /* Space for this was allocated in the beginning of this function. */
  strcat(p + plen, "\n");
}

/**
 * address_header_decode - Parse an email's headers
 * @param[out] h Array of header strings
 * @retval 0 Success
 * @retval 1 Failure
 */
static int address_header_decode(char **h)
{
  char *s = *h;
  size_t l;
  bool rp = false;

  switch (tolower((unsigned char) *s))
  {
    case 'b':
    {
      if (!(l = mutt_istr_startswith(s, "bcc:")))
        return 0;
      break;
    }
    case 'c':
    {
      if (!(l = mutt_istr_startswith(s, "cc:")))
        return 0;
      break;
    }
    case 'f':
    {
      if (!(l = mutt_istr_startswith(s, "from:")))
        return 0;
      break;
    }
    case 'm':
    {
      if (!(l = mutt_istr_startswith(s, "mail-followup-to:")))
        return 0;
      break;
    }
    case 'r':
    {
      if ((l = mutt_istr_startswith(s, "return-path:")))
      {
        rp = true;
        break;
      }
      else if ((l = mutt_istr_startswith(s, "reply-to:")))
      {
        break;
      }
      return 0;
    }
    case 's':
    {
      if (!(l = mutt_istr_startswith(s, "sender:")))
        return 0;
      break;
    }
    case 't':
    {
      if (!(l = mutt_istr_startswith(s, "to:")))
        return 0;
      break;
    }
    default:
      return 0;
  }

  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  mutt_addrlist_parse(&al, s + l);
  if (TAILQ_EMPTY(&al))
    return 0;

  mutt_addrlist_to_local(&al);
  rfc2047_decode_addrlist(&al);
  struct Address *a = NULL;
  TAILQ_FOREACH(a, &al, entries)
  {
    if (a->personal)
    {
      mutt_str_dequote_comment(a->personal);
    }
  }

  /* angle brackets for return path are mandated by RFC5322,
   * so leave Return-Path as-is */
  if (rp)
    *h = mutt_str_dup(s);
  else
  {
    *h = mutt_mem_calloc(1, l + 2);
    mutt_str_copy(*h, s, l + 1);
    format_address_header(h, &al);
  }

  mutt_addrlist_clear(&al);

  FREE(&s);
  return 1;
}
