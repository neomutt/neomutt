/**
 * @file
 * Rich text handler
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page enriched Rich text handler
 *
 * Rich text handler
 */

/**
 * A (not so) minimal implementation of RFC1563.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "enriched.h"
#include "state.h"

#define INDENT_SIZE 4

/**
 * enum RichAttribs - Rich text attributes
 */
enum RichAttribs
{
  RICH_PARAM = 0,    ///< Parameter label
  RICH_BOLD,         ///< Bold text
  RICH_UNDERLINE,    ///< Underlined text
  RICH_ITALIC,       ///< Italic text
  RICH_NOFILL,       ///< Text will not be reformatted
  RICH_INDENT,       ///< Indented text
  RICH_INDENT_RIGHT, ///< Right-indented text
  RICH_EXCERPT,      ///< Excerpt text
  RICH_CENTER,       ///< Centred text
  RICH_FLUSHLEFT,    ///< Left-justified text
  RICH_FLUSHRIGHT,   ///< Right-justified text
  RICH_COLOR,        ///< Coloured text
  RICH_MAX,
};

/**
 * struct Etags - Enriched text tags
 */
struct Etags
{
  const wchar_t *tag_name;
  int index;
};

// clang-format off
static const struct Etags EnrichedTags[] = {
  { L"param",       RICH_PARAM        },
  { L"bold",        RICH_BOLD         },
  { L"italic",      RICH_ITALIC       },
  { L"underline",   RICH_UNDERLINE    },
  { L"nofill",      RICH_NOFILL       },
  { L"excerpt",     RICH_EXCERPT      },
  { L"indent",      RICH_INDENT       },
  { L"indentright", RICH_INDENT_RIGHT },
  { L"center",      RICH_CENTER       },
  { L"flushleft",   RICH_FLUSHLEFT    },
  { L"flushright",  RICH_FLUSHRIGHT   },
  { L"flushboth",   RICH_FLUSHLEFT    },
  { L"color",       RICH_COLOR        },
  { L"x-color",     RICH_COLOR        },
  { NULL,           -1                },
};
// clang-format on

/**
 * struct EnrichedState - State of enriched-text parser
 */
struct EnrichedState
{
  wchar_t *buffer;
  wchar_t *line;
  wchar_t *param;
  size_t buf_len;
  size_t line_len;
  size_t line_used;
  size_t line_max;
  size_t indent_len;
  size_t word_len;
  size_t buf_used;
  size_t param_used;
  size_t param_len;
  int tag_level[RICH_MAX];
  int wrap_margin;
  struct State *s;
};

/**
 * enriched_wrap - Wrap enriched text
 * @param stte State of enriched text
 */
static void enriched_wrap(struct EnrichedState *stte)
{
  if (!stte)
    return;

  int x;

  if (stte->line_len)
  {
    if (stte->tag_level[RICH_CENTER] || stte->tag_level[RICH_FLUSHRIGHT])
    {
      /* Strip trailing white space */
      size_t y = stte->line_used - 1;

      while (y && iswspace(stte->line[y]))
      {
        stte->line[y] = (wchar_t) '\0';
        y--;
        stte->line_used--;
        stte->line_len--;
      }
      if (stte->tag_level[RICH_CENTER])
      {
        /* Strip leading whitespace */
        y = 0;

        while (stte->line[y] && iswspace(stte->line[y]))
          y++;
        if (y)
        {
          for (size_t z = y; z <= stte->line_used; z++)
          {
            stte->line[z - y] = stte->line[z];
          }

          stte->line_len -= y;
          stte->line_used -= y;
        }
      }
    }

    const int extra = stte->wrap_margin - stte->line_len - stte->indent_len -
                      (stte->tag_level[RICH_INDENT_RIGHT] * INDENT_SIZE);
    if (extra > 0)
    {
      if (stte->tag_level[RICH_CENTER])
      {
        x = extra / 2;
        while (x)
        {
          state_putc(stte->s, ' ');
          x--;
        }
      }
      else if (stte->tag_level[RICH_FLUSHRIGHT])
      {
        x = extra - 1;
        while (x)
        {
          state_putc(stte->s, ' ');
          x--;
        }
      }
    }
    state_putws(stte->s, (const wchar_t *) stte->line);
  }

  state_putc(stte->s, '\n');
  stte->line[0] = (wchar_t) '\0';
  stte->line_len = 0;
  stte->line_used = 0;
  stte->indent_len = 0;
  if (stte->s->prefix)
  {
    state_puts(stte->s, stte->s->prefix);
    stte->indent_len += mutt_str_len(stte->s->prefix);
  }

  if (stte->tag_level[RICH_EXCERPT])
  {
    x = stte->tag_level[RICH_EXCERPT];
    while (x)
    {
      if (stte->s->prefix)
      {
        state_puts(stte->s, stte->s->prefix);
        stte->indent_len += mutt_str_len(stte->s->prefix);
      }
      else
      {
        state_puts(stte->s, "> ");
        stte->indent_len += mutt_str_len("> ");
      }
      x--;
    }
  }
  else
    stte->indent_len = 0;
  if (stte->tag_level[RICH_INDENT])
  {
    x = stte->tag_level[RICH_INDENT] * INDENT_SIZE;
    stte->indent_len += x;
    while (x)
    {
      state_putc(stte->s, ' ');
      x--;
    }
  }
}

/**
 * enriched_flush - Write enriched text to the State
 * @param stte State of Enriched text
 * @param wrap true if the text should be wrapped
 */
static void enriched_flush(struct EnrichedState *stte, bool wrap)
{
  if (!stte || !stte->buffer)
    return;

  if (!stte->tag_level[RICH_NOFILL] &&
      ((stte->line_len + stte->word_len) >
       (stte->wrap_margin - (stte->tag_level[RICH_INDENT_RIGHT] * INDENT_SIZE) - stte->indent_len)))
  {
    enriched_wrap(stte);
  }

  if (stte->buf_used)
  {
    stte->buffer[stte->buf_used] = (wchar_t) '\0';
    stte->line_used += stte->buf_used;
    if (stte->line_used > stte->line_max)
    {
      stte->line_max = stte->line_used;
      mutt_mem_realloc(&stte->line, (stte->line_max + 1) * sizeof(wchar_t));
    }
    wcscat(stte->line, stte->buffer);
    stte->line_len += stte->word_len;
    stte->word_len = 0;
    stte->buf_used = 0;
  }
  if (wrap)
    enriched_wrap(stte);
  fflush(stte->s->fp_out);
}

/**
 * enriched_putwc - Write one wide character to the state
 * @param c    Character to write
 * @param stte State of Enriched text
 */
static void enriched_putwc(wchar_t c, struct EnrichedState *stte)
{
  if (!stte)
    return;

  if (stte->tag_level[RICH_PARAM])
  {
    if (stte->tag_level[RICH_COLOR])
    {
      if (stte->param_used + 1 >= stte->param_len)
        mutt_mem_realloc(&stte->param, (stte->param_len += 256) * sizeof(wchar_t));

      stte->param[stte->param_used++] = c;
    }
    return; /* nothing to do */
  }

  /* see if more space is needed (plus extra for possible rich characters) */
  if ((stte->buf_len < (stte->buf_used + 3)) || !stte->buffer)
  {
    stte->buf_len += 1024;
    mutt_mem_realloc(&stte->buffer, (stte->buf_len + 1) * sizeof(wchar_t));
  }

  if ((!stte->tag_level[RICH_NOFILL] && iswspace(c)) || (c == (wchar_t) '\0'))
  {
    if (c == (wchar_t) '\t')
      stte->word_len += 8 - (stte->line_len + stte->word_len) % 8;
    else
      stte->word_len++;

    stte->buffer[stte->buf_used++] = c;
    enriched_flush(stte, false);
  }
  else
  {
    if (stte->s->flags & MUTT_DISPLAY)
    {
      if (stte->tag_level[RICH_BOLD])
      {
        stte->buffer[stte->buf_used++] = c;
        stte->buffer[stte->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        stte->buffer[stte->buf_used++] = c;
      }
      else if (stte->tag_level[RICH_UNDERLINE])
      {
        stte->buffer[stte->buf_used++] = '_';
        stte->buffer[stte->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        stte->buffer[stte->buf_used++] = c;
      }
      else if (stte->tag_level[RICH_ITALIC])
      {
        stte->buffer[stte->buf_used++] = c;
        stte->buffer[stte->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        stte->buffer[stte->buf_used++] = '_';
      }
      else
      {
        stte->buffer[stte->buf_used++] = c;
      }
    }
    else
    {
      stte->buffer[stte->buf_used++] = c;
    }
    stte->word_len++;
  }
}

/**
 * enriched_puts - Write an enriched text string to the State
 * @param s    String to write
 * @param stte State of Enriched text
 */
static void enriched_puts(const char *s, struct EnrichedState *stte)
{
  if (!stte)
    return;

  const char *c = NULL;

  if ((stte->buf_len < (stte->buf_used + mutt_str_len(s))) || !stte->buffer)
  {
    stte->buf_len += 1024;
    mutt_mem_realloc(&stte->buffer, (stte->buf_len + 1) * sizeof(wchar_t));
  }
  c = s;
  while (*c)
  {
    stte->buffer[stte->buf_used++] = (wchar_t) *c;
    c++;
  }
}

/**
 * enriched_set_flags - Set flags on the enriched text state
 * @param tag  Tag to set
 * @param stte State of Enriched text
 */
static void enriched_set_flags(const wchar_t *tag, struct EnrichedState *stte)
{
  if (!stte)
    return;

  const wchar_t *tagptr = tag;
  int i, j;

  if (*tagptr == (wchar_t) '/')
    tagptr++;

  for (i = 0, j = -1; EnrichedTags[i].tag_name; i++)
  {
    if (wcscasecmp(EnrichedTags[i].tag_name, tagptr) == 0)
    {
      j = EnrichedTags[i].index;
      break;
    }
  }

  if (j != -1)
  {
    if ((j == RICH_CENTER) || (j == RICH_FLUSHLEFT) || (j == RICH_FLUSHRIGHT))
      enriched_flush(stte, true);

    if (*tag == (wchar_t) '/')
    {
      if (stte->tag_level[j]) /* make sure not to go negative */
        stte->tag_level[j]--;
      if ((stte->s->flags & MUTT_DISPLAY) && (j == RICH_PARAM) && stte->tag_level[RICH_COLOR])
      {
        stte->param[stte->param_used] = (wchar_t) '\0';
        if (wcscasecmp(L"black", stte->param) == 0)
        {
          enriched_puts("\033[30m", stte); // Escape
        }
        else if (wcscasecmp(L"red", stte->param) == 0)
        {
          enriched_puts("\033[31m", stte); // Escape
        }
        else if (wcscasecmp(L"green", stte->param) == 0)
        {
          enriched_puts("\033[32m", stte); // Escape
        }
        else if (wcscasecmp(L"yellow", stte->param) == 0)
        {
          enriched_puts("\033[33m", stte); // Escape
        }
        else if (wcscasecmp(L"blue", stte->param) == 0)
        {
          enriched_puts("\033[34m", stte); // Escape
        }
        else if (wcscasecmp(L"magenta", stte->param) == 0)
        {
          enriched_puts("\033[35m", stte); // Escape
        }
        else if (wcscasecmp(L"cyan", stte->param) == 0)
        {
          enriched_puts("\033[36m", stte); // Escape
        }
        else if (wcscasecmp(L"white", stte->param) == 0)
        {
          enriched_puts("\033[37m", stte); // Escape
        }
      }
      if ((stte->s->flags & MUTT_DISPLAY) && (j == RICH_COLOR))
      {
        enriched_puts("\033[0m", stte); // Escape
      }

      /* flush parameter buffer when closing the tag */
      if (j == RICH_PARAM)
      {
        stte->param_used = 0;
        stte->param[0] = (wchar_t) '\0';
      }
    }
    else
      stte->tag_level[j]++;

    if (j == RICH_EXCERPT)
      enriched_flush(stte, true);
  }
}

/**
 * text_enriched_handler - Handler for enriched text - Implements ::handler_t
 * @retval 0 Always
 */
int text_enriched_handler(struct Body *a, struct State *s)
{
  enum
  {
    TEXT,
    LANGLE,
    TAG,
    BOGUS_TAG,
    NEWLINE,
    ST_EOF,
    DONE
  } state = TEXT;

  long bytes = a->length;
  struct EnrichedState stte = { 0 };
  wint_t wc = 0;
  int tag_len = 0;
  wchar_t tag[1024 + 1];

  stte.s = s;
  stte.wrap_margin =
      ((s->wraplen > 4) && ((s->flags & MUTT_DISPLAY) || (s->wraplen < 76))) ?
          s->wraplen - 4 :
          72;
  stte.line_max = stte.wrap_margin * 4;
  stte.line = mutt_mem_calloc((stte.line_max + 1), sizeof(wchar_t));
  stte.param = mutt_mem_calloc(256, sizeof(wchar_t));

  stte.param_len = 256;
  stte.param_used = 0;

  if (s->prefix)
  {
    state_puts(s, s->prefix);
    stte.indent_len += mutt_str_len(s->prefix);
  }

  while (state != DONE)
  {
    if (state != ST_EOF)
    {
      if (!bytes || ((wc = fgetwc(s->fp_in)) == WEOF))
        state = ST_EOF;
      else
        bytes--;
    }

    switch (state)
    {
      case TEXT:
        switch (wc)
        {
          case '<':
            state = LANGLE;
            break;

          case '\n':
            if (stte.tag_level[RICH_NOFILL])
            {
              enriched_flush(&stte, true);
            }
            else
            {
              enriched_putwc((wchar_t) ' ', &stte);
              state = NEWLINE;
            }
            break;

          default:
            enriched_putwc(wc, &stte);
        }
        break;

      case LANGLE:
        if (wc == (wchar_t) '<')
        {
          enriched_putwc(wc, &stte);
          state = TEXT;
          break;
        }
        else
        {
          tag_len = 0;
          state = TAG;
        }
      /* Yes, (it wasn't a <<, so this char is first in TAG) */
      /* fallthrough */
      case TAG:
        if (wc == (wchar_t) '>')
        {
          tag[tag_len] = (wchar_t) '\0';
          enriched_set_flags(tag, &stte);
          state = TEXT;
        }
        else if (tag_len < 1024) /* ignore overly long tags */
          tag[tag_len++] = wc;
        else
          state = BOGUS_TAG;
        break;

      case BOGUS_TAG:
        if (wc == (wchar_t) '>')
          state = TEXT;
        break;

      case NEWLINE:
        if (wc == (wchar_t) '\n')
          enriched_flush(&stte, true);
        else
        {
          ungetwc(wc, s->fp_in);
          bytes++;
          state = TEXT;
        }
        break;

      case ST_EOF:
        enriched_putwc((wchar_t) '\0', &stte);
        enriched_flush(&stte, true);
        state = DONE;
        break;

      case DONE:
        /* not reached */
        break;
    }
  }

  state_putc(s, '\n'); /* add a final newline */

  FREE(&(stte.buffer));
  FREE(&(stte.line));
  FREE(&(stte.param));

  return 0;
}
