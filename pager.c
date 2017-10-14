/**
 * @file
 * GUI display a file/email/help in a viewport with paging
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include "lib/lib.h"
#include "mutt.h"
#include "alias.h"
#include "attach.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mailbox.h"
#include "mbyte.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_regex.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pattern.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_NNTP
#include "nntp.h"
#endif

#define ISHEADER(x) ((x) == MT_COLOR_HEADER || (x) == MT_COLOR_HDEFAULT)

#define IsAttach(x) (x && (x)->bdy)
#define IsMsgAttach(x) (x && (x)->fp && (x)->bdy && (x)->bdy->hdr)
#define IsHeader(x) (x && (x)->hdr && !(x)->bdy)

static const char *Not_available_in_this_menu =
    N_("Not available in this menu.");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only.");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode.");

/* hack to return to position when returning from index to same message */
static int TopLine = 0;
static struct Header *OldHdr = NULL;

#define CHECK_MODE(x)                                                          \
  if (!(x))                                                                    \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Not_available_in_this_menu));                                 \
    break;                                                                     \
  }

#define CHECK_READONLY                                                         \
  if (!Context || Context->readonly)                                           \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Mailbox_is_read_only));                                       \
    break;                                                                     \
  }

#define CHECK_ATTACH                                                           \
  if (option(OPT_ATTACH_MSG))                                                  \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Function_not_permitted_in_attach_message_mode));              \
    break;                                                                     \
  }

#define CHECK_ACL(aclbit, action)                                              \
  if (!Context || !mutt_bit_isset(Context->rights, aclbit))                    \
  {                                                                            \
    mutt_flushinp();                                                           \
    /* L10N: %s is one of the CHECK_ACL entries below. */                      \
    mutt_error(_("%s: Operation not permitted by ACL"), action);               \
    break;                                                                     \
  }

/**
 * struct QClass - Style of quoted text
 */
struct QClass
{
  int length;
  int index;
  int color;
  char *prefix;
  struct QClass *next, *prev;
  struct QClass *down, *up;
};

/**
 * struct Syntax - Highlighting for a line of text
 */
struct Syntax
{
  int color;
  int first;
  int last;
};

/**
 * struct Line - A line of text in the pager
 */
struct Line
{
  LOFF_T offset;
  short type;
  short continuation;
  short chunks;
  short search_cnt;
  struct Syntax *syntax;
  struct Syntax *search;
  struct QClass *quote;
  unsigned int is_cont_hdr; /**< this line is a continuation of the previous header line */
};

#define ANSI_OFF (1 << 0)
#define ANSI_BLINK (1 << 1)
#define ANSI_BOLD (1 << 2)
#define ANSI_UNDERLINE (1 << 3)
#define ANSI_REVERSE (1 << 4)
#define ANSI_COLOR (1 << 5)

/**
 * struct AnsiAttr - An ANSI escape sequence
 */
struct AnsiAttr
{
  int attr;
  int fg;
  int bg;
  int pair;
};

static short InHelp = 0;

#if defined(USE_SLANG_CURSES) || defined(HAVE_RESIZETERM)
/**
 * struct Resize - Keep track of screen resizing
 */
static struct Resize
{
  int line;
  int search_compiled;
  int search_back;
} *Resize = NULL;
#endif

#define NUM_SIG_LINES 4

static int check_sig(const char *s, struct Line *info, int n)
{
  int count = 0;

  while (n > 0 && count <= NUM_SIG_LINES)
  {
    if (info[n].type != MT_COLOR_SIGNATURE)
      break;
    count++;
    n--;
  }

  if (count == 0)
    return -1;

  if (count > NUM_SIG_LINES)
  {
    /* check for a blank line */
    while (*s)
    {
      if (!ISSPACE(*s))
        return 0;
      s++;
    }

    return -1;
  }

  return 0;
}

static void resolve_color(struct Line *line_info, int n, int cnt, int flags,
                          int special, struct AnsiAttr *a)
{
  int def_color;         /* color without syntax highlight */
  int color;             /* final color */
  static int last_color; /* last color set */
  bool search = false;
  int m;

  if (!cnt)
    last_color = -1; /* force attrset() */

  if (line_info[n].continuation)
  {
    if (!cnt && option(OPT_MARKERS))
    {
      SETCOLOR(MT_COLOR_MARKERS);
      addch('+');
      last_color = ColorDefs[MT_COLOR_MARKERS];
    }
    m = (line_info[n].syntax)[0].first;
    cnt += (line_info[n].syntax)[0].last;
  }
  else
    m = n;
  if (!(flags & MUTT_SHOWCOLOR))
    def_color = ColorDefs[MT_COLOR_NORMAL];
  else if (line_info[m].type == MT_COLOR_HEADER)
    def_color = (line_info[m].syntax)[0].color;
  else
    def_color = ColorDefs[line_info[m].type];

  if ((flags & MUTT_SHOWCOLOR) && line_info[m].type == MT_COLOR_QUOTED)
  {
    struct QClass *class = line_info[m].quote;

    if (class)
    {
      def_color = class->color;

      while (class && class->length > cnt)
      {
        def_color = class->color;
        class = class->up;
      }
    }
  }

  color = def_color;
  if (flags & MUTT_SHOWCOLOR)
  {
    for (int i = 0; i < line_info[m].chunks; i++)
    {
      /* we assume the chunks are sorted */
      if (cnt > (line_info[m].syntax)[i].last)
        continue;
      if (cnt < (line_info[m].syntax)[i].first)
        break;
      if (cnt != (line_info[m].syntax)[i].last)
      {
        color = (line_info[m].syntax)[i].color;
        break;
      }
      /* don't break here, as cnt might be
       * in the next chunk as well */
    }
  }

  if (flags & MUTT_SEARCH)
  {
    for (int i = 0; i < line_info[m].search_cnt; i++)
    {
      if (cnt > (line_info[m].search)[i].last)
        continue;
      if (cnt < (line_info[m].search)[i].first)
        break;
      if (cnt != (line_info[m].search)[i].last)
      {
        color = ColorDefs[MT_COLOR_SEARCH];
        search = true;
        break;
      }
    }
  }

  /* handle "special" bold & underlined characters */
  if (special || a->attr)
  {
#ifdef HAVE_COLOR
    if ((a->attr & ANSI_COLOR))
    {
      if (a->pair == -1)
        a->pair = mutt_alloc_color(a->fg, a->bg);
      color = a->pair;
      if (a->attr & ANSI_BOLD)
        color |= A_BOLD;
    }
    else
#endif
        if ((special & A_BOLD) || (a->attr & ANSI_BOLD))
    {
      if (ColorDefs[MT_COLOR_BOLD] && !search)
        color = ColorDefs[MT_COLOR_BOLD];
      else
        color ^= A_BOLD;
    }
    if ((special & A_UNDERLINE) || (a->attr & ANSI_UNDERLINE))
    {
      if (ColorDefs[MT_COLOR_UNDERLINE] && !search)
        color = ColorDefs[MT_COLOR_UNDERLINE];
      else
        color ^= A_UNDERLINE;
    }
    else if (a->attr & ANSI_REVERSE)
    {
      color ^= A_REVERSE;
    }
    else if (a->attr & ANSI_BLINK)
    {
      color ^= A_BLINK;
    }
    else if (a->attr & ANSI_OFF)
    {
      a->attr = 0;
    }
  }

  if (color != last_color)
  {
    ATTRSET(color);
    last_color = color;
  }
}

static void append_line(struct Line *line_info, int n, int cnt)
{
  int m;

  line_info[n + 1].type = line_info[n].type;
  (line_info[n + 1].syntax)[0].color = (line_info[n].syntax)[0].color;
  line_info[n + 1].continuation = 1;

  /* find the real start of the line */
  for (m = n; m >= 0; m--)
    if (line_info[m].continuation == 0)
      break;

  (line_info[n + 1].syntax)[0].first = m;
  (line_info[n + 1].syntax)[0].last =
      (line_info[n].continuation) ? cnt + (line_info[n].syntax)[0].last : cnt;
}

static void new_class_color(struct QClass *class, int *q_level)
{
  class->index = (*q_level)++;
  class->color = ColorQuote[class->index % ColorQuoteUsed];
}

static void shift_class_colors(struct QClass *quote_list,
                               struct QClass *new_class, int index, int *q_level)
{
  struct QClass *q_list = NULL;

  q_list = quote_list;
  new_class->index = -1;

  while (q_list)
  {
    if (q_list->index >= index)
    {
      q_list->index++;
      q_list->color = ColorQuote[q_list->index % ColorQuoteUsed];
    }
    if (q_list->down)
      q_list = q_list->down;
    else if (q_list->next)
      q_list = q_list->next;
    else
    {
      while (!q_list->next)
      {
        q_list = q_list->up;
        if (!q_list)
          break;
      }
      if (q_list)
        q_list = q_list->next;
    }
  }

  new_class->index = index;
  new_class->color = ColorQuote[index % ColorQuoteUsed];
  (*q_level)++;
}

static void cleanup_quote(struct QClass **quote_list)
{
  struct QClass *ptr = NULL;

  while (*quote_list)
  {
    if ((*quote_list)->down)
      cleanup_quote(&((*quote_list)->down));
    ptr = (*quote_list)->next;
    if ((*quote_list)->prefix)
      FREE(&(*quote_list)->prefix);
    FREE(quote_list);
    *quote_list = ptr;
  }

  return;
}

static struct QClass *classify_quote(struct QClass **quote_list, const char *qptr,
                                     int length, int *force_redraw, int *q_level)
{
  struct QClass *q_list = *quote_list;
  struct QClass *class = NULL, *tmp = NULL, *ptr = NULL, *save = NULL;
  char *tail_qptr = NULL;
  int offset, tail_lng;
  int index = -1;

  if (ColorQuoteUsed <= 1)
  {
    /* not much point in classifying quotes... */

    if (!*quote_list)
    {
      class = safe_calloc(1, sizeof(struct QClass));
      class->color = ColorQuote[0];
      *quote_list = class;
    }
    return *quote_list;
  }

  /* Did I mention how much I like emulating Lisp in C? */

  /* classify quoting prefix */
  while (q_list)
  {
    if (length <= q_list->length)
    {
      /* case 1: check the top level nodes */

      if (mutt_strncmp(qptr, q_list->prefix, length) == 0)
      {
        if (length == q_list->length)
          return q_list; /* same prefix: return the current class */

        /* found shorter prefix */
        if (!tmp)
        {
          /* add a node above q_list */
          tmp = safe_calloc(1, sizeof(struct QClass));
          tmp->prefix = safe_calloc(1, length + 1);
          strncpy(tmp->prefix, qptr, length);
          tmp->length = length;

          /* replace q_list by tmp in the top level list */
          if (q_list->next)
          {
            tmp->next = q_list->next;
            q_list->next->prev = tmp;
          }
          if (q_list->prev)
          {
            tmp->prev = q_list->prev;
            q_list->prev->next = tmp;
          }

          /* make q_list a child of tmp */
          tmp->down = q_list;
          q_list->up = tmp;

          /* q_list has no siblings for now */
          q_list->next = NULL;
          q_list->prev = NULL;

          /* update the root if necessary */
          if (q_list == *quote_list)
            *quote_list = tmp;

          index = q_list->index;

          /* tmp should be the return class too */
          class = tmp;

          /* next class to test; if tmp is a shorter prefix for another
           * node, that node can only be in the top level list, so don't
           * go down after this point
           */
          q_list = tmp->next;
        }
        else
        {
          /* found another branch for which tmp is a shorter prefix */

          /* save the next sibling for later */
          save = q_list->next;

          /* unlink q_list from the top level list */
          if (q_list->next)
            q_list->next->prev = q_list->prev;
          if (q_list->prev)
            q_list->prev->next = q_list->next;

          /* at this point, we have a tmp->down; link q_list to it */
          ptr = tmp->down;
          /* sibling order is important here, q_list should be linked last */
          while (ptr->next)
            ptr = ptr->next;
          ptr->next = q_list;
          q_list->next = NULL;
          q_list->prev = ptr;
          q_list->up = tmp;

          index = q_list->index;

          /* next class to test; as above, we shouldn't go down */
          q_list = save;
        }

        /* we found a shorter prefix, so certain quotes have changed classes */
        *force_redraw = 1;
        continue;
      }
      else
      {
        /* shorter, but not a substring of the current class: try next */
        q_list = q_list->next;
        continue;
      }
    }
    else
    {
      /* case 2: try subclassing the current top level node */

      /* tmp != NULL means we already found a shorter prefix at case 1 */
      if (tmp == NULL && (mutt_strncmp(qptr, q_list->prefix, q_list->length) == 0))
      {
        /* ok, it's a subclass somewhere on this branch */

        ptr = q_list;
        offset = q_list->length;

        q_list = q_list->down;
        tail_lng = length - offset;
        tail_qptr = (char *) qptr + offset;

        while (q_list)
        {
          if (length <= q_list->length)
          {
            if (mutt_strncmp(tail_qptr, (q_list->prefix) + offset, tail_lng) == 0)
            {
              /* same prefix: return the current class */
              if (length == q_list->length)
                return q_list;

              /* found shorter common prefix */
              if (!tmp)
              {
                /* add a node above q_list */
                tmp = safe_calloc(1, sizeof(struct QClass));
                tmp->prefix = safe_calloc(1, length + 1);
                strncpy(tmp->prefix, qptr, length);
                tmp->length = length;

                /* replace q_list by tmp */
                if (q_list->next)
                {
                  tmp->next = q_list->next;
                  q_list->next->prev = tmp;
                }
                if (q_list->prev)
                {
                  tmp->prev = q_list->prev;
                  q_list->prev->next = tmp;
                }

                /* make q_list a child of tmp */
                tmp->down = q_list;
                tmp->up = q_list->up;
                q_list->up = tmp;
                if (tmp->up->down == q_list)
                  tmp->up->down = tmp;

                /* q_list has no siblings */
                q_list->next = NULL;
                q_list->prev = NULL;

                index = q_list->index;

                /* tmp should be the return class too */
                class = tmp;

                /* next class to test */
                q_list = tmp->next;
              }
              else
              {
                /* found another branch for which tmp is a shorter prefix */

                /* save the next sibling for later */
                save = q_list->next;

                /* unlink q_list from the top level list */
                if (q_list->next)
                  q_list->next->prev = q_list->prev;
                if (q_list->prev)
                  q_list->prev->next = q_list->next;

                /* at this point, we have a tmp->down; link q_list to it */
                ptr = tmp->down;
                while (ptr->next)
                  ptr = ptr->next;
                ptr->next = q_list;
                q_list->next = NULL;
                q_list->prev = ptr;
                q_list->up = tmp;

                index = q_list->index;

                /* next class to test */
                q_list = save;
              }

              /* we found a shorter prefix, so we need a redraw */
              *force_redraw = 1;
              continue;
            }
            else
            {
              q_list = q_list->next;
              continue;
            }
          }
          else
          {
            /* longer than the current prefix: try subclassing it */
            if (tmp == NULL && (mutt_strncmp(tail_qptr, (q_list->prefix) + offset,
                                             q_list->length - offset) == 0))
            {
              /* still a subclass: go down one level */
              ptr = q_list;
              offset = q_list->length;

              q_list = q_list->down;
              tail_lng = length - offset;
              tail_qptr = (char *) qptr + offset;

              continue;
            }
            else
            {
              /* nope, try the next prefix */
              q_list = q_list->next;
              continue;
            }
          }
        }

        /* still not found so far: add it as a sibling to the current node */
        if (!class)
        {
          tmp = safe_calloc(1, sizeof(struct QClass));
          tmp->prefix = safe_calloc(1, length + 1);
          strncpy(tmp->prefix, qptr, length);
          tmp->length = length;

          if (ptr->down)
          {
            tmp->next = ptr->down;
            ptr->down->prev = tmp;
          }
          ptr->down = tmp;
          tmp->up = ptr;

          new_class_color(tmp, q_level);

          return tmp;
        }
        else
        {
          if (index != -1)
            shift_class_colors(*quote_list, tmp, index, q_level);

          return class;
        }
      }
      else
      {
        /* nope, try the next prefix */
        q_list = q_list->next;
        continue;
      }
    }
  }

  if (!class)
  {
    /* not found so far: add it as a top level class */
    class = safe_calloc(1, sizeof(struct QClass));
    class->prefix = safe_calloc(1, length + 1);
    strncpy(class->prefix, qptr, length);
    class->length = length;
    new_class_color(class, q_level);

    if (*quote_list)
    {
      class->next = *quote_list;
      (*quote_list)->prev = class;
    }
    *quote_list = class;
  }

  if (index != -1)
    shift_class_colors(*quote_list, tmp, index, q_level);

  return class;
}

static int brailleLine = -1;
static int brailleCol = -1;

static int check_attachment_marker(char *p)
{
  char *q = AttachmentMarker;

  for (; *p == *q && *q && *p && *q != '\a' && *p != '\a'; p++, q++)
    ;
  return (int) (*p - *q);
}

static void resolve_types(char *buf, char *raw, struct Line *line_info, int n,
                          int last, struct QClass **quote_list, int *q_level,
                          int *force_redraw, int q_classify)
{
  struct ColorLine *color_line = NULL;
  regmatch_t pmatch[1], smatch[1];
  bool found;
  bool null_rx;
  int offset, i;

  if (n == 0 || ISHEADER(line_info[n - 1].type))
  {
    if (buf[0] == '\n') /* end of header */
    {
      line_info[n].type = MT_COLOR_NORMAL;
      getyx(stdscr, brailleLine, brailleCol);
    }
    else
    {
      /* if this is a continuation of the previous line, use the previous
       * line's color as default. */
      if (n > 0 && (buf[0] == ' ' || buf[0] == '\t'))
      {
        line_info[n].type = line_info[n - 1].type; /* wrapped line */
        if (!option(OPT_HEADER_COLOR_PARTIAL))
        {
          (line_info[n].syntax)[0].color = (line_info[n - 1].syntax)[0].color;
          line_info[n].is_cont_hdr = 1;
        }
      }
      else
      {
        line_info[n].type = MT_COLOR_HDEFAULT;
      }

      /* When this option is unset, we color the entire header the
       * same color.  Otherwise, we handle the header patterns just
       * like body patterns (further below).
       */
      if (!option(OPT_HEADER_COLOR_PARTIAL))
      {
        STAILQ_FOREACH(color_line, &ColorHdrList, entries)
        {
          if (REGEXEC(color_line->regex, buf) == 0)
          {
            line_info[n].type = MT_COLOR_HEADER;
            line_info[n].syntax[0].color = color_line->pair;
            if (line_info[n].is_cont_hdr)
            {
              /* adjust the previous continuation lines to reflect the color of this continuation line */
              int j;
              for (j = n - 1; j >= 0 && line_info[j].is_cont_hdr; --j)
              {
                line_info[j].type = line_info[n].type;
                line_info[j].syntax[0].color = line_info[n].syntax[0].color;
              }
              /* now adjust the first line of this header field */
              if (j >= 0)
              {
                line_info[j].type = line_info[n].type;
                line_info[j].syntax[0].color = line_info[n].syntax[0].color;
              }
              *force_redraw = 1; /* the previous lines have already been drawn on the screen */
            }
            break;
          }
        }
      }
    }
  }
  else if (mutt_strncmp("\033[0m", raw, 4) == 0) /* a little hack... */
    line_info[n].type = MT_COLOR_NORMAL;
  else if (check_attachment_marker((char *) raw) == 0)
    line_info[n].type = MT_COLOR_ATTACHMENT;
  else if ((mutt_strcmp("-- \n", buf) == 0) || (mutt_strcmp("-- \r\n", buf) == 0))
  {
    i = n + 1;

    line_info[n].type = MT_COLOR_SIGNATURE;
    while (i < last && check_sig(buf, line_info, i - 1) == 0 &&
           (line_info[i].type == MT_COLOR_NORMAL || line_info[i].type == MT_COLOR_QUOTED ||
            line_info[i].type == MT_COLOR_HEADER))
    {
      /* oops... */
      if (line_info[i].chunks)
      {
        line_info[i].chunks = 0;
        safe_realloc(&(line_info[n].syntax), sizeof(struct Syntax));
      }
      line_info[i++].type = MT_COLOR_SIGNATURE;
    }
  }
  else if (check_sig(buf, line_info, n - 1) == 0)
    line_info[n].type = MT_COLOR_SIGNATURE;
  else if (regexec((regex_t *) QuoteRegexp.regex, buf, 1, pmatch, 0) == 0)
  {
    if (regexec((regex_t *) Smileys.regex, buf, 1, smatch, 0) == 0)
    {
      if (smatch[0].rm_so > 0)
      {
        char c;

        /* hack to avoid making an extra copy of buf */
        c = buf[smatch[0].rm_so];
        buf[smatch[0].rm_so] = 0;

        if (regexec((regex_t *) QuoteRegexp.regex, buf, 1, pmatch, 0) == 0)
        {
          if (q_classify && line_info[n].quote == NULL)
            line_info[n].quote = classify_quote(quote_list, buf + pmatch[0].rm_so,
                                                pmatch[0].rm_eo - pmatch[0].rm_so,
                                                force_redraw, q_level);
          line_info[n].type = MT_COLOR_QUOTED;
        }
        else
          line_info[n].type = MT_COLOR_NORMAL;

        buf[smatch[0].rm_so] = c;
      }
      else
        line_info[n].type = MT_COLOR_NORMAL;
    }
    else
    {
      if (q_classify && line_info[n].quote == NULL)
        line_info[n].quote = classify_quote(quote_list, buf + pmatch[0].rm_so,
                                            pmatch[0].rm_eo - pmatch[0].rm_so,
                                            force_redraw, q_level);
      line_info[n].type = MT_COLOR_QUOTED;
    }
  }
  else
    line_info[n].type = MT_COLOR_NORMAL;

  /* body patterns */
  if (line_info[n].type == MT_COLOR_NORMAL || line_info[n].type == MT_COLOR_QUOTED ||
      (line_info[n].type == MT_COLOR_HDEFAULT && option(OPT_HEADER_COLOR_PARTIAL)))
  {
    size_t nl;

    /* don't consider line endings part of the buffer
     * for regex matching */
    if ((nl = mutt_strlen(buf)) > 0 && buf[nl - 1] == '\n')
      buf[nl - 1] = 0;

    i = 0;
    offset = 0;
    line_info[n].chunks = 0;
    do
    {
      if (!buf[offset])
        break;

      found = false;
      null_rx = false;
      struct ColorLineHead *head = NULL;
      if (line_info[n].type == MT_COLOR_HDEFAULT)
        head = &ColorHdrList;
      else
        head = &ColorBodyList;
      STAILQ_FOREACH(color_line, head, entries)
      {
        if (regexec(&color_line->regex, buf + offset, 1, pmatch,
                    (offset ? REG_NOTBOL : 0)) == 0)
        {
          if (pmatch[0].rm_eo != pmatch[0].rm_so)
          {
            if (!found)
            {
              /* Abort if we fill up chunks.
               * Yes, this really happened. See #3888 */
              if (line_info[n].chunks == SHRT_MAX)
              {
                null_rx = false;
                break;
              }
              if (++(line_info[n].chunks) > 1)
                safe_realloc(&(line_info[n].syntax),
                             (line_info[n].chunks) * sizeof(struct Syntax));
            }
            i = line_info[n].chunks - 1;
            pmatch[0].rm_so += offset;
            pmatch[0].rm_eo += offset;
            if (!found || pmatch[0].rm_so < (line_info[n].syntax)[i].first ||
                (pmatch[0].rm_so == (line_info[n].syntax)[i].first &&
                 pmatch[0].rm_eo > (line_info[n].syntax)[i].last))
            {
              (line_info[n].syntax)[i].color = color_line->pair;
              (line_info[n].syntax)[i].first = pmatch[0].rm_so;
              (line_info[n].syntax)[i].last = pmatch[0].rm_eo;
            }
            found = true;
            null_rx = false;
          }
          else
            null_rx = true; /* empty regex; don't add it, but keep looking */
        }
      }

      if (null_rx)
        offset++; /* avoid degenerate cases */
      else
        offset = (line_info[n].syntax)[i].last;
    } while (found || null_rx);
    if (nl > 0)
      buf[nl] = '\n';
  }

  /* attachment patterns */
  if (line_info[n].type == MT_COLOR_ATTACHMENT)
  {
    size_t nl;

    /* don't consider line endings part of the buffer for regex matching */
    nl = mutt_strlen(buf);
    if ((nl > 0) && (buf[nl - 1] == '\n'))
      buf[nl - 1] = 0;

    i = 0;
    offset = 0;
    line_info[n].chunks = 0;
    do
    {
      if (!buf[offset])
        break;

      found = false;
      null_rx = false;
      STAILQ_FOREACH(color_line, &ColorAttachList, entries)
      {
        if (regexec(&color_line->regex, buf + offset, 1, pmatch,
                    (offset ? REG_NOTBOL : 0)) == 0)
        {
          if (pmatch[0].rm_eo != pmatch[0].rm_so)
          {
            if (!found)
            {
              if (++(line_info[n].chunks) > 1)
                safe_realloc(&(line_info[n].syntax),
                             (line_info[n].chunks) * sizeof(struct Syntax));
            }
            i = line_info[n].chunks - 1;
            pmatch[0].rm_so += offset;
            pmatch[0].rm_eo += offset;
            if (!found || pmatch[0].rm_so < (line_info[n].syntax)[i].first ||
                (pmatch[0].rm_so == (line_info[n].syntax)[i].first &&
                 pmatch[0].rm_eo > (line_info[n].syntax)[i].last))
            {
              (line_info[n].syntax)[i].color = color_line->pair;
              (line_info[n].syntax)[i].first = pmatch[0].rm_so;
              (line_info[n].syntax)[i].last = pmatch[0].rm_eo;
            }
            found = 1;
            null_rx = 0;
          }
          else
            null_rx = 1; /* empty regex; don't add it, but keep looking */
        }
      }

      if (null_rx)
        offset++; /* avoid degenerate cases */
      else
        offset = (line_info[n].syntax)[i].last;
    } while (found || null_rx);
    if (nl > 0)
      buf[nl] = '\n';
  }
}

static int is_ansi(unsigned char *buf)
{
  while (*buf && (isdigit(*buf) || *buf == ';'))
    buf++;
  return (*buf == 'm');
}

static int grok_ansi(unsigned char *buf, int pos, struct AnsiAttr *a)
{
  int x = pos;

  while (isdigit(buf[x]) || buf[x] == ';')
    x++;

  /* Character Attributes */
  if (option(OPT_ALLOW_ANSI) && a != NULL && buf[x] == 'm')
  {
    if (pos == x)
    {
#ifdef HAVE_COLOR
      if (a->pair != -1)
        mutt_free_color(a->fg, a->bg);
#endif
      a->attr = ANSI_OFF;
      a->pair = -1;
    }
    while (pos < x)
    {
      if (buf[pos] == '1' && (pos + 1 == x || buf[pos + 1] == ';'))
      {
        a->attr |= ANSI_BOLD;
        pos += 2;
      }
      else if (buf[pos] == '4' && (pos + 1 == x || buf[pos + 1] == ';'))
      {
        a->attr |= ANSI_UNDERLINE;
        pos += 2;
      }
      else if (buf[pos] == '5' && (pos + 1 == x || buf[pos + 1] == ';'))
      {
        a->attr |= ANSI_BLINK;
        pos += 2;
      }
      else if (buf[pos] == '7' && (pos + 1 == x || buf[pos + 1] == ';'))
      {
        a->attr |= ANSI_REVERSE;
        pos += 2;
      }
      else if (buf[pos] == '0' && (pos + 1 == x || buf[pos + 1] == ';'))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_free_color(a->fg, a->bg);
#endif
        a->attr = ANSI_OFF;
        a->pair = -1;
        pos += 2;
      }
      else if (buf[pos] == '3' && isdigit(buf[pos + 1]))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_free_color(a->fg, a->bg);
#endif
        a->pair = -1;
        a->attr |= ANSI_COLOR;
        a->fg = buf[pos + 1] - '0';
        pos += 3;
      }
      else if (buf[pos] == '4' && isdigit(buf[pos + 1]))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_free_color(a->fg, a->bg);
#endif
        a->pair = -1;
        a->attr |= ANSI_COLOR;
        a->bg = buf[pos + 1] - '0';
        pos += 3;
      }
      else
      {
        while (pos < x && buf[pos] != ';')
          pos++;
        pos++;
      }
    }
  }
  pos = x;
  return pos;
}

/**
 * trim_incomplete_mbyte - Remove an incomplete character
 *
 * trim tail of buf so that it contains complete multibyte characters
 */
static int trim_incomplete_mbyte(unsigned char *buf, size_t len)
{
  mbstate_t mbstate;
  size_t k;

  memset(&mbstate, 0, sizeof(mbstate));
  for (; len > 0; buf += k, len -= k)
  {
    k = mbrtowc(NULL, (char *) buf, len, &mbstate);
    if (k == (size_t)(-2))
      break;
    else if (k == (size_t)(-1) || k == 0)
    {
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = 1;
    }
  }
  *buf = '\0';

  return len;
}

static int fill_buffer(FILE *f, LOFF_T *last_pos, LOFF_T offset, unsigned char **buf,
                       unsigned char **fmt, size_t *blen, int *buf_ready)
{
  unsigned char *p = NULL, *q = NULL;
  static int b_read;
  int l = 0;

  if (*buf_ready == 0)
  {
    if (offset != *last_pos)
      fseeko(f, offset, SEEK_SET);
    *buf = (unsigned char *) mutt_read_line((char *) *buf, blen, f, &l, MUTT_EOL);
    if (!*buf)
    {
      fmt[0] = 0;
      return -1;
    }
    *last_pos = ftello(f);
    b_read = (int) (*last_pos - offset);
    *buf_ready = 1;

    safe_realloc(fmt, *blen);

    /* incomplete mbyte characters trigger a segfault in regex processing for
     * certain versions of glibc. Trim them if necessary. */
    if (b_read == *blen - 2)
      b_read -= trim_incomplete_mbyte(*buf, b_read);

    /* copy "buf" to "fmt", but without bold and underline controls */
    p = *buf;
    q = *fmt;
    while (*p)
    {
      if (*p == '\010' && (p > *buf))
      {
        if (*(p + 1) == '_') /* underline */
          p += 2;
        else if (*(p + 1) && q > *fmt) /* bold or overstrike */
        {
          *(q - 1) = *(p + 1);
          p += 2;
        }
        else /* ^H */
          *q++ = *p++;
      }
      else if (*p == '\033' && *(p + 1) == '[' && is_ansi(p + 2))
      {
        while (*p++ != 'm') /* skip ANSI sequence */
          ;
      }
      else if (*p == '\033' && *(p + 1) == ']' && check_attachment_marker((char *) p) == 0)
      {
        mutt_debug(2, "fill_buffer: Seen attachment marker.\n");
        while (*p++ != '\a') /* skip pseudo-ANSI sequence */
          ;
      }
      else
        *q++ = *p++;
    }
    *q = 0;
  }
  return b_read;
}

static int format_line(struct Line **line_info, int n, unsigned char *buf, int flags,
                       struct AnsiAttr *pa, int cnt, int *pspace, int *pvch,
                       int *pcol, int *pspecial, struct MuttWindow *pager_window)
{
  int space = -1; /* index of the last space or TAB */
  int col = option(OPT_MARKERS) ? (*line_info)[n].continuation : 0;
  size_t k;
  int ch, vch, last_special = -1, special = 0, t;
  wchar_t wc;
  mbstate_t mbstate;
  int wrap_cols =
      mutt_window_wrap_cols(pager_window, (flags & MUTT_PAGER_NOWRAP) ? 0 : Wrap);

  if (check_attachment_marker((char *) buf) == 0)
    wrap_cols = pager_window->cols;

  /* FIXME: this should come from line_info */
  memset(&mbstate, 0, sizeof(mbstate));

  for (ch = 0, vch = 0; ch < cnt; ch += k, vch += k)
  {
    /* Handle ANSI sequences */
    while (cnt - ch >= 2 && buf[ch] == '\033' && buf[ch + 1] == '[' && is_ansi(buf + ch + 2))
      ch = grok_ansi(buf, ch + 2, pa) + 1;

    while (cnt - ch >= 2 && buf[ch] == '\033' && buf[ch + 1] == ']' &&
           check_attachment_marker((char *) buf + ch) == 0)
    {
      while (buf[ch++] != '\a')
        if (ch >= cnt)
          break;
    }

    /* is anything left to do? */
    if (ch >= cnt)
      break;

    k = mbrtowc(&wc, (char *) buf + ch, cnt - ch, &mbstate);
    if (k == (size_t)(-2) || k == (size_t)(-1))
    {
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      mutt_debug(1, "%s:%d: mbrtowc returned %lu; errno = %d.\n", __FILE__,
                 __LINE__, k, errno);
      if (col + 4 > wrap_cols)
        break;
      col += 4;
      if (pa)
        printw("\\%03o", buf[ch]);
      k = 1;
      continue;
    }
    if (k == 0)
      k = 1;

    if (Charset_is_utf8)
    {
      if (wc == 0x200B || wc == 0xFEFF)
      {
        mutt_debug(3, "skip zero-width character U+%04X\n", (unsigned short) wc);
        continue;
      }
      if (is_display_corrupting_utf8(wc))
      {
        mutt_debug(3, "filtered U+%04X\n", (unsigned short) wc);
        continue;
      }
    }

    /* Handle backspace */
    special = 0;
    if (IsWPrint(wc))
    {
      wchar_t wc1;
      mbstate_t mbstate1;
      size_t k1, k2;

      mbstate1 = mbstate;
      k1 = mbrtowc(&wc1, (char *) buf + ch + k, cnt - ch - k, &mbstate1);
      while ((k1 != (size_t)(-2)) && (k1 != (size_t)(-1)) && (k1 > 0) && (wc1 == '\b'))
      {
        k2 = mbrtowc(&wc1, (char *) buf + ch + k + k1, cnt - ch - k - k1, &mbstate1);
        if ((k2 == (size_t)(-2)) || (k2 == (size_t)(-1)) || (k2 == 0) || (!IsWPrint(wc1)))
          break;

        if (wc == wc1)
        {
          special |= (wc == '_' && special & A_UNDERLINE) ? A_UNDERLINE : A_BOLD;
        }
        else if (wc == '_' || wc1 == '_')
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

    if (pa && ((flags & (MUTT_SHOWCOLOR | MUTT_SEARCH | MUTT_PAGER_MARKER)) ||
               special || last_special || pa->attr))
    {
      resolve_color(*line_info, n, vch, flags, special, pa);
      last_special = special;
    }

    if (IsWPrint(wc) || (Charset_is_utf8 && (wc == 0x00A0 || wc == 0x202F)))
    {
      if (wc == ' ')
        space = ch;
      else if (Charset_is_utf8 && (wc == 0x00A0 || wc == 0x202F))
      {
        /* Convert non-breaking space to normal space. The local variable
         * `space' is not set here so that the caller of this function won't
         * attempt to wrap at this character. */
        wc = ' ';
      }
      t = wcwidth(wc);
      if (col + t > wrap_cols)
        break;
      col += t;
      if (pa)
        mutt_addwch(wc);
    }
    else if (wc == '\n')
      break;
    else if (wc == '\t')
    {
      space = ch;
      t = (col & ~7) + 8;
      if (t > wrap_cols)
        break;
      if (pa)
        for (; col < t; col++)
          addch(' ');
      else
        col = t;
    }
    else if (wc < 0x20 || wc == 0x7f)
    {
      if (col + 2 > wrap_cols)
        break;
      col += 2;
      if (pa)
        printw("^%c", ('@' + wc) & 0x7f);
    }
    else if (wc < 0x100)
    {
      if (col + 4 > wrap_cols)
        break;
      col += 4;
      if (pa)
        printw("\\%03o", wc);
    }
    else
    {
      if (col + 1 > wrap_cols)
        break;
      col += k;
      if (pa)
        addch(replacement_char());
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
 * @param f               File to read from
 * @param last_pos        Offset into file
 * @param line_info       Line attributes
 * @param n               Line number
 * @param last            Last line
 * @param max             Maximum number of lines
 * @param flags           See below
 * @param quote_list      Email quoting style
 * @param q_level         Level of quoting
 * @param force_redraw    Force a repaint
 * @param search_re       Regex to highlight
 * @param pager_window    Window to draw into
 * @retval -1 EOF was reached
 * @retval 0  normal exit, line was not displayed
 * @retval >0 normal exit, line was displayed
 *
 * flags:
 * * #MUTT_SHOWFLAT, show characters (used for displaying help)
 * * #MUTT_SHOWCOLOR, show characters in color otherwise don't show characters
 * * #MUTT_HIDE, don't show quoted text
 * * #MUTT_SEARCH, resolve search patterns
 * * #MUTT_TYPES, compute line's type
 * * #MUTT_PAGER_NSKIP, keeps leading whitespace
 * * #MUTT_PAGER_MARKER, eventually show markers
 */
static int display_line(FILE *f, LOFF_T *last_pos, struct Line **line_info,
                        int n, int *last, int *max, int flags,
                        struct QClass **quote_list, int *q_level, int *force_redraw,
                        regex_t *search_re, struct MuttWindow *pager_window)
{
  unsigned char *buf = NULL, *fmt = NULL;
  size_t buflen = 0;
  unsigned char *buf_ptr = buf;
  int ch, vch, col, cnt, b_read;
  int buf_ready = 0;
  bool change_last = false;
  int special;
  int offset;
  int def_color;
  int m;
  int rc = -1;
  struct AnsiAttr a = { 0, 0, 0, -1 };
  regmatch_t pmatch[1];

  if (n == *last)
  {
    (*last)++;
    change_last = true;
  }

  if (*last == *max)
  {
    safe_realloc(line_info, sizeof(struct Line) * (*max += LINES));
    for (ch = *last; ch < *max; ch++)
    {
      memset(&((*line_info)[ch]), 0, sizeof(struct Line));
      (*line_info)[ch].type = -1;
      (*line_info)[ch].search_cnt = -1;
      (*line_info)[ch].syntax = safe_malloc(sizeof(struct Syntax));
      ((*line_info)[ch].syntax)[0].first = ((*line_info)[ch].syntax)[0].last = -1;
    }
  }

  /* only do color highlighting if we are viewing a message */
  if (flags & (MUTT_SHOWCOLOR | MUTT_TYPES))
  {
    if ((*line_info)[n].type == -1)
    {
      /* determine the line class */
      if (fill_buffer(f, last_pos, (*line_info)[n].offset, &buf, &fmt, &buflen, &buf_ready) < 0)
      {
        if (change_last)
          (*last)--;
        goto out;
      }

      resolve_types((char *) fmt, (char *) buf, *line_info, n, *last,
                    quote_list, q_level, force_redraw, flags & MUTT_SHOWCOLOR);

      /* avoid race condition for continuation lines when scrolling up */
      for (m = n + 1; m < *last && (*line_info)[m].offset && (*line_info)[m].continuation; m++)
        (*line_info)[m].type = (*line_info)[n].type;
    }

    /* this also prevents searching through the hidden lines */
    if ((flags & MUTT_HIDE) && (*line_info)[n].type == MT_COLOR_QUOTED)
      flags = 0; /* MUTT_NOSHOW */
  }

  /* At this point, (*line_info[n]).quote may still be undefined. We
   * don't want to compute it every time MUTT_TYPES is set, since this
   * would slow down the "bottom" function unacceptably. A compromise
   * solution is hence to call regexec() again, just to find out the
   * length of the quote prefix.
   */
  if ((flags & MUTT_SHOWCOLOR) && !(*line_info)[n].continuation &&
      (*line_info)[n].type == MT_COLOR_QUOTED && (*line_info)[n].quote == NULL)
  {
    if (fill_buffer(f, last_pos, (*line_info)[n].offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*last)--;
      goto out;
    }
    if (regexec((regex_t *) QuoteRegexp.regex, (char *) fmt, 1, pmatch, 0) != 0)
      goto out;
    (*line_info)[n].quote =
        classify_quote(quote_list, (char *) fmt + pmatch[0].rm_so,
                       pmatch[0].rm_eo - pmatch[0].rm_so, force_redraw, q_level);
  }

  if ((flags & MUTT_SEARCH) && !(*line_info)[n].continuation &&
      (*line_info)[n].search_cnt == -1)
  {
    if (fill_buffer(f, last_pos, (*line_info)[n].offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*last)--;
      goto out;
    }

    offset = 0;
    (*line_info)[n].search_cnt = 0;
    while (regexec(search_re, (char *) fmt + offset, 1, pmatch,
                   (offset ? REG_NOTBOL : 0)) == 0)
    {
      if (++((*line_info)[n].search_cnt) > 1)
        safe_realloc(&((*line_info)[n].search),
                     ((*line_info)[n].search_cnt) * sizeof(struct Syntax));
      else
        (*line_info)[n].search = safe_malloc(sizeof(struct Syntax));
      pmatch[0].rm_so += offset;
      pmatch[0].rm_eo += offset;
      ((*line_info)[n].search)[(*line_info)[n].search_cnt - 1].first = pmatch[0].rm_so;
      ((*line_info)[n].search)[(*line_info)[n].search_cnt - 1].last = pmatch[0].rm_eo;

      if (pmatch[0].rm_eo == pmatch[0].rm_so)
        offset++; /* avoid degenerate cases */
      else
        offset = pmatch[0].rm_eo;
      if (!fmt[offset])
        break;
    }
  }

  if (!(flags & MUTT_SHOW) && (*line_info)[n + 1].offset > 0)
  {
    /* we've already scanned this line, so just exit */
    rc = 0;
    goto out;
  }
  if ((flags & MUTT_SHOWCOLOR) && *force_redraw && (*line_info)[n + 1].offset > 0)
  {
    /* no need to try to display this line... */
    rc = 1;
    goto out; /* fake display */
  }

  if ((b_read = fill_buffer(f, last_pos, (*line_info)[n].offset, &buf, &fmt,
                            &buflen, &buf_ready)) < 0)
  {
    if (change_last)
      (*last)--;
    goto out;
  }

  /* now chose a good place to break the line */
  cnt = format_line(line_info, n, buf, flags, 0, b_read, &ch, &vch, &col, &special, pager_window);
  buf_ptr = buf + cnt;

  /* move the break point only if smart_wrap is set */
  if (option(OPT_SMART_WRAP))
  {
    if (cnt < b_read)
    {
      if (ch != -1 && buf[0] != ' ' && buf[0] != '\t' && buf[cnt] != ' ' &&
          buf[cnt] != '\t' && buf[cnt] != '\n' && buf[cnt] != '\r')
      {
        buf_ptr = buf + ch;
        /* skip trailing blanks */
        while (ch && (buf[ch] == ' ' || buf[ch] == '\t' || buf[ch] == '\r'))
          ch--;
        /* a very long word with leading spaces causes infinite wrapping */
        if ((!ch) && (flags & MUTT_PAGER_NSKIP))
          buf_ptr = buf + cnt;
        else
          cnt = ch + 1;
      }
      else
        buf_ptr = buf + cnt; /* a very long word... */
    }
    if (!(flags & MUTT_PAGER_NSKIP))
      /* skip leading blanks on the next line too */
      while (*buf_ptr == ' ' || *buf_ptr == '\t')
        buf_ptr++;
  }

  if (*buf_ptr == '\r')
    buf_ptr++;
  if (*buf_ptr == '\n')
    buf_ptr++;

  if ((int) (buf_ptr - buf) < b_read && !(*line_info)[n + 1].continuation)
    append_line(*line_info, n, (int) (buf_ptr - buf));
  (*line_info)[n + 1].offset = (*line_info)[n].offset + (long) (buf_ptr - buf);

  /* if we don't need to display the line we are done */
  if (!(flags & MUTT_SHOW))
  {
    rc = 0;
    goto out;
  }

  /* display the line */
  format_line(line_info, n, buf, flags, &a, cnt, &ch, &vch, &col, &special, pager_window);

/* avoid a bug in ncurses... */
#ifndef USE_SLANG_CURSES
  if (col == 0)
  {
    NORMAL_COLOR;
    addch(' ');
  }
#endif

  /* end the last color pattern (needed by S-Lang) */
  if (special || (col != pager_window->cols && (flags & (MUTT_SHOWCOLOR | MUTT_SEARCH))))
    resolve_color(*line_info, n, vch, flags, 0, &a);

  /*
   * Fill the blank space at the end of the line with the prevailing color.
   * ncurses does an implicit clrtoeol() when you do addch('\n') so we have
   * to make sure to reset the color *after* that
   */
  if (flags & MUTT_SHOWCOLOR)
  {
    m = ((*line_info)[n].continuation) ? ((*line_info)[n].syntax)[0].first : n;
    if ((*line_info)[m].type == MT_COLOR_HEADER)
      def_color = ((*line_info)[m].syntax)[0].color;
    else
      def_color = ColorDefs[(*line_info)[m].type];

    ATTRSET(def_color);
  }

  if (col < pager_window->cols)
    mutt_window_clrtoeol(pager_window);

  /*
   * reset the color back to normal.  This *must* come after the
   * clrtoeol, otherwise the color for this line will not be
   * filled to the right margin.
   */
  if (flags & MUTT_SHOWCOLOR)
    NORMAL_COLOR;

  /* build a return code */
  if (!(flags & MUTT_SHOW))
    flags = 0;

  rc = flags;

out:
  FREE(&buf);
  FREE(&fmt);
  return rc;
}

static int up_n_lines(int nlines, struct Line *info, int cur, int hiding)
{
  while (cur > 0 && nlines > 0)
  {
    cur--;
    if (!hiding || info[cur].type != MT_COLOR_QUOTED)
      nlines--;
  }

  return cur;
}

static const struct Mapping PagerHelp[] = {
  { N_("Exit"), OP_EXIT }, { N_("PrevPg"), OP_PREV_PAGE }, { N_("NextPg"), OP_NEXT_PAGE }, { NULL, 0 },
};
static const struct Mapping PagerHelpExtra[] = {
  { N_("View Attachm."), OP_VIEW_ATTACHMENTS },
  { N_("Del"), OP_DELETE },
  { N_("Reply"), OP_REPLY },
  { N_("Next"), OP_MAIN_NEXT_UNDELETED },
  { NULL, 0 },
};

#ifdef USE_NNTP
static struct Mapping PagerNewsHelpExtra[] = {
  { N_("Post"), OP_POST },
  { N_("Followup"), OP_FOLLOWUP },
  { N_("Del"), OP_DELETE },
  { N_("Next"), OP_MAIN_NEXT_UNDELETED },
  { NULL, 0 },
};
#endif

void mutt_clear_pager_position(void)
{
  TopLine = 0;
  OldHdr = NULL;
}

/**
 * struct PagerRedrawData - Keep track when the pager needs redrawing
 */
struct PagerRedrawData
{
  int flags;
  struct Pager *extra;
  int indexlen;
  int indicator; /**< the indicator line of the PI */
  int oldtopline;
  int lines;
  int max_line;
  int last_line;
  int curline;
  int topline;
  int force_redraw;
  int has_types;
  int hide_quoted;
  int q_level;
  struct QClass *quote_list;
  LOFF_T last_pos;
  LOFF_T last_offset;
  struct MuttWindow *index_status_window;
  struct MuttWindow *index_window;
  struct MuttWindow *pager_status_window;
  struct MuttWindow *pager_window;
  struct Menu *index; /**< the Pager Index (PI) */
  regex_t search_re;
  int search_compiled;
  int search_flag;
  int search_back;
  const char *banner;
  char *helpstr;
  char *searchbuf;
  struct Line *line_info;
  FILE *fp;
  struct stat sb;
};

static void pager_menu_redraw(struct Menu *pager_menu)
{
  struct PagerRedrawData *rd = pager_menu->redraw_data;
  int i, j;
  char buffer[LONG_STRING];
#if defined(USE_SLANG_CURSES) || defined(HAVE_RESIZETERM)
  int err;
#endif

  if (!rd)
    return;

  if (pager_menu->redraw & REDRAW_FULL)
  {
#if !(defined(USE_SLANG_CURSES) || defined(HAVE_RESIZETERM))
    mutt_reflow_windows();
#endif
    NORMAL_COLOR;
    /* clear() doesn't optimize screen redraws */
    move(0, 0);
    clrtobot();

    if (IsHeader(rd->extra) && Context && ((Context->vcount + 1) < PagerIndexLines))
      rd->indexlen = Context->vcount + 1;
    else
      rd->indexlen = PagerIndexLines;

    rd->indicator = rd->indexlen / 3;

    memcpy(rd->pager_window, MuttIndexWindow, sizeof(struct MuttWindow));
    memcpy(rd->pager_status_window, MuttStatusWindow, sizeof(struct MuttWindow));
    rd->index_status_window->rows = rd->index_window->rows = 0;

    if (IsHeader(rd->extra) && PagerIndexLines)
    {
      memcpy(rd->index_window, MuttIndexWindow, sizeof(struct MuttWindow));
      rd->index_window->rows = rd->indexlen > 0 ? rd->indexlen - 1 : 0;

      if (option(OPT_STATUS_ON_TOP))
      {
        memcpy(rd->index_status_window, MuttStatusWindow, sizeof(struct MuttWindow));

        memcpy(rd->pager_status_window, MuttIndexWindow, sizeof(struct MuttWindow));
        rd->pager_status_window->rows = 1;
        rd->pager_status_window->row_offset += rd->index_window->rows;

        rd->pager_window->rows -=
            rd->index_window->rows + rd->pager_status_window->rows;
        rd->pager_window->row_offset +=
            rd->index_window->rows + rd->pager_status_window->rows;
      }
      else
      {
        memcpy(rd->index_status_window, MuttIndexWindow, sizeof(struct MuttWindow));
        rd->index_status_window->rows = 1;
        rd->index_status_window->row_offset += rd->index_window->rows;

        rd->pager_window->rows -=
            rd->index_window->rows + rd->index_status_window->rows;
        rd->pager_window->row_offset +=
            rd->index_window->rows + rd->index_status_window->rows;
      }
    }

    if (option(OPT_HELP))
    {
      SETCOLOR(MT_COLOR_STATUS);
      mutt_window_move(MuttHelpWindow, 0, 0);
      mutt_paddstr(MuttHelpWindow->cols, rd->helpstr);
      NORMAL_COLOR;
    }

#if defined(USE_SLANG_CURSES) || defined(HAVE_RESIZETERM)
    if (Resize)
    {
      if ((rd->search_compiled = Resize->search_compiled))
      {
        if ((err = REGCOMP(&rd->search_re, rd->searchbuf,
                           REG_NEWLINE | mutt_which_case(rd->searchbuf))) != 0)
        {
          regerror(err, &rd->search_re, buffer, sizeof(buffer));
          mutt_error("%s", buffer);
          rd->search_compiled = 0;
        }
        else
        {
          rd->search_flag = MUTT_SEARCH;
          rd->search_back = Resize->search_back;
        }
      }
      rd->lines = Resize->line;
      pager_menu->redraw |= REDRAW_FLOW;

      FREE(&Resize);
    }
#endif

    if (IsHeader(rd->extra) && PagerIndexLines)
    {
      if (!rd->index)
      {
        /* only allocate the space if/when we need the index.
           Initialise the menu as per the main index */
        rd->index = mutt_new_menu(MENU_MAIN);
        rd->index->make_entry = index_make_entry;
        rd->index->color = index_color;
        rd->index->max = Context ? Context->vcount : 0;
        rd->index->current = rd->extra->hdr->virtual;
        rd->index->indexwin = rd->index_window;
        rd->index->statuswin = rd->index_status_window;
      }

      NORMAL_COLOR;
      rd->index->pagelen = rd->index_window->rows;
      ;

      /* some fudge to work out whereabouts the indicator should go */
      if (rd->index->current - rd->indicator < 0)
        rd->index->top = 0;
      else if (rd->index->max - rd->index->current < rd->index->pagelen - rd->indicator)
        rd->index->top = rd->index->max - rd->index->pagelen;
      else
        rd->index->top = rd->index->current - rd->indicator;

      menu_redraw_index(rd->index);
    }

    pager_menu->redraw |= REDRAW_BODY | REDRAW_INDEX | REDRAW_STATUS;
#ifdef USE_SIDEBAR
    pager_menu->redraw |= REDRAW_SIDEBAR;
#endif
    mutt_show_error();
  }

  if (pager_menu->redraw & REDRAW_FLOW)
  {
    if (!(rd->flags & MUTT_PAGER_RETWINCH))
    {
      rd->lines = -1;
      for (i = 0; i <= rd->topline; i++)
        if (!rd->line_info[i].continuation)
          rd->lines++;
      for (i = 0; i < rd->max_line; i++)
      {
        rd->line_info[i].offset = 0;
        rd->line_info[i].type = -1;
        rd->line_info[i].continuation = 0;
        rd->line_info[i].chunks = 0;
        rd->line_info[i].search_cnt = -1;
        rd->line_info[i].quote = NULL;

        safe_realloc(&(rd->line_info[i].syntax), sizeof(struct Syntax));
        if (rd->search_compiled && rd->line_info[i].search)
          FREE(&(rd->line_info[i].search));
      }

      rd->last_line = 0;
      rd->topline = 0;
    }
    i = -1;
    j = -1;
    while (display_line(rd->fp, &rd->last_pos, &rd->line_info, ++i, &rd->last_line,
                        &rd->max_line, rd->has_types | rd->search_flag | (rd->flags & MUTT_PAGER_NOWRAP),
                        &rd->quote_list, &rd->q_level, &rd->force_redraw,
                        &rd->search_re, rd->pager_window) == 0)
      if (!rd->line_info[i].continuation && ++j == rd->lines)
      {
        rd->topline = i;
        if (!rd->search_flag)
          break;
      }
  }

#ifdef USE_SIDEBAR
  if (pager_menu->redraw & REDRAW_SIDEBAR)
  {
    menu_redraw_sidebar(pager_menu);
  }
#endif

  if ((pager_menu->redraw & REDRAW_BODY) || rd->topline != rd->oldtopline)
  {
    do
    {
      mutt_window_move(rd->pager_window, 0, 0);
      rd->curline = rd->oldtopline = rd->topline;
      rd->lines = 0;
      rd->force_redraw = 0;

      while (rd->lines < rd->pager_window->rows &&
             rd->line_info[rd->curline].offset <= rd->sb.st_size - 1)
      {
        if (display_line(rd->fp, &rd->last_pos, &rd->line_info, rd->curline,
                         &rd->last_line, &rd->max_line,
                         (rd->flags & MUTT_DISPLAYFLAGS) | rd->hide_quoted |
                             rd->search_flag | (rd->flags & MUTT_PAGER_NOWRAP),
                         &rd->quote_list, &rd->q_level, &rd->force_redraw,
                         &rd->search_re, rd->pager_window) > 0)
          rd->lines++;
        rd->curline++;
        mutt_window_move(rd->pager_window, rd->lines, 0);
      }
      rd->last_offset = rd->line_info[rd->curline].offset;
    } while (rd->force_redraw);

    SETCOLOR(MT_COLOR_TILDE);
    while (rd->lines < rd->pager_window->rows)
    {
      mutt_window_clrtoeol(rd->pager_window);
      if (option(OPT_TILDE))
        addch('~');
      rd->lines++;
      mutt_window_move(rd->pager_window, rd->lines, 0);
    }
    NORMAL_COLOR;

    /* We are going to update the pager status bar, so it isn't
     * necessary to reset to normal color now. */

    pager_menu->redraw |= REDRAW_STATUS; /* need to update the % seen */
  }

  if (pager_menu->redraw & REDRAW_STATUS)
  {
    struct HdrFormatInfo hfi;
    char pager_progress_str[4];

    hfi.ctx = Context;
    hfi.pager_progress = pager_progress_str;

    if (rd->last_pos < rd->sb.st_size - 1)
      snprintf(pager_progress_str, sizeof(pager_progress_str), OFF_T_FMT "%%",
               (100 * rd->last_offset / rd->sb.st_size));
    else
      strfcpy(pager_progress_str, (rd->topline == 0) ? "all" : "end",
              sizeof(pager_progress_str));

    /* print out the pager status bar */
    mutt_window_move(rd->pager_status_window, 0, 0);
    SETCOLOR(MT_COLOR_STATUS);

    if (IsHeader(rd->extra) || IsMsgAttach(rd->extra))
    {
      size_t l1 = rd->pager_status_window->cols * MB_LEN_MAX;
      size_t l2 = sizeof(buffer);
      hfi.hdr = (IsHeader(rd->extra)) ? rd->extra->hdr : rd->extra->bdy->hdr;
      mutt_make_string_info(buffer, l1 < l2 ? l1 : l2, rd->pager_status_window->cols,
                            NONULL(PagerFormat), &hfi, MUTT_FORMAT_MAKEPRINT);
      mutt_draw_statusline(rd->pager_status_window->cols, buffer, l2);
    }
    else
    {
      char bn[STRING];
      snprintf(bn, sizeof(bn), "%s (%s)", rd->banner, pager_progress_str);
      mutt_draw_statusline(rd->pager_status_window->cols, bn, sizeof(bn));
    }
    NORMAL_COLOR;
    if (option(OPT_TS_ENABLED) && TSSupported && rd->index)
    {
      menu_status_line(buffer, sizeof(buffer), rd->index, NONULL(TSStatusFormat));
      mutt_ts_status(buffer);
      menu_status_line(buffer, sizeof(buffer), rd->index, NONULL(TSIconFormat));
      mutt_ts_icon(buffer);
    }
  }

  if ((pager_menu->redraw & REDRAW_INDEX) && rd->index)
  {
    /* redraw the pager_index indicator, because the
     * flags for this message might have changed. */
    if (rd->index_window->rows > 0)
      menu_redraw_current(rd->index);

    /* print out the index status bar */
    menu_status_line(buffer, sizeof(buffer), rd->index, NONULL(StatusFormat));

    mutt_window_move(rd->index_status_window, 0, 0);
    SETCOLOR(MT_COLOR_STATUS);
    mutt_draw_statusline(rd->index_status_window->cols, buffer, sizeof(buffer));
    NORMAL_COLOR;
  }

  pager_menu->redraw = 0;
}

/**
 * mutt_pager - Display a file, or help, in a window
 *
 * This pager is actually not so simple as it once was.  It now operates in two
 * modes: one for viewing messages and the other for viewing help.  These can
 * be distinguished by whether or not ``hdr'' is NULL.  The ``hdr'' arg is
 * there so that we can do operations on the current message without the need
 * to pop back out to the main-menu.
 */
int mutt_pager(const char *banner, const char *fname, int flags, struct Pager *extra)
{
  static char searchbuf[STRING] = "";
  char buffer[LONG_STRING];
  char helpstr[SHORT_STRING * 2];
  char tmphelp[SHORT_STRING * 2];
  int i, ch = 0, rc = -1;
  int err, first = 1;
  int r = -1, searchctx = 0;
  bool wrapped = false;

  struct Menu *pager_menu = NULL;
  int old_PagerIndexLines; /* some people want to resize it
                                         * while inside the pager... */
  int index_hint = 0;      /* used to restore cursor position */
  int oldcount = -1;
  int check;

#ifdef USE_NNTP
  char *followup_to = NULL;
#endif

  struct PagerRedrawData rd;

  if (!(flags & MUTT_SHOWCOLOR))
    flags |= MUTT_SHOWFLAT;

  memset(&rd, 0, sizeof(rd));
  rd.banner = banner;
  rd.flags = flags;
  rd.extra = extra;
  rd.indexlen = PagerIndexLines;
  rd.indicator = rd.indexlen / 3;
  rd.helpstr = helpstr;
  rd.searchbuf = searchbuf;
  rd.has_types = (IsHeader(extra) || (flags & MUTT_SHOWCOLOR)) ? MUTT_TYPES : 0; /* main message or rfc822 attachment */

  rd.fp = fopen(fname, "r");
  if (!rd.fp)
  {
    mutt_perror(fname);
    return -1;
  }

  if (stat(fname, &rd.sb) != 0)
  {
    mutt_perror(fname);
    safe_fclose(&rd.fp);
    return -1;
  }
  unlink(fname);

  /* Initialize variables */

  if (Context && IsHeader(extra) && !extra->hdr->read)
  {
    Context->msgnotreadyet = extra->hdr->msgno;
    mutt_set_flag(Context, extra->hdr, MUTT_READ, 1);
  }

  rd.line_info = safe_malloc(sizeof(struct Line) * (rd.max_line = LINES));
  for (i = 0; i < rd.max_line; i++)
  {
    memset(&rd.line_info[i], 0, sizeof(struct Line));
    rd.line_info[i].type = -1;
    rd.line_info[i].search_cnt = -1;
    rd.line_info[i].syntax = safe_malloc(sizeof(struct Syntax));
    (rd.line_info[i].syntax)[0].first = (rd.line_info[i].syntax)[0].last = -1;
  }

  mutt_compile_help(helpstr, sizeof(helpstr), MENU_PAGER, PagerHelp);
  if (IsHeader(extra))
  {
    strfcpy(tmphelp, helpstr, sizeof(tmphelp));
    mutt_compile_help(buffer, sizeof(buffer), MENU_PAGER,
#ifdef USE_NNTP
                      (Context && (Context->magic == MUTT_NNTP)) ? PagerNewsHelpExtra :
#endif
                                                                   PagerHelpExtra);
    snprintf(helpstr, sizeof(helpstr), "%s %s", tmphelp, buffer);
  }
  if (!InHelp)
  {
    strfcpy(tmphelp, helpstr, sizeof(tmphelp));
    mutt_make_help(buffer, sizeof(buffer), _("Help"), MENU_PAGER, OP_HELP);
    snprintf(helpstr, sizeof(helpstr), "%s %s", tmphelp, buffer);
  }

  rd.index_status_window = safe_calloc(1, sizeof(struct MuttWindow));
  rd.index_window = safe_calloc(1, sizeof(struct MuttWindow));
  rd.pager_status_window = safe_calloc(1, sizeof(struct MuttWindow));
  rd.pager_window = safe_calloc(1, sizeof(struct MuttWindow));

  pager_menu = mutt_new_menu(MENU_PAGER);
  pager_menu->custom_menu_redraw = pager_menu_redraw;
  pager_menu->redraw_data = &rd;
  mutt_push_current_menu(pager_menu);

  while (ch != -1)
  {
    mutt_curs_set(0);

    pager_menu_redraw(pager_menu);

    if (option(OPT_BRAILLE_FRIENDLY))
    {
      if (brailleLine != -1)
      {
        move(brailleLine + 1, 0);
        brailleLine = -1;
      }
    }
    else
      mutt_window_move(rd.pager_status_window, 0, rd.pager_status_window->cols - 1);

    mutt_refresh();

    if (IsHeader(extra) && OldHdr == extra->hdr && TopLine != rd.topline &&
        rd.line_info[rd.curline].offset < rd.sb.st_size - 1)
    {
      if (TopLine - rd.topline > rd.lines)
        rd.topline += rd.lines;
      else
        rd.topline = TopLine;
      continue;
    }
    else
      OldHdr = NULL;

    ch = km_dokey(MENU_PAGER);
    if (ch >= 0)
    {
      mutt_clear_error();
    }
    mutt_curs_set(1);

    bool do_new_mail = false;

    if (Context && !option(OPT_ATTACH_MSG))
    {
      oldcount = Context ? Context->msgcount : 0;
      /* check for new mail */
      check = mx_check_mailbox(Context, &index_hint);
      if (check < 0)
      {
        if (!Context->path)
        {
          /* fatal error occurred */
          FREE(&Context);
          pager_menu->redraw = REDRAW_FULL;
          ch = -1;
          break;
        }
      }
      else if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED) || (check == MUTT_FLAGS))
      {
        /* notify user of newly arrived mail */
        if (check == MUTT_NEW_MAIL)
        {
          for (i = oldcount; i < Context->msgcount; i++)
          {
            struct Header *h = Context->hdrs[i];

            if (h && !h->read)
            {
              mutt_message(_("New mail in this mailbox."));
              do_new_mail = true;
              break;
            }
          }

          if (rd.index && Context)
          {
            /* After the mailbox has been updated,
             * rd.index->current might be invalid */
            rd.index->current = MIN(rd.index->current, (Context->msgcount - 1));
            index_hint = Context->hdrs[Context->v2r[rd.index->current]]->index;

            bool q = Context->quiet;
            Context->quiet = true;
            update_index(rd.index, Context, check, oldcount, index_hint);
            Context->quiet = q;

            rd.index->max = Context->vcount;

            /* If these header pointers don't match, then our email may have
             * been deleted.  Make the pointer safe, then leave the pager */
            if (extra->hdr != Context->hdrs[Context->v2r[rd.index->current]])
            {
              extra->hdr = Context->hdrs[Context->v2r[rd.index->current]];
              ch = -1;
              break;
            }
          }

          pager_menu->redraw = REDRAW_FULL;
          set_option(OPT_SEARCH_INVALID);
        }
      }

      if (mutt_buffy_notify() || do_new_mail)
      {
        if (option(OPT_BEEP_NEW))
          beep();
        if (NewMailCommand)
        {
          char cmd[LONG_STRING];
          menu_status_line(cmd, sizeof(cmd), rd.index, NONULL(NewMailCommand));
          mutt_system(cmd);
        }
      }
    }

#if defined(USE_SLANG_CURSES) || defined(HAVE_RESIZETERM)
    if (SigWinch)
    {
      SigWinch = 0;
      mutt_resize_screen();
      clearok(stdscr, TRUE); /* force complete redraw */

      if (flags & MUTT_PAGER_RETWINCH)
      {
        /* Store current position. */
        rd.lines = -1;
        for (i = 0; i <= rd.topline; i++)
          if (!rd.line_info[i].continuation)
            rd.lines++;

        Resize = safe_malloc(sizeof(struct Resize));

        Resize->line = rd.lines;
        Resize->search_compiled = rd.search_compiled;
        Resize->search_back = rd.search_back;

        ch = -1;
        rc = OP_REFORMAT_WINCH;
      }
      else
      {
        /* note: mutt_resize_screen() -> mutt_reflow_windows() sets
         * REDRAW_FULL and REDRAW_FLOW */
        ch = 0;
      }
      continue;
    }
#endif
    if (ch < 0)
    {
      ch = 0;
      mutt_timeout_hook();
      continue;
    }

    rc = ch;

    switch (ch)
    {
      case OP_EXIT:
        rc = -1;
        ch = -1;
        break;

      case OP_QUIT:
        if (query_quadoption(OPT_QUIT, _("Quit NeoMutt?")) == MUTT_YES)
        {
          /* avoid prompting again in the index menu */
          set_quadoption(OPT_QUIT, MUTT_YES);
          ch = -1;
        }
        break;

      case OP_NEXT_PAGE:
        if (rd.line_info[rd.curline].offset < rd.sb.st_size - 1)
        {
          rd.topline = up_n_lines(PagerContext, rd.line_info, rd.curline, rd.hide_quoted);
        }
        else if (option(OPT_PAGER_STOP))
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_error(_("Bottom of message is shown."));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          ch = -1;
        }
        break;

      case OP_PREV_PAGE:
        if (rd.topline != 0)
        {
          rd.topline = up_n_lines(rd.pager_window->rows - PagerContext,
                                  rd.line_info, rd.topline, rd.hide_quoted);
        }
        else
          mutt_error(_("Top of message is shown."));
        break;

      case OP_NEXT_LINE:
        if (rd.line_info[rd.curline].offset < rd.sb.st_size - 1)
        {
          rd.topline++;
          if (rd.hide_quoted)
          {
            while (rd.line_info[rd.topline].type == MT_COLOR_QUOTED && rd.topline < rd.last_line)
              rd.topline++;
          }
        }
        else
          mutt_error(_("Bottom of message is shown."));
        break;

      case OP_PREV_LINE:
        if (rd.topline)
          rd.topline = up_n_lines(1, rd.line_info, rd.topline, rd.hide_quoted);
        else
          mutt_error(_("Top of message is shown."));
        break;

      case OP_PAGER_TOP:
        if (rd.topline)
          rd.topline = 0;
        else
          mutt_error(_("Top of message is shown."));
        break;

      case OP_HALF_UP:
        if (rd.topline)
          rd.topline = up_n_lines(rd.pager_window->rows / 2, rd.line_info,
                                  rd.topline, rd.hide_quoted);
        else
          mutt_error(_("Top of message is shown."));
        break;

      case OP_HALF_DOWN:
        if (rd.line_info[rd.curline].offset < rd.sb.st_size - 1)
        {
          rd.topline = up_n_lines(rd.pager_window->rows / 2, rd.line_info,
                                  rd.curline, rd.hide_quoted);
        }
        else if (option(OPT_PAGER_STOP))
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_error(_("Bottom of message is shown."));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          ch = -1;
        }
        break;

      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (rd.search_compiled)
        {
          wrapped = false;

          if (SearchContext > 0 && SearchContext < rd.pager_window->rows)
            searchctx = SearchContext;
          else
            searchctx = 0;

        search_next:
          if ((!rd.search_back && ch == OP_SEARCH_NEXT) ||
              (rd.search_back && ch == OP_SEARCH_OPPOSITE))
          {
            /* searching forward */
            for (i = wrapped ? 0 : rd.topline + searchctx + 1; i < rd.last_line; i++)
            {
              if ((!rd.hide_quoted || rd.line_info[i].type != MT_COLOR_QUOTED) &&
                  !rd.line_info[i].continuation && rd.line_info[i].search_cnt > 0)
                break;
            }

            if (i < rd.last_line)
              rd.topline = i;
            else if (wrapped || !option(OPT_WRAP_SEARCH))
              mutt_error(_("Not found."));
            else
            {
              mutt_message(_("Search wrapped to top."));
              wrapped = true;
              goto search_next;
            }
          }
          else
          {
            /* searching backward */
            for (i = wrapped ? rd.last_line : rd.topline + searchctx - 1; i >= 0; i--)
            {
              if ((!rd.hide_quoted || (rd.has_types && rd.line_info[i].type != MT_COLOR_QUOTED)) &&
                  !rd.line_info[i].continuation && rd.line_info[i].search_cnt > 0)
                break;
            }

            if (i >= 0)
              rd.topline = i;
            else if (wrapped || !option(OPT_WRAP_SEARCH))
              mutt_error(_("Not found."));
            else
            {
              mutt_message(_("Search wrapped to bottom."));
              wrapped = true;
              goto search_next;
            }
          }

          if (rd.line_info[rd.topline].search_cnt > 0)
          {
            rd.search_flag = MUTT_SEARCH;
            /* give some context for search results */
            if (rd.topline - searchctx > 0)
              rd.topline -= searchctx;
          }

          break;
        }
      /* no previous search pattern, so fall through to search */

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
        strfcpy(buffer, searchbuf, sizeof(buffer));
        if (mutt_get_field((ch == OP_SEARCH || ch == OP_SEARCH_NEXT) ?
                               _("Search for: ") :
                               _("Reverse search for: "),
                           buffer, sizeof(buffer), MUTT_CLEAR) != 0)
          break;

        if (strcmp(buffer, searchbuf) == 0)
        {
          if (rd.search_compiled)
          {
            /* do an implicit search-next */
            if (ch == OP_SEARCH)
              ch = OP_SEARCH_NEXT;
            else
              ch = OP_SEARCH_OPPOSITE;

            wrapped = false;
            goto search_next;
          }
        }

        if (!buffer[0])
          break;

        strfcpy(searchbuf, buffer, sizeof(searchbuf));

        /* leave search_back alone if ch == OP_SEARCH_NEXT */
        if (ch == OP_SEARCH)
          rd.search_back = 0;
        else if (ch == OP_SEARCH_REVERSE)
          rd.search_back = 1;

        if (rd.search_compiled)
        {
          regfree(&rd.search_re);
          for (i = 0; i < rd.last_line; i++)
          {
            if (rd.line_info[i].search)
              FREE(&(rd.line_info[i].search));
            rd.line_info[i].search_cnt = -1;
          }
        }

        if ((err = REGCOMP(&rd.search_re, searchbuf,
                           REG_NEWLINE | mutt_which_case(searchbuf))) != 0)
        {
          regerror(err, &rd.search_re, buffer, sizeof(buffer));
          mutt_error("%s", buffer);
          for (i = 0; i < rd.max_line; i++)
          {
            /* cleanup */
            if (rd.line_info[i].search)
              FREE(&(rd.line_info[i].search));
            rd.line_info[i].search_cnt = -1;
          }
          rd.search_flag = 0;
          rd.search_compiled = 0;
        }
        else
        {
          rd.search_compiled = 1;
          /* update the search pointers */
          i = 0;
          while (display_line(rd.fp, &rd.last_pos, &rd.line_info, i, &rd.last_line, &rd.max_line,
                              MUTT_SEARCH | (flags & MUTT_PAGER_NSKIP) | (flags & MUTT_PAGER_NOWRAP),
                              &rd.quote_list, &rd.q_level, &rd.force_redraw,
                              &rd.search_re, rd.pager_window) == 0)
            i++;

          if (!rd.search_back)
          {
            /* searching forward */
            for (i = rd.topline; i < rd.last_line; i++)
            {
              if ((!rd.hide_quoted || rd.line_info[i].type != MT_COLOR_QUOTED) &&
                  !rd.line_info[i].continuation && rd.line_info[i].search_cnt > 0)
                break;
            }

            if (i < rd.last_line)
              rd.topline = i;
          }
          else
          {
            /* searching backward */
            for (i = rd.topline; i >= 0; i--)
            {
              if ((!rd.hide_quoted || rd.line_info[i].type != MT_COLOR_QUOTED) &&
                  !rd.line_info[i].continuation && rd.line_info[i].search_cnt > 0)
                break;
            }

            if (i >= 0)
              rd.topline = i;
          }

          if (rd.line_info[rd.topline].search_cnt == 0)
          {
            rd.search_flag = 0;
            mutt_error(_("Not found."));
          }
          else
          {
            rd.search_flag = MUTT_SEARCH;
            /* give some context for search results */
            if (SearchContext > 0 && SearchContext < rd.pager_window->rows)
              searchctx = SearchContext;
            else
              searchctx = 0;
            if (rd.topline - searchctx > 0)
              rd.topline -= searchctx;
          }
        }
        pager_menu->redraw = REDRAW_BODY;
        break;

      case OP_SEARCH_TOGGLE:
        if (rd.search_compiled)
        {
          rd.search_flag ^= MUTT_SEARCH;
          pager_menu->redraw = REDRAW_BODY;
        }
        break;

      case OP_SORT:
      case OP_SORT_REVERSE:
        CHECK_MODE(IsHeader(extra))
        if (mutt_select_sort((ch == OP_SORT_REVERSE)) == 0)
        {
          set_option(OPT_NEED_RESORT);
          ch = -1;
          rc = OP_DISPLAY_MESSAGE;
        }
        break;

      case OP_HELP:
        /* don't let the user enter the help-menu from the help screen! */
        if (!InHelp)
        {
          InHelp = 1;
          mutt_help(MENU_PAGER);
          pager_menu->redraw = REDRAW_FULL;
          InHelp = 0;
        }
        else
          mutt_error(_("Help is currently being shown."));
        break;

      case OP_PAGER_HIDE_QUOTED:
        if (rd.has_types)
        {
          rd.hide_quoted ^= MUTT_HIDE;
          if (rd.hide_quoted && rd.line_info[rd.topline].type == MT_COLOR_QUOTED)
            rd.topline = up_n_lines(1, rd.line_info, rd.topline, rd.hide_quoted);
          else
            pager_menu->redraw = REDRAW_BODY;
        }
        break;

      case OP_PAGER_SKIP_QUOTED:
        if (rd.has_types)
        {
          int dretval = 0;
          int new_topline = rd.topline;

          /* Skip all the email headers */
          if (ISHEADER(rd.line_info[new_topline].type))
          {
            while ((new_topline < rd.last_line ||
                    (0 == (dretval = display_line(
                               rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                               &rd.max_line, MUTT_TYPES | (flags & MUTT_PAGER_NOWRAP),
                               &rd.quote_list, &rd.q_level, &rd.force_redraw,
                               &rd.search_re, rd.pager_window)))) &&
                   ISHEADER(rd.line_info[new_topline].type))
            {
              new_topline++;
            }
            rd.topline = new_topline;
            break;
          }

          while (((new_topline + SkipQuotedOffset) < rd.last_line ||
                  (0 == (dretval = display_line(
                             rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                             &rd.max_line, MUTT_TYPES | (flags & MUTT_PAGER_NOWRAP),
                             &rd.quote_list, &rd.q_level, &rd.force_redraw,
                             &rd.search_re, rd.pager_window)))) &&
                 rd.line_info[new_topline + SkipQuotedOffset].type != MT_COLOR_QUOTED)
            new_topline++;

          if (dretval < 0)
          {
            mutt_error(_("No more quoted text."));
            break;
          }

          while (((new_topline + SkipQuotedOffset) < rd.last_line ||
                  (0 == (dretval = display_line(
                             rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                             &rd.max_line, MUTT_TYPES | (flags & MUTT_PAGER_NOWRAP),
                             &rd.quote_list, &rd.q_level, &rd.force_redraw,
                             &rd.search_re, rd.pager_window)))) &&
                 rd.line_info[new_topline + SkipQuotedOffset].type == MT_COLOR_QUOTED)
            new_topline++;

          if (dretval < 0)
          {
            mutt_error(_("No more unquoted text after quoted text."));
            break;
          }
          rd.topline = new_topline;
        }
        break;

      case OP_PAGER_BOTTOM: /* move to the end of the file */
        if (rd.line_info[rd.curline].offset < rd.sb.st_size - 1)
        {
          i = rd.curline;
          /* make sure the types are defined to the end of file */
          while (display_line(rd.fp, &rd.last_pos, &rd.line_info, i, &rd.last_line,
                              &rd.max_line, rd.has_types | (flags & MUTT_PAGER_NOWRAP),
                              &rd.quote_list, &rd.q_level, &rd.force_redraw,
                              &rd.search_re, rd.pager_window) == 0)
            i++;
          rd.topline = up_n_lines(rd.pager_window->rows, rd.line_info,
                                  rd.last_line, rd.hide_quoted);
        }
        else
          mutt_error(_("Bottom of message is shown."));
        break;

      case OP_REDRAW:
        clearok(stdscr, true);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_NULL:
        km_error_key(MENU_PAGER);
        break;

      /* --------------------------------------------------------------------
         * The following are operations on the current message rather than
         * adjusting the view of the message.
         */

      case OP_BOUNCE_MESSAGE:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra))
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_bounce(extra->fp, extra->hdr, extra->actx, extra->bdy);
        else
          ci_bounce_message(extra->hdr);
        break;

      case OP_RESEND:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra))
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_resend(extra->fp, extra->hdr, extra->actx, extra->bdy);
        else
          mutt_resend_message(NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_COMPOSE_TO_SENDER:
        mutt_compose_to_sender(extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_CHECK_TRADITIONAL:
        CHECK_MODE(IsHeader(extra));
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!(extra->hdr->security & PGP_TRADITIONAL_CHECKED))
        {
          ch = -1;
          rc = OP_CHECK_TRADITIONAL;
        }
        break;

      case OP_CREATE_ALIAS:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        if (IsMsgAttach(extra))
          mutt_create_alias(extra->bdy->hdr->env, NULL);
        else
          mutt_create_alias(extra->hdr->env, NULL);
        break;

      case OP_PURGE_MESSAGE:
      case OP_DELETE:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message"));

        mutt_set_flag(Context, extra->hdr, MUTT_DELETE, 1);
        mutt_set_flag(Context, extra->hdr, MUTT_PURGE, (ch == OP_PURGE_MESSAGE));
        if (option(OPT_DELETE_UNTAG))
          mutt_set_flag(Context, extra->hdr, MUTT_TAG, 0);
        pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;

        if (mutt_change_flag(extra->hdr, (ch == OP_MAIN_SET_FLAG)) == 0)
          pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (extra->hdr->deleted && option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message(s)"));

        {
          int subthread = (ch == OP_DELETE_SUBTHREAD);
          r = mutt_thread_set_flag(extra->hdr, MUTT_DELETE, 1, subthread);
          if (r == -1)
            break;
          if (ch == OP_PURGE_THREAD)
          {
            r = mutt_thread_set_flag(extra->hdr, MUTT_PURGE, 1, subthread);
            if (r == -1)
              break;
          }

          if (option(OPT_DELETE_UNTAG))
            mutt_thread_set_flag(extra->hdr, MUTT_TAG, 0, subthread);
          if (option(OPT_RESOLVE))
          {
            rc = OP_MAIN_NEXT_UNDELETED;
            ch = -1;
          }

          if (!option(OPT_RESOLVE) && PagerIndexLines)
            pager_menu->redraw = REDRAW_FULL;
          else
            pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        break;

      case OP_DISPLAY_ADDRESS:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        if (IsMsgAttach(extra))
          mutt_display_address(extra->bdy->hdr->env);
        else
          mutt_display_address(extra->hdr->env);
        break;

      case OP_ENTER_COMMAND:
        old_PagerIndexLines = PagerIndexLines;

        mutt_enter_command();

        if (option(OPT_NEED_RESORT))
        {
          unset_option(OPT_NEED_RESORT);
          CHECK_MODE(IsHeader(extra));
          set_option(OPT_NEED_RESORT);
        }

        if (old_PagerIndexLines != PagerIndexLines)
        {
          if (rd.index)
            mutt_menu_destroy(&rd.index);
          rd.index = NULL;
        }

        if ((pager_menu->redraw & REDRAW_FLOW) && (flags & MUTT_PAGER_RETWINCH))
        {
          ch = -1;
          rc = OP_REFORMAT_WINCH;
          continue;
        }

        ch = 0;
        break;

      case OP_FLAG_MESSAGE:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_WRITE, "Cannot flag message");

        mutt_set_flag(Context, extra->hdr, MUTT_FLAG, !extra->hdr->flagged);
        pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;

      case OP_PIPE:
        CHECK_MODE(IsHeader(extra) || IsAttach(extra));
        if (IsAttach(extra))
          mutt_pipe_attachment_list(extra->actx, extra->fp, false, extra->bdy, false);
        else
          mutt_pipe_message(extra->hdr);
        break;

      case OP_PRINT:
        CHECK_MODE(IsHeader(extra) || IsAttach(extra));
        if (IsAttach(extra))
          mutt_print_attachment_list(extra->actx, extra->fp, false, extra->bdy);
        else
          mutt_print_message(extra->hdr);
        break;

      case OP_MAIL:
        CHECK_MODE(IsHeader(extra) && !IsAttach(extra));
        CHECK_ATTACH;
        ci_send_message(0, NULL, NULL, extra->ctx, NULL);
        pager_menu->redraw = REDRAW_FULL;
        break;

#ifdef USE_NNTP
      case OP_POST:
        CHECK_MODE(IsHeader(extra) && !IsAttach(extra));
        CHECK_ATTACH;
        if (extra->ctx && extra->ctx->magic == MUTT_NNTP &&
            !((struct NntpData *) extra->ctx->data)->allowed && query_quadoption(OPT_POST_MODERATED, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
          break;
        ci_send_message(SENDNEWS, NULL, NULL, extra->ctx, NULL);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_FORWARD_TO_GROUP:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;
        if (extra->ctx && extra->ctx->magic == MUTT_NNTP &&
            !((struct NntpData *) extra->ctx->data)->allowed && query_quadoption(OPT_POST_MODERATED, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
          break;
        if (IsMsgAttach(extra))
          mutt_attach_forward(extra->fp, extra->hdr, extra->actx, extra->bdy, SENDNEWS);
        else
          ci_send_message(SENDNEWS | SENDFORWARD, NULL, NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_FOLLOWUP:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;

        if (IsMsgAttach(extra))
          followup_to = extra->bdy->hdr->env->followup_to;
        else
          followup_to = extra->hdr->env->followup_to;

        if (!followup_to || (mutt_strcasecmp(followup_to, "poster") != 0) ||
            query_quadoption(OPT_FOLLOWUP_TO_POSTER,
                             _("Reply by mail as poster prefers?")) != MUTT_YES)
        {
          if (extra->ctx && extra->ctx->magic == MUTT_NNTP &&
              !((struct NntpData *) extra->ctx->data)->allowed && query_quadoption(OPT_POST_MODERATED, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
            break;
          if (IsMsgAttach(extra))
            mutt_attach_reply(extra->fp, extra->hdr, extra->actx, extra->bdy,
                              SENDNEWS | SENDREPLY);
          else
            ci_send_message(SENDNEWS | SENDREPLY, NULL, NULL, extra->ctx, extra->hdr);
          pager_menu->redraw = REDRAW_FULL;
          break;
        }
#endif

      case OP_REPLY:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_reply(extra->fp, extra->hdr, extra->actx, extra->bdy, SENDREPLY);
        else
          ci_send_message(SENDREPLY, NULL, NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_RECALL_MESSAGE:
        CHECK_MODE(IsHeader(extra) && !IsAttach(extra));
        CHECK_ATTACH;
        ci_send_message(SENDPOSTPONED, NULL, NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_GROUP_REPLY:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_reply(extra->fp, extra->hdr, extra->actx, extra->bdy,
                            SENDREPLY | SENDGROUPREPLY);
        else
          ci_send_message(SENDREPLY | SENDGROUPREPLY, NULL, NULL, extra->ctx,
                          extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_LIST_REPLY:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_reply(extra->fp, extra->hdr, extra->actx, extra->bdy,
                            SENDREPLY | SENDLISTREPLY);
        else
          ci_send_message(SENDREPLY | SENDLISTREPLY, NULL, NULL, extra->ctx,
                          extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_FORWARD_MESSAGE:
        CHECK_MODE(IsHeader(extra) || IsMsgAttach(extra));
        CHECK_ATTACH;
        if (IsMsgAttach(extra))
          mutt_attach_forward(extra->fp, extra->hdr, extra->actx, extra->bdy, 0);
        else
          ci_send_message(SENDFORWARD, NULL, NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_DECRYPT_SAVE:
        if (!WithCrypto)
        {
          ch = -1;
          break;
        }
      /* fall through */
      case OP_SAVE:
        if (IsAttach(extra))
        {
          mutt_save_attachment_list(extra->actx, extra->fp, false, extra->bdy,
                                    extra->hdr, NULL);
          break;
        }
      /* fall through */
      case OP_COPY_MESSAGE:
      case OP_DECODE_SAVE:
      case OP_DECODE_COPY:
      case OP_DECRYPT_COPY:
        if (!WithCrypto && ch == OP_DECRYPT_COPY)
        {
          ch = -1;
          break;
        }
        CHECK_MODE(IsHeader(extra));
        if (mutt_save_message(
                extra->hdr, (ch == OP_DECRYPT_SAVE) || (ch == OP_SAVE) || (ch == OP_DECODE_SAVE),
                (ch == OP_DECODE_SAVE) || (ch == OP_DECODE_COPY),
                (ch == OP_DECRYPT_SAVE) || (ch == OP_DECRYPT_COPY) || 0) == 0 &&
            (ch == OP_SAVE || ch == OP_DECODE_SAVE || ch == OP_DECRYPT_SAVE))
        {
          if (option(OPT_RESOLVE))
          {
            ch = -1;
            rc = OP_MAIN_NEXT_UNDELETED;
          }
          else
            pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        break;

      case OP_SHELL_ESCAPE:
        mutt_shell_escape();
        break;

      case OP_TAG:
        CHECK_MODE(IsHeader(extra));
        if (Context)
        {
          mutt_set_flag(Context, extra->hdr, MUTT_TAG, !extra->hdr->tagged);

          Context->last_tag =
              extra->hdr->tagged ?
                  extra->hdr :
                  ((Context->last_tag == extra->hdr && !extra->hdr->tagged) ?
                       NULL :
                       Context->last_tag);
        }

        pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;

      case OP_TOGGLE_NEW:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_SEEN, _("Cannot toggle new"));

        if (extra->hdr->read || extra->hdr->old)
          mutt_set_flag(Context, extra->hdr, MUTT_NEW, 1);
        else if (!first)
          mutt_set_flag(Context, extra->hdr, MUTT_READ, 1);
        first = 0;
        Context->msgnotreadyet = -1;
        pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;

      case OP_UNDELETE:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message"));

        mutt_set_flag(Context, extra->hdr, MUTT_DELETE, 0);
        mutt_set_flag(Context, extra->hdr, MUTT_PURGE, 0);
        pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        if (option(OPT_RESOLVE))
        {
          ch = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:
        CHECK_MODE(IsHeader(extra));
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message(s)"));

        r = mutt_thread_set_flag(extra->hdr, MUTT_DELETE, 0,
                                 ch == OP_UNDELETE_THREAD ? 0 : 1);
        if (r != -1)
          r = mutt_thread_set_flag(extra->hdr, MUTT_PURGE, 0,
                                   ch == OP_UNDELETE_THREAD ? 0 : 1);
        if (r != -1)
        {
          if (option(OPT_RESOLVE))
          {
            rc = (ch == OP_DELETE_THREAD) ? OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
            ch = -1;
          }

          if (!option(OPT_RESOLVE) && PagerIndexLines)
            pager_menu->redraw = REDRAW_FULL;
          else
            pager_menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        break;

      case OP_VERSION:
        mutt_version();
        break;

      case OP_BUFFY_LIST:
        mutt_buffy_list();
        break;

      case OP_VIEW_ATTACHMENTS:
        if (flags & MUTT_PAGER_ATTACHMENT)
        {
          ch = -1;
          rc = OP_ATTACH_COLLAPSE;
          break;
        }
        CHECK_MODE(IsHeader(extra));
        mutt_view_attachments(extra->hdr);
        if (Context && extra->hdr->attach_del)
          Context->changed = true;
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIL_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
        {
          ch = -1;
          break;
        }
        CHECK_MODE(IsHeader(extra));
        CHECK_ATTACH;
        ci_send_message(SENDKEY, NULL, NULL, extra->ctx, extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_EDIT_LABEL:
        CHECK_MODE(IsHeader(extra));
        rc = mutt_label_message(extra->hdr);
        if (rc > 0)
        {
          Context->changed = true;
          pager_menu->redraw = REDRAW_FULL;
          mutt_message(_("%d labels changed."), rc);
        }
        else
        {
          mutt_message(_("No labels changed."));
        }
        break;

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_EXTRACT_KEYS:
        if (!WithCrypto)
        {
          ch = -1;
          break;
        }
        CHECK_MODE(IsHeader(extra));
        crypt_extract_keys_from_messages(extra->hdr);
        pager_menu->redraw = REDRAW_FULL;
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_NEXT:
      case OP_SIDEBAR_NEXT_NEW:
      case OP_SIDEBAR_PAGE_DOWN:
      case OP_SIDEBAR_PAGE_UP:
      case OP_SIDEBAR_PREV:
      case OP_SIDEBAR_PREV_NEW:
        mutt_sb_change_mailbox(ch);
        break;

      case OP_SIDEBAR_TOGGLE_VISIBLE:
        toggle_option(OPT_SIDEBAR_VISIBLE);
        mutt_reflow_windows();
        break;
#endif

      default:
        ch = -1;
        break;
    }
  }

  safe_fclose(&rd.fp);
  if (IsHeader(extra))
  {
    if (Context)
      Context->msgnotreadyet = -1;
    switch (rc)
    {
      case -1:
      case OP_DISPLAY_HEADERS:
        mutt_clear_pager_position();
        break;
      default:
        TopLine = rd.topline;
        OldHdr = extra->hdr;
        break;
    }
  }

  cleanup_quote(&rd.quote_list);

  for (i = 0; i < rd.max_line; i++)
  {
    FREE(&(rd.line_info[i].syntax));
    if (rd.search_compiled && rd.line_info[i].search)
      FREE(&(rd.line_info[i].search));
  }
  if (rd.search_compiled)
  {
    regfree(&rd.search_re);
    rd.search_compiled = 0;
  }
  FREE(&rd.line_info);
  mutt_pop_current_menu(pager_menu);
  mutt_menu_destroy(&pager_menu);
  if (rd.index)
    mutt_menu_destroy(&rd.index);

  FREE(&rd.index_status_window);
  FREE(&rd.index_window);
  FREE(&rd.pager_status_window);
  FREE(&rd.pager_window);

  return (rc != -1 ? rc : 0);
}
