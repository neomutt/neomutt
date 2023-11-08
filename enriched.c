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
 * @page neo_enriched Rich text handler
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
  const wchar_t *tag_name; ///< Tag name
  int index;               ///< Index number
};

/// EnrichedTags - Lookup table of tags allowed in enriched text
static const struct Etags EnrichedTags[] = {
  // clang-format off
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
  { NULL, -1 },
  // clang-format on
};

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
  struct State *state;
};

/**
 * enriched_wrap - Wrap enriched text
 * @param enriched State of enriched text
 */
static void enriched_wrap(struct EnrichedState *enriched)
{
  if (!enriched)
    return;

  int x;

  if (enriched->line_len)
  {
    if (enriched->tag_level[RICH_CENTER] || enriched->tag_level[RICH_FLUSHRIGHT])
    {
      /* Strip trailing white space */
      size_t y = enriched->line_used - 1;

      while (y && iswspace(enriched->line[y]))
      {
        enriched->line[y] = (wchar_t) '\0';
        y--;
        enriched->line_used--;
        enriched->line_len--;
      }
      if (enriched->tag_level[RICH_CENTER])
      {
        /* Strip leading whitespace */
        y = 0;

        while (enriched->line[y] && iswspace(enriched->line[y]))
          y++;
        if (y)
        {
          for (size_t z = y; z <= enriched->line_used; z++)
          {
            enriched->line[z - y] = enriched->line[z];
          }

          enriched->line_len -= y;
          enriched->line_used -= y;
        }
      }
    }

    const int extra = enriched->wrap_margin - enriched->line_len - enriched->indent_len -
                      (enriched->tag_level[RICH_INDENT_RIGHT] * INDENT_SIZE);
    if (extra > 0)
    {
      if (enriched->tag_level[RICH_CENTER])
      {
        x = extra / 2;
        while (x)
        {
          state_putc(enriched->state, ' ');
          x--;
        }
      }
      else if (enriched->tag_level[RICH_FLUSHRIGHT])
      {
        x = extra - 1;
        while (x)
        {
          state_putc(enriched->state, ' ');
          x--;
        }
      }
    }
    state_putws(enriched->state, (const wchar_t *) enriched->line);
  }

  state_putc(enriched->state, '\n');
  enriched->line[0] = (wchar_t) '\0';
  enriched->line_len = 0;
  enriched->line_used = 0;
  enriched->indent_len = 0;
  if (enriched->state->prefix)
  {
    state_puts(enriched->state, enriched->state->prefix);
    enriched->indent_len += mutt_str_len(enriched->state->prefix);
  }

  if (enriched->tag_level[RICH_EXCERPT])
  {
    x = enriched->tag_level[RICH_EXCERPT];
    while (x)
    {
      if (enriched->state->prefix)
      {
        state_puts(enriched->state, enriched->state->prefix);
        enriched->indent_len += mutt_str_len(enriched->state->prefix);
      }
      else
      {
        state_puts(enriched->state, "> ");
        enriched->indent_len += mutt_str_len("> ");
      }
      x--;
    }
  }
  else
  {
    enriched->indent_len = 0;
  }
  if (enriched->tag_level[RICH_INDENT])
  {
    x = enriched->tag_level[RICH_INDENT] * INDENT_SIZE;
    enriched->indent_len += x;
    while (x)
    {
      state_putc(enriched->state, ' ');
      x--;
    }
  }
}

/**
 * enriched_flush - Write enriched text to the State
 * @param enriched State of Enriched text
 * @param wrap     true if the text should be wrapped
 */
static void enriched_flush(struct EnrichedState *enriched, bool wrap)
{
  if (!enriched || !enriched->buffer)
    return;

  if (!enriched->tag_level[RICH_NOFILL] &&
      ((enriched->line_len + enriched->word_len) >
       (enriched->wrap_margin - (enriched->tag_level[RICH_INDENT_RIGHT] * INDENT_SIZE) -
        enriched->indent_len)))
  {
    enriched_wrap(enriched);
  }

  if (enriched->buf_used)
  {
    enriched->buffer[enriched->buf_used] = (wchar_t) '\0';
    enriched->line_used += enriched->buf_used;
    if (enriched->line_used > enriched->line_max)
    {
      enriched->line_max = enriched->line_used;
      mutt_mem_realloc(&enriched->line, (enriched->line_max + 1) * sizeof(wchar_t));
    }
    wcscat(enriched->line, enriched->buffer);
    enriched->line_len += enriched->word_len;
    enriched->word_len = 0;
    enriched->buf_used = 0;
  }
  if (wrap)
    enriched_wrap(enriched);
  fflush(enriched->state->fp_out);
}

/**
 * enriched_putwc - Write one wide character to the state
 * @param c    Character to write
 * @param enriched State of Enriched text
 */
static void enriched_putwc(wchar_t c, struct EnrichedState *enriched)
{
  if (!enriched)
    return;

  if (enriched->tag_level[RICH_PARAM])
  {
    if (enriched->tag_level[RICH_COLOR])
    {
      if ((enriched->param_used + 1) >= enriched->param_len)
        mutt_mem_realloc(&enriched->param, (enriched->param_len += 256) * sizeof(wchar_t));

      enriched->param[enriched->param_used++] = c;
    }
    return; /* nothing to do */
  }

  /* see if more space is needed (plus extra for possible rich characters) */
  if ((enriched->buf_len < (enriched->buf_used + 3)) || !enriched->buffer)
  {
    enriched->buf_len += 1024;
    mutt_mem_realloc(&enriched->buffer, (enriched->buf_len + 1) * sizeof(wchar_t));
  }

  if ((!enriched->tag_level[RICH_NOFILL] && iswspace(c)) || (c == (wchar_t) '\0'))
  {
    if (c == (wchar_t) '\t')
      enriched->word_len += 8 - (enriched->line_len + enriched->word_len) % 8;
    else
      enriched->word_len++;

    enriched->buffer[enriched->buf_used++] = c;
    enriched_flush(enriched, false);
  }
  else
  {
    if (enriched->state->flags & STATE_DISPLAY)
    {
      if (enriched->tag_level[RICH_BOLD])
      {
        enriched->buffer[enriched->buf_used++] = c;
        enriched->buffer[enriched->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        enriched->buffer[enriched->buf_used++] = c;
      }
      else if (enriched->tag_level[RICH_UNDERLINE])
      {
        enriched->buffer[enriched->buf_used++] = '_';
        enriched->buffer[enriched->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        enriched->buffer[enriched->buf_used++] = c;
      }
      else if (enriched->tag_level[RICH_ITALIC])
      {
        enriched->buffer[enriched->buf_used++] = c;
        enriched->buffer[enriched->buf_used++] = (wchar_t) '\010'; // Ctrl-H (backspace)
        enriched->buffer[enriched->buf_used++] = '_';
      }
      else
      {
        enriched->buffer[enriched->buf_used++] = c;
      }
    }
    else
    {
      enriched->buffer[enriched->buf_used++] = c;
    }
    enriched->word_len++;
  }
}

/**
 * enriched_puts - Write an enriched text string to the State
 * @param s        String to write
 * @param enriched State of Enriched text
 */
static void enriched_puts(const char *s, struct EnrichedState *enriched)
{
  if (!enriched)
    return;

  const char *c = NULL;

  if ((enriched->buf_len < (enriched->buf_used + mutt_str_len(s))) || !enriched->buffer)
  {
    enriched->buf_len += 1024;
    mutt_mem_realloc(&enriched->buffer, (enriched->buf_len + 1) * sizeof(wchar_t));
  }
  c = s;
  while (*c)
  {
    enriched->buffer[enriched->buf_used++] = (wchar_t) *c;
    c++;
  }
}

/**
 * enriched_set_flags - Set flags on the enriched text state
 * @param tag      Tag to set
 * @param enriched State of Enriched text
 */
static void enriched_set_flags(const wchar_t *tag, struct EnrichedState *enriched)
{
  if (!enriched)
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
      enriched_flush(enriched, true);

    if (*tag == (wchar_t) '/')
    {
      if (enriched->tag_level[j]) /* make sure not to go negative */
        enriched->tag_level[j]--;
      if ((enriched->state->flags & STATE_DISPLAY) && (j == RICH_PARAM) &&
          enriched->tag_level[RICH_COLOR])
      {
        enriched->param[enriched->param_used] = (wchar_t) '\0';
        if (wcscasecmp(L"black", enriched->param) == 0)
        {
          enriched_puts("\033[30m", enriched); // Escape
        }
        else if (wcscasecmp(L"red", enriched->param) == 0)
        {
          enriched_puts("\033[31m", enriched); // Escape
        }
        else if (wcscasecmp(L"green", enriched->param) == 0)
        {
          enriched_puts("\033[32m", enriched); // Escape
        }
        else if (wcscasecmp(L"yellow", enriched->param) == 0)
        {
          enriched_puts("\033[33m", enriched); // Escape
        }
        else if (wcscasecmp(L"blue", enriched->param) == 0)
        {
          enriched_puts("\033[34m", enriched); // Escape
        }
        else if (wcscasecmp(L"magenta", enriched->param) == 0)
        {
          enriched_puts("\033[35m", enriched); // Escape
        }
        else if (wcscasecmp(L"cyan", enriched->param) == 0)
        {
          enriched_puts("\033[36m", enriched); // Escape
        }
        else if (wcscasecmp(L"white", enriched->param) == 0)
        {
          enriched_puts("\033[37m", enriched); // Escape
        }
      }
      if ((enriched->state->flags & STATE_DISPLAY) && (j == RICH_COLOR))
      {
        enriched_puts("\033[0m", enriched); // Escape
      }

      /* flush parameter buffer when closing the tag */
      if (j == RICH_PARAM)
      {
        enriched->param_used = 0;
        enriched->param[0] = (wchar_t) '\0';
      }
    }
    else
    {
      enriched->tag_level[j]++;
    }

    if (j == RICH_EXCERPT)
      enriched_flush(enriched, true);
  }
}

/**
 * text_enriched_handler - Handler for enriched text - Implements ::handler_t - @ingroup handler_api
 * @retval 0 Always
 */
int text_enriched_handler(struct Body *b_email, struct State *state)
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
  } text_state = TEXT;

  long bytes = b_email->length;
  struct EnrichedState enriched = { 0 };
  wint_t wc = 0;
  int tag_len = 0;
  wchar_t tag[1024 + 1];

  enriched.state = state;
  enriched.wrap_margin = ((state->wraplen > 4) &&
                          ((state->flags & STATE_DISPLAY) || (state->wraplen < 76))) ?
                             state->wraplen - 4 :
                             72;
  enriched.line_max = enriched.wrap_margin * 4;
  enriched.line = mutt_mem_calloc((enriched.line_max + 1), sizeof(wchar_t));
  enriched.param = mutt_mem_calloc(256, sizeof(wchar_t));

  enriched.param_len = 256;
  enriched.param_used = 0;

  if (state->prefix)
  {
    state_puts(state, state->prefix);
    enriched.indent_len += mutt_str_len(state->prefix);
  }

  while (text_state != DONE)
  {
    if (text_state != ST_EOF)
    {
      if (!bytes || ((wc = fgetwc(state->fp_in)) == WEOF))
        text_state = ST_EOF;
      else
        bytes--;
    }

    switch (text_state)
    {
      case TEXT:
        switch (wc)
        {
          case '<':
            text_state = LANGLE;
            break;

          case '\n':
            if (enriched.tag_level[RICH_NOFILL])
            {
              enriched_flush(&enriched, true);
            }
            else
            {
              enriched_putwc((wchar_t) ' ', &enriched);
              text_state = NEWLINE;
            }
            break;

          default:
            enriched_putwc(wc, &enriched);
        }
        break;

      case LANGLE:
        if (wc == (wchar_t) '<')
        {
          enriched_putwc(wc, &enriched);
          text_state = TEXT;
          break;
        }
        else
        {
          tag_len = 0;
          text_state = TAG;
        }
        /* Yes, (it wasn't a <<, so this char is first in TAG) */
        FALLTHROUGH;

      case TAG:
        if (wc == (wchar_t) '>')
        {
          tag[tag_len] = (wchar_t) '\0';
          enriched_set_flags(tag, &enriched);
          text_state = TEXT;
        }
        else if (tag_len < 1024) /* ignore overly long tags */
        {
          tag[tag_len++] = wc;
        }
        else
        {
          text_state = BOGUS_TAG;
        }
        break;

      case BOGUS_TAG:
        if (wc == (wchar_t) '>')
          text_state = TEXT;
        break;

      case NEWLINE:
        if (wc == (wchar_t) '\n')
        {
          enriched_flush(&enriched, true);
        }
        else
        {
          ungetwc(wc, state->fp_in);
          bytes++;
          text_state = TEXT;
        }
        break;

      case ST_EOF:
        enriched_putwc((wchar_t) '\0', &enriched);
        enriched_flush(&enriched, true);
        text_state = DONE;
        break;

      default:
        /* not reached */
        break;
    }
  }

  state_putc(state, '\n'); /* add a final newline */

  FREE(&(enriched.buffer));
  FREE(&(enriched.line));
  FREE(&(enriched.param));

  return 0;
}
