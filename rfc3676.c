/**
 * @file
 * RFC3676 Format Flowed routines
 *
 * @authors
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005-2009 Rocco Rutte <pdmef@gmx.net>
 * Copyright (C) 2010 Michael R. Elkins <me@mutt.org>
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
 * @page neo_rfc3676 RFC3676 Format Flowed routines
 *
 * RFC3676 Format Flowed routines
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "rfc3676.h"

#define FLOWED_MAX 72

/**
 * struct FlowedState - State of a Format-Flowed line of text
 */
struct FlowedState
{
  size_t width;
  size_t spaces;
  bool delsp;
};

/**
 * get_quote_level - Get the quote level of a line
 * @param line Text to examine
 * @retval num Quote level
 */
static int get_quote_level(const char *line)
{
  int quoted = 0;
  const char *p = line;

  while (p && (*p == '>'))
  {
    quoted++;
    p++;
  }

  return quoted;
}

/**
 * space_quotes - Should we add spaces between quote levels
 * @param state State to use
 * @retval true Spaces should be added
 *
 * Determines whether to add spacing between/after each quote level:
 * `   >>>foo`
 * becomes
 * `   > > > foo`
 */
static int space_quotes(struct State *state)
{
  /* Allow quote spacing in the pager even for `$text_flowed`,
   * but obviously not when replying.  */
  const bool c_text_flowed = cs_subset_bool(NeoMutt->sub, "text_flowed");
  if (c_text_flowed && (state->flags & STATE_REPLYING))
    return 0;

  const bool c_reflow_space_quotes = cs_subset_bool(NeoMutt->sub, "reflow_space_quotes");
  return c_reflow_space_quotes;
}

/**
 * add_quote_suffix - Should we add a trailing space to quotes
 * @param state State to use
 * @param ql    Quote level
 * @retval true Spaces should be added
 *
 * Determines whether to add a trailing space to quotes:
 * `   >>> foo`
 * as opposed to
 * `   >>>foo`
 */
static bool add_quote_suffix(struct State *state, int ql)
{
  if (state->flags & STATE_REPLYING)
    return false;

  if (space_quotes(state))
    return false;

  if (!ql && !state->prefix)
    return false;

  /* The prefix will add its own space */
  const bool c_text_flowed = cs_subset_bool(NeoMutt->sub, "text_flowed");
  if (!c_text_flowed && !ql && state->prefix)
    return false;

  return true;
}

/**
 * print_indent - Print indented text
 * @param ql         Quote level
 * @param state      State to work with
 * @param add_suffix If true, write a trailing space character
 * @retval num Number of characters written
 */
static size_t print_indent(int ql, struct State *state, int add_suffix)
{
  size_t wid = 0;

  if (state->prefix)
  {
    /* use given prefix only for format=fixed replies to format=flowed,
     * for format=flowed replies to format=flowed, use '>' indentation */
    const bool c_text_flowed = cs_subset_bool(NeoMutt->sub, "text_flowed");
    if (c_text_flowed)
    {
      ql++;
    }
    else
    {
      state_puts(state, state->prefix);
      wid = mutt_strwidth(state->prefix);
    }
  }
  for (int i = 0; i < ql; i++)
  {
    state_putc(state, '>');
    if (space_quotes(state))
      state_putc(state, ' ');
  }
  if (add_suffix)
    state_putc(state, ' ');

  if (space_quotes(state))
    ql *= 2;

  return ql + add_suffix + wid;
}

/**
 * flush_par - Write out the paragraph
 * @param state State to work with
 * @param fst   State of the flowed text
 */
static void flush_par(struct State *state, struct FlowedState *fst)
{
  if (fst->width > 0)
  {
    state_putc(state, '\n');
    fst->width = 0;
  }
  fst->spaces = 0;
}

/**
 * quote_width - Calculate the paragraph width based upon the quote level
 * @param state State to use
 * @param ql    Quote level
 * @retval num Paragraph width
 *
 * The start of a quoted line will be ">>> ", so we need to subtract the space
 * required for the prefix from the terminal width.
 */
static int quote_width(struct State *state, int ql)
{
  const int screen_width = (state->flags & STATE_DISPLAY) ? state->wraplen : 80;
  const short c_reflow_wrap = cs_subset_number(NeoMutt->sub, "reflow_wrap");
  int width = mutt_window_wrap_cols(screen_width, c_reflow_wrap);
  const bool c_text_flowed = cs_subset_bool(NeoMutt->sub, "text_flowed");
  if (c_text_flowed && (state->flags & STATE_REPLYING))
  {
    /* When replying, force a wrap at FLOWED_MAX to comply with RFC3676
     * guidelines */
    if (width > FLOWED_MAX)
      width = FLOWED_MAX;
    ql++; /* When replying, we will add an additional quote level */
  }
  /* adjust the paragraph width subtracting the number of prefix chars */
  width -= space_quotes(state) ? ql * 2 : ql;
  /* When displaying (not replying), there may be a space between the prefix
   * string and the paragraph */
  if (add_quote_suffix(state, ql))
    width--;
  /* failsafe for really long quotes */
  if (width <= 0)
    width = FLOWED_MAX; /* arbitrary, since the line will wrap */
  return width;
}

/**
 * print_flowed_line - Print a format-flowed line
 * @param line  Text to print
 * @param state State to work with
 * @param ql    Quote level
 * @param fst   State of the flowed text
 * @param term  If true, terminate with a new line
 */
static void print_flowed_line(char *line, struct State *state, int ql,
                              struct FlowedState *fst, bool term)
{
  size_t width, w, words = 0;
  char *p = NULL;
  char last;

  if (!line || (*line == '\0'))
  {
    /* flush current paragraph (if any) first */
    flush_par(state, fst);
    print_indent(ql, state, 0);
    state_putc(state, '\n');
    return;
  }

  width = quote_width(state, ql);
  last = line[mutt_str_len(line) - 1];

  mutt_debug(LL_DEBUG5, "f=f: line [%s], width = %ld, spaces = %zu\n", line,
             (long) width, fst->spaces);

  for (words = 0; (p = mutt_str_sep(&line, " "));)
  {
    mutt_debug(LL_DEBUG5, "f=f: word [%s], width: %zu, remaining = [%s]\n", p,
               fst->width, line);

    /* remember number of spaces */
    if (*p == '\0')
    {
      mutt_debug(LL_DEBUG3, "f=f: additional space\n");
      fst->spaces++;
      continue;
    }
    /* there's exactly one space prior to every but the first word */
    if (words)
      fst->spaces++;

    w = mutt_strwidth(p);
    /* see if we need to break the line but make sure the first word is put on
     * the line regardless; if for DelSp=yes only one trailing space is used,
     * we probably have a long word that we should break within (we leave that
     * up to the pager or user) */
    if (!(!fst->spaces && fst->delsp && (last != ' ')) && (w < width) &&
        (w + fst->width + fst->spaces > width))
    {
      mutt_debug(LL_DEBUG3, "f=f: break line at %zu, %zu spaces left\n",
                 fst->width, fst->spaces);
      /* only honor trailing spaces for format=flowed replies */
      const bool c_text_flowed = cs_subset_bool(NeoMutt->sub, "text_flowed");
      if (c_text_flowed)
        for (; fst->spaces; fst->spaces--)
          state_putc(state, ' ');
      state_putc(state, '\n');
      fst->width = 0;
      fst->spaces = 0;
      words = 0;
    }

    if (!words && !fst->width)
      fst->width = print_indent(ql, state, add_quote_suffix(state, ql));
    fst->width += w + fst->spaces;
    for (; fst->spaces; fst->spaces--)
      state_putc(state, ' ');
    state_puts(state, p);
    words++;
  }

  if (term)
    flush_par(state, fst);
}

/**
 * print_fixed_line - Print a fixed format line
 * @param line  Text to print
 * @param state State to work with
 * @param ql    Quote level
 * @param fst   State of the flowed text
 */
static void print_fixed_line(const char *line, struct State *state, int ql,
                             struct FlowedState *fst)
{
  print_indent(ql, state, add_quote_suffix(state, ql));
  if (line && *line)
    state_puts(state, line);
  state_putc(state, '\n');

  fst->width = 0;
  fst->spaces = 0;
}

/**
 * rfc3676_handler - Handler for format=flowed - Implements ::handler_t - @ingroup handler_api
 * @retval 0 Always
 */
int rfc3676_handler(struct Body *b_email, struct State *state)
{
  char *buf = NULL;
  unsigned int quotelevel = 0;
  bool delsp = false;
  size_t sz = 0;
  struct FlowedState fst = { 0 };

  /* respect DelSp of RFC3676 only with f=f parts */
  char *t = mutt_param_get(&b_email->parameter, "delsp");
  if (t)
  {
    delsp = mutt_istr_equal(t, "yes");
    t = NULL;
    fst.delsp = true;
  }

  mutt_debug(LL_DEBUG3, "f=f: DelSp: %s\n", delsp ? "yes" : "no");

  while ((buf = mutt_file_read_line(buf, &sz, state->fp_in, NULL, MUTT_RL_NO_FLAGS)))
  {
    const size_t buf_len = mutt_str_len(buf);
    const unsigned int newql = get_quote_level(buf);

    /* end flowed paragraph (if we're within one) if quoting level
     * changes (should not but can happen, see RFC3676, sec. 4.5.) */
    if (newql != quotelevel)
      flush_par(state, &fst);

    quotelevel = newql;
    int buf_off = newql;

    /* respect sender's space-stuffing by removing one leading space */
    if (buf[buf_off] == ' ')
      buf_off++;

    /* test for signature separator */
    const unsigned int sigsep = mutt_str_equal(buf + buf_off, "-- ");

    /* a fixed line either has no trailing space or is the
     * signature separator */
    const bool fixed = (buf_len == buf_off) || (buf[buf_len - 1] != ' ') || sigsep;

    /* print fixed-and-standalone, fixed-and-empty and sigsep lines as
     * fixed lines */
    if ((fixed && ((fst.width == 0) || (buf_len == 0))) || sigsep)
    {
      /* if we're within a flowed paragraph, terminate it */
      flush_par(state, &fst);
      print_fixed_line(buf + buf_off, state, quotelevel, &fst);
      continue;
    }

    /* for DelSp=yes, we need to strip one SP prior to CRLF on flowed lines */
    if (delsp && !fixed)
      buf[buf_len - 1] = '\0';

    print_flowed_line(buf + buf_off, state, quotelevel, &fst, fixed);
  }

  flush_par(state, &fst);

  FREE(&buf);
  return 0;
}

/**
 * mutt_rfc3676_is_format_flowed - Is the Email "format-flowed"?
 * @param b Email Body to examine
 * @retval true Email is "format-flowed"
 */
bool mutt_rfc3676_is_format_flowed(struct Body *b)
{
  if (b && (b->type == TYPE_TEXT) && mutt_istr_equal("plain", b->subtype))
  {
    const char *format = mutt_param_get(&b->parameter, "format");
    if (mutt_istr_equal("flowed", format))
      return true;
  }

  return false;
}

/**
 * rfc3676_space_stuff - Perform required RFC3676 space stuffing
 * @param filename Attachment file
 * @param unstuff  If true, remove space stuffing
 *
 * Space stuffing means that we have to add leading spaces to
 * certain lines:
 *   - lines starting with a space
 *   - lines starting with 'From '
 *
 * Care is taken to preserve the e->body->filename, as
 * mutt -i -E can directly edit a passed in filename.
 */
static void rfc3676_space_stuff(const char *filename, bool unstuff)
{
  FILE *fp_out = NULL;
  char *buf = NULL;
  size_t blen = 0;

  struct Buffer *tmpfile = buf_pool_get();

  FILE *fp_in = mutt_file_fopen(filename, "r");
  if (!fp_in)
    goto bail;

  buf_mktemp(tmpfile);
  fp_out = mutt_file_fopen(buf_string(tmpfile), "w+");
  if (!fp_out)
    goto bail;

  while ((buf = mutt_file_read_line(buf, &blen, fp_in, NULL, MUTT_RL_NO_FLAGS)) != NULL)
  {
    if (unstuff)
    {
      if (buf[0] == ' ')
        fputs(buf + 1, fp_out);
      else
        fputs(buf, fp_out);
    }
    else
    {
      if ((buf[0] == ' ') || mutt_str_startswith(buf, "From "))
        fputc(' ', fp_out);
      fputs(buf, fp_out);
    }
    fputc('\n', fp_out);
  }
  FREE(&buf);
  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);
  mutt_file_set_mtime(filename, buf_string(tmpfile));

  fp_in = mutt_file_fopen(buf_string(tmpfile), "r");
  if (!fp_in)
    goto bail;

  if ((truncate(filename, 0) == -1) || ((fp_out = mutt_file_fopen(filename, "a")) == NULL))
  {
    mutt_perror("%s", filename);
    goto bail;
  }

  mutt_file_copy_stream(fp_in, fp_out);
  mutt_file_set_mtime(buf_string(tmpfile), filename);
  unlink(buf_string(tmpfile));

bail:
  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);
  buf_pool_release(&tmpfile);
}

/**
 * mutt_rfc3676_space_stuff - Perform RFC3676 space stuffing on an Email
 * @param e Email
 *
 * @note We don't check the option `$text_flowed` because we want to stuff based
 *       the actual content type.  The option only decides whether to *set*
 *       format=flowed on new messages.
 */
void mutt_rfc3676_space_stuff(struct Email *e)
{
  if (!e || !e->body || !e->body->filename)
    return;

  if (mutt_rfc3676_is_format_flowed(e->body))
    rfc3676_space_stuff(e->body->filename, false);
}

/**
 * mutt_rfc3676_space_unstuff - Remove RFC3676 space stuffing
 * @param e Email
 */
void mutt_rfc3676_space_unstuff(struct Email *e)
{
  if (!e || !e->body || !e->body->filename)
    return;

  if (mutt_rfc3676_is_format_flowed(e->body))
    rfc3676_space_stuff(e->body->filename, true);
}

/**
 * mutt_rfc3676_space_unstuff_attachment - Unstuff attachments
 * @param b        Email Body (OPTIONAL)
 * @param filename Attachment file
 *
 * This routine is used when saving/piping/viewing rfc3676 attachments.
 *
 * If b is provided, the function will verify that the Email is format-flowed.
 * The filename will be unstuffed, not b->filename or b->fp.
 */
void mutt_rfc3676_space_unstuff_attachment(struct Body *b, const char *filename)
{
  if (!filename)
    return;

  if (b && !mutt_rfc3676_is_format_flowed(b))
    return;

  rfc3676_space_stuff(filename, true);
}

/**
 * mutt_rfc3676_space_stuff_attachment - Stuff attachments
 * @param b        Email Body (OPTIONAL)
 * @param filename Attachment file
 *
 * This routine is used when filtering rfc3676 attachments.
 *
 * If b is provided, the function will verify that the Email is format-flowed.
 * The filename will be unstuffed, not b->filename or b->fp.
 */
void mutt_rfc3676_space_stuff_attachment(struct Body *b, const char *filename)
{
  if (!filename)
    return;

  if (b && !mutt_rfc3676_is_format_flowed(b))
    return;

  rfc3676_space_stuff(filename, false);
}
