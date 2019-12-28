/**
 * @file
 * Decide how to display email content
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2010,2013 Michael R. Elkins <me@mutt.org>
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
 * @page handler Decide how to display email content
 *
 * Decide how to display email content
 */

#include "config.h"
#include <stddef.h>
#include <ctype.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "handler.h"
#include "copy.h"
#include "enriched.h"
#include "filter.h"
#include "globals.h"
#include "keymap.h"
#include "mailcap.h"
#include "mutt_attach.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "rfc3676.h"
#include "state.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* These Config Variables are only used in handler.c */
bool C_HonorDisposition; ///< Config: Don't display MIME parts inline if they have a disposition of 'attachment'
bool C_ImplicitAutoview; ///< Config: Display MIME attachments inline if a 'copiousoutput' mailcap entry exists
bool C_IncludeEncrypted; ///< Config: Whether to include encrypted content when replying
bool C_IncludeOnlyfirst; ///< Config: Only include the first attachment when replying
struct Slist *C_PreferredLanguages; ///< Config: Preferred languages for multilingual MIME
bool C_ReflowText; ///< Config: Reformat paragraphs of 'format=flowed' text
char *C_ShowMultipartAlternative; ///< Config: How to display 'multipart/alternative' MIME parts

#define BUFI_SIZE 1000
#define BUFO_SIZE 2000

#define TXT_HTML 1
#define TXT_PLAIN 2
#define TXT_ENRICHED 3

/**
 * typedef handler_t - Manage a PGP or S/MIME encrypted MIME part
 * @param m Body of the email
 * @param s State of text being processed
 * @retval 0 Success
 * @retval -1 Error
 */
typedef int (*handler_t)(struct Body *b, struct State *s);

/**
 * print_part_line - Print a separator for the Mime part
 * @param s State of text being processed
 * @param b Body of the email
 * @param n Part number for multipart emails (0 otherwise)
 */
static void print_part_line(struct State *s, struct Body *b, int n)
{
  char length[5];
  mutt_str_pretty_size(length, sizeof(length), b->length);
  state_mark_attach(s);
  char *charset = mutt_param_get(&b->parameter, "charset");
  if (n == 0)
  {
    state_printf(s, _("[-- Type: %s/%s%s%s, Encoding: %s, Size: %s --]\n"),
                 TYPE(b), b->subtype, charset ? "; charset=" : "",
                 charset ? charset : "", ENCODING(b->encoding), length);
  }
  else
  {
    state_printf(s, _("[-- Alternative Type #%d: %s/%s%s%s, Encoding: %s, Size: %s --]\n"),
                 n, TYPE(b), b->subtype, charset ? "; charset=" : "",
                 charset ? charset : "", ENCODING(b->encoding), length);
  }
}

/**
 * convert_to_state - Convert text and write it to a file
 * @param cd   Iconv conversion descriptor
 * @param bufi Buffer with text to convert
 * @param l    Length of buffer
 * @param s    State to write to
 */
static void convert_to_state(iconv_t cd, char *bufi, size_t *l, struct State *s)
{
  char bufo[BUFO_SIZE];
  const char *ib = NULL;
  char *ob = NULL;
  size_t ibl, obl;

  if (!bufi)
  {
    if (cd != (iconv_t)(-1))
    {
      ob = bufo;
      obl = sizeof(bufo);
      iconv(cd, NULL, NULL, &ob, &obl);
      if (ob != bufo)
        state_prefix_put(s, bufo, ob - bufo);
    }
    return;
  }

  if (cd == (iconv_t)(-1))
  {
    state_prefix_put(s, bufi, *l);
    *l = 0;
    return;
  }

  ib = bufi;
  ibl = *l;
  while (true)
  {
    ob = bufo;
    obl = sizeof(bufo);
    mutt_ch_iconv(cd, &ib, &ibl, &ob, &obl, 0, "?", NULL);
    if (ob == bufo)
      break;
    state_prefix_put(s, bufo, ob - bufo);
  }
  memmove(bufi, ib, ibl);
  *l = ibl;
}

/**
 * decode_xbit - Decode xbit-encoded text
 * @param s      State to work with
 * @param len    Length of text to decode
 * @param istext Mime part is plain text
 * @param cd     Iconv conversion descriptor
 */
static void decode_xbit(struct State *s, long len, bool istext, iconv_t cd)
{
  if (!istext)
  {
    mutt_file_copy_bytes(s->fp_in, s->fp_out, len);
    return;
  }

  state_set_prefix(s);

  int c;
  char bufi[BUFI_SIZE];
  size_t l = 0;
  while (((c = fgetc(s->fp_in)) != EOF) && len--)
  {
    if ((c == '\r') && len)
    {
      const int ch = fgetc(s->fp_in);
      if (ch == '\n')
      {
        c = ch;
        len--;
      }
      else
        ungetc(ch, s->fp_in);
    }

    bufi[l++] = c;
    if (l == sizeof(bufi))
      convert_to_state(cd, bufi, &l, s);
  }

  convert_to_state(cd, bufi, &l, s);
  convert_to_state(cd, 0, 0, s);

  state_reset_prefix(s);
}

/**
 * qp_decode_triple - Decode a quoted-printable triplet
 * @param s State to work with
 * @param d Decoded character
 * @retval 0 Success
 * @retval -1 Error
 */
static int qp_decode_triple(char *s, char *d)
{
  /* soft line break */
  if ((s[0] == '=') && (s[1] == '\0'))
    return 1;

  /* quoted-printable triple */
  if ((s[0] == '=') && isxdigit((unsigned char) s[1]) && isxdigit((unsigned char) s[2]))
  {
    *d = (hexval(s[1]) << 4) | hexval(s[2]);
    return 0;
  }

  /* something else */
  return -1;
}

/**
 * qp_decode_line - Decode a line of quoted-printable text
 * @param dest Buffer for result
 * @param src  Text to decode
 * @param l    Bytes written to buffer
 * @param last Last character of the line
 */
static void qp_decode_line(char *dest, char *src, size_t *l, int last)
{
  char *d = NULL, *s = NULL;
  char c = 0;

  int kind = -1;
  bool soft = false;

  /* decode the line */

  for (d = dest, s = src; *s;)
  {
    switch ((kind = qp_decode_triple(s, &c)))
    {
      case 0:
        *d++ = c;
        s += 3;
        break; /* qp triple */
      case -1:
        *d++ = *s++;
        break; /* single character */
      case 1:
        soft = true;
        s++;
        break; /* soft line break */
    }
  }

  if (!soft && (last == '\n'))
  {
    /* neither \r nor \n as part of line-terminating CRLF
     * may be qp-encoded, so remove \r and \n-terminate;
     * see RFC2045, sect. 6.7, (1): General 8bit representation */
    if ((kind == 0) && (c == '\r'))
      *(d - 1) = '\n';
    else
      *d++ = '\n';
  }

  *d = '\0';
  *l = d - dest;
}

/**
 * decode_quoted - Decode an attachment encoded with quoted-printable
 * @param s      State to work with
 * @param len    Length of text to decode
 * @param istext Mime part is plain text
 * @param cd     Iconv conversion descriptor
 *
 * Why doesn't this overflow any buffers? First, it's guaranteed that the
 * length of a line grows when you _en_-code it to quoted-printable. That
 * means that we always can store the result in a buffer of at most the _same_
 * size.
 *
 * Now, we don't special-case if the line we read with fgets() isn't
 * terminated. We don't care about this, since 256 > 78, so corrupted input
 * will just be corrupted a bit more. That implies that 256+1 bytes are
 * always sufficient to store the result of qp_decode_line.
 *
 * Finally, at soft line breaks, some part of a multibyte character may have
 * been left over by convert_to_state(). This shouldn't be more than 6
 * characters, so 256+7 should be sufficient memory to store the decoded
 * data.
 *
 * Just to make sure that I didn't make some off-by-one error above, we just
 * use 512 for the target buffer's size.
 */
static void decode_quoted(struct State *s, long len, bool istext, iconv_t cd)
{
  char line[256];
  char decline[512];
  size_t l = 0;
  size_t l3;

  if (istext)
    state_set_prefix(s);

  while (len > 0)
  {
    /* It's ok to use a fixed size buffer for input, even if the line turns
     * out to be longer than this.  Just process the line in chunks.  This
     * really shouldn't happen according the MIME spec, since Q-P encoded
     * lines are at most 76 characters, but we should be liberal about what
     * we accept.  */
    if (!fgets(line, MIN((ssize_t) sizeof(line), len + 1), s->fp_in))
      break;

    size_t linelen = strlen(line);
    len -= linelen;

    /* inspect the last character we read so we can tell if we got the
     * entire line.  */
    const int last = (linelen != 0) ? line[linelen - 1] : 0;

    /* chop trailing whitespace if we got the full line */
    if (last == '\n')
    {
      while ((linelen > 0) && IS_SPACE(line[linelen - 1]))
        linelen--;
      line[linelen] = '\0';
    }

    /* decode and do character set conversion */
    qp_decode_line(decline + l, line, &l3, last);
    l += l3;
    convert_to_state(cd, decline, &l, s);
  }

  convert_to_state(cd, 0, 0, s);
  state_reset_prefix(s);
}

/**
 * decode_byte - Decode a uuencoded byte
 * @param ch Character to decode
 * @retval num Decoded value
 */
static unsigned char decode_byte(char ch)
{
  if ((ch < 32) || (ch > 95))
    return 0;
  return ch - 32;
}

/**
 * decode_uuencoded - Decode uuencoded text
 * @param s      State to work with
 * @param len    Length of text to decode
 * @param istext Mime part is plain text
 * @param cd     Iconv conversion descriptor
 */
static void decode_uuencoded(struct State *s, long len, bool istext, iconv_t cd)
{
  char tmps[128];
  char *pt = NULL;
  char bufi[BUFI_SIZE];
  size_t k = 0;

  if (istext)
    state_set_prefix(s);

  while (len > 0)
  {
    if (!fgets(tmps, sizeof(tmps), s->fp_in))
      return;
    len -= mutt_str_strlen(tmps);
    if (mutt_str_startswith(tmps, "begin ", CASE_MATCH))
      break;
  }
  while (len > 0)
  {
    if (!fgets(tmps, sizeof(tmps), s->fp_in))
      return;
    len -= mutt_str_strlen(tmps);
    if (mutt_str_startswith(tmps, "end", CASE_MATCH))
      break;
    pt = tmps;
    const unsigned char linelen = decode_byte(*pt);
    pt++;
    for (unsigned char c = 0; c < linelen;)
    {
      for (char l = 2; l <= 6; l += 2)
      {
        char out = decode_byte(*pt) << l;
        pt++;
        out |= (decode_byte(*pt) >> (6 - l));
        bufi[k++] = out;
        c++;
        if (c == linelen)
          break;
      }
      convert_to_state(cd, bufi, &k, s);
      pt++;
    }
  }

  convert_to_state(cd, bufi, &k, s);
  convert_to_state(cd, 0, 0, s);

  state_reset_prefix(s);
}

/**
 * is_mmnoask - Metamail compatibility: should the attachment be autoviewed?
 * @param buf Mime type, e.g. 'text/plain'
 * @retval true Metamail "no ask" is true
 *
 * Test if the `MM_NOASK` environment variable should allow autoviewing of the
 * attachment.
 *
 * @note If `MM_NOASK=1` then the function will automatically return true.
 */
static bool is_mmnoask(const char *buf)
{
  const char *val = mutt_str_getenv("MM_NOASK");
  if (!val)
    return false;

  char *p = NULL;
  char tmp[1024];
  char *q = NULL;

  if (mutt_str_strcmp(val, "1") == 0)
    return true;

  mutt_str_strfcpy(tmp, val, sizeof(tmp));
  p = tmp;

  while ((p = strtok(p, ",")))
  {
    q = strrchr(p, '/');
    if (q)
    {
      if (q[1] == '*')
      {
        if (mutt_str_strncasecmp(buf, p, q - p) == 0)
          return true;
      }
      else
      {
        if (mutt_str_strcasecmp(buf, p) == 0)
          return true;
      }
    }
    else
    {
      const size_t plen = mutt_str_startswith(buf, p, CASE_IGNORE);
      if ((plen != 0) && (buf[plen] == '/'))
        return true;
    }

    p = NULL;
  }

  return false;
}

/**
 * is_autoview - Should email body be filtered by mailcap
 * @param b Body of the email
 * @retval 1 body part should be filtered by a mailcap entry prior to viewing inline
 * @retval 0 otherwise
 */
static bool is_autoview(struct Body *b)
{
  char type[256];
  bool is_av = false;

  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);

  if (C_ImplicitAutoview)
  {
    /* $implicit_autoview is essentially the same as "auto_view *" */
    is_av = true;
  }
  else
  {
    /* determine if this type is on the user's auto_view list */
    mutt_check_lookup_list(b, type, sizeof(type));
    struct ListNode *np = NULL;
    STAILQ_FOREACH(np, &AutoViewList, entries)
    {
      int i = mutt_str_strlen(np->data) - 1;
      if (((i > 0) && (np->data[i - 1] == '/') && (np->data[i] == '*') &&
           (mutt_str_strncasecmp(type, np->data, i) == 0)) ||
          (mutt_str_strcasecmp(type, np->data) == 0))
      {
        is_av = true;
        break;
      }
    }

    if (is_mmnoask(type))
      is_av = true;
  }

  /* determine if there is a mailcap entry suitable for auto_view
   *
   * @warning type is altered by this call as a result of 'mime_lookup' support */
  if (is_av)
    return mailcap_lookup(b, type, sizeof(type), NULL, MUTT_MC_AUTOVIEW);

  return false;
}

/**
 * autoview_handler - Handler for autoviewable attachments - Implements ::handler_t
 */
static int autoview_handler(struct Body *a, struct State *s)
{
  struct MailcapEntry *entry = mailcap_entry_new();
  char buf[1024];
  char type[256];
  struct Buffer *cmd = mutt_buffer_pool_get();
  struct Buffer *tempfile = mutt_buffer_pool_get();
  char *fname = NULL;
  FILE *fp_in = NULL;
  FILE *fp_out = NULL;
  FILE *fp_err = NULL;
  pid_t pid;
  int rc = 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_AUTOVIEW);

  fname = mutt_str_strdup(a->filename);
  mutt_file_sanitize_filename(fname, true);
  mailcap_expand_filename(entry->nametemplate, fname, tempfile);
  FREE(&fname);

  if (entry->command)
  {
    mutt_buffer_strcpy(cmd, entry->command);

    /* mailcap_expand_command returns 0 if the file is required */
    bool piped = mailcap_expand_command(a, mutt_b2s(tempfile), type, cmd);

    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach(s);
      state_printf(s, _("[-- Autoview using %s --]\n"), mutt_b2s(cmd));
      mutt_message(_("Invoking autoview command: %s"), mutt_b2s(cmd));
    }

    fp_in = mutt_file_fopen(mutt_b2s(tempfile), "w+");
    if (!fp_in)
    {
      mutt_perror("fopen");
      mailcap_entry_free(&entry);
      rc = -1;
      goto cleanup;
    }

    mutt_file_copy_bytes(s->fp_in, fp_in, a->length);

    if (piped)
    {
      unlink(mutt_b2s(tempfile));
      fflush(fp_in);
      rewind(fp_in);
      pid = mutt_create_filter_fd(mutt_b2s(cmd), NULL, &fp_out, &fp_err,
                                  fileno(fp_in), -1, -1);
    }
    else
    {
      mutt_file_fclose(&fp_in);
      pid = mutt_create_filter(mutt_b2s(cmd), NULL, &fp_out, &fp_err);
    }

    if (pid < 0)
    {
      mutt_perror(_("Can't create filter"));
      if (s->flags & MUTT_DISPLAY)
      {
        state_mark_attach(s);
        state_printf(s, _("[-- Can't run %s. --]\n"), mutt_b2s(cmd));
      }
      rc = -1;
      goto bail;
    }

    if (s->prefix)
    {
      while (fgets(buf, sizeof(buf), fp_out))
      {
        state_puts(s, s->prefix);
        state_puts(s, buf);
      }
      /* check for data on stderr */
      if (fgets(buf, sizeof(buf), fp_err))
      {
        if (s->flags & MUTT_DISPLAY)
        {
          state_mark_attach(s);
          state_printf(s, _("[-- Autoview stderr of %s --]\n"), mutt_b2s(cmd));
        }

        state_puts(s, s->prefix);
        state_puts(s, buf);
        while (fgets(buf, sizeof(buf), fp_err))
        {
          state_puts(s, s->prefix);
          state_puts(s, buf);
        }
      }
    }
    else
    {
      mutt_file_copy_stream(fp_out, s->fp_out);
      /* Check for stderr messages */
      if (fgets(buf, sizeof(buf), fp_err))
      {
        if (s->flags & MUTT_DISPLAY)
        {
          state_mark_attach(s);
          state_printf(s, _("[-- Autoview stderr of %s --]\n"), mutt_b2s(cmd));
        }

        state_puts(s, buf);
        mutt_file_copy_stream(fp_err, s->fp_out);
      }
    }

  bail:
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_err);

    mutt_wait_filter(pid);
    if (piped)
      mutt_file_fclose(&fp_in);
    else
      mutt_file_unlink(mutt_b2s(tempfile));

    if (s->flags & MUTT_DISPLAY)
      mutt_clear_error();
  }

cleanup:
  mailcap_entry_free(&entry);

  mutt_buffer_pool_release(&cmd);
  mutt_buffer_pool_release(&tempfile);

  return rc;
}

/**
 * text_plain_handler - Handler for plain text - Implements ::handler_t
 * @retval 0 Always
 *
 * When generating format=flowed ($text_flowed is set) from format=fixed, strip
 * all trailing spaces to improve interoperability; if $text_flowed is unset,
 * simply verbatim copy input.
 */
static int text_plain_handler(struct Body *b, struct State *s)
{
  char *buf = NULL;
  size_t sz = 0;

  while ((buf = mutt_file_read_line(buf, &sz, s->fp_in, NULL, 0)))
  {
    if ((mutt_str_strcmp(buf, "-- ") != 0) && C_TextFlowed)
    {
      size_t len = mutt_str_strlen(buf);
      while ((len > 0) && (buf[len - 1] == ' '))
        buf[--len] = '\0';
    }
    if (s->prefix)
      state_puts(s, s->prefix);
    state_puts(s, buf);
    state_putc(s, '\n');
  }

  FREE(&buf);
  return 0;
}

/**
 * message_handler - Handler for message/rfc822 body parts - Implements ::handler_t
 */
static int message_handler(struct Body *a, struct State *s)
{
  struct stat st;
  struct Body *b = NULL;
  LOFF_T off_start;
  int rc = 0;

  off_start = ftello(s->fp_in);
  if (off_start < 0)
    return -1;

  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    fstat(fileno(s->fp_in), &st);
    b = mutt_body_new();
    b->length = (LOFF_T) st.st_size;
    b->parts = mutt_rfc822_parse_message(s->fp_in, b);
  }
  else
    b = a;

  if (b->parts)
  {
    CopyHeaderFlags chflags = CH_DECODE | CH_FROM;
    if ((s->flags & MUTT_WEED) || ((s->flags & (MUTT_DISPLAY | MUTT_PRINTING)) && C_Weed))
      chflags |= CH_WEED | CH_REORDER;
    if (s->prefix)
      chflags |= CH_PREFIX;
    if (s->flags & MUTT_DISPLAY)
      chflags |= CH_DISPLAY;

    mutt_copy_hdr(s->fp_in, s->fp_out, off_start, b->parts->offset, chflags, s->prefix, 0);

    if (s->prefix)
      state_puts(s, s->prefix);
    state_putc(s, '\n');

    rc = mutt_body_handler(b->parts, s);
  }

  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    mutt_body_free(&b);
  }

  return rc;
}

/**
 * external_body_handler - Handler for external-body emails - Implements ::handler_t
 */
static int external_body_handler(struct Body *b, struct State *s)
{
  const char *str = NULL;
  char strbuf[1024];

  const char *access_type = mutt_param_get(&b->parameter, "access-type");
  if (!access_type)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach(s);
      state_puts(s, _("[-- Error: message/external-body has no access-type "
                      "parameter --]\n"));
      return 0;
    }
    else
      return -1;
  }

  const char *expiration = mutt_param_get(&b->parameter, "expiration");
  time_t expire;
  if (expiration)
    expire = mutt_date_parse_date(expiration, NULL);
  else
    expire = -1;

  if (mutt_str_strcasecmp(access_type, "x-mutt-deleted") == 0)
  {
    if (s->flags & (MUTT_DISPLAY | MUTT_PRINTING))
    {
      char pretty_size[10];
      char *length = mutt_param_get(&b->parameter, "length");
      if (length)
      {
        long size = strtol(length, NULL, 10);
        mutt_str_pretty_size(pretty_size, sizeof(pretty_size), size);
        if (expire != -1)
        {
          str = ngettext(
              /* L10N: If the translation of this string is a multi line string, then
                 each line should start with "[-- " and end with " --]".
                 The first "%s/%s" is a MIME type, e.g. "text/plain". The last %s
                 expands to a date as returned by `mutt_date_parse_date()`.

                 Note: The size argument printed is not the actual number as passed
                 to gettext but the prettified version, e.g. size = 2048 will be
                 printed as 2K.  Your language might be sensitive to that: For
                 example although '1K' and '1024' represent the same number your
                 language might inflect the noun 'byte' differently.

                 Sadly, we can't do anything about that at the moment besides
                 passing the precise size in bytes. If you are interested the
                 function responsible for the prettification is
                 mutt_str_pretty_size() in mutt/string.c. */
              "[-- This %s/%s attachment (size %s byte) has been deleted --]\n"
              "[-- on %s --]\n",
              "[-- This %s/%s attachment (size %s bytes) has been deleted --]\n"
              "[-- on %s --]\n",
              size);
        }
        else
        {
          str = ngettext(
              /* L10N: If the translation of this string is a multi line string, then
                 each line should start with "[-- " and end with " --]".
                 The first "%s/%s" is a MIME type, e.g. "text/plain".

                 Note: The size argument printed is not the actual number as passed
                 to gettext but the prettified version, e.g. size = 2048 will be
                 printed as 2K.  Your language might be sensitive to that: For
                 example although '1K' and '1024' represent the same number your
                 language might inflect the noun 'byte' differently.

                 Sadly, we can't do anything about that at the moment besides
                 passing the precise size in bytes. If you are interested the
                 function responsible for the prettification is
                 mutt_str_pretty_size() in mutt/string.c.  */
              "[-- This %s/%s attachment (size %s byte) has been deleted --]\n",
              "[-- This %s/%s attachment (size %s bytes) has been deleted "
              "--]\n",
              size);
        }
      }
      else
      {
        pretty_size[0] = '\0';
        if (expire != -1)
        {
          /* L10N: If the translation of this string is a multi line string, then
             each line should start with "[-- " and end with " --]".
             The first "%s/%s" is a MIME type, e.g. "text/plain". The last %s
             expands to a date as returned by `mutt_date_parse_date()`.

             Caution: Argument three %3$ is also defined but should not be used
             in this translation!  */
          str = _("[-- This %s/%s attachment has been deleted --]\n[-- on %4$s "
                  "--]\n");
        }
        else
        {
          /* L10N: If the translation of this string is a multi line string, then
             each line should start with "[-- " and end with " --]".
             The first "%s/%s" is a MIME type, e.g. "text/plain". */
          str = _("[-- This %s/%s attachment has been deleted --]\n");
        }
      }

      snprintf(strbuf, sizeof(strbuf), str, TYPE(b->parts), b->parts->subtype,
               pretty_size, expiration);
      state_attach_puts(s, strbuf);
      if (b->parts->filename)
      {
        state_mark_attach(s);
        state_printf(s, _("[-- name: %s --]\n"), b->parts->filename);
      }

      CopyHeaderFlags chflags = CH_DECODE;
      if (C_Weed)
        chflags |= CH_WEED | CH_REORDER;

      mutt_copy_hdr(s->fp_in, s->fp_out, ftello(s->fp_in), b->parts->offset,
                    chflags, NULL, 0);
    }
  }
  else if (expiration && (expire < mutt_date_epoch()))
  {
    if (s->flags & MUTT_DISPLAY)
    {
      /* L10N: If the translation of this string is a multi line string, then
         each line should start with "[-- " and end with " --]".
         The "%s/%s" is a MIME type, e.g. "text/plain". */
      snprintf(strbuf, sizeof(strbuf), _("[-- This %s/%s attachment is not included, --]\n[-- and the indicated external source has --]\n[-- expired. --]\n"),
               TYPE(b->parts), b->parts->subtype);
      state_attach_puts(s, strbuf);

      CopyHeaderFlags chflags = CH_DECODE | CH_DISPLAY;
      if (C_Weed)
        chflags |= CH_WEED | CH_REORDER;

      mutt_copy_hdr(s->fp_in, s->fp_out, ftello(s->fp_in), b->parts->offset,
                    chflags, NULL, 0);
    }
  }
  else
  {
    if (s->flags & MUTT_DISPLAY)
    {
      /* L10N: If the translation of this string is a multi line string, then
         each line should start with "[-- " and end with " --]".
         The "%s/%s" is a MIME type, e.g. "text/plain".  The %s after
         access-type is an access-type as defined by the MIME RFCs, e.g. "FTP",
         "LOCAL-FILE", "MAIL-SERVER". */
      snprintf(strbuf, sizeof(strbuf), _("[-- This %s/%s attachment is not included, --]\n[-- and the indicated access-type %s is unsupported --]\n"),
               TYPE(b->parts), b->parts->subtype, access_type);
      state_attach_puts(s, strbuf);

      CopyHeaderFlags chflags = CH_DECODE | CH_DISPLAY;
      if (C_Weed)
        chflags |= CH_WEED | CH_REORDER;

      mutt_copy_hdr(s->fp_in, s->fp_out, ftello(s->fp_in), b->parts->offset,
                    chflags, NULL, 0);
    }
  }

  return 0;
}

/**
 * alternative_handler - Handler for multipart alternative emails - Implements ::handler_t
 */
static int alternative_handler(struct Body *a, struct State *s)
{
  struct Body *choice = NULL;
  struct Body *b = NULL;
  bool mustfree = false;
  int rc = 0;

  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    struct stat st;
    mustfree = true;
    fstat(fileno(s->fp_in), &st);
    b = mutt_body_new();
    b->length = (long) st.st_size;
    b->parts = mutt_parse_multipart(
        s->fp_in, mutt_param_get(&a->parameter, "boundary"), (long) st.st_size,
        (mutt_str_strcasecmp("digest", a->subtype) == 0));
  }
  else
    b = a;

  a = b;

  /* First, search list of preferred types */
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &AlternativeOrderList, entries)
  {
    int btlen; /* length of basetype */
    bool wild; /* do we have a wildcard to match all subtypes? */

    char *c = strchr(np->data, '/');
    if (c)
    {
      wild = ((c[1] == '*') && (c[2] == '\0'));
      btlen = c - np->data;
    }
    else
    {
      wild = true;
      btlen = mutt_str_strlen(np->data);
    }

    if (a->parts)
      b = a->parts;
    else
      b = a;
    while (b)
    {
      const char *bt = TYPE(b);
      if ((mutt_str_strncasecmp(bt, np->data, btlen) == 0) && (bt[btlen] == 0))
      {
        /* the basetype matches */
        if (wild || (mutt_str_strcasecmp(np->data + btlen + 1, b->subtype) == 0))
        {
          choice = b;
        }
      }
      b = b->next;
    }

    if (choice)
      break;
  }

  /* Next, look for an autoviewable type */
  if (!choice)
  {
    if (a->parts)
      b = a->parts;
    else
      b = a;
    while (b)
    {
      if (is_autoview(b))
        choice = b;
      b = b->next;
    }
  }

  /* Then, look for a text entry */
  if (!choice)
  {
    if (a->parts)
      b = a->parts;
    else
      b = a;
    int type = 0;
    while (b)
    {
      if (b->type == TYPE_TEXT)
      {
        if ((mutt_str_strcasecmp("plain", b->subtype) == 0) && (type <= TXT_PLAIN))
        {
          choice = b;
          type = TXT_PLAIN;
        }
        else if ((mutt_str_strcasecmp("enriched", b->subtype) == 0) && (type <= TXT_ENRICHED))
        {
          choice = b;
          type = TXT_ENRICHED;
        }
        else if ((mutt_str_strcasecmp("html", b->subtype) == 0) && (type <= TXT_HTML))
        {
          choice = b;
          type = TXT_HTML;
        }
      }
      b = b->next;
    }
  }

  /* Finally, look for other possibilities */
  if (!choice)
  {
    if (a->parts)
      b = a->parts;
    else
      b = a;
    while (b)
    {
      if (mutt_can_decode(b))
        choice = b;
      b = b->next;
    }
  }

  if (choice)
  {
    if (s->flags & MUTT_DISPLAY && !C_Weed)
    {
      fseeko(s->fp_in, choice->hdr_offset, SEEK_SET);
      mutt_file_copy_bytes(s->fp_in, s->fp_out, choice->offset - choice->hdr_offset);
    }

    if (mutt_str_strcmp("info", C_ShowMultipartAlternative) == 0)
    {
      print_part_line(s, choice, 0);
    }
    mutt_body_handler(choice, s);

    if (mutt_str_strcmp("info", C_ShowMultipartAlternative) == 0)
    {
      if (a->parts)
        b = a->parts;
      else
        b = a;
      int count = 0;
      while (b)
      {
        if (choice != b)
        {
          count += 1;
          if (count == 1)
            state_putc(s, '\n');

          print_part_line(s, b, count);
        }
        b = b->next;
      }
    }
  }
  else if (s->flags & MUTT_DISPLAY)
  {
    /* didn't find anything that we could display! */
    state_mark_attach(s);
    state_puts(s, _("[-- Error:  Could not display any parts of "
                    "Multipart/Alternative --]\n"));
    rc = -1;
  }

  if (mustfree)
    mutt_body_free(&a);

  return rc;
}

/**
 * multilingual_handler - Handler for multi-lingual emails - Implements ::handler_t
 * @retval 0 Always
 */
static int multilingual_handler(struct Body *a, struct State *s)
{
  struct Body *b = NULL;
  bool mustfree = false;
  int rc = 0;

  mutt_debug(LL_DEBUG2,
             "RFC8255 >> entering in handler multilingual handler\n");
  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    struct stat st;
    mustfree = true;
    fstat(fileno(s->fp_in), &st);
    b = mutt_body_new();
    b->length = (long) st.st_size;
    b->parts = mutt_parse_multipart(
        s->fp_in, mutt_param_get(&a->parameter, "boundary"), (long) st.st_size,
        (mutt_str_strcasecmp("digest", a->subtype) == 0));
  }
  else
    b = a;

  a = b;

  if (a->parts)
    b = a->parts;
  else
    b = a;

  struct Body *choice = NULL;
  struct Body *first_part = NULL;
  struct Body *zxx_part = NULL;
  struct ListNode *np = NULL;

  if (C_PreferredLanguages)
  {
    struct Buffer *langs = mutt_buffer_pool_get();
    cs_str_string_get(NeoMutt->sub->cs, "preferred_languages", langs);
    mutt_debug(LL_DEBUG2, "RFC8255 >> preferred_languages set in config to '%s'\n",
               mutt_b2s(langs));
    mutt_buffer_pool_release(&langs);

    STAILQ_FOREACH(np, &C_PreferredLanguages->head, entries)
    {
      while (b)
      {
        if (mutt_can_decode(b))
        {
          if (!first_part)
            first_part = b;

          if (b->language && (mutt_str_strcmp("zxx", b->language) == 0))
            zxx_part = b;

          mutt_debug(LL_DEBUG2, "RFC8255 >> comparing configuration preferred_language='%s' to mail part content-language='%s'\n",
                     np->data, b->language);
          if (b->language && (mutt_str_strcmp(np->data, b->language) == 0))
          {
            mutt_debug(LL_DEBUG2, "RFC8255 >> preferred_language='%s' matches content-language='%s' >> part selected to be displayed\n",
                       np->data, b->language);
            choice = b;
            break;
          }
        }

        b = b->next;
      }

      if (choice)
        break;

      if (a->parts)
        b = a->parts;
      else
        b = a;
    }
  }

  if (choice)
    mutt_body_handler(choice, s);
  else
  {
    if (zxx_part)
      mutt_body_handler(zxx_part, s);
    else
      mutt_body_handler(first_part, s);
  }

  if (mustfree)
    mutt_body_free(&a);

  return rc;
}

/**
 * multipart_handler - Handler for multipart emails - Implements ::handler_t
 */
static int multipart_handler(struct Body *a, struct State *s)
{
  struct Body *b = NULL, *p = NULL;
  struct stat st;
  int count;
  int rc = 0;

  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    fstat(fileno(s->fp_in), &st);
    b = mutt_body_new();
    b->length = (long) st.st_size;
    b->parts = mutt_parse_multipart(
        s->fp_in, mutt_param_get(&a->parameter, "boundary"), (long) st.st_size,
        (mutt_str_strcasecmp("digest", a->subtype) == 0));
  }
  else
    b = a;

  for (p = b->parts, count = 1; p; p = p->next, count++)
  {
    if (s->flags & MUTT_DISPLAY)
    {
      state_mark_attach(s);
      if (p->description || p->filename || p->form_name)
      {
        /* L10N: %s is the attachment description, filename or form_name. */
        state_printf(s, _("[-- Attachment #%d: %s --]\n"), count,
                     p->description ? p->description :
                                      p->filename ? p->filename : p->form_name);
      }
      else
        state_printf(s, _("[-- Attachment #%d --]\n"), count);
      print_part_line(s, p, 0);
      if (C_Weed)
      {
        state_putc(s, '\n');
      }
      else
      {
        fseeko(s->fp_in, p->hdr_offset, SEEK_SET);
        mutt_file_copy_bytes(s->fp_in, s->fp_out, p->offset - p->hdr_offset);
      }
    }

    rc = mutt_body_handler(p, s);
    state_putc(s, '\n');

    if (rc != 0)
    {
      mutt_error(_("One or more parts of this message could not be displayed"));
      mutt_debug(LL_DEBUG1, "Failed on attachment #%d, type %s/%s\n", count,
                 TYPE(p), NONULL(p->subtype));
    }

    if ((s->flags & MUTT_REPLYING) && C_IncludeOnlyfirst && (s->flags & MUTT_FIRSTDONE))
    {
      break;
    }
  }

  if ((a->encoding == ENC_BASE64) || (a->encoding == ENC_QUOTED_PRINTABLE) ||
      (a->encoding == ENC_UUENCODED))
  {
    mutt_body_free(&b);
  }

  /* make failure of a single part non-fatal */
  if (rc < 0)
    rc = 1;
  return rc;
}

/**
 * run_decode_and_handler - Run an appropriate decoder for an email
 * @param b         Body of the email
 * @param s         State to work with
 * @param handler   Callback function to process the content - Implements ::handler_t
 * @param plaintext Is the content in plain text
 * @retval 0 Success
 * @retval -1 Error
 */
static int run_decode_and_handler(struct Body *b, struct State *s,
                                  handler_t handler, bool plaintext)
{
  char *save_prefix = NULL;
  FILE *fp = NULL;
  size_t tmplength = 0;
  LOFF_T tmpoffset = 0;
  int decode = 0;
  int rc = 0;
#ifndef USE_FMEMOPEN
  struct Buffer *tempfile = NULL;
#endif

  fseeko(s->fp_in, b->offset, SEEK_SET);

#ifdef USE_FMEMOPEN
  char *temp = NULL;
  size_t tempsize = 0;
#endif

  /* see if we need to decode this part before processing it */
  if ((b->encoding == ENC_BASE64) || (b->encoding == ENC_QUOTED_PRINTABLE) ||
      (b->encoding == ENC_UUENCODED) || (plaintext || mutt_is_text_part(b)))
  /* text subtypes may require character set conversion even with 8bit encoding */
  {
    const int orig_type = b->type;
    if (!plaintext)
    {
      /* decode to a tempfile, saving the original destination */
      fp = s->fp_out;
#ifdef USE_FMEMOPEN
      s->fp_out = open_memstream(&temp, &tempsize);
      if (!s->fp_out)
      {
        mutt_error(_("Unable to open 'memory stream'"));
        mutt_debug(LL_DEBUG1, "Can't open 'memory stream'\n");
        return -1;
      }
#else
      tempfile = mutt_buffer_pool_get();
      mutt_buffer_mktemp(tempfile);
      s->fp_out = mutt_file_fopen(mutt_b2s(tempfile), "w");
      if (!s->fp_out)
      {
        mutt_error(_("Unable to open temporary file"));
        mutt_debug(LL_DEBUG1, "Can't open %s\n", mutt_b2s(tempfile));
        mutt_buffer_pool_release(&tempfile);
        return -1;
      }
#endif
      /* decoding the attachment changes the size and offset, so save a copy
       * of the "real" values now, and restore them after processing */
      tmplength = b->length;
      tmpoffset = b->offset;

      /* if we are decoding binary bodies, we don't want to prefix each
       * line with the prefix or else the data will get corrupted.  */
      save_prefix = s->prefix;
      s->prefix = NULL;

      decode = 1;
    }
    else
      b->type = TYPE_TEXT;

    mutt_decode_attachment(b, s);

    if (decode)
    {
      b->length = ftello(s->fp_out);
      b->offset = 0;
#ifdef USE_FMEMOPEN
      /* When running under torify, mutt_file_fclose(&s->fp_out) does not seem to
       * update tempsize. On the other hand, fflush does.  See
       * https://github.com/neomutt/neomutt/issues/440 */
      fflush(s->fp_out);
#endif
      mutt_file_fclose(&s->fp_out);

      /* restore final destination and substitute the tempfile for input */
      s->fp_out = fp;
      fp = s->fp_in;
#ifdef USE_FMEMOPEN
      if (tempsize)
      {
        s->fp_in = fmemopen(temp, tempsize, "r");
      }
      else
      { /* fmemopen can't handle zero-length buffers */
        s->fp_in = mutt_file_fopen("/dev/null", "r");
      }
      if (!s->fp_in)
      {
        mutt_perror(_("failed to re-open 'memory stream'"));
        return -1;
      }
#else
      s->fp_in = fopen(mutt_b2s(tempfile), "r");
      unlink(mutt_b2s(tempfile));
      mutt_buffer_pool_release(&tempfile);
#endif
      /* restore the prefix */
      s->prefix = save_prefix;
    }

    b->type = orig_type;
  }

  /* process the (decoded) body part */
  if (handler)
  {
    rc = handler(b, s);
    if (rc != 0)
    {
      mutt_debug(LL_DEBUG1, "Failed on attachment of type %s/%s\n", TYPE(b),
                 NONULL(b->subtype));
    }

    if (decode)
    {
      b->length = tmplength;
      b->offset = tmpoffset;

      /* restore the original source stream */
      mutt_file_fclose(&s->fp_in);
#ifdef USE_FMEMOPEN
      FREE(&temp);
#endif
      s->fp_in = fp;
    }
  }
  s->flags |= MUTT_FIRSTDONE;

  return rc;
}

/**
 * valid_pgp_encrypted_handler - Handler for valid pgp-encrypted emails - Implements ::handler_t
 */
static int valid_pgp_encrypted_handler(struct Body *b, struct State *s)
{
  struct Body *octetstream = b->parts->next;

  /* clear out any mime headers before the handler, so they can't be spoofed. */
  mutt_env_free(&b->mime_headers);
  mutt_env_free(&octetstream->mime_headers);

  int rc;
  /* Some clients improperly encode the octetstream part. */
  if (octetstream->encoding != ENC_7BIT)
    rc = run_decode_and_handler(octetstream, s, crypt_pgp_encrypted_handler, 0);
  else
    rc = crypt_pgp_encrypted_handler(octetstream, s);
  b->goodsig |= octetstream->goodsig;

  /* Relocate protected headers onto the multipart/encrypted part */
  if (!rc && octetstream->mime_headers)
  {
    b->mime_headers = octetstream->mime_headers;
    octetstream->mime_headers = NULL;
  }

  return rc;
}

/**
 * malformed_pgp_encrypted_handler - Handler for invalid pgp-encrypted emails - Implements ::handler_t
 */
static int malformed_pgp_encrypted_handler(struct Body *b, struct State *s)
{
  struct Body *octetstream = b->parts->next->next;

  /* clear out any mime headers before the handler, so they can't be spoofed. */
  mutt_env_free(&b->mime_headers);
  mutt_env_free(&octetstream->mime_headers);

  /* exchange encodes the octet-stream, so re-run it through the decoder */
  int rc = run_decode_and_handler(octetstream, s, crypt_pgp_encrypted_handler, false);
  b->goodsig |= octetstream->goodsig;
#ifdef USE_AUTOCRYPT
  b->is_autocrypt |= octetstream->is_autocrypt;
#endif

  /* Relocate protected headers onto the multipart/encrypted part */
  if (!rc && octetstream->mime_headers)
  {
    b->mime_headers = octetstream->mime_headers;
    octetstream->mime_headers = NULL;
  }

  return rc;
}

/**
 * mutt_decode_base64 - Decode base64-encoded text
 * @param s      State to work with
 * @param len    Length of text to decode
 * @param istext Mime part is plain text
 * @param cd     Iconv conversion descriptor
 */
void mutt_decode_base64(struct State *s, size_t len, bool istext, iconv_t cd)
{
  char buf[5];
  int ch, i;
  char bufi[BUFI_SIZE];
  size_t l = 0;

  buf[4] = '\0';

  if (istext)
    state_set_prefix(s);

  while (len > 0)
  {
    for (i = 0; (i < 4) && (len > 0); len--)
    {
      ch = fgetc(s->fp_in);
      if (ch == EOF)
        break;
      if ((ch >= 0) && (ch < 128) && ((base64val(ch) != -1) || (ch == '=')))
        buf[i++] = ch;
    }
    if (i != 4)
    {
      /* "i" may be zero if there is trailing whitespace, which is not an error */
      if (i != 0)
        mutt_debug(LL_DEBUG2, "didn't get a multiple of 4 chars\n");
      break;
    }

    const int c1 = base64val(buf[0]);
    const int c2 = base64val(buf[1]);
    ch = (c1 << 2) | (c2 >> 4);
    bufi[l++] = ch;

    if (buf[2] == '=')
      break;
    const int c3 = base64val(buf[2]);
    ch = ((c2 & 0xf) << 4) | (c3 >> 2);
    bufi[l++] = ch;

    if (buf[3] == '=')
      break;
    const int c4 = base64val(buf[3]);
    ch = ((c3 & 0x3) << 6) | c4;
    bufi[l++] = ch;

    if ((l + 8) >= sizeof(bufi))
      convert_to_state(cd, bufi, &l, s);
  }

  convert_to_state(cd, bufi, &l, s);
  convert_to_state(cd, 0, 0, s);

  state_reset_prefix(s);
}

/**
 * mutt_body_handler - Handler for the Body of an email
 * @param b Body of the email
 * @param s State to work with
 * @retval 0 Success
 * @retval -1 Error
 */
int mutt_body_handler(struct Body *b, struct State *s)
{
  if (!b || !s)
    return -1;

  bool plaintext = false;
  handler_t handler = NULL;
  handler_t encrypted_handler = NULL;
  int rc = 0;

  int oflags = s->flags;

  /* first determine which handler to use to process this part */

  if (is_autoview(b))
  {
    handler = autoview_handler;
    s->flags &= ~MUTT_CHARCONV;
  }
  else if (b->type == TYPE_TEXT)
  {
    if (mutt_str_strcasecmp("plain", b->subtype) == 0)
    {
      /* avoid copying this part twice since removing the transfer-encoding is
       * the only operation needed.  */
      if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(b))
      {
        encrypted_handler = crypt_pgp_application_handler;
        handler = encrypted_handler;
      }
      else if (C_ReflowText &&
               (mutt_str_strcasecmp("flowed",
                                    mutt_param_get(&b->parameter, "format")) == 0))
      {
        handler = rfc3676_handler;
      }
      else
      {
        handler = text_plain_handler;
      }
    }
    else if (mutt_str_strcasecmp("enriched", b->subtype) == 0)
      handler = text_enriched_handler;
    else /* text body type without a handler */
      plaintext = false;
  }
  else if (b->type == TYPE_MESSAGE)
  {
    if (mutt_is_message_type(b->type, b->subtype))
      handler = message_handler;
    else if (mutt_str_strcasecmp("delivery-status", b->subtype) == 0)
      plaintext = true;
    else if (mutt_str_strcasecmp("external-body", b->subtype) == 0)
      handler = external_body_handler;
  }
  else if (b->type == TYPE_MULTIPART)
  {
    if ((mutt_str_strcmp("inline", C_ShowMultipartAlternative) != 0) &&
        (mutt_str_strcasecmp("alternative", b->subtype) == 0))
    {
      handler = alternative_handler;
    }
    else if ((mutt_str_strcmp("inline", C_ShowMultipartAlternative) != 0) &&
             (mutt_str_strcasecmp("multilingual", b->subtype) == 0))
    {
      handler = multilingual_handler;
    }
    else if ((WithCrypto != 0) && (mutt_str_strcasecmp("signed", b->subtype) == 0))
    {
      if (!mutt_param_get(&b->parameter, "protocol"))
        mutt_error(_("Error: multipart/signed has no protocol"));
      else if (s->flags & MUTT_VERIFY)
        handler = mutt_signed_handler;
    }
    else if (mutt_is_valid_multipart_pgp_encrypted(b))
    {
      encrypted_handler = valid_pgp_encrypted_handler;
      handler = encrypted_handler;
    }
    else if (mutt_is_malformed_multipart_pgp_encrypted(b))
    {
      encrypted_handler = malformed_pgp_encrypted_handler;
      handler = encrypted_handler;
    }

    if (!handler)
      handler = multipart_handler;

    if ((b->encoding != ENC_7BIT) && (b->encoding != ENC_8BIT) && (b->encoding != ENC_BINARY))
    {
      mutt_debug(LL_DEBUG1, "Bad encoding type %d for multipart entity, assuming 7 bit\n",
                 b->encoding);
      b->encoding = ENC_7BIT;
    }
  }
  else if ((WithCrypto != 0) && (b->type == TYPE_APPLICATION))
  {
    if (OptDontHandlePgpKeys && (mutt_str_strcasecmp("pgp-keys", b->subtype) == 0))
    {
      /* pass raw part through for key extraction */
      plaintext = true;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(b))
    {
      encrypted_handler = crypt_pgp_application_handler;
      handler = encrypted_handler;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(b))
    {
      encrypted_handler = crypt_smime_application_handler;
      handler = encrypted_handler;
    }
  }

  /* only respect disposition == attachment if we're not
   * displaying from the attachment menu (i.e. pager) */
  if ((!C_HonorDisposition || ((b->disposition != DISP_ATTACH) || OptViewAttach)) &&
      (plaintext || handler))
  {
    /* Prevent encrypted attachments from being included in replies
     * unless $include_encrypted is set. */
    if ((s->flags & MUTT_REPLYING) && (s->flags & MUTT_FIRSTDONE) &&
        encrypted_handler && !C_IncludeEncrypted)
    {
      goto cleanup;
    }

    rc = run_decode_and_handler(b, s, handler, plaintext);
  }
  /* print hint to use attachment menu for disposition == attachment
   * if we're not already being called from there */
  else if (s->flags & MUTT_DISPLAY)
  {
    char keystroke[128] = { 0 };
    struct Buffer msg = mutt_buffer_make(256);

    if (!OptViewAttach)
    {
      if (km_expand_key(keystroke, sizeof(keystroke),
                        km_find_func(MENU_PAGER, OP_VIEW_ATTACHMENTS)))
      {
        if (C_HonorDisposition && (b->disposition == DISP_ATTACH))
        {
          /* L10N: %s expands to a keystroke/key binding, e.g. 'v'.  */
          mutt_buffer_printf(&msg, _("[-- This is an attachment (use '%s' to view this part) --]\n"),
                             keystroke);
        }
        else
        {
          /* L10N: %s/%s is a MIME type, e.g. "text/plain".
             The last %s expands to a keystroke/key binding, e.g. 'v'. */
          mutt_buffer_printf(&msg, _("[-- %s/%s is unsupported (use '%s' to view this part) --]\n"),
                             TYPE(b), b->subtype, keystroke);
        }
      }
      else
      {
        if (C_HonorDisposition && (b->disposition == DISP_ATTACH))
        {
          mutt_buffer_strcpy(&msg, _("[-- This is an attachment (need "
                                     "'view-attachments' bound to key) --]\n"));
        }
        else
        {
          /* L10N: %s/%s is a MIME type, e.g. "text/plain". */
          mutt_buffer_printf(&msg, _("[-- %s/%s is unsupported (need 'view-attachments' bound to key) --]\n"),
                             TYPE(b), b->subtype);
        }
      }
    }
    else
    {
      if (C_HonorDisposition && (b->disposition == DISP_ATTACH))
      {
        mutt_buffer_strcpy(&msg, _("[-- This is an attachment --]\n"));
      }
      else
      {
        /* L10N: %s/%s is a MIME type, e.g. "text/plain". */
        mutt_buffer_printf(&msg, _("[-- %s/%s is unsupported --]\n"), TYPE(b), b->subtype);
      }
    }
    state_mark_attach(s);
    state_printf(s, "%s", mutt_b2s(&msg));
    mutt_buffer_dealloc(&msg);
  }

cleanup:
  s->flags = oflags | (s->flags & MUTT_FIRSTDONE);
  if (rc != 0)
  {
    mutt_debug(LL_DEBUG1, "Bailing on attachment of type %s/%s\n", TYPE(b),
               NONULL(b->subtype));
  }

  return rc;
}

/**
 * mutt_can_decode - Will decoding the attachment produce any output
 * @param a Body of email to test
 * @retval true Decoding the attachment will produce output
 */
bool mutt_can_decode(struct Body *a)
{
  if (is_autoview(a))
    return true;
  if (a->type == TYPE_TEXT)
    return true;
  if (a->type == TYPE_MESSAGE)
    return true;
  if (a->type == TYPE_MULTIPART)
  {
    if (WithCrypto)
    {
      if ((mutt_str_strcasecmp(a->subtype, "signed") == 0) ||
          (mutt_str_strcasecmp(a->subtype, "encrypted") == 0))
      {
        return true;
      }
    }

    for (struct Body *b = a->parts; b; b = b->next)
    {
      if (mutt_can_decode(b))
        return true;
    }
  }
  else if ((WithCrypto != 0) && (a->type == TYPE_APPLICATION))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(a))
      return true;
    if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(a))
      return true;
  }

  return false;
}

/**
 * mutt_decode_attachment - Decode an email's attachment
 * @param b Body of the email
 * @param s State of text being processed
 */
void mutt_decode_attachment(struct Body *b, struct State *s)
{
  int istext = mutt_is_text_part(b);
  iconv_t cd = (iconv_t)(-1);

  if (istext && s->flags & MUTT_CHARCONV)
  {
    char *charset = mutt_param_get(&b->parameter, "charset");
    if (!charset && C_AssumedCharset)
      charset = mutt_ch_get_default_charset();
    if (charset && C_Charset)
      cd = mutt_ch_iconv_open(C_Charset, charset, MUTT_ICONV_HOOK_FROM);
  }
  else if (istext && b->charset)
    cd = mutt_ch_iconv_open(C_Charset, b->charset, MUTT_ICONV_HOOK_FROM);

  fseeko(s->fp_in, b->offset, SEEK_SET);
  switch (b->encoding)
  {
    case ENC_QUOTED_PRINTABLE:
      decode_quoted(s, b->length,
                    istext || (((WithCrypto & APPLICATION_PGP) != 0) &&
                               mutt_is_application_pgp(b)),
                    cd);
      break;
    case ENC_BASE64:
      mutt_decode_base64(s, b->length,
                         istext || (((WithCrypto & APPLICATION_PGP) != 0) &&
                                    mutt_is_application_pgp(b)),
                         cd);
      break;
    case ENC_UUENCODED:
      decode_uuencoded(s, b->length,
                       istext || (((WithCrypto & APPLICATION_PGP) != 0) &&
                                  mutt_is_application_pgp(b)),
                       cd);
      break;
    default:
      decode_xbit(s, b->length,
                  istext || (((WithCrypto & APPLICATION_PGP) != 0) &&
                             mutt_is_application_pgp(b)),
                  cd);
      break;
  }

  if (cd != (iconv_t)(-1))
    iconv_close(cd);
}
