/**
 * @file
 * General purpose object for storing and parsing strings
 *
 * @authors
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "buffer.h"
#include "filter.h"
#include "lib.h"
#include "myvar.h"

/**
 * mutt_buffer_new - creates and initializes a Buffer
 */
struct Buffer *mutt_buffer_new(void)
{
  struct Buffer *b = NULL;

  b = safe_malloc(sizeof(struct Buffer));

  mutt_buffer_init(b);

  return b;
}

/**
 * mutt_buffer_init - initialize a new Buffer
 */
struct Buffer *mutt_buffer_init(struct Buffer *b)
{
  memset(b, 0, sizeof(struct Buffer));
  return b;
}

/**
 * mutt_buffer_from - Create Buffer from an existing Buffer
 *
 * Creates and initializes a Buffer*. If passed an existing Buffer*,
 * just initializes. Frees anything already in the buffer. Copies in
 * the seed string.
 *
 * Disregards the 'destroy' flag, which seems reserved for caller.
 * This is bad, but there's no apparent protocol for it.
 */
struct Buffer *mutt_buffer_from(char *seed)
{
  struct Buffer *b = NULL;

  if (!seed)
    return NULL;

  b = mutt_buffer_new();
  b->data = safe_strdup(seed);
  b->dsize = mutt_strlen(seed);
  b->dptr = (char *) b->data + b->dsize;
  return b;
}

void mutt_buffer_free(struct Buffer **p)
{
  if (!p || !*p)
    return;

  FREE(&(*p)->data);
  /* dptr is just an offset to data and shouldn't be freed */
  FREE(p);
}

int mutt_buffer_printf(struct Buffer *buf, const char *fmt, ...)
{
  va_list ap, ap_retry;
  int len, blen, doff;

  va_start(ap, fmt);
  va_copy(ap_retry, ap);

  if (!buf->dptr)
    buf->dptr = buf->data;

  doff = buf->dptr - buf->data;
  blen = buf->dsize - doff;
  /* solaris 9 vsnprintf barfs when blen is 0 */
  if (!blen)
  {
    blen = 128;
    buf->dsize += blen;
    safe_realloc(&buf->data, buf->dsize);
    buf->dptr = buf->data + doff;
  }
  if ((len = vsnprintf(buf->dptr, blen, fmt, ap)) >= blen)
  {
    blen = ++len - blen;
    if (blen < 128)
      blen = 128;
    buf->dsize += blen;
    safe_realloc(&buf->data, buf->dsize);
    buf->dptr = buf->data + doff;
    len = vsnprintf(buf->dptr, len, fmt, ap_retry);
  }
  if (len > 0)
    buf->dptr += len;

  va_end(ap);
  va_end(ap_retry);

  return len;
}

/**
 * mutt_buffer_add - Add a string to a Buffer, expanding it if necessary
 *
 * dynamically grows a Buffer to accommodate s, in increments of 128 bytes.
 * Always one byte bigger than necessary for the null terminator, and the
 * buffer is always null-terminated
 */
static void mutt_buffer_add(struct Buffer *buf, const char *s, size_t len)
{
  if (!buf || !s)
    return;

  if ((buf->dptr + len + 1) > (buf->data + buf->dsize))
  {
    size_t offset = buf->dptr - buf->data;
    buf->dsize += (len < 128) ? 128 : len + 1;
    safe_realloc(&buf->data, buf->dsize);
    buf->dptr = buf->data + offset;
  }
  if (!buf->dptr)
    return;
  memcpy(buf->dptr, s, len);
  buf->dptr += len;
  *(buf->dptr) = '\0';
}

void mutt_buffer_addstr(struct Buffer *buf, const char *s)
{
  mutt_buffer_add(buf, s, mutt_strlen(s));
}

void mutt_buffer_addch(struct Buffer *buf, char c)
{
  mutt_buffer_add(buf, &c, 1);
}

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, int flags)
{
  if (!dest || !tok)
    return -1;

  char ch;
  char qc = 0; /* quote char */
  char *pc = NULL;

  /* reset the destination pointer to the beginning of the buffer */
  dest->dptr = dest->data;

  SKIPWS(tok->dptr);
  while ((ch = *tok->dptr))
  {
    if (!qc)
    {
      if ((ISSPACE(ch) && !(flags & MUTT_TOKEN_SPACE)) ||
          (ch == '#' && !(flags & MUTT_TOKEN_COMMENT)) ||
          (ch == '=' && (flags & MUTT_TOKEN_EQUAL)) ||
          (ch == ';' && !(flags & MUTT_TOKEN_SEMICOLON)) ||
          ((flags & MUTT_TOKEN_PATTERN) && strchr("~%=!|", ch)))
        break;
    }

    tok->dptr++;

    if (ch == qc)
      qc = 0; /* end of quote */
    else if (!qc && (ch == '\'' || ch == '"') && !(flags & MUTT_TOKEN_QUOTE))
      qc = ch;
    else if (ch == '\\' && qc != '\'')
    {
      if (!*tok->dptr)
        return -1; /* premature end of token */
      switch (ch = *tok->dptr++)
      {
        case 'c':
        case 'C':
          if (!*tok->dptr)
            return -1; /* premature end of token */
          mutt_buffer_addch(dest, (toupper((unsigned char) *tok->dptr) - '@') & 0x7f);
          tok->dptr++;
          break;
        case 'r':
          mutt_buffer_addch(dest, '\r');
          break;
        case 'n':
          mutt_buffer_addch(dest, '\n');
          break;
        case 't':
          mutt_buffer_addch(dest, '\t');
          break;
        case 'f':
          mutt_buffer_addch(dest, '\f');
          break;
        case 'e':
          mutt_buffer_addch(dest, '\033');
          break;
        default:
          if (isdigit((unsigned char) ch) && isdigit((unsigned char) *tok->dptr) &&
              isdigit((unsigned char) *(tok->dptr + 1)))
          {
            mutt_buffer_addch(dest, (ch << 6) + (*tok->dptr << 3) + *(tok->dptr + 1) - 3504);
            tok->dptr += 2;
          }
          else
            mutt_buffer_addch(dest, ch);
      }
    }
    else if (ch == '^' && (flags & MUTT_TOKEN_CONDENSE))
    {
      if (!*tok->dptr)
        return -1; /* premature end of token */
      ch = *tok->dptr++;
      if (ch == '^')
        mutt_buffer_addch(dest, ch);
      else if (ch == '[')
        mutt_buffer_addch(dest, '\033');
      else if (isalpha((unsigned char) ch))
        mutt_buffer_addch(dest, toupper((unsigned char) ch) - '@');
      else
      {
        mutt_buffer_addch(dest, '^');
        mutt_buffer_addch(dest, ch);
      }
    }
    else if (ch == '`' && (!qc || qc == '"'))
    {
      FILE *fp = NULL;
      pid_t pid;
      char *cmd = NULL, *ptr = NULL;
      size_t expnlen;
      struct Buffer expn;
      int line = 0;

      pc = tok->dptr;
      do
      {
        if ((pc = strpbrk(pc, "\\`")))
        {
          /* skip any quoted chars */
          if (*pc == '\\')
            pc += 2;
        }
      } while (pc && *pc != '`');
      if (!pc)
      {
        mutt_debug(1, "mutt_get_token: mismatched backticks\n");
        return -1;
      }
      cmd = mutt_substrdup(tok->dptr, pc);
      if ((pid = mutt_create_filter(cmd, NULL, &fp, NULL)) < 0)
      {
        mutt_debug(1, "mutt_get_token: unable to fork command: %s\n", cmd);
        FREE(&cmd);
        return -1;
      }
      FREE(&cmd);

      tok->dptr = pc + 1;

      /* read line */
      mutt_buffer_init(&expn);
      expn.data = mutt_read_line(NULL, &expn.dsize, fp, &line, 0);
      safe_fclose(&fp);
      mutt_wait_filter(pid);

      /* if we got output, make a new string consisting of the shell output
         plus whatever else was left on the original line */
      /* BUT: If this is inside a quoted string, directly add output to
       * the token */
      if (expn.data && qc)
      {
        mutt_buffer_addstr(dest, expn.data);
        FREE(&expn.data);
      }
      else if (expn.data)
      {
        expnlen = mutt_strlen(expn.data);
        tok->dsize = expnlen + mutt_strlen(tok->dptr) + 1;
        ptr = safe_malloc(tok->dsize);
        memcpy(ptr, expn.data, expnlen);
        strcpy(ptr + expnlen, tok->dptr);
        if (tok->destroy)
          FREE(&tok->data);
        tok->data = ptr;
        tok->dptr = ptr;
        tok->destroy = 1; /* mark that the caller should destroy this data */
        ptr = NULL;
        FREE(&expn.data);
      }
    }
    else if (ch == '$' && (!qc || qc == '"') &&
             (*tok->dptr == '{' || isalpha((unsigned char) *tok->dptr)))
    {
      const char *env = NULL;
      char *var = NULL;
      int idx;

      if (*tok->dptr == '{')
      {
        tok->dptr++;
        if ((pc = strchr(tok->dptr, '}')))
        {
          var = mutt_substrdup(tok->dptr, pc);
          tok->dptr = pc + 1;
        }
      }
      else
      {
        for (pc = tok->dptr; isalnum((unsigned char) *pc) || *pc == '_'; pc++)
          ;
        var = mutt_substrdup(tok->dptr, pc);
        tok->dptr = pc;
      }
      if (var)
      {
        if ((env = getenv(var)) || (env = myvar_get(var)))
          mutt_buffer_addstr(dest, env);
        else if ((idx = mutt_option_index(var)) != -1)
        {
          /* expand settable mutt variables */
          char val[LONG_STRING];

          if (var_to_string(idx, val, sizeof(val)))
            mutt_buffer_addstr(dest, val);
        }
        FREE(&var);
      }
    }
    else
      mutt_buffer_addch(dest, ch);
  }
  mutt_buffer_addch(dest, 0); /* terminate the string */
  SKIPWS(tok->dptr);
  return 0;
}
