/**
 * @file
 * Pager Display
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_display Pager Display
 *
 * Parse and Display pager data
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "display.h"
#include "lib.h"
#include "color/lib.h"
#include "protos.h"

/**
 * check_sig - Check for an email signature
 * @param s      Text to examine
 * @param info   Line info array to update
 * @param offset An offset line to start the check from
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_sig(const char *s, struct Line *info, int offset)
{
  const unsigned int NUM_SIG_LINES = 4; // The amount of lines a signature takes
  unsigned int count = 0;

  while ((offset > 0) && (count <= NUM_SIG_LINES))
  {
    if (info[offset].color != MT_COLOR_SIGNATURE)
      break;
    count++;
    offset--;
  }

  if (count == 0)
    return -1;

  if (count > NUM_SIG_LINES)
  {
    /* check for a blank line */
    while (*s)
    {
      if (!IS_SPACE(*s))
        return 0;
      s++;
    }

    return -1;
  }

  return 0;
}

/**
 * comp_syntax_t - Search for a Syntax using bsearch(3)
 * @param m1 Search key
 * @param m2 Array member
 * @retval -1 m1 precedes m2
 * @retval  0 m1 matches m2
 * @retval  1 m2 precedes m1
 */
static int comp_syntax_t(const void *m1, const void *m2)
{
  const int *cnt = (const int *) m1;
  const struct TextSyntax *stx = (const struct TextSyntax *) m2;

  if (*cnt < stx->first)
    return -1;
  if (*cnt >= stx->last)
    return 1;
  return 0;
}

/**
 * resolve_color - Set the colour for a line of text
 * @param win       Window
 * @param lines     Line info array
 * @param line_num  Line Number (index into lines)
 * @param cnt       If true, this is a continuation line
 * @param flags     Flags, see #PagerFlags
 * @param special   Flags, e.g. A_BOLD
 * @param aa        ANSI attributes
 */
static void resolve_color(struct MuttWindow *win, struct Line *lines, int line_num,
                          int cnt, PagerFlags flags, int special, struct AnsiAttr *aa)
{
  int def_color;         /* color without syntax highlight */
  int color;             /* final color */
  static int last_color; /* last color set */
  bool search = false;
  int m;
  struct TextSyntax *matching_chunk = NULL;

  if (cnt == 0)
    last_color = -1;

  if (lines[line_num].cont_line)
  {
    const bool c_markers = cs_subset_bool(NeoMutt->sub, "markers");
    if (!cnt && c_markers)
    {
      mutt_curses_set_color_by_id(MT_COLOR_MARKERS);
      mutt_window_addch(win, '+');
      last_color = simple_colors_get(MT_COLOR_MARKERS);
    }
    m = (lines[line_num].syntax)[0].first;
    cnt += (lines[line_num].syntax)[0].last;
  }
  else
    m = line_num;
  if (flags & MUTT_PAGER_LOGS)
  {
    def_color = simple_colors_get(lines[line_num].syntax[0].color);
  }
  else if (!(flags & MUTT_SHOWCOLOR))
    def_color = simple_colors_get(MT_COLOR_NORMAL);
  else if (lines[m].color == MT_COLOR_HEADER)
    def_color = lines[m].syntax[0].color;
  else
    def_color = simple_colors_get(lines[m].color);

  if ((flags & MUTT_SHOWCOLOR) && (lines[m].color == MT_COLOR_QUOTED))
  {
    struct QuoteStyle *qc = lines[m].quote;

    if (qc)
    {
      def_color = qc->color;

      while (qc && (qc->prefix_len > cnt))
      {
        def_color = qc->color;
        qc = qc->up;
      }
    }
  }

  color = def_color;
  if ((flags & MUTT_SHOWCOLOR) && lines[m].syntax_arr_size)
  {
    matching_chunk = bsearch(&cnt, lines[m].syntax, lines[m].syntax_arr_size,
                             sizeof(struct TextSyntax), comp_syntax_t);
    if (matching_chunk && (cnt >= matching_chunk->first) &&
        (cnt < matching_chunk->last))
    {
      color = matching_chunk->color;
    }
  }

  if ((flags & MUTT_SEARCH) && lines[m].search_arr_size)
  {
    matching_chunk = bsearch(&cnt, lines[m].search, lines[m].search_arr_size,
                             sizeof(struct TextSyntax), comp_syntax_t);
    if (matching_chunk && (cnt >= matching_chunk->first) &&
        (cnt < matching_chunk->last))
    {
      color = simple_colors_get(MT_COLOR_SEARCH);
      search = 1;
    }
  }

  /* handle "special" bold & underlined characters */
  if (special || aa->attr)
  {
    if ((aa->attr & ANSI_COLOR))
    {
      if (aa->pair == -1)
        aa->pair = mutt_color_alloc(aa->fg, aa->bg);
      color = aa->pair;
      if (aa->attr & ANSI_BOLD)
        color |= A_BOLD;
    }
    else if ((special & A_BOLD) || (aa->attr & ANSI_BOLD))
    {
      if (simple_color_is_set(MT_COLOR_BOLD) && !search)
        color = simple_colors_get(MT_COLOR_BOLD);
      else
        color ^= A_BOLD;
    }
    if ((special & A_UNDERLINE) || (aa->attr & ANSI_UNDERLINE))
    {
      if (simple_color_is_set(MT_COLOR_UNDERLINE) && !search)
        color = simple_colors_get(MT_COLOR_UNDERLINE);
      else
        color ^= A_UNDERLINE;
    }
    else if (aa->attr & ANSI_REVERSE)
    {
      color ^= A_REVERSE;
    }
    else if (aa->attr & ANSI_BLINK)
    {
      color ^= A_BLINK;
    }
    else if (aa->attr == ANSI_OFF)
    {
      aa->attr = 0;
    }
  }

  if (color != last_color)
  {
    mutt_curses_set_attr(color);
    last_color = color;
  }
}

/**
 * append_line - Add a new Line to the array
 * @param lines    Array of Line info
 * @param line_num Line number to add
 * @param cnt      Offset of the overflow if line is a continuation
 */
static void append_line(struct Line *lines, int line_num, int cnt)
{
  int m;

  lines[line_num + 1].color = lines[line_num].color;
  (lines[line_num + 1].syntax)[0].color = (lines[line_num].syntax)[0].color;
  lines[line_num + 1].cont_line = 1;

  /* find the real start of the line */
  for (m = line_num; m >= 0; m--)
    if (lines[m].cont_line == 0)
      break;

  (lines[line_num + 1].syntax)[0].first = m;
  (lines[line_num + 1].syntax)[0].last =
      (lines[line_num].cont_line) ? cnt + (lines[line_num].syntax)[0].last : cnt;
}

/**
 * check_marker - Check that the unique marker is present
 * @param q Marker string
 * @param p String to check
 * @retval num Offset of marker
 */
static int check_marker(const char *q, const char *p)
{
  for (; (p[0] == q[0]) && (q[0] != '\0') && (p[0] != '\0') && (q[0] != '\a') &&
         (p[0] != '\a');
       p++, q++)
  {
  }

  return (int) (*p - *q);
}

/**
 * check_attachment_marker - Check that the unique marker is present
 * @param p String to check
 * @retval num Offset of marker
 */
static int check_attachment_marker(const char *p)
{
  return check_marker(state_attachment_marker(), p);
}

/**
 * check_protected_header_marker - Check that the unique marker is present
 * @param p String to check
 * @retval num Offset of marker
 */
static int check_protected_header_marker(const char *p)
{
  return check_marker(state_protected_header_marker(), p);
}

/**
 * mutt_is_quote_line - Is a line of message text a quote?
 * @param[in]  line   Line to test
 * @param[out] pmatch Regex sub-matches
 * @retval true Line is quoted
 *
 * Checks if line matches the `$quote_regex` and doesn't match `$smileys`.
 * This is used by the pager for calling qstyle_classify.
 */
int mutt_is_quote_line(char *line, regmatch_t *pmatch)
{
  bool is_quote = false;
  const struct Regex *c_smileys = cs_subset_regex(NeoMutt->sub, "smileys");
  regmatch_t pmatch_internal[1], smatch[1];

  if (!pmatch)
    pmatch = pmatch_internal;

  const struct Regex *c_quote_regex =
      cs_subset_regex(NeoMutt->sub, "quote_regex");
  if (mutt_regex_capture(c_quote_regex, line, 1, pmatch))
  {
    if (mutt_regex_capture(c_smileys, line, 1, smatch))
    {
      if (smatch[0].rm_so > 0)
      {
        char c = line[smatch[0].rm_so];
        line[smatch[0].rm_so] = 0;

        if (mutt_regex_capture(c_quote_regex, line, 1, pmatch))
          is_quote = true;

        line[smatch[0].rm_so] = c;
      }
    }
    else
      is_quote = true;
  }

  return is_quote;
}

/**
 * resolve_types - Determine the style for a line of text
 * @param[in]  win          Window
 * @param[in]  buf          Formatted text
 * @param[in]  raw          Raw text
 * @param[in]  lines        Line info array
 * @param[in]  line_num     Line number (index into lines)
 * @param[in]  lines_used   Last line
 * @param[out] quote_list   List of quote colours
 * @param[out] q_level      Quote level
 * @param[out] force_redraw Set to true if a screen redraw is needed
 * @param[in]  q_classify   If true, style the text
 */
static void resolve_types(struct MuttWindow *win, char *buf, char *raw,
                          struct Line *lines, int line_num, int lines_used,
                          struct QuoteStyle **quote_list, int *q_level,
                          bool *force_redraw, bool q_classify)
{
  struct RegexColor *color_line = NULL;
  struct RegexColorList *head = NULL;
  regmatch_t pmatch[1];
  bool found;
  bool null_rx;
  const bool c_header_color_partial =
      cs_subset_bool(NeoMutt->sub, "header_color_partial");
  int offset, i = 0;

  if ((line_num == 0) || simple_color_is_header(lines[line_num - 1].color) ||
      (check_protected_header_marker(raw) == 0))
  {
    if (buf[0] == '\n') /* end of header */
    {
      lines[line_num].color = MT_COLOR_NORMAL;
      mutt_window_get_coords(win, &braille_col, &braille_row);
    }
    else
    {
      /* if this is a continuation of the previous line, use the previous
       * line's color as default. */
      if ((line_num > 0) && ((buf[0] == ' ') || (buf[0] == '\t')))
      {
        lines[line_num].color = lines[line_num - 1].color; /* wrapped line */
        if (!c_header_color_partial)
        {
          (lines[line_num].syntax)[0].color = (lines[line_num - 1].syntax)[0].color;
          lines[line_num].cont_header = 1;
        }
      }
      else
      {
        lines[line_num].color = MT_COLOR_HDRDEFAULT;
      }

      /* When this option is unset, we color the entire header the
       * same color.  Otherwise, we handle the header patterns just
       * like body patterns (further below).  */
      if (!c_header_color_partial)
      {
        STAILQ_FOREACH(color_line, regex_colors_get_list(MT_COLOR_HEADER), entries)
        {
          if (regexec(&color_line->regex, buf, 0, NULL, 0) == 0)
          {
            lines[line_num].color = MT_COLOR_HEADER;
            lines[line_num].syntax[0].color = color_line->pair;
            if (lines[line_num].cont_header)
            {
              /* adjust the previous continuation lines to reflect the color of this continuation line */
              int j;
              for (j = line_num - 1; j >= 0 && lines[j].cont_header; --j)
              {
                lines[j].color = lines[line_num].color;
                lines[j].syntax[0].color = lines[line_num].syntax[0].color;
              }
              /* now adjust the first line of this header field */
              if (j >= 0)
              {
                lines[j].color = lines[line_num].color;
                lines[j].syntax[0].color = lines[line_num].syntax[0].color;
              }
              *force_redraw = true; /* the previous lines have already been drawn on the screen */
            }
            break;
          }
        }
      }
    }
  }
  else if (mutt_str_startswith(raw, "\033[0m")) // Escape: a little hack...
    lines[line_num].color = MT_COLOR_NORMAL;
  else if (check_attachment_marker((char *) raw) == 0)
    lines[line_num].color = MT_COLOR_ATTACHMENT;
  else if (mutt_str_equal("-- \n", buf) || mutt_str_equal("-- \r\n", buf))
  {
    i = line_num + 1;

    lines[line_num].color = MT_COLOR_SIGNATURE;
    while ((i < lines_used) && (check_sig(buf, lines, i - 1) == 0) &&
           ((lines[i].color == MT_COLOR_NORMAL) || (lines[i].color == MT_COLOR_QUOTED) ||
            (lines[i].color == MT_COLOR_HEADER)))
    {
      /* oops... */
      if (lines[i].syntax_arr_size)
      {
        lines[i].syntax_arr_size = 0;
        mutt_mem_realloc(&(lines[line_num].syntax), sizeof(struct TextSyntax));
      }
      lines[i++].color = MT_COLOR_SIGNATURE;
    }
  }
  else if (check_sig(buf, lines, line_num - 1) == 0)
    lines[line_num].color = MT_COLOR_SIGNATURE;
  else if (mutt_is_quote_line(buf, pmatch))

  {
    if (q_classify && (lines[line_num].quote == NULL))
    {
      lines[line_num].quote = qstyle_classify(quote_list, buf + pmatch[0].rm_so,
                                              pmatch[0].rm_eo - pmatch[0].rm_so,
                                              force_redraw, q_level);
    }
    lines[line_num].color = MT_COLOR_QUOTED;
  }
  else
    lines[line_num].color = MT_COLOR_NORMAL;

  /* body patterns */
  if ((lines[line_num].color == MT_COLOR_NORMAL) || (lines[line_num].color == MT_COLOR_QUOTED) ||
      ((lines[line_num].color == MT_COLOR_HDRDEFAULT) && c_header_color_partial))
  {
    size_t nl;

    /* don't consider line endings part of the buffer
     * for regex matching */
    nl = mutt_str_len(buf);
    if ((nl > 0) && (buf[nl - 1] == '\n'))
      buf[nl - 1] = '\0';

    i = 0;
    offset = 0;
    lines[line_num].syntax_arr_size = 0;
    if (lines[line_num].color == MT_COLOR_HDRDEFAULT)
      head = regex_colors_get_list(MT_COLOR_HEADER);
    else
      head = regex_colors_get_list(MT_COLOR_BODY);
    STAILQ_FOREACH(color_line, head, entries)
    {
      color_line->stop_matching = false;
    }
    do

    {
      if (!buf[offset])
        break;

      found = false;
      null_rx = false;
      STAILQ_FOREACH(color_line, head, entries)
      {
        if (!color_line->stop_matching &&
            (regexec(&color_line->regex, buf + offset, 1, pmatch,
                     ((offset != 0) ? REG_NOTBOL : 0)) == 0))
        {
          if (pmatch[0].rm_eo == pmatch[0].rm_so)
          {
            null_rx = true; /* empty regex; don't add it, but keep looking */
            continue;
          }

          if (!found)
          {
            /* Abort if we fill up chunks.
              * Yes, this really happened. */
            if (lines[line_num].syntax_arr_size == SHRT_MAX)
            {
              null_rx = false;
              break;
            }
            if (++(lines[line_num].syntax_arr_size) > 1)
            {
              mutt_mem_realloc(&(lines[line_num].syntax),
                               (lines[line_num].syntax_arr_size) * sizeof(struct TextSyntax));
            }
          }
          i = lines[line_num].syntax_arr_size - 1;
          pmatch[0].rm_so += offset;
          pmatch[0].rm_eo += offset;
          if (!found || (pmatch[0].rm_so < (lines[line_num].syntax)[i].first) ||
              ((pmatch[0].rm_so == (lines[line_num].syntax)[i].first) &&
               (pmatch[0].rm_eo > (lines[line_num].syntax)[i].last)))
          {
            (lines[line_num].syntax)[i].color = color_line->pair;
            (lines[line_num].syntax)[i].first = pmatch[0].rm_so;
            (lines[line_num].syntax)[i].last = pmatch[0].rm_eo;
          }
          found = true;
          null_rx = false;
        }
        else
        {
          /* Once a regexp fails to match, don't try matching it again.
           * On very long lines this can cause a performance issue if there
           * are other regexps that have many matches. */
          color_line->stop_matching = true;
        }
      }

      if (null_rx)
        offset++; /* avoid degenerate cases */
      else
        offset = (lines[line_num].syntax)[i].last;
    } while (found || null_rx);
    if (nl > 0)
      buf[nl] = '\n';
  }

  /* attachment patterns */
  if (lines[line_num].color == MT_COLOR_ATTACHMENT)
  {
    size_t nl;

    /* don't consider line endings part of the buffer for regex matching */
    nl = mutt_str_len(buf);
    if ((nl > 0) && (buf[nl - 1] == '\n'))
      buf[nl - 1] = '\0';

    i = 0;
    offset = 0;
    lines[line_num].syntax_arr_size = 0;
    do
    {
      if (!buf[offset])
        break;

      found = false;
      null_rx = false;
      STAILQ_FOREACH(color_line, regex_colors_get_list(MT_COLOR_ATTACH_HEADERS), entries)
      {
        if (regexec(&color_line->regex, buf + offset, 1, pmatch,
                    ((offset != 0) ? REG_NOTBOL : 0)) != 0)
        {
          continue;
        }

        if (pmatch[0].rm_eo != pmatch[0].rm_so)
        {
          if (!found)
          {
            if (++(lines[line_num].syntax_arr_size) > 1)
            {
              mutt_mem_realloc(&(lines[line_num].syntax),
                               (lines[line_num].syntax_arr_size) * sizeof(struct TextSyntax));
            }
          }
          i = lines[line_num].syntax_arr_size - 1;
          pmatch[0].rm_so += offset;
          pmatch[0].rm_eo += offset;
          if (!found || (pmatch[0].rm_so < (lines[line_num].syntax)[i].first) ||
              ((pmatch[0].rm_so == (lines[line_num].syntax)[i].first) &&
               (pmatch[0].rm_eo > (lines[line_num].syntax)[i].last)))
          {
            (lines[line_num].syntax)[i].color = color_line->pair;
            (lines[line_num].syntax)[i].first = pmatch[0].rm_so;
            (lines[line_num].syntax)[i].last = pmatch[0].rm_eo;
          }
          found = 1;
          null_rx = false;
        }
        else
          null_rx = true; /* empty regex; don't add it, but keep looking */
      }

      if (null_rx)
        offset++; /* avoid degenerate cases */
      else
        offset = (lines[line_num].syntax)[i].last;
    } while (found || null_rx);
    if (nl > 0)
      buf[nl] = '\n';
  }
}

/**
 * is_ansi - Is this an ANSI escape sequence?
 * @param str String to test
 * @retval true It's an ANSI escape sequence
 */
static bool is_ansi(const char *str)
{
  while (*str && (isdigit(*str) || (*str == ';')))
    str++;
  return (*str == 'm');
}

/**
 * grok_ansi - Parse an ANSI escape sequence
 * @param buf String to parse
 * @param pos Starting position in string
 * @param aa  AnsiAttr for the result
 * @retval num Index of first character after the escape sequence
 */
static int grok_ansi(const unsigned char *buf, int pos, struct AnsiAttr *aa)
{
  int x = pos;

  while (isdigit(buf[x]) || (buf[x] == ';'))
    x++;

  /* Character Attributes */
  const bool c_allow_ansi = cs_subset_bool(NeoMutt->sub, "allow_ansi");
  if (c_allow_ansi && aa && (buf[x] == 'm'))
  {
    if (pos == x)
    {
      if (aa->pair != -1)
        mutt_color_free(aa->fg, aa->bg);
      aa->attr = ANSI_OFF;
      aa->pair = -1;
    }
    while (pos < x)
    {
      if ((buf[pos] == '1') && (((pos + 1) == x) || (buf[pos + 1] == ';')))
      {
        aa->attr |= ANSI_BOLD;
        pos += 2;
      }
      else if ((buf[pos] == '4') && (((pos + 1) == x) || (buf[pos + 1] == ';')))
      {
        aa->attr |= ANSI_UNDERLINE;
        pos += 2;
      }
      else if ((buf[pos] == '5') && (((pos + 1) == x) || (buf[pos + 1] == ';')))
      {
        aa->attr |= ANSI_BLINK;
        pos += 2;
      }
      else if ((buf[pos] == '7') && (((pos + 1) == x) || (buf[pos + 1] == ';')))
      {
        aa->attr |= ANSI_REVERSE;
        pos += 2;
      }
      else if ((buf[pos] == '0') && (((pos + 1) == x) || (buf[pos + 1] == ';')))
      {
        if (aa->pair != -1)
          mutt_color_free(aa->fg, aa->bg);
        aa->attr = ANSI_OFF;
        aa->pair = -1;
        pos += 2;
      }
      else if ((buf[pos] == '3') && isdigit(buf[pos + 1]))
      {
        if (aa->pair != -1)
          mutt_color_free(aa->fg, aa->bg);
        aa->pair = -1;
        aa->attr |= ANSI_COLOR;
        aa->fg = buf[pos + 1] - '0';
        pos += 3;
      }
      else if ((buf[pos] == '4') && isdigit(buf[pos + 1]))
      {
        if (aa->pair != -1)
          mutt_color_free(aa->fg, aa->bg);
        aa->pair = -1;
        aa->attr |= ANSI_COLOR;
        aa->bg = buf[pos + 1] - '0';
        pos += 3;
      }
      else
      {
        while ((pos < x) && (buf[pos] != ';'))
          pos++;
        pos++;
      }
    }
  }
  pos = x;
  return pos;
}

/**
 * mutt_buffer_strip_formatting - Removes ANSI and backspace formatting
 * @param dest Buffer for the result
 * @param src  String to strip
 * @param strip_markers Remove
 *
 * Removes ANSI and backspace formatting, and optionally markers.
 * This is separated out so that it can be used both by the pager
 * and the autoview handler.
 *
 * This logic is pulled from the pager fill_buffer() function, for use
 * in stripping reply-quoted autoview output of ansi sequences.
 */
void mutt_buffer_strip_formatting(struct Buffer *dest, const char *src, bool strip_markers)
{
  const char *s = src;

  mutt_buffer_reset(dest);

  if (!s)
    return;

  while (s[0] != '\0')
  {
    if ((s[0] == '\010') && (s > src))
    {
      if (s[1] == '_') /* underline */
        s += 2;
      else if (s[1] && mutt_buffer_len(dest)) /* bold or overstrike */
      {
        dest->dptr--;
        mutt_buffer_addch(dest, s[1]);
        s += 2;
      }
      else /* ^H */
        mutt_buffer_addch(dest, *s++);
    }
    else if ((s[0] == '\033') && (s[1] == '[') && is_ansi(s + 2))
    {
      while (*s++ != 'm')
        ; /* skip ANSI sequence */
    }
    else if (strip_markers && (s[0] == '\033') && (s[1] == ']') &&
             ((check_attachment_marker(s) == 0) || (check_protected_header_marker(s) == 0)))
    {
      mutt_debug(LL_DEBUG2, "Seen attachment marker\n");
      while (*s++ != '\a')
        ; /* skip pseudo-ANSI sequence */
    }
    else
      mutt_buffer_addch(dest, *s++);
  }
}

/**
 * fill_buffer - Fill a buffer from a file
 * @param[in]     fp         File to read from
 * @param[in,out] bytes_read End of last read
 * @param[in]     offset     Position start reading from
 * @param[out]    buf        Buffer to fill
 * @param[out]    fmt        Copy of buffer, stripped of attributes
 * @param[out]    blen       Length of the buffer
 * @param[in,out] buf_ready  true if the buffer already has data in it
 * @retval >=0 Bytes read
 * @retval -1  Error
 */
static int fill_buffer(FILE *fp, LOFF_T *bytes_read, LOFF_T offset, unsigned char **buf,
                       unsigned char **fmt, size_t *blen, int *buf_ready)
{
  static int b_read;
  struct Buffer stripped;

  if (*buf_ready == 0)
  {
    if (offset != *bytes_read)
    {
      if (fseeko(fp, offset, SEEK_SET) != 0)
      {
        mutt_perror("fseeko");
        return -1;
      }
    }

    *buf = (unsigned char *) mutt_file_read_line((char *) *buf, blen, fp, NULL, MUTT_RL_EOL);
    if (!*buf)
    {
      fmt[0] = NULL;
      return -1;
    }

    *bytes_read = ftello(fp);
    b_read = (int) (*bytes_read - offset);
    *buf_ready = 1;

    mutt_buffer_init(&stripped);
    mutt_buffer_alloc(&stripped, *blen);
    mutt_buffer_strip_formatting(&stripped, (const char *) *buf, 1);
    /* This should be a noop, because *fmt should be NULL */
    FREE(fmt);
    *fmt = (unsigned char *) stripped.data;
  }

  return b_read;
}

/**
 * format_line - Display a line of text in the pager
 * @param[in]  win       Window
 * @param[out] lines     Line info
 * @param[in]  line_num  Line number (index into lines)
 * @param[in]  buf       Text to display
 * @param[in]  flags     Flags, see #PagerFlags
 * @param[out] aa        ANSI attributes used
 * @param[in]  cnt       Length of text buffer
 * @param[out] pspace    Index of last whitespace character
 * @param[out] pvch      Number of bytes read
 * @param[out] pcol      Number of columns used
 * @param[out] pspecial  Attribute flags, e.g. A_UNDERLINE
 * @param[in]  width     Width of screen (to wrap to)
 * @retval num Number of characters displayed
 */
static int format_line(struct MuttWindow *win, struct Line **lines, int line_num,
                       unsigned char *buf, PagerFlags flags, struct AnsiAttr *aa,
                       int cnt, int *pspace, int *pvch, int *pcol, int *pspecial, int width)
{
  int space = -1; /* index of the last space or TAB */
  const bool c_markers = cs_subset_bool(NeoMutt->sub, "markers");
  size_t col = c_markers ? (*lines)[line_num].cont_line : 0;
  size_t k;
  int ch, vch, last_special = -1, special = 0, t;
  wchar_t wc;
  mbstate_t mbstate;
  const size_t c_wrap = cs_subset_number(NeoMutt->sub, "wrap");
  size_t wrap_cols = mutt_window_wrap_cols(width, (flags & MUTT_PAGER_NOWRAP) ? 0 : c_wrap);

  if (check_attachment_marker((char *) buf) == 0)
    wrap_cols = width;

  /* FIXME: this should come from lines */
  memset(&mbstate, 0, sizeof(mbstate));

  for (ch = 0, vch = 0; ch < cnt; ch += k, vch += k)
  {
    /* Handle ANSI sequences */
    while ((cnt - ch >= 2) && (buf[ch] == '\033') && (buf[ch + 1] == '[') && // Escape
           is_ansi((char *) buf + ch + 2))
    {
      ch = grok_ansi(buf, ch + 2, aa) + 1;
    }

    while ((cnt - ch >= 2) && (buf[ch] == '\033') && (buf[ch + 1] == ']') && // Escape
           ((check_attachment_marker((char *) buf + ch) == 0) ||
            (check_protected_header_marker((char *) buf + ch) == 0)))
    {
      while (buf[ch++] != '\a')
        if (ch >= cnt)
          break;
    }

    /* is anything left to do? */
    if (ch >= cnt)
      break;

    k = mbrtowc(&wc, (char *) buf + ch, cnt - ch, &mbstate);
    if ((k == (size_t) (-2)) || (k == (size_t) (-1)))
    {
      if (k == (size_t) (-1))
        memset(&mbstate, 0, sizeof(mbstate));
      mutt_debug(LL_DEBUG1, "mbrtowc returned %lu; errno = %d\n", k, errno);
      if (col + 4 > wrap_cols)
        break;
      col += 4;
      if (aa)
        mutt_window_printf(win, "\\%03o", buf[ch]);
      k = 1;
      continue;
    }
    if (k == 0)
      k = 1;

    if (CharsetIsUtf8)
    {
      /* zero width space, zero width no-break space */
      if ((wc == 0x200B) || (wc == 0xFEFF))
      {
        mutt_debug(LL_DEBUG3, "skip zero-width character U+%04X\n", (unsigned short) wc);
        continue;
      }
      if (mutt_mb_is_display_corrupting_utf8(wc))
      {
        mutt_debug(LL_DEBUG3, "filtered U+%04X\n", (unsigned short) wc);
        continue;
      }
    }

    /* Handle backspace */
    special = 0;
    if (IsWPrint(wc))
    {
      wchar_t wc1;
      mbstate_t mbstate1 = mbstate;
      size_t k1 = mbrtowc(&wc1, (char *) buf + ch + k, cnt - ch - k, &mbstate1);
      while ((k1 != (size_t) (-2)) && (k1 != (size_t) (-1)) && (k1 > 0) && (wc1 == '\b'))
      {
        const size_t k2 =
            mbrtowc(&wc1, (char *) buf + ch + k + k1, cnt - ch - k - k1, &mbstate1);
        if ((k2 == (size_t) (-2)) || (k2 == (size_t) (-1)) || (k2 == 0) || (!IsWPrint(wc1)))
          break;

        if (wc == wc1)
        {
          special |= ((wc == '_') && (special & A_UNDERLINE)) ? A_UNDERLINE : A_BOLD;
        }
        else if ((wc == '_') || (wc1 == '_'))
        {
          special |= A_UNDERLINE;
          wc = (wc1 == '_') ? wc : wc1;
        }
        else
        {
          /* special = 0; / * overstrike: nothing to do! */
          wc = wc1;
        }

        ch += k + k1;
        k = k2;
        mbstate = mbstate1;
        k1 = mbrtowc(&wc1, (char *) buf + ch + k, cnt - ch - k, &mbstate1);
      }
    }

    if (aa && ((flags & (MUTT_SHOWCOLOR | MUTT_SEARCH | MUTT_PAGER_MARKER)) ||
               special || last_special || aa->attr))
    {
      resolve_color(win, *lines, line_num, vch, flags, special, aa);
      last_special = special;
    }

    /* no-break space, narrow no-break space */
    if (IsWPrint(wc) || (CharsetIsUtf8 && ((wc == 0x00A0) || (wc == 0x202F))))
    {
      if (wc == ' ')
      {
        space = ch;
      }
      t = wcwidth(wc);
      if (col + t > wrap_cols)
        break;
      col += t;
      if (aa)
        mutt_addwch(win, wc);
    }
    else if (wc == '\n')
      break;
    else if (wc == '\t')
    {
      space = ch;
      t = (col & ~7) + 8;
      if (t > wrap_cols)
        break;
      if (aa)
        for (; col < t; col++)
          mutt_window_addch(win, ' ');
      else
        col = t;
    }
    else if ((wc < 0x20) || (wc == 0x7f))
    {
      if (col + 2 > wrap_cols)
        break;
      col += 2;
      if (aa)
        mutt_window_printf(win, "^%c", ('@' + wc) & 0x7f);
    }
    else if (wc < 0x100)
    {
      if (col + 4 > wrap_cols)
        break;
      col += 4;
      if (aa)
        mutt_window_printf(win, "\\%03o", wc);
    }
    else
    {
      if (col + 1 > wrap_cols)
        break;
      col += k;
      if (aa)
        mutt_addwch(win, ReplacementChar);
    }
  }
  *pspace = space;
  *pcol = col;
  *pvch = vch;
  *pspecial = special;
  return ch;
}

/**
 * display_line - Print a line on screen
 * @param[in]  fp              File to read from
 * @param[out] bytes_read      Offset into file
 * @param[out] lines           Line attributes
 * @param[in]  line_num        Line number
 * @param[out] lines_used      Last line
 * @param[out] lines_max       Maximum number of lines
 * @param[in]  flags           Flags, see #PagerFlags
 * @param[out] quote_list      Email quoting style
 * @param[out] q_level         Level of quoting
 * @param[out] force_redraw    Force a repaint
 * @param[out] search_re       Regex to highlight
 * @param[in]  win_pager       Window to draw into
 * @retval -1 EOF was reached
 * @retval 0  normal exit, line was not displayed
 * @retval >0 normal exit, line was displayed
 */
int display_line(FILE *fp, LOFF_T *bytes_read, struct Line **lines,
                 int line_num, int *lines_used, int *lines_max, PagerFlags flags,
                 struct QuoteStyle **quote_list, int *q_level, bool *force_redraw,
                 regex_t *search_re, struct MuttWindow *win_pager)
{
  unsigned char *buf = NULL, *fmt = NULL;
  size_t buflen = 0;
  unsigned char *buf_ptr = NULL;
  int ch, vch, col, cnt, b_read;
  int buf_ready = 0;
  bool change_last = false;
  int special;
  int offset;
  int def_color;
  int m;
  int rc = -1;
  struct AnsiAttr aa = { 0, 0, 0, -1 };
  regmatch_t pmatch[1];

  if (line_num == *lines_used)
  {
    (*lines_used)++;
    change_last = true;
  }

  if (*lines_used == *lines_max)
  {
    mutt_mem_realloc(lines, sizeof(struct Line) * (*lines_max += LINES));
    for (ch = *lines_used; ch < *lines_max; ch++)
    {
      memset(&((*lines)[ch]), 0, sizeof(struct Line));
      (*lines)[ch].color = -1;
      (*lines)[ch].search_arr_size = -1;
      (*lines)[ch].syntax = mutt_mem_malloc(sizeof(struct TextSyntax));
      ((*lines)[ch].syntax)[0].first = -1;
      ((*lines)[ch].syntax)[0].last = -1;
    }
  }

  struct Line *const curr_line = &(*lines)[line_num];

  if (flags & MUTT_PAGER_LOGS)
  {
    /* determine the line class */
    if (fill_buffer(fp, bytes_read, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*lines_used)--;
      goto out;
    }

    curr_line->color = MT_COLOR_MESSAGE_LOG;
    if (buf[11] == 'M')
      curr_line->syntax[0].color = MT_COLOR_MESSAGE;
    else if (buf[11] == 'W')
      curr_line->syntax[0].color = MT_COLOR_WARNING;
    else if (buf[11] == 'E')
      curr_line->syntax[0].color = MT_COLOR_ERROR;
    else
      curr_line->syntax[0].color = MT_COLOR_NORMAL;
  }

  /* only do color highlighting if we are viewing a message */
  if (flags & (MUTT_SHOWCOLOR | MUTT_TYPES))
  {
    if (curr_line->color == -1)
    {
      /* determine the line class */
      if (fill_buffer(fp, bytes_read, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
      {
        if (change_last)
          (*lines_used)--;
        goto out;
      }

      resolve_types(win_pager, (char *) fmt, (char *) buf, *lines, line_num, *lines_used,
                    quote_list, q_level, force_redraw, flags & MUTT_SHOWCOLOR);

      /* avoid race condition for continuation lines when scrolling up */
      for (m = line_num + 1;
           m < *lines_used && (*lines)[m].offset && (*lines)[m].cont_line; m++)
        (*lines)[m].color = curr_line->color;
    }

    /* this also prevents searching through the hidden lines */
    const short c_toggle_quoted_show_levels =
        cs_subset_number(NeoMutt->sub, "toggle_quoted_show_levels");
    if ((flags & MUTT_HIDE) && (curr_line->color == MT_COLOR_QUOTED) &&
        ((curr_line->quote == NULL) || (curr_line->quote->quote_n >= c_toggle_quoted_show_levels)))
    {
      flags = 0; /* MUTT_NOSHOW */
    }
  }

  /* At this point, (*lines[line_num]).quote may still be undefined. We
   * don't want to compute it every time MUTT_TYPES is set, since this
   * would slow down the "bottom" function unacceptably. A compromise
   * solution is hence to call regexec() again, just to find out the
   * length of the quote prefix.  */
  if ((flags & MUTT_SHOWCOLOR) && !curr_line->cont_line &&
      (curr_line->color == MT_COLOR_QUOTED) && !curr_line->quote)
  {
    if (fill_buffer(fp, bytes_read, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*lines_used)--;
      goto out;
    }

    const struct Regex *c_quote_regex =
        cs_subset_regex(NeoMutt->sub, "quote_regex");
    if (mutt_regex_capture(c_quote_regex, (char *) fmt, 1, pmatch))
    {
      curr_line->quote =
          qstyle_classify(quote_list, (char *) fmt + pmatch[0].rm_so,
                          pmatch[0].rm_eo - pmatch[0].rm_so, force_redraw, q_level);
    }
    else
    {
      goto out;
    }
  }

  if ((flags & MUTT_SEARCH) && !curr_line->cont_line && (curr_line->search_arr_size == -1))
  {
    if (fill_buffer(fp, bytes_read, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*lines_used)--;
      goto out;
    }

    offset = 0;
    curr_line->search_arr_size = 0;
    while (regexec(search_re, (char *) fmt + offset, 1, pmatch,
                   (offset ? REG_NOTBOL : 0)) == 0)
    {
      if (++(curr_line->search_arr_size) > 1)
      {
        mutt_mem_realloc(&(curr_line->search),
                         (curr_line->search_arr_size) * sizeof(struct TextSyntax));
      }
      else
        curr_line->search = mutt_mem_malloc(sizeof(struct TextSyntax));
      pmatch[0].rm_so += offset;
      pmatch[0].rm_eo += offset;
      (curr_line->search)[curr_line->search_arr_size - 1].first = pmatch[0].rm_so;
      (curr_line->search)[curr_line->search_arr_size - 1].last = pmatch[0].rm_eo;

      if (pmatch[0].rm_eo == pmatch[0].rm_so)
        offset++; /* avoid degenerate cases */
      else
        offset = pmatch[0].rm_eo;
      if (!fmt[offset])
        break;
    }
  }

  if (!(flags & MUTT_SHOW) && ((*lines)[line_num + 1].offset > 0))
  {
    /* we've already scanned this line, so just exit */
    rc = 0;
    goto out;
  }
  if ((flags & MUTT_SHOWCOLOR) && *force_redraw && ((*lines)[line_num + 1].offset > 0))
  {
    /* no need to try to display this line... */
    rc = 1;
    goto out; /* fake display */
  }

  b_read = fill_buffer(fp, bytes_read, curr_line->offset, &buf, &fmt, &buflen, &buf_ready);
  if (b_read < 0)
  {
    if (change_last)
      (*lines_used)--;
    goto out;
  }

  /* now chose a good place to break the line */
  cnt = format_line(win_pager, lines, line_num, buf, flags, NULL, b_read, &ch,
                    &vch, &col, &special, win_pager->state.cols);
  buf_ptr = buf + cnt;

  /* move the break point only if smart_wrap is set */
  const bool c_smart_wrap = cs_subset_bool(NeoMutt->sub, "smart_wrap");
  if (c_smart_wrap)
  {
    if ((cnt < b_read) && (ch != -1) &&
        !simple_color_is_header(curr_line->color) && !IS_SPACE(buf[cnt]))
    {
      buf_ptr = buf + ch;
      /* skip trailing blanks */
      while (ch && ((buf[ch] == ' ') || (buf[ch] == '\t') || (buf[ch] == '\r')))
        ch--;
      /* A very long word with leading spaces causes infinite
       * wrapping when MUTT_PAGER_NSKIP is set.  A folded header
       * with a single long word shouldn't be smartwrapped
       * either.  So just disable smart_wrap if it would wrap at the
       * beginning of the line. */
      if (ch == 0)
        buf_ptr = buf + cnt;
      else
        cnt = ch + 1;
    }
    if (!(flags & MUTT_PAGER_NSKIP))
    {
      /* skip leading blanks on the next line too */
      while ((*buf_ptr == ' ') || (*buf_ptr == '\t'))
        buf_ptr++;
    }
  }

  if (*buf_ptr == '\r')
    buf_ptr++;
  if (*buf_ptr == '\n')
    buf_ptr++;

  if (((int) (buf_ptr - buf) < b_read) && !(*lines)[line_num + 1].cont_line)
    append_line(*lines, line_num, (int) (buf_ptr - buf));
  (*lines)[line_num + 1].offset = curr_line->offset + (long) (buf_ptr - buf);

  /* if we don't need to display the line we are done */
  if (!(flags & MUTT_SHOW))
  {
    rc = 0;
    goto out;
  }

  /* display the line */
  format_line(win_pager, lines, line_num, buf, flags, &aa, cnt, &ch, &vch, &col,
              &special, win_pager->state.cols);

  /* avoid a bug in ncurses... */
  if (col == 0)
  {
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    mutt_window_addch(win_pager, ' ');
  }

  /* end the last color pattern (needed by S-Lang) */
  if (special || ((col != win_pager->state.cols) && (flags & (MUTT_SHOWCOLOR | MUTT_SEARCH))))
    resolve_color(win_pager, *lines, line_num, vch, flags, 0, &aa);

  /* Fill the blank space at the end of the line with the prevailing color.
   * ncurses does an implicit clrtoeol() when you do mutt_window_addch('\n') so we have
   * to make sure to reset the color *after* that */
  if (flags & MUTT_SHOWCOLOR)
  {
    m = (curr_line->cont_line) ? (curr_line->syntax)[0].first : line_num;
    if ((*lines)[m].color == MT_COLOR_HEADER)
      def_color = ((*lines)[m].syntax)[0].color;
    else
      def_color = simple_colors_get((*lines)[m].color);

    mutt_curses_set_attr(def_color);
  }

  if (col < win_pager->state.cols)
    mutt_window_clrtoeol(win_pager);

  /* reset the color back to normal.  This *must* come after the
   * clrtoeol, otherwise the color for this line will not be
   * filled to the right margin.  */
  if (flags & MUTT_SHOWCOLOR)
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  /* build a return code */
  if (!(flags & MUTT_SHOW))
    flags = 0;

  rc = flags;

out:
  FREE(&buf);
  FREE(&fmt);
  return rc;
}
