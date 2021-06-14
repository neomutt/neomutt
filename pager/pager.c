/**
 * @file
 * GUI display a file/email/help in a viewport with paging
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page pager_pager GUI display a file/email/help in a viewport with paging
 *
 * GUI display a file/email/help in a viewport with paging
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "pbar.h"
#include "private_data.h"
#include "protos.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/mdata.h" // IWYU pragma: keep
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// clang-format off
typedef uint8_t AnsiFlags;      ///< Flags, e.g. #ANSI_OFF
#define ANSI_NO_FLAGS        0  ///< No flags are set
#define ANSI_OFF       (1 << 0) ///< Turn off colours and attributes
#define ANSI_BLINK     (1 << 1) ///< Blinking text
#define ANSI_BOLD      (1 << 2) ///< Bold text
#define ANSI_UNDERLINE (1 << 3) ///< Underlined text
#define ANSI_REVERSE   (1 << 4) ///< Reverse video
#define ANSI_COLOR     (1 << 5) ///< Use colours
// clang-format on

/**
 * struct QClass - Style of quoted text
 */
struct QClass
{
  size_t length;
  int index;
  int color;
  char *prefix;
  struct QClass *next, *prev;
  struct QClass *down, *up;
};

/**
 * struct TextSyntax - Highlighting for a line of text
 */
struct TextSyntax
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
  struct TextSyntax *syntax;
  struct TextSyntax *search;
  struct QClass *quote;
  unsigned int is_cont_hdr; ///< this line is a continuation of the previous header line
};

/**
 * struct AnsiAttr - An ANSI escape sequence
 */
struct AnsiAttr
{
  AnsiFlags attr; ///< Attributes, e.g. underline, bold, etc
  int fg;         ///< Foreground colour
  int bg;         ///< Background colour
  int pair;       ///< Curses colour pair
};

/**
 * struct Resize - Keep track of screen resizing
 */
struct Resize
{
  int line;
  bool search_compiled;
  bool search_back;
};

/* hack to return to position when returning from index to same message */
static int TopLine = 0;
static struct Email *OldEmail = NULL;

static int braille_row = -1;
static int braille_col = -1;

static struct Resize *Resize = NULL;

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode");

/// Help Bar for the Pager's Help Page
static const struct Mapping PagerHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/// Help Bar for the Help Page itself
static const struct Mapping PagerHelpHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { NULL, 0 },
  // clang-format on
};

/// Help Bar for the Pager of a normal Mailbox
static const struct Mapping PagerNormalHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("View Attachm."), OP_VIEW_ATTACHMENTS },
  { N_("Del"),           OP_DELETE },
  { N_("Reply"),         OP_REPLY },
  { N_("Next"),          OP_MAIN_NEXT_UNDELETED },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};

#ifdef USE_NNTP
/// Help Bar for the Pager of an NNTP Mailbox
static const struct Mapping PagerNewsHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("Post"),          OP_POST },
  { N_("Followup"),      OP_FOLLOWUP },
  { N_("Del"),           OP_DELETE },
  { N_("Next"),          OP_MAIN_NEXT_UNDELETED },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};
#endif

/**
 * assert_pager_mode - Check that pager is in correct mode
 * @param test   Test condition
 * @retval true  Expected mode is set
 * @retval false Pager is is some other mode
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_pager_mode(bool test)
{
  if (test)
    return true;

  mutt_flushinp();
  mutt_error(_(Not_available_in_this_menu));
  return false;
}

/**
 * assert_mailbox_writable - checks that mailbox is writable
 * @param mailbox mailbox to check
 * @retval true  Mailbox is writable
 * @retval false Mailbox is not writable
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_writable(struct Mailbox *mailbox)
{
  assert(mailbox);
  if (mailbox->readonly)
  {
    mutt_flushinp();
    mutt_error(_(Mailbox_is_read_only));
    return false;
  }
  return true;
}

/**
 * assert_attach_msg_mode - Check that attach message mode is on
 * @param attach_msg Globally-named boolean pseudo-option
 * @retval true  Attach message mode in on
 * @retval false Attach message mode is off
 *
 * @note On true, the input will be flushed and an error message displayed
 */
static inline bool assert_attach_msg_mode(bool attach_msg)
{
  if (attach_msg)
  {
    mutt_flushinp();
    mutt_error(_(Function_not_permitted_in_attach_message_mode));
    return true;
  }
  return false;
}

/**
 * assert_mailbox_permissions - checks that mailbox is has requested acl flags set
 * @param m      Mailbox to check
 * @param acl    AclFlags required to be set on a given mailbox
 * @param action String to augment error message
 * @retval true  Mailbox has necessary flags set
 * @retval false Mailbox does not have necessary flags set
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_permissions(struct Mailbox *m, AclFlags acl, char *action)
{
  assert(m);
  assert(action);
  if (m->rights & acl)
  {
    return true;
  }
  mutt_flushinp();
  /* L10N: %s is one of the CHECK_ACL entries below. */
  mutt_error(_("%s: Operation not permitted by ACL"), action);
  return false;
}

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
    if (info[offset].type != MT_COLOR_SIGNATURE)
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
 * comp_syntax_t - Search for a Syntax using bsearch
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
 * @param line_info Line info array
 * @param n         Line Number (index into line_info)
 * @param cnt       If true, this is a continuation line
 * @param flags     Flags, see #PagerFlags
 * @param special   Flags, e.g. A_BOLD
 * @param a         ANSI attributes
 */
static void resolve_color(struct MuttWindow *win, struct Line *line_info, int n,
                          int cnt, PagerFlags flags, int special, struct AnsiAttr *a)
{
  int def_color;         /* color without syntax highlight */
  int color;             /* final color */
  static int last_color; /* last color set */
  bool search = false;
  int m;
  struct TextSyntax *matching_chunk = NULL;

  if (cnt == 0)
    last_color = -1; /* force attrset() */

  if (line_info[n].continuation)
  {
    const bool c_markers = cs_subset_bool(NeoMutt->sub, "markers");
    if (!cnt && c_markers)
    {
      mutt_curses_set_color(MT_COLOR_MARKERS);
      mutt_window_addch(win, '+');
      last_color = mutt_color(MT_COLOR_MARKERS);
    }
    m = (line_info[n].syntax)[0].first;
    cnt += (line_info[n].syntax)[0].last;
  }
  else
    m = n;
  if (flags & MUTT_PAGER_LOGS)
  {
    def_color = mutt_color(line_info[n].syntax[0].color);
  }
  else if (!(flags & MUTT_SHOWCOLOR))
    def_color = mutt_color(MT_COLOR_NORMAL);
  else if (line_info[m].type == MT_COLOR_HEADER)
    def_color = line_info[m].syntax[0].color;
  else
    def_color = mutt_color(line_info[m].type);

  if ((flags & MUTT_SHOWCOLOR) && (line_info[m].type == MT_COLOR_QUOTED))
  {
    struct QClass *qc = line_info[m].quote;

    if (qc)
    {
      def_color = qc->color;

      while (qc && (qc->length > cnt))
      {
        def_color = qc->color;
        qc = qc->up;
      }
    }
  }

  color = def_color;
  if ((flags & MUTT_SHOWCOLOR) && line_info[m].chunks)
  {
    matching_chunk = bsearch(&cnt, line_info[m].syntax, line_info[m].chunks,
                             sizeof(struct TextSyntax), comp_syntax_t);
    if (matching_chunk && (cnt >= matching_chunk->first) &&
        (cnt < matching_chunk->last))
    {
      color = matching_chunk->color;
    }
  }

  if ((flags & MUTT_SEARCH) && line_info[m].search_cnt)
  {
    matching_chunk = bsearch(&cnt, line_info[m].search, line_info[m].search_cnt,
                             sizeof(struct TextSyntax), comp_syntax_t);
    if (matching_chunk && (cnt >= matching_chunk->first) &&
        (cnt < matching_chunk->last))
    {
      color = mutt_color(MT_COLOR_SEARCH);
      search = 1;
    }
  }

  /* handle "special" bold & underlined characters */
  if (special || a->attr)
  {
#ifdef HAVE_COLOR
    if ((a->attr & ANSI_COLOR))
    {
      if (a->pair == -1)
        a->pair = mutt_color_alloc(a->fg, a->bg);
      color = a->pair;
      if (a->attr & ANSI_BOLD)
        color |= A_BOLD;
    }
    else
#endif
        if ((special & A_BOLD) || (a->attr & ANSI_BOLD))
    {
      if (mutt_color(MT_COLOR_BOLD) && !search)
        color = mutt_color(MT_COLOR_BOLD);
      else
        color ^= A_BOLD;
    }
    if ((special & A_UNDERLINE) || (a->attr & ANSI_UNDERLINE))
    {
      if (mutt_color(MT_COLOR_UNDERLINE) && !search)
        color = mutt_color(MT_COLOR_UNDERLINE);
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
    else if (a->attr == ANSI_OFF)
    {
      a->attr = 0;
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
 * @param line_info Array of Line info
 * @param n         Line number to add
 * @param cnt       true, if line is a continuation
 */
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

/**
 * class_color_new - Create a new quoting colour
 * @param[in]     qc      Class of quoted text
 * @param[in,out] q_level Quote level
 */
static void class_color_new(struct QClass *qc, int *q_level)
{
  qc->index = (*q_level)++;
  qc->color = mutt_color_quote(qc->index);
}

/**
 * shift_class_colors - Insert a new quote colour class into a list
 * @param[in]     quote_list List of quote colours
 * @param[in]     new_class  New quote colour to inset
 * @param[in]     index      Index to insert at
 * @param[in,out] q_level    Quote level
 */
static void shift_class_colors(struct QClass *quote_list,
                               struct QClass *new_class, int index, int *q_level)
{
  struct QClass *q_list = quote_list;
  new_class->index = -1;

  while (q_list)
  {
    if (q_list->index >= index)
    {
      q_list->index++;
      q_list->color = mutt_color_quote(q_list->index);
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
  new_class->color = mutt_color_quote(index);
  (*q_level)++;
}

/**
 * cleanup_quote - Free a quote list
 * @param[out] quote_list Quote list to free
 */
static void cleanup_quote(struct QClass **quote_list)
{
  struct QClass *ptr = NULL;

  while (*quote_list)
  {
    if ((*quote_list)->down)
      cleanup_quote(&((*quote_list)->down));
    ptr = (*quote_list)->next;
    FREE(&(*quote_list)->prefix);
    FREE(quote_list);
    *quote_list = ptr;
  }
}

/**
 * classify_quote - Find a style for a string
 * @param[out] quote_list   List of quote colours
 * @param[in]  qptr         String to classify
 * @param[in]  length       Length of string
 * @param[out] force_redraw Set to true if a screen redraw is needed
 * @param[out] q_level      Quoting level
 * @retval ptr Quoting style
 */
static struct QClass *classify_quote(struct QClass **quote_list, const char *qptr,
                                     size_t length, bool *force_redraw, int *q_level)
{
  struct QClass *q_list = *quote_list;
  struct QClass *qc = NULL, *tmp = NULL, *ptr = NULL, *save = NULL;
  const char *tail_qptr = NULL;
  size_t offset, tail_lng;
  int index = -1;

  if (mutt_color_quotes_used() <= 1)
  {
    /* not much point in classifying quotes... */

    if (!*quote_list)
    {
      qc = mutt_mem_calloc(1, sizeof(struct QClass));
      qc->color = mutt_color_quote(0);
      *quote_list = qc;
    }
    return *quote_list;
  }

  /* classify quoting prefix */
  while (q_list)
  {
    if (length <= q_list->length)
    {
      /* case 1: check the top level nodes */

      if (mutt_strn_equal(qptr, q_list->prefix, length))
      {
        if (length == q_list->length)
          return q_list; /* same prefix: return the current class */

        /* found shorter prefix */
        if (!tmp)
        {
          /* add a node above q_list */
          tmp = mutt_mem_calloc(1, sizeof(struct QClass));
          tmp->prefix = mutt_mem_calloc(1, length + 1);
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
          qc = tmp;

          /* next class to test; if tmp is a shorter prefix for another
           * node, that node can only be in the top level list, so don't
           * go down after this point */
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
        *force_redraw = true;
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
      if (!tmp && mutt_strn_equal(qptr, q_list->prefix, q_list->length))
      {
        /* ok, it's a subclass somewhere on this branch */

        ptr = q_list;
        offset = q_list->length;

        q_list = q_list->down;
        tail_lng = length - offset;
        tail_qptr = qptr + offset;

        while (q_list)
        {
          if (length <= q_list->length)
          {
            if (mutt_strn_equal(tail_qptr, (q_list->prefix) + offset, tail_lng))
            {
              /* same prefix: return the current class */
              if (length == q_list->length)
                return q_list;

              /* found shorter common prefix */
              if (!tmp)
              {
                /* add a node above q_list */
                tmp = mutt_mem_calloc(1, sizeof(struct QClass));
                tmp->prefix = mutt_mem_calloc(1, length + 1);
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
                qc = tmp;

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
              *force_redraw = true;
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
            if (!tmp && mutt_strn_equal(tail_qptr, (q_list->prefix) + offset,
                                        q_list->length - offset))
            {
              /* still a subclass: go down one level */
              ptr = q_list;
              offset = q_list->length;

              q_list = q_list->down;
              tail_lng = length - offset;
              tail_qptr = qptr + offset;

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
        if (!qc)
        {
          tmp = mutt_mem_calloc(1, sizeof(struct QClass));
          tmp->prefix = mutt_mem_calloc(1, length + 1);
          strncpy(tmp->prefix, qptr, length);
          tmp->length = length;

          if (ptr->down)
          {
            tmp->next = ptr->down;
            ptr->down->prev = tmp;
          }
          ptr->down = tmp;
          tmp->up = ptr;

          class_color_new(tmp, q_level);

          return tmp;
        }
        else
        {
          if (index != -1)
            shift_class_colors(*quote_list, tmp, index, q_level);

          return qc;
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

  if (!qc)
  {
    /* not found so far: add it as a top level class */
    qc = mutt_mem_calloc(1, sizeof(struct QClass));
    qc->prefix = mutt_mem_calloc(1, length + 1);
    strncpy(qc->prefix, qptr, length);
    qc->length = length;
    class_color_new(qc, q_level);

    if (*quote_list)
    {
      qc->next = *quote_list;
      (*quote_list)->prev = qc;
    }
    *quote_list = qc;
  }

  if (index != -1)
    shift_class_colors(*quote_list, tmp, index, q_level);

  return qc;
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
 * This is used by the pager for calling classify_quote.
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
 * @param[in]  line_info    Line info array
 * @param[in]  n            Line number (index into line_info)
 * @param[in]  last         Last line
 * @param[out] quote_list   List of quote colours
 * @param[out] q_level      Quote level
 * @param[out] force_redraw Set to true if a screen redraw is needed
 * @param[in]  q_classify   If true, style the text
 */
static void resolve_types(struct MuttWindow *win, char *buf, char *raw,
                          struct Line *line_info, int n, int last, struct QClass **quote_list,
                          int *q_level, bool *force_redraw, bool q_classify)
{
  struct ColorLine *color_line = NULL;
  struct ColorLineList *head = NULL;
  regmatch_t pmatch[1];
  bool found;
  bool null_rx;
  const bool c_header_color_partial =
      cs_subset_bool(NeoMutt->sub, "header_color_partial");
  int offset, i = 0;

  if ((n == 0) || mutt_color_is_header(line_info[n - 1].type) ||
      (check_protected_header_marker(raw) == 0))
  {
    if (buf[0] == '\n') /* end of header */
    {
      line_info[n].type = MT_COLOR_NORMAL;
      mutt_window_get_coords(win, &braille_col, &braille_row);
    }
    else
    {
      /* if this is a continuation of the previous line, use the previous
       * line's color as default. */
      if ((n > 0) && ((buf[0] == ' ') || (buf[0] == '\t')))
      {
        line_info[n].type = line_info[n - 1].type; /* wrapped line */
        if (!c_header_color_partial)
        {
          (line_info[n].syntax)[0].color = (line_info[n - 1].syntax)[0].color;
          line_info[n].is_cont_hdr = 1;
        }
      }
      else
      {
        line_info[n].type = MT_COLOR_HDRDEFAULT;
      }

      /* When this option is unset, we color the entire header the
       * same color.  Otherwise, we handle the header patterns just
       * like body patterns (further below).  */
      if (!c_header_color_partial)
      {
        STAILQ_FOREACH(color_line, mutt_color_headers(), entries)
        {
          if (regexec(&color_line->regex, buf, 0, NULL, 0) == 0)
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
              *force_redraw = true; /* the previous lines have already been drawn on the screen */
            }
            break;
          }
        }
      }
    }
  }
  else if (mutt_str_startswith(raw, "\033[0m")) // Escape: a little hack...
    line_info[n].type = MT_COLOR_NORMAL;
  else if (check_attachment_marker((char *) raw) == 0)
    line_info[n].type = MT_COLOR_ATTACHMENT;
  else if (mutt_str_equal("-- \n", buf) || mutt_str_equal("-- \r\n", buf))
  {
    i = n + 1;

    line_info[n].type = MT_COLOR_SIGNATURE;
    while ((i < last) && (check_sig(buf, line_info, i - 1) == 0) &&
           ((line_info[i].type == MT_COLOR_NORMAL) || (line_info[i].type == MT_COLOR_QUOTED) ||
            (line_info[i].type == MT_COLOR_HEADER)))
    {
      /* oops... */
      if (line_info[i].chunks)
      {
        line_info[i].chunks = 0;
        mutt_mem_realloc(&(line_info[n].syntax), sizeof(struct TextSyntax));
      }
      line_info[i++].type = MT_COLOR_SIGNATURE;
    }
  }
  else if (check_sig(buf, line_info, n - 1) == 0)
    line_info[n].type = MT_COLOR_SIGNATURE;
  else if (mutt_is_quote_line(buf, pmatch))

  {
    if (q_classify && (line_info[n].quote == NULL))
    {
      line_info[n].quote = classify_quote(quote_list, buf + pmatch[0].rm_so,
                                          pmatch[0].rm_eo - pmatch[0].rm_so,
                                          force_redraw, q_level);
    }
    line_info[n].type = MT_COLOR_QUOTED;
  }
  else
    line_info[n].type = MT_COLOR_NORMAL;

  /* body patterns */
  if ((line_info[n].type == MT_COLOR_NORMAL) || (line_info[n].type == MT_COLOR_QUOTED) ||
      ((line_info[n].type == MT_COLOR_HDRDEFAULT) && c_header_color_partial))
  {
    size_t nl;

    /* don't consider line endings part of the buffer
     * for regex matching */
    nl = mutt_str_len(buf);
    if ((nl > 0) && (buf[nl - 1] == '\n'))
      buf[nl - 1] = '\0';

    i = 0;
    offset = 0;
    line_info[n].chunks = 0;
    if (line_info[n].type == MT_COLOR_HDRDEFAULT)
      head = mutt_color_headers();
    else
      head = mutt_color_body();
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
          if (pmatch[0].rm_eo != pmatch[0].rm_so)
          {
            if (!found)
            {
              /* Abort if we fill up chunks.
               * Yes, this really happened. */
              if (line_info[n].chunks == SHRT_MAX)
              {
                null_rx = false;
                break;
              }
              if (++(line_info[n].chunks) > 1)
              {
                mutt_mem_realloc(&(line_info[n].syntax),
                                 (line_info[n].chunks) * sizeof(struct TextSyntax));
              }
            }
            i = line_info[n].chunks - 1;
            pmatch[0].rm_so += offset;
            pmatch[0].rm_eo += offset;
            if (!found || (pmatch[0].rm_so < (line_info[n].syntax)[i].first) ||
                ((pmatch[0].rm_so == (line_info[n].syntax)[i].first) &&
                 (pmatch[0].rm_eo > (line_info[n].syntax)[i].last)))
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
    nl = mutt_str_len(buf);
    if ((nl > 0) && (buf[nl - 1] == '\n'))
      buf[nl - 1] = '\0';

    i = 0;
    offset = 0;
    line_info[n].chunks = 0;
    do
    {
      if (!buf[offset])
        break;

      found = false;
      null_rx = false;
      STAILQ_FOREACH(color_line, mutt_color_attachments(), entries)
      {
        if (regexec(&color_line->regex, buf + offset, 1, pmatch,
                    ((offset != 0) ? REG_NOTBOL : 0)) == 0)
        {
          if (pmatch[0].rm_eo != pmatch[0].rm_so)
          {
            if (!found)
            {
              if (++(line_info[n].chunks) > 1)
              {
                mutt_mem_realloc(&(line_info[n].syntax),
                                 (line_info[n].chunks) * sizeof(struct TextSyntax));
              }
            }
            i = line_info[n].chunks - 1;
            pmatch[0].rm_so += offset;
            pmatch[0].rm_eo += offset;
            if (!found || (pmatch[0].rm_so < (line_info[n].syntax)[i].first) ||
                ((pmatch[0].rm_so == (line_info[n].syntax)[i].first) &&
                 (pmatch[0].rm_eo > (line_info[n].syntax)[i].last)))
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

/**
 * is_ansi - Is this an ANSI escape sequence?
 * @param str String to test
 * @retval true It's an ANSI escape sequence
 */
static bool is_ansi(const char *str)
{
  while (*str && (isdigit(*str) || *str == ';'))
    str++;
  return (*str == 'm');
}

/**
 * grok_ansi - Parse an ANSI escape sequence
 * @param buf String to parse
 * @param pos Starting position in string
 * @param a   AnsiAttr for the result
 * @retval num Index of first character after the escape sequence
 */
static int grok_ansi(const unsigned char *buf, int pos, struct AnsiAttr *a)
{
  int x = pos;

  while (isdigit(buf[x]) || (buf[x] == ';'))
    x++;

  /* Character Attributes */
  const bool c_allow_ansi = cs_subset_bool(NeoMutt->sub, "allow_ansi");
  if (c_allow_ansi && a && (buf[x] == 'm'))
  {
    if (pos == x)
    {
#ifdef HAVE_COLOR
      if (a->pair != -1)
        mutt_color_free(a->fg, a->bg);
#endif
      a->attr = ANSI_OFF;
      a->pair = -1;
    }
    while (pos < x)
    {
      if ((buf[pos] == '1') && ((pos + 1 == x) || (buf[pos + 1] == ';')))
      {
        a->attr |= ANSI_BOLD;
        pos += 2;
      }
      else if ((buf[pos] == '4') && ((pos + 1 == x) || (buf[pos + 1] == ';')))
      {
        a->attr |= ANSI_UNDERLINE;
        pos += 2;
      }
      else if ((buf[pos] == '5') && ((pos + 1 == x) || (buf[pos + 1] == ';')))
      {
        a->attr |= ANSI_BLINK;
        pos += 2;
      }
      else if ((buf[pos] == '7') && ((pos + 1 == x) || (buf[pos + 1] == ';')))
      {
        a->attr |= ANSI_REVERSE;
        pos += 2;
      }
      else if ((buf[pos] == '0') && ((pos + 1 == x) || (buf[pos + 1] == ';')))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_color_free(a->fg, a->bg);
#endif
        a->attr = ANSI_OFF;
        a->pair = -1;
        pos += 2;
      }
      else if ((buf[pos] == '3') && isdigit(buf[pos + 1]))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_color_free(a->fg, a->bg);
#endif
        a->pair = -1;
        a->attr |= ANSI_COLOR;
        a->fg = buf[pos + 1] - '0';
        pos += 3;
      }
      else if ((buf[pos] == '4') && isdigit(buf[pos + 1]))
      {
#ifdef HAVE_COLOR
        if (a->pair != -1)
          mutt_color_free(a->fg, a->bg);
#endif
        a->pair = -1;
        a->attr |= ANSI_COLOR;
        a->bg = buf[pos + 1] - '0';
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
 * @param[in]     fp        File to read from
 * @param[in,out] last_pos  End of last read
 * @param[in]     offset    Position start reading from
 * @param[out]    buf       Buffer to fill
 * @param[out]    fmt       Copy of buffer, stripped of attributes
 * @param[out]    blen      Length of the buffer
 * @param[in,out] buf_ready true if the buffer already has data in it
 * @retval >=0 Bytes read
 * @retval -1  Error
 */
static int fill_buffer(FILE *fp, LOFF_T *last_pos, LOFF_T offset, unsigned char **buf,
                       unsigned char **fmt, size_t *blen, int *buf_ready)
{
  static int b_read;
  struct Buffer stripped;

  if (*buf_ready == 0)
  {
    if (offset != *last_pos)
      fseeko(fp, offset, SEEK_SET);

    *buf = (unsigned char *) mutt_file_read_line((char *) *buf, blen, fp, NULL, MUTT_RL_EOL);
    if (!*buf)
    {
      fmt[0] = NULL;
      return -1;
    }

    *last_pos = ftello(fp);
    b_read = (int) (*last_pos - offset);
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
 * @param[out] line_info Line info
 * @param[in]  n         Line number (index into line_info)
 * @param[in]  buf       Text to display
 * @param[in]  flags     Flags, see #PagerFlags
 * @param[out] pa        ANSI attributes used
 * @param[in]  cnt       Length of text buffer
 * @param[out] pspace    Index of last whitespace character
 * @param[out] pvch      Number of bytes read
 * @param[out] pcol      Number of columns used
 * @param[out] pspecial  Attribute flags, e.g. A_UNDERLINE
 * @param[in]  width     Width of screen (to wrap to)
 * @retval num Number of characters displayed
 */
static int format_line(struct MuttWindow *win, struct Line **line_info, int n,
                       unsigned char *buf, PagerFlags flags, struct AnsiAttr *pa,
                       int cnt, int *pspace, int *pvch, int *pcol, int *pspecial, int width)
{
  int space = -1; /* index of the last space or TAB */
  const bool c_markers = cs_subset_bool(NeoMutt->sub, "markers");
  size_t col = c_markers ? (*line_info)[n].continuation : 0;
  size_t k;
  int ch, vch, last_special = -1, special = 0, t;
  wchar_t wc;
  mbstate_t mbstate;
  const size_t c_wrap = cs_subset_number(NeoMutt->sub, "wrap");
  size_t wrap_cols = mutt_window_wrap_cols(width, (flags & MUTT_PAGER_NOWRAP) ? 0 : c_wrap);

  if (check_attachment_marker((char *) buf) == 0)
    wrap_cols = width;

  /* FIXME: this should come from line_info */
  memset(&mbstate, 0, sizeof(mbstate));

  for (ch = 0, vch = 0; ch < cnt; ch += k, vch += k)
  {
    /* Handle ANSI sequences */
    while ((cnt - ch >= 2) && (buf[ch] == '\033') && (buf[ch + 1] == '[') && // Escape
           is_ansi((char *) buf + ch + 2))
    {
      ch = grok_ansi(buf, ch + 2, pa) + 1;
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
      if (pa)
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

    if (pa && ((flags & (MUTT_SHOWCOLOR | MUTT_SEARCH | MUTT_PAGER_MARKER)) ||
               special || last_special || pa->attr))
    {
      resolve_color(win, *line_info, n, vch, flags, special, pa);
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
      if (pa)
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
      if (pa)
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
      if (pa)
        mutt_window_printf(win, "^%c", ('@' + wc) & 0x7f);
    }
    else if (wc < 0x100)
    {
      if (col + 4 > wrap_cols)
        break;
      col += 4;
      if (pa)
        mutt_window_printf(win, "\\%03o", wc);
    }
    else
    {
      if (col + 1 > wrap_cols)
        break;
      col += k;
      if (pa)
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
 * @param[out] last_pos        Offset into file
 * @param[out] line_info       Line attributes
 * @param[in]  n               Line number
 * @param[out] last            Last line
 * @param[out] max             Maximum number of lines
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
static int display_line(FILE *fp, LOFF_T *last_pos, struct Line **line_info,
                        int n, int *last, int *max, PagerFlags flags,
                        struct QClass **quote_list, int *q_level, bool *force_redraw,
                        regex_t *search_re, struct MuttWindow *win_pager)
{
  unsigned char *buf = NULL, *fmt = NULL;
  size_t buflen = 0;
  unsigned char *buf_ptr = NULL;
  int ch, vch, col, cnt, b_read;
  int buf_ready = 0;
  bool change_last = false;
  const bool c_smart_wrap = cs_subset_bool(NeoMutt->sub, "smart_wrap");
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
    mutt_mem_realloc(line_info, sizeof(struct Line) * (*max += LINES));
    for (ch = *last; ch < *max; ch++)
    {
      memset(&((*line_info)[ch]), 0, sizeof(struct Line));
      (*line_info)[ch].type = -1;
      (*line_info)[ch].search_cnt = -1;
      (*line_info)[ch].syntax = mutt_mem_malloc(sizeof(struct TextSyntax));
      ((*line_info)[ch].syntax)[0].first = -1;
      ((*line_info)[ch].syntax)[0].last = -1;
    }
  }

  struct Line *const curr_line = &(*line_info)[n];

  if (flags & MUTT_PAGER_LOGS)
  {
    /* determine the line class */
    if (fill_buffer(fp, last_pos, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*last)--;
      goto out;
    }

    curr_line->type = MT_COLOR_MESSAGE_LOG;
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
    if (curr_line->type == -1)
    {
      /* determine the line class */
      if (fill_buffer(fp, last_pos, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
      {
        if (change_last)
          (*last)--;
        goto out;
      }

      resolve_types(win_pager, (char *) fmt, (char *) buf, *line_info, n, *last,
                    quote_list, q_level, force_redraw, flags & MUTT_SHOWCOLOR);

      /* avoid race condition for continuation lines when scrolling up */
      for (m = n + 1; m < *last && (*line_info)[m].offset && (*line_info)[m].continuation; m++)
        (*line_info)[m].type = curr_line->type;
    }

    /* this also prevents searching through the hidden lines */
    const short c_toggle_quoted_show_levels =
        cs_subset_number(NeoMutt->sub, "toggle_quoted_show_levels");
    if ((flags & MUTT_HIDE) && (curr_line->type == MT_COLOR_QUOTED) &&
        ((curr_line->quote == NULL) || (curr_line->quote->index >= c_toggle_quoted_show_levels)))
    {
      flags = 0; /* MUTT_NOSHOW */
    }
  }

  /* At this point, (*line_info[n]).quote may still be undefined. We
   * don't want to compute it every time MUTT_TYPES is set, since this
   * would slow down the "bottom" function unacceptably. A compromise
   * solution is hence to call regexec() again, just to find out the
   * length of the quote prefix.  */
  if ((flags & MUTT_SHOWCOLOR) && !curr_line->continuation &&
      (curr_line->type == MT_COLOR_QUOTED) && !curr_line->quote)
  {
    if (fill_buffer(fp, last_pos, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*last)--;
      goto out;
    }

    const struct Regex *c_quote_regex =
        cs_subset_regex(NeoMutt->sub, "quote_regex");
    if (mutt_regex_capture(c_quote_regex, (char *) fmt, 1, pmatch))
    {
      curr_line->quote =
          classify_quote(quote_list, (char *) fmt + pmatch[0].rm_so,
                         pmatch[0].rm_eo - pmatch[0].rm_so, force_redraw, q_level);
    }
    else
    {
      goto out;
    }
  }

  if ((flags & MUTT_SEARCH) && !curr_line->continuation && (curr_line->search_cnt == -1))
  {
    if (fill_buffer(fp, last_pos, curr_line->offset, &buf, &fmt, &buflen, &buf_ready) < 0)
    {
      if (change_last)
        (*last)--;
      goto out;
    }

    offset = 0;
    curr_line->search_cnt = 0;
    while (regexec(search_re, (char *) fmt + offset, 1, pmatch,
                   (offset ? REG_NOTBOL : 0)) == 0)
    {
      if (++(curr_line->search_cnt) > 1)
      {
        mutt_mem_realloc(&(curr_line->search),
                         (curr_line->search_cnt) * sizeof(struct TextSyntax));
      }
      else
        curr_line->search = mutt_mem_malloc(sizeof(struct TextSyntax));
      pmatch[0].rm_so += offset;
      pmatch[0].rm_eo += offset;
      (curr_line->search)[curr_line->search_cnt - 1].first = pmatch[0].rm_so;
      (curr_line->search)[curr_line->search_cnt - 1].last = pmatch[0].rm_eo;

      if (pmatch[0].rm_eo == pmatch[0].rm_so)
        offset++; /* avoid degenerate cases */
      else
        offset = pmatch[0].rm_eo;
      if (!fmt[offset])
        break;
    }
  }

  if (!(flags & MUTT_SHOW) && ((*line_info)[n + 1].offset > 0))
  {
    /* we've already scanned this line, so just exit */
    rc = 0;
    goto out;
  }
  if ((flags & MUTT_SHOWCOLOR) && *force_redraw && ((*line_info)[n + 1].offset > 0))
  {
    /* no need to try to display this line... */
    rc = 1;
    goto out; /* fake display */
  }

  b_read = fill_buffer(fp, last_pos, curr_line->offset, &buf, &fmt, &buflen, &buf_ready);
  if (b_read < 0)
  {
    if (change_last)
      (*last)--;
    goto out;
  }

  /* now chose a good place to break the line */
  cnt = format_line(win_pager, line_info, n, buf, flags, NULL, b_read, &ch,
                    &vch, &col, &special, win_pager->state.cols);
  buf_ptr = buf + cnt;

  /* move the break point only if smart_wrap is set */
  if (c_smart_wrap)
  {
    if ((cnt < b_read) && (ch != -1) &&
        !mutt_color_is_header(curr_line->type) && !IS_SPACE(buf[cnt]))
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

  if (((int) (buf_ptr - buf) < b_read) && !(*line_info)[n + 1].continuation)
    append_line(*line_info, n, (int) (buf_ptr - buf));
  (*line_info)[n + 1].offset = curr_line->offset + (long) (buf_ptr - buf);

  /* if we don't need to display the line we are done */
  if (!(flags & MUTT_SHOW))
  {
    rc = 0;
    goto out;
  }

  /* display the line */
  format_line(win_pager, line_info, n, buf, flags, &a, cnt, &ch, &vch, &col,
              &special, win_pager->state.cols);

/* avoid a bug in ncurses... */
#ifndef USE_SLANG_CURSES
  if (col == 0)
  {
    mutt_curses_set_color(MT_COLOR_NORMAL);
    mutt_window_addch(win_pager, ' ');
  }
#endif

  /* end the last color pattern (needed by S-Lang) */
  if (special || ((col != win_pager->state.cols) && (flags & (MUTT_SHOWCOLOR | MUTT_SEARCH))))
    resolve_color(win_pager, *line_info, n, vch, flags, 0, &a);

  /* Fill the blank space at the end of the line with the prevailing color.
   * ncurses does an implicit clrtoeol() when you do mutt_window_addch('\n') so we have
   * to make sure to reset the color *after* that */
  if (flags & MUTT_SHOWCOLOR)
  {
    m = (curr_line->continuation) ? (curr_line->syntax)[0].first : n;
    if ((*line_info)[m].type == MT_COLOR_HEADER)
      def_color = ((*line_info)[m].syntax)[0].color;
    else
      def_color = mutt_color((*line_info)[m].type);

    mutt_curses_set_attr(def_color);
  }

  if (col < win_pager->state.cols)
    mutt_window_clrtoeol(win_pager);

  /* reset the color back to normal.  This *must* come after the
   * clrtoeol, otherwise the color for this line will not be
   * filled to the right margin.  */
  if (flags & MUTT_SHOWCOLOR)
    mutt_curses_set_color(MT_COLOR_NORMAL);

  /* build a return code */
  if (!(flags & MUTT_SHOW))
    flags = 0;

  rc = flags;

out:
  FREE(&buf);
  FREE(&fmt);
  return rc;
}

/**
 * up_n_lines - Reposition the pager's view up by n lines
 * @param nlines Number of lines to move
 * @param info   Line info array
 * @param cur    Current line number
 * @param hiding true if lines have been hidden
 * @retval num New current line number
 */
static int up_n_lines(int nlines, struct Line *info, int cur, bool hiding)
{
  while ((cur > 0) && (nlines > 0))
  {
    cur--;
    if (!hiding || (info[cur].type != MT_COLOR_QUOTED))
      nlines--;
  }

  return cur;
}

/**
 * mutt_clear_pager_position - Reset the pager's viewing position
 */
void mutt_clear_pager_position(void)
{
  TopLine = 0;
  OldEmail = NULL;
}

/**
 * pager_custom_redraw - Redraw the pager window - Implements Menu::custom_redraw()
 */
static void pager_custom_redraw(struct Menu *pager_menu)
{
  //---------------------------------------------------------------------------
  // ASSUMPTIONS & SANITY CHECKS
  //---------------------------------------------------------------------------
  // Since pager_custom_redraw() is a static function and it is always called
  // after mutt_pager() we can rely on a series of sanity checks in
  // mutt_pager(), namely:
  // - PAGER_MODE_EMAIL  guarantees ( data->email) and (!data->body)
  // - PAGER_MODE_ATTACH guarantees ( data->email) and ( data->body)
  // - PAGER_MODE_OTHER  guarantees (!data->email) and (!data->body)
  //
  // Additionally, while refactoring is still in progress the following checks
  // are still here to ensure data model consistency.
  assert(pager_menu);
  struct PagerRedrawData *rd = pager_menu->mdata;
  assert(rd);        // Redraw function can't be called without it's data.
  assert(rd->pview); // Redraw data can't exist separately without the view.
  assert(rd->pview->pdata); // View can't exist without it's data
  //---------------------------------------------------------------------------

  char buf[1024] = { 0 };
  struct IndexSharedData *shared = dialog_find(rd->pview->win_pager)->wdata;
  struct Mailbox *m = rd->pview->pdata->ctx ? rd->pview->pdata->ctx->mailbox : NULL;

  const bool c_tilde = cs_subset_bool(NeoMutt->sub, "tilde");
  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");

  if (pager_menu->redraw & MENU_REDRAW_FULL)
  {
    mutt_curses_set_color(MT_COLOR_NORMAL);
    mutt_window_clear(rd->pview->win_pager);

    if ((rd->pview->mode == PAGER_MODE_EMAIL) && ((m->vcount + 1) < c_pager_index_lines))
    {
      rd->indexlen = m->vcount + 1;
    }
    else
    {
      rd->indexlen = c_pager_index_lines;
    }

    rd->indicator = rd->indexlen / 3;

    if (Resize)
    {
      rd->search_compiled = Resize->search_compiled;
      if (rd->search_compiled)
      {
        uint16_t flags = mutt_mb_is_lower(rd->searchbuf) ? REG_ICASE : 0;
        const int err = REG_COMP(&rd->search_re, rd->searchbuf, REG_NEWLINE | flags);
        if (err == 0)
        {
          rd->search_flag = MUTT_SEARCH;
          rd->search_back = Resize->search_back;
        }
        else
        {
          regerror(err, &rd->search_re, buf, sizeof(buf));
          mutt_error("%s", buf);
          rd->search_compiled = false;
        }
      }
      rd->lines = Resize->line;
      menu_queue_redraw(pager_menu, MENU_REDRAW_FLOW);

      FREE(&Resize);
    }

    if ((rd->pview->mode == PAGER_MODE_EMAIL) && (c_pager_index_lines != 0) && rd->menu)
    {
      mutt_curses_set_color(MT_COLOR_NORMAL);

      /* some fudge to work out whereabouts the indicator should go */
      const int index = menu_get_index(rd->menu);
      if ((index - rd->indicator) < 0)
        rd->menu->top = 0;
      else if ((rd->menu->max - index) < (rd->menu->pagelen - rd->indicator))
        rd->menu->top = rd->menu->max - rd->menu->pagelen;
      else
        rd->menu->top = index - rd->indicator;

      menu_redraw_index(rd->menu);
    }

    menu_queue_redraw(pager_menu, MENU_REDRAW_BODY | MENU_REDRAW_INDEX | MENU_REDRAW_STATUS);
  }

  if (pager_menu->redraw & MENU_REDRAW_FLOW)
  {
    if (!(rd->pview->flags & MUTT_PAGER_RETWINCH))
    {
      rd->lines = -1;
      for (int i = 0; i <= rd->topline; i++)
        if (!rd->line_info[i].continuation)
          rd->lines++;
      for (int i = 0; i < rd->max_line; i++)
      {
        rd->line_info[i].offset = 0;
        rd->line_info[i].type = -1;
        rd->line_info[i].continuation = 0;
        rd->line_info[i].chunks = 0;
        rd->line_info[i].search_cnt = -1;
        rd->line_info[i].quote = NULL;

        mutt_mem_realloc(&(rd->line_info[i].syntax), sizeof(struct TextSyntax));
        if (rd->search_compiled && rd->line_info[i].search)
          FREE(&(rd->line_info[i].search));
      }

      rd->last_line = 0;
      rd->topline = 0;
    }
    int i = -1;
    int j = -1;
    while (display_line(rd->fp, &rd->last_pos, &rd->line_info, ++i, &rd->last_line, &rd->max_line,
                        rd->has_types | rd->search_flag | (rd->pview->flags & MUTT_PAGER_NOWRAP),
                        &rd->quote_list, &rd->q_level, &rd->force_redraw,
                        &rd->search_re, rd->pview->win_pager) == 0)
    {
      if (!rd->line_info[i].continuation && (++j == rd->lines))
      {
        rd->topline = i;
        if (!rd->search_flag)
          break;
      }
    }
  }

  if ((pager_menu->redraw & MENU_REDRAW_BODY) || (rd->topline != rd->oldtopline))
  {
    do
    {
      mutt_window_move(rd->pview->win_pager, 0, 0);
      rd->curline = rd->topline;
      rd->oldtopline = rd->topline;
      rd->lines = 0;
      rd->force_redraw = false;

      while ((rd->lines < rd->pview->win_pager->state.rows) &&
             (rd->line_info[rd->curline].offset <= rd->sb.st_size - 1))
      {
        if (display_line(rd->fp, &rd->last_pos, &rd->line_info, rd->curline,
                         &rd->last_line, &rd->max_line,
                         (rd->pview->flags & MUTT_DISPLAYFLAGS) | rd->hide_quoted |
                             rd->search_flag | (rd->pview->flags & MUTT_PAGER_NOWRAP),
                         &rd->quote_list, &rd->q_level, &rd->force_redraw,
                         &rd->search_re, rd->pview->win_pager) > 0)
        {
          rd->lines++;
        }
        rd->curline++;
        mutt_window_move(rd->pview->win_pager, 0, rd->lines);
      }
      rd->last_offset = rd->line_info[rd->curline].offset;
    } while (rd->force_redraw);

    mutt_curses_set_color(MT_COLOR_TILDE);
    while (rd->lines < rd->pview->win_pager->state.rows)
    {
      mutt_window_clrtoeol(rd->pview->win_pager);
      if (c_tilde)
        mutt_window_addch(rd->pview->win_pager, '~');
      rd->lines++;
      mutt_window_move(rd->pview->win_pager, 0, rd->lines);
    }
    mutt_curses_set_color(MT_COLOR_NORMAL);

    /* We are going to update the pager status bar, so it isn't
     * necessary to reset to normal color now. */

    menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS); /* need to update the % seen */
  }

  if (pager_menu->redraw & MENU_REDRAW_STATUS)
  {
    char pager_progress_str[65]; /* Lots of space for translations */

    if (rd->last_pos < rd->sb.st_size - 1)
    {
      snprintf(pager_progress_str, sizeof(pager_progress_str), OFF_T_FMT "%%",
               (100 * rd->last_offset / rd->sb.st_size));
    }
    else
    {
      const char *msg = (rd->topline == 0) ?
                            /* L10N: Status bar message: the entire email is visible in the pager */
                            _("all") :
                            /* L10N: Status bar message: the end of the email is visible in the pager */
                            _("end");
      mutt_str_copy(pager_progress_str, msg, sizeof(pager_progress_str));
    }

    /* print out the pager status bar */
    mutt_window_move(rd->pview->win_pbar, 0, 0);
    mutt_curses_set_color(MT_COLOR_STATUS);

    if (rd->pview->mode == PAGER_MODE_EMAIL || rd->pview->mode == PAGER_MODE_ATTACH_E)
    {
      const size_t l1 = rd->pview->win_pbar->state.cols * MB_LEN_MAX;
      const size_t l2 = sizeof(buf);
      const size_t buflen = (l1 < l2) ? l1 : l2;
      struct Email *e = (rd->pview->mode == PAGER_MODE_EMAIL) ?
                            rd->pview->pdata->email :      // PAGER_MODE_EMAIL
                            rd->pview->pdata->body->email; // PAGER_MODE_ATTACH_E

      const char *const c_pager_format =
          cs_subset_string(NeoMutt->sub, "pager_format");
      mutt_make_string(buf, buflen, rd->pview->win_pbar->state.cols,
                       NONULL(c_pager_format), m, rd->pview->pdata->ctx->msg_in_pager,
                       e, MUTT_FORMAT_NO_FLAGS, pager_progress_str);
      mutt_draw_statusline(rd->pview->win_pbar, rd->pview->win_pbar->state.cols, buf, l2);
    }
    else
    {
      char bn[256];
      snprintf(bn, sizeof(bn), "%s (%s)", rd->pview->banner, pager_progress_str);
      mutt_draw_statusline(rd->pview->win_pbar, rd->pview->win_pbar->state.cols,
                           bn, sizeof(bn));
    }
    mutt_curses_set_color(MT_COLOR_NORMAL);
    const bool c_ts_enabled = cs_subset_bool(NeoMutt->sub, "ts_enabled");
    if (c_ts_enabled && TsSupported && rd->menu)
    {
      const char *const c_ts_status_format =
          cs_subset_string(NeoMutt->sub, "ts_status_format");
      menu_status_line(buf, sizeof(buf), shared, rd->menu, sizeof(buf),
                       NONULL(c_ts_status_format));
      mutt_ts_status(buf);
      const char *const c_ts_icon_format =
          cs_subset_string(NeoMutt->sub, "ts_icon_format");
      menu_status_line(buf, sizeof(buf), shared, rd->menu, sizeof(buf),
                       NONULL(c_ts_icon_format));
      mutt_ts_icon(buf);
    }
  }

  pager_menu->redraw = MENU_REDRAW_NO_FLAGS;
}

/**
 * pager_resolve_help_mapping - determine help mapping based on pager mode and
 * mailbox type
 * @param mode pager mode
 * @param type mailbox type
 */
static const struct Mapping *pager_resolve_help_mapping(enum PagerMode mode, enum MailboxType type)
{
  const struct Mapping *result;
  switch (mode)
  {
    case PAGER_MODE_EMAIL:
    case PAGER_MODE_ATTACH:
    case PAGER_MODE_ATTACH_E:
      if (type == MUTT_NNTP)
        result = PagerNewsHelp;
      else
        result = PagerNormalHelp;
      break;

    case PAGER_MODE_HELP:
      result = PagerHelpHelp;
      break;

    case PAGER_MODE_OTHER:
      result = PagerHelp;
      break;

    case PAGER_MODE_UNKNOWN:
    case PAGER_MODE_MAX:
    default:
      assert(false); // something went really wrong
  }
  assert(result);
  return result;
}

/**
 * jump_to_bottom - make sure the bottom line is displayed
 * @param rd PagerRedrawData
 * @param pview PagerView
 * @retval true Something changed
 * @retval false Bottom was already displayed
 */
static bool jump_to_bottom(struct PagerRedrawData *rd, struct PagerView *pview)
{
  if (!(rd->line_info[rd->curline].offset < (rd->sb.st_size - 1)))
  {
    return false;
  }

  int line_num = rd->curline;
  /* make sure the types are defined to the end of file */
  while (display_line(rd->fp, &rd->last_pos, &rd->line_info, line_num, &rd->last_line,
                      &rd->max_line, rd->has_types | (pview->flags & MUTT_PAGER_NOWRAP),
                      &rd->quote_list, &rd->q_level, &rd->force_redraw,
                      &rd->search_re, rd->pview->win_pager) == 0)
  {
    line_num++;
  }
  rd->topline = up_n_lines(rd->pview->win_pager->state.rows, rd->line_info,
                           rd->last_line, rd->hide_quoted);
  return true;
}

/**
 * pager_check_read_delay - Is it time to mark the message read?
 * @param pview      Pager view
 * @param mailbox    Mailbox of message being visited
 * @param timestamp  Time when message should be marked read, or 0
 * @param final      True if is the final call for the given message
 * @retval 0 if delay has expired, or @timestamp if delay is not yet met
 */
static uint64_t pager_check_read_delay(struct PagerView *pview,
                                       struct Mailbox *m, uint64_t timestamp,
                                       bool final)
{
  if (timestamp == 0)
    return 0;

  const uint64_t now = mutt_date_epoch_ms();
  if (now > timestamp)
  {
    if (final)
      mutt_debug(LL_DEBUG4, "delay elapsed before leaving message, marking message read\n");
    else
      mutt_debug(LL_DEBUG4, "delay elapsed while still on message, marking message read\n");
    mutt_set_flag(m, pview->pdata->email, MUTT_READ, true);
    return 0;
  }

  if (final)
    mutt_debug(LL_DEBUG4, "leaving message before delay elapsed, leaving unread\n");
  return timestamp;
}

/**
 * mutt_pager - Display an email, attachment, or help, in a window
 * @param pview Pager view settings
 * @retval  0 Success
 * @retval -1 Error
 *
 * This pager is actually not so simple as it once was. But it will be again.
 * Currently it operates in 3 modes:
 * - viewing messages.                (PAGER_MODE_EMAIL)
 * - viewing attachments.             (PAGER_MODE_ATTACH)
 * - viewing other stuff (e.g. help). (PAGER_MODE_OTHER)
 * These can be distinguished by PagerMode in PagerView.
 * Data is not yet polymorphic and is fused into a single struct (PagerData).
 * Different elements of PagerData are expected to be present depending on the
 * mode:
 * - PAGER_MODE_EMAIL expects data->email and not expects data->body
 * - PAGER_MODE_ATTACH expects data->email and data->body
 *   special sub-case of this mode is viewing attached email message
 *   it is recognized by presence of data->fp and data->body->email
 * - PAGER_MODE_OTHER does not expect data->email or data->body
 */
int mutt_pager(struct PagerView *pview)
{
  //===========================================================================
  // ACT 1 - Ensure sanity of the caller and determine the mode
  //===========================================================================
  assert(pview);
  assert((pview->mode > PAGER_MODE_UNKNOWN) && (pview->mode < PAGER_MODE_MAX));
  assert(pview->pdata); // view can't exist in a vacuum
  assert(pview->win_pager);
  assert(pview->win_pbar);

  switch (pview->mode)
  {
    case PAGER_MODE_EMAIL:
      // This case was previously identified by IsEmail macro
      // we expect data to contain email and not contain body
      // We also expect email to always belong to some mailbox
      assert(pview->pdata->email);
      assert(pview->pdata->ctx);
      assert(pview->pdata->ctx->mailbox);
      assert(!pview->pdata->body);

      break;

    case PAGER_MODE_ATTACH:
      // this case was previously identified by IsAttach and IsMsgAttach
      // macros, we expect data to contain:
      //  - body (viewing regular attachment)
      //  - email
      //  - fp and body->email in special case of viewing an attached email.
      assert(pview->pdata->email); // This should point to the top level email
      assert(pview->pdata->body);
      assert(pview->pdata->ctx);
      assert(pview->pdata->ctx->mailbox);
      if (pview->pdata->fp && pview->pdata->body->email)
      {
        // Special case: attachment is a full-blown email message.
        // Yes, emails can contain other emails.
        pview->mode = PAGER_MODE_ATTACH_E;
      }
      break;

    case PAGER_MODE_HELP:
    case PAGER_MODE_OTHER:
      assert(!pview->pdata->email);
      assert(!pview->pdata->body);
      assert(!pview->pdata->ctx);
      break;

    case PAGER_MODE_UNKNOWN:
    case PAGER_MODE_MAX:
    default:
      // Unexpected mode. Catch fire and explode.
      // This *should* happen if mode is PAGER_MODE_ATTACH_E, since
      // we do not expect any caller to pass it to us.
      assert(false);
      break;
  }

  //===========================================================================
  // ACT 2 - Declare, initialize local variables, read config, etc.
  //===========================================================================

  //---------- reading config values ------------------------------------------
  const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
  const short c_pager_context = cs_subset_number(NeoMutt->sub, "pager_context");
  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");
  const short c_pager_read_delay =
      cs_subset_number(NeoMutt->sub, "pager_read_delay");
  const short c_search_context =
      cs_subset_number(NeoMutt->sub, "search_context");
  const short c_skip_quoted_offset =
      cs_subset_number(NeoMutt->sub, "skip_quoted_offset");

  //---------- local variables ------------------------------------------------
  struct Mailbox *m = pview->pdata->ctx ? pview->pdata->ctx->mailbox : NULL;
  struct IndexSharedData *shared = dialog_find(pview->win_pager)->wdata;

  struct Menu *pager_menu = NULL;
  struct PagerRedrawData rd = { 0 };
  char buf[1024] = { 0 };
  int op = 0;
  int rc = -1;
  int searchctx = 0;
  int index_space = m ? MIN(c_pager_index_lines, m->vcount) : c_pager_index_lines;
  bool first = true;
  bool wrapped = false;
  enum MailboxType mailbox_type = m ? m->type : MUTT_UNKNOWN;
  uint64_t delay_read_timestamp = 0;

  struct PagerPrivateData *priv = pview->win_pager->parent->wdata;

  priv->rd = &rd;
  priv->win_pbar = pview->win_pbar;

  //---------- setup flags ----------------------------------------------------
  if (!(pview->flags & MUTT_SHOWCOLOR))
    pview->flags |= MUTT_SHOWFLAT;

  if (pview->mode == PAGER_MODE_EMAIL && !pview->pdata->email->read)
  {
    pview->pdata->ctx->msg_in_pager = pview->pdata->email->msgno;
    priv->win_pbar->actions |= WA_RECALC;
    if (c_pager_read_delay != 0)
    {
      mutt_debug(LL_DEBUG4, "delaying %d seconds before marking message read\n",
                 c_pager_read_delay);
      delay_read_timestamp = mutt_date_epoch_ms() + 1000 * c_pager_read_delay;
    }
    else
      mutt_set_flag(m, pview->pdata->email, MUTT_READ, true);
  }
  //---------- setup help menu ------------------------------------------------
  pview->win_pager->help_data = pager_resolve_help_mapping(pview->mode, mailbox_type);
  pview->win_pager->help_menu = MENU_PAGER;

  //---------- initialize redraw pdata  -----------------------------------------
  pview->win_pager->size = MUTT_WIN_SIZE_MAXIMISE;
  rd.pview = pview;
  rd.indexlen = c_pager_index_lines;
  rd.indicator = rd.indexlen / 3;
  rd.max_line = LINES; // number of lines on screen, from curses
  rd.line_info = mutt_mem_calloc(rd.max_line, sizeof(struct Line));
  rd.fp = fopen(pview->pdata->fname, "r");
  rd.has_types = ((pview->mode == PAGER_MODE_EMAIL) || (pview->flags & MUTT_SHOWCOLOR)) ?
                     MUTT_TYPES :
                     0; // main message or rfc822 attachment

  for (size_t i = 0; i < rd.max_line; i++)
  {
    rd.line_info[i].type = -1;
    rd.line_info[i].search_cnt = -1;
    rd.line_info[i].syntax = mutt_mem_malloc(sizeof(struct TextSyntax));
    (rd.line_info[i].syntax)[0].first = -1;
    (rd.line_info[i].syntax)[0].last = -1;
  }

  // ---------- try to open the pdata file -------------------------------------
  if (!rd.fp)
  {
    mutt_perror(pview->pdata->fname);
    return -1;
  }

  if (stat(pview->pdata->fname, &rd.sb) != 0)
  {
    mutt_perror(pview->pdata->fname);
    mutt_file_fclose(&rd.fp);
    return -1;
  }
  unlink(pview->pdata->fname);

  //---------- setup pager menu------------------------------------------------
  pager_menu = pview->win_pager->wdata;
  pager_menu->win_ibar = pview->win_pbar;
  pager_menu->custom_redraw = pager_custom_redraw;
  pager_menu->mdata = &rd;
  priv->menu = pager_menu;

  //---------- restore global state if needed ---------------------------------
  while (pview->mode == PAGER_MODE_EMAIL && (OldEmail == pview->pdata->email) // are we "resuming" to the same Email?
         && (TopLine != rd.topline) // is saved offset different?
         && rd.line_info[rd.curline].offset < (rd.sb.st_size - 1))
  {
    // needed to avoid SIGSEGV
    pager_custom_redraw(pager_menu);
    // trick user, as if nothing happened
    // scroll down to previosly saved offset
    rd.topline = ((TopLine - rd.topline) > rd.lines) ? rd.topline + rd.lines : TopLine;
  }

  TopLine = 0;
  OldEmail = NULL;

  //---------- show windows, set focus and visibility --------------------------
  if (rd.pview->win_index)
  {
    rd.menu = rd.pview->win_index->wdata;
    rd.pview->win_index->size = MUTT_WIN_SIZE_FIXED;
    rd.pview->win_index->req_rows = index_space;
    rd.pview->win_index->parent->size = MUTT_WIN_SIZE_MINIMISE;
    window_set_visible(rd.pview->win_index->parent, (index_space > 0));
  }
  window_set_visible(rd.pview->win_pager->parent, true);
  mutt_window_reflow(dialog_find(rd.pview->win_pager));
  window_set_focus(pview->win_pager);

  //---------- jump to the bottom if requested ------------------------------
  if (pview->flags & MUTT_PAGER_BOTTOM)
  {
    jump_to_bottom(&rd, pview);
  }

  //-------------------------------------------------------------------------
  // ACT 3: Read user input and decide what to do with it
  //        ...but also do a whole lot of other things.
  //-------------------------------------------------------------------------
  while (op != -1)
  {
    delay_read_timestamp = pager_check_read_delay(pview, m,
                                                  delay_read_timestamp, false);

    mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
    window_redraw(NULL);
    pager_custom_redraw(pager_menu);

    const bool c_braille_friendly =
        cs_subset_bool(NeoMutt->sub, "braille_friendly");
    if (c_braille_friendly)
    {
      if (braille_row != -1)
      {
        mutt_window_move(rd.pview->win_pager, braille_col, braille_row + 1);
        braille_row = -1;
      }
    }
    else
      mutt_window_move(rd.pview->win_pbar, rd.pview->win_pager->state.cols - 1, 0);

    // force redraw of the screen at every iteration of the event loop
    mutt_refresh();

    //-------------------------------------------------------------------------
    // Check if information in the status bar needs an update
    // This is done because pager is a single-threaded application, which
    // tries to emulate concurrency.
    //-------------------------------------------------------------------------
    bool do_new_mail = false;
    if (m && !OptAttachMsg)
    {
      int oldcount = m->msg_count;
      /* check for new mail */
      enum MxStatus check = mx_mbox_check(m);
      if (check == MX_STATUS_ERROR)
      {
        if (!m || mutt_buffer_is_empty(&m->pathbuf))
        {
          /* fatal error occurred */
          menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
          break;
        }
      }
      else if ((check == MX_STATUS_NEW_MAIL) || (check == MX_STATUS_REOPENED) ||
               (check == MX_STATUS_FLAGS))
      {
        /* notify user of newly arrived mail */
        if (check == MX_STATUS_NEW_MAIL)
        {
          for (size_t i = oldcount; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];

            if (e && !e->read)
            {
              mutt_message(_("New mail in this mailbox"));
              do_new_mail = true;
              break;
            }
          }
        }

        if ((check == MX_STATUS_NEW_MAIL) || (check == MX_STATUS_REOPENED))
        {
          if (rd.menu && m)
          {
            /* After the mailbox has been updated, selection might be invalid */
            int index = menu_get_index(rd.menu);
            menu_set_index(rd.menu, MIN(index, MAX(m->msg_count - 1, 0)));
            index = menu_get_index(rd.menu);
            struct Email *e = mutt_get_virt_email(m, index);
            if (!e)
              continue;

            bool verbose = m->verbose;
            m->verbose = false;
            mutt_update_index(rd.menu, pview->pdata->ctx, check, oldcount, shared);
            m->verbose = verbose;

            rd.menu->max = m->vcount;

            /* If these header pointers don't match, then our email may have
             * been deleted.  Make the pointer safe, then leave the pager.
             * This have a unpleasant behaviour to close the pager even the
             * deleted message is not the opened one, but at least it's safe. */
            e = mutt_get_virt_email(m, index);
            if (pview->pdata->email != e)
            {
              pview->pdata->email = e;
              break;
            }
          }

          menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
          OptSearchInvalid = true;
        }
      }

      if (mutt_mailbox_notify(m) || do_new_mail)
      {
        const bool c_beep_new = cs_subset_bool(NeoMutt->sub, "beep_new");
        if (c_beep_new)
          mutt_beep(true);
        const char *const c_new_mail_command =
            cs_subset_string(NeoMutt->sub, "new_mail_command");
        if (c_new_mail_command)
        {
          char cmd[1024];
          menu_status_line(cmd, sizeof(cmd), shared, rd.menu, sizeof(cmd),
                           NONULL(c_new_mail_command));
          if (mutt_system(cmd) != 0)
            mutt_error(_("Error running \"%s\""), cmd);
        }
      }
    }
    //-------------------------------------------------------------------------

    if (SigWinch)
    {
      SigWinch = false;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
      msgwin_clear_text();

      if (pview->flags & MUTT_PAGER_RETWINCH)
      {
        /* Store current position. */
        rd.lines = -1;
        for (size_t i = 0; i <= rd.topline; i++)
          if (!rd.line_info[i].continuation)
            rd.lines++;

        Resize = mutt_mem_malloc(sizeof(struct Resize));

        Resize->line = rd.lines;
        Resize->search_compiled = rd.search_compiled;
        Resize->search_back = rd.search_back;

        op = -1;
        rc = OP_REFORMAT_WINCH;
      }
      else
      {
        /* note: mutt_resize_screen() -> mutt_window_reflow() sets
         * MENU_REDRAW_FULL and MENU_REDRAW_FLOW */
        op = 0;
      }
      continue;
    }
    //-------------------------------------------------------------------------
    // Finally, read user's key press
    //-------------------------------------------------------------------------
    // km_dokey() reads not only user's key strokes, but also a MacroBuffer
    // MacroBuffer may contain OP codes of the operations.
    // MacroBuffer is global
    // OP codes inserted into the MacroBuffer by various functions.
    // One of such functions is `mutt_enter_command()`
    // Some OP codes are not handled by pager, they cause pager to quit returning
    // OP code to index. Index hadles the operation and then restarts pager
    op = km_dokey(MENU_PAGER);

    if (op >= 0)
    {
      mutt_clear_error();
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);
    }
    mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

    if (op < 0)
    {
      op = 0;
      mutt_timeout_hook();
      continue;
    }

    rc = op;

    switch (op)
    {
        //=======================================================================

      case OP_EXIT:
        rc = -1;
        op = -1;
        break;

        //=======================================================================

      case OP_QUIT:
      {
        const enum QuadOption c_quit = cs_subset_quad(NeoMutt->sub, "quit");
        if (query_quadoption(c_quit, _("Quit NeoMutt?")) == MUTT_YES)
        {
          /* avoid prompting again in the index menu */
          cs_subset_str_native_set(NeoMutt->sub, "quit", MUTT_YES, NULL);
          op = -1;
        }
        break;
      }

        //=======================================================================

      case OP_NEXT_PAGE:
        if (rd.line_info[rd.curline].offset < (rd.sb.st_size - 1))
        {
          rd.topline = up_n_lines(c_pager_context, rd.line_info, rd.curline, rd.hide_quoted);
        }
        else if (c_pager_stop)
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_message(_("Bottom of message is shown"));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }
        break;

        //=======================================================================

      case OP_PREV_PAGE:
        if (rd.topline == 0)
        {
          mutt_message(_("Top of message is shown"));
        }
        else
        {
          rd.topline = up_n_lines(rd.pview->win_pager->state.rows - c_pager_context,
                                  rd.line_info, rd.topline, rd.hide_quoted);
        }
        break;

        //=======================================================================

      case OP_NEXT_LINE:
        if (rd.line_info[rd.curline].offset < (rd.sb.st_size - 1))
        {
          rd.topline++;
          if (rd.hide_quoted)
          {
            while ((rd.line_info[rd.topline].type == MT_COLOR_QUOTED) &&
                   (rd.topline < rd.last_line))
            {
              rd.topline++;
            }
          }
        }
        else
          mutt_message(_("Bottom of message is shown"));
        break;

        //=======================================================================

      case OP_PREV_LINE:
        if (rd.topline)
          rd.topline = up_n_lines(1, rd.line_info, rd.topline, rd.hide_quoted);
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_PAGER_TOP:
        if (rd.topline)
          rd.topline = 0;
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_HALF_UP:
        if (rd.topline)
        {
          rd.topline = up_n_lines(rd.pview->win_pager->state.rows / 2 +
                                      (rd.pview->win_pager->state.rows % 2),
                                  rd.line_info, rd.topline, rd.hide_quoted);
        }
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_HALF_DOWN:
        if (rd.line_info[rd.curline].offset < (rd.sb.st_size - 1))
        {
          rd.topline = up_n_lines(rd.pview->win_pager->state.rows / 2,
                                  rd.line_info, rd.curline, rd.hide_quoted);
        }
        else if (c_pager_stop)
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_message(_("Bottom of message is shown"));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }
        break;

        //=======================================================================

      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (rd.search_compiled)
        {
          wrapped = false;

          if (c_search_context < rd.pview->win_pager->state.rows)
            searchctx = c_search_context;
          else
            searchctx = 0;

        search_next:
          if ((!rd.search_back && (op == OP_SEARCH_NEXT)) ||
              (rd.search_back && (op == OP_SEARCH_OPPOSITE)))
          {
            /* searching forward */
            int i;
            for (i = wrapped ? 0 : rd.topline + searchctx + 1; i < rd.last_line; i++)
            {
              if ((!rd.hide_quoted || (rd.line_info[i].type != MT_COLOR_QUOTED)) &&
                  !rd.line_info[i].continuation && (rd.line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            const bool c_wrap_search =
                cs_subset_bool(NeoMutt->sub, "wrap_search");
            if (i < rd.last_line)
              rd.topline = i;
            else if (wrapped || !c_wrap_search)
              mutt_error(_("Not found"));
            else
            {
              mutt_message(_("Search wrapped to top"));
              wrapped = true;
              goto search_next;
            }
          }
          else
          {
            /* searching backward */
            int i;
            for (i = wrapped ? rd.last_line : rd.topline + searchctx - 1; i >= 0; i--)
            {
              if ((!rd.hide_quoted ||
                   (rd.has_types && (rd.line_info[i].type != MT_COLOR_QUOTED))) &&
                  !rd.line_info[i].continuation && (rd.line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            const bool c_wrap_search =
                cs_subset_bool(NeoMutt->sub, "wrap_search");
            if (i >= 0)
              rd.topline = i;
            else if (wrapped || !c_wrap_search)
              mutt_error(_("Not found"));
            else
            {
              mutt_message(_("Search wrapped to bottom"));
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
        /* no previous search pattern */
        /* fallthrough */
        //=======================================================================

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
        mutt_str_copy(buf, rd.searchbuf, sizeof(buf));
        if (mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                               _("Search for: ") :
                               _("Reverse search for: "),
                           buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN, false,
                           NULL, NULL) != 0)
        {
          break;
        }

        if (strcmp(buf, rd.searchbuf) == 0)
        {
          if (rd.search_compiled)
          {
            /* do an implicit search-next */
            if (op == OP_SEARCH)
              op = OP_SEARCH_NEXT;
            else
              op = OP_SEARCH_OPPOSITE;

            wrapped = false;
            goto search_next;
          }
        }

        if (buf[0] == '\0')
          break;

        mutt_str_copy(rd.searchbuf, buf, sizeof(rd.searchbuf));

        /* leave search_back alone if op == OP_SEARCH_NEXT */
        if (op == OP_SEARCH)
          rd.search_back = false;
        else if (op == OP_SEARCH_REVERSE)
          rd.search_back = true;

        if (rd.search_compiled)
        {
          regfree(&rd.search_re);
          for (size_t i = 0; i < rd.last_line; i++)
          {
            FREE(&(rd.line_info[i].search));
            rd.line_info[i].search_cnt = -1;
          }
        }

        uint16_t rflags = mutt_mb_is_lower(rd.searchbuf) ? REG_ICASE : 0;
        int err = REG_COMP(&rd.search_re, rd.searchbuf, REG_NEWLINE | rflags);
        if (err != 0)
        {
          regerror(err, &rd.search_re, buf, sizeof(buf));
          mutt_error("%s", buf);
          for (size_t i = 0; i < rd.max_line; i++)
          {
            /* cleanup */
            FREE(&(rd.line_info[i].search));
            rd.line_info[i].search_cnt = -1;
          }
          rd.search_flag = 0;
          rd.search_compiled = false;
        }
        else
        {
          rd.search_compiled = true;
          /* update the search pointers */
          int line_num = 0;
          while (display_line(rd.fp, &rd.last_pos, &rd.line_info, line_num,
                              &rd.last_line, &rd.max_line,
                              MUTT_SEARCH | (pview->flags & MUTT_PAGER_NSKIP) |
                                  (pview->flags & MUTT_PAGER_NOWRAP),
                              &rd.quote_list, &rd.q_level, &rd.force_redraw,
                              &rd.search_re, rd.pview->win_pager) == 0)
          {
            line_num++;
          }

          if (!rd.search_back)
          {
            /* searching forward */
            int i;
            for (i = rd.topline; i < rd.last_line; i++)
            {
              if ((!rd.hide_quoted || (rd.line_info[i].type != MT_COLOR_QUOTED)) &&
                  !rd.line_info[i].continuation && (rd.line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            if (i < rd.last_line)
              rd.topline = i;
          }
          else
          {
            /* searching backward */
            int i;
            for (i = rd.topline; i >= 0; i--)
            {
              if ((!rd.hide_quoted || (rd.line_info[i].type != MT_COLOR_QUOTED)) &&
                  !rd.line_info[i].continuation && (rd.line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            if (i >= 0)
              rd.topline = i;
          }

          if (rd.line_info[rd.topline].search_cnt == 0)
          {
            rd.search_flag = 0;
            mutt_error(_("Not found"));
          }
          else
          {
            rd.search_flag = MUTT_SEARCH;
            /* give some context for search results */
            if (c_search_context < rd.pview->win_pager->state.rows)
              searchctx = c_search_context;
            else
              searchctx = 0;
            if (rd.topline - searchctx > 0)
              rd.topline -= searchctx;
          }
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_BODY);
        break;

        //=======================================================================

      case OP_SEARCH_TOGGLE:
        if (rd.search_compiled)
        {
          rd.search_flag ^= MUTT_SEARCH;
          menu_queue_redraw(pager_menu, MENU_REDRAW_BODY);
        }
        break;

        //=======================================================================

      case OP_SORT:
      case OP_SORT_REVERSE:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (mutt_select_sort(op == OP_SORT_REVERSE))
        {
          OptNeedResort = true;
          op = -1;
          rc = OP_DISPLAY_MESSAGE;
        }
        break;

        //=======================================================================

      case OP_HELP:
        if (pview->mode == PAGER_MODE_HELP)
        {
          /* don't let the user enter the help-menu from the help screen! */
          mutt_error(_("Help is currently being shown"));
          break;
        }
        mutt_help(MENU_PAGER);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_PAGER_HIDE_QUOTED:
        if (!rd.has_types)
          break;

        rd.hide_quoted ^= MUTT_HIDE;
        if (rd.hide_quoted && (rd.line_info[rd.topline].type == MT_COLOR_QUOTED))
          rd.topline = up_n_lines(1, rd.line_info, rd.topline, rd.hide_quoted);
        else
          menu_queue_redraw(pager_menu, MENU_REDRAW_BODY);
        break;

        //=======================================================================

      case OP_PAGER_SKIP_QUOTED:
      {
        if (!rd.has_types)
          break;

        int dretval = 0;
        int new_topline = rd.topline;

        /* Skip all the email headers */
        if (mutt_color_is_header(rd.line_info[new_topline].type))
        {
          while (((new_topline < rd.last_line) ||
                  (0 == (dretval = display_line(
                             rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                             &rd.max_line, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                             &rd.quote_list, &rd.q_level, &rd.force_redraw,
                             &rd.search_re, rd.pview->win_pager)))) &&
                 mutt_color_is_header(rd.line_info[new_topline].type))
          {
            new_topline++;
          }
          rd.topline = new_topline;
          break;
        }

        while ((((new_topline + c_skip_quoted_offset) < rd.last_line) ||
                (0 == (dretval = display_line(
                           rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                           &rd.max_line, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                           &rd.quote_list, &rd.q_level, &rd.force_redraw,
                           &rd.search_re, rd.pview->win_pager)))) &&
               (rd.line_info[new_topline + c_skip_quoted_offset].type != MT_COLOR_QUOTED))
        {
          new_topline++;
        }

        if (dretval < 0)
        {
          mutt_error(_("No more quoted text"));
          break;
        }

        while ((((new_topline + c_skip_quoted_offset) < rd.last_line) ||
                (0 == (dretval = display_line(
                           rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                           &rd.max_line, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                           &rd.quote_list, &rd.q_level, &rd.force_redraw,
                           &rd.search_re, rd.pview->win_pager)))) &&
               (rd.line_info[new_topline + c_skip_quoted_offset].type == MT_COLOR_QUOTED))
        {
          new_topline++;
        }

        if (dretval < 0)
        {
          mutt_error(_("No more unquoted text after quoted text"));
          break;
        }
        rd.topline = new_topline;
        break;
      }

        //=======================================================================

      case OP_PAGER_SKIP_HEADERS:
      {
        if (!rd.has_types)
          break;

        int dretval = 0;
        int new_topline = rd.topline;

        if (!mutt_color_is_header(rd.line_info[new_topline].type))
        {
          /* L10N: Displayed if <skip-headers> is invoked in the pager, but we
             are already past the headers */
          mutt_message(_("Already skipped past headers"));
          break;
        }

        while (((new_topline < rd.last_line) ||
                (0 == (dretval = display_line(
                           rd.fp, &rd.last_pos, &rd.line_info, new_topline, &rd.last_line,
                           &rd.max_line, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                           &rd.quote_list, &rd.q_level, &rd.force_redraw,
                           &rd.search_re, rd.pview->win_pager)))) &&
               mutt_color_is_header(rd.line_info[new_topline].type))
        {
          new_topline++;
        }

        if (dretval < 0)
        {
          /* L10N: Displayed if <skip-headers> is invoked in the pager, but
             there is no text past the headers.
             (I don't think this is actually possible in Mutt's code, but
             display some kind of message in case it somehow occurs.) */
          mutt_warning(_("No text past headers"));
          break;
        }
        rd.topline = new_topline;
        break;
      }

        //=======================================================================

      case OP_PAGER_BOTTOM: /* move to the end of the file */
        if (!jump_to_bottom(&rd, pview))
        {
          mutt_message(_("Bottom of message is shown"));
        }
        break;

        //=======================================================================

      case OP_REDRAW:
        mutt_window_reflow(NULL);
        clearok(stdscr, true);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_NULL:
        km_error_key(MENU_PAGER);
        break;

      //=======================================================================
      // The following are operations on the current message rather than
      // adjusting the view of the message.
      //=======================================================================
      case OP_BOUNCE_MESSAGE:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_bounce(m, pview->pdata->fp, pview->pdata->actx,
                             pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, pview->pdata->email);
          ci_bounce_message(m, &el);
          emaillist_clear(&el);
        }
        break;
      }

        //=======================================================================

      case OP_RESEND:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_resend(pview->pdata->fp, ctx_mailbox(pview->pdata->ctx),
                             pview->pdata->actx, pview->pdata->body);
        }
        else
        {
          mutt_resend_message(NULL, ctx_mailbox(pview->pdata->ctx),
                              pview->pdata->email, NeoMutt->sub);
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_COMPOSE_TO_SENDER:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_mail_sender(pview->pdata->fp, pview->pdata->email,
                                  pview->pdata->actx, pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, pview->pdata->email);

          mutt_send_message(SEND_TO_SENDER, NULL, NULL,
                            ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_CHECK_TRADITIONAL:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!(pview->pdata->email->security & PGP_TRADITIONAL_CHECKED))
        {
          op = -1;
          rc = OP_CHECK_TRADITIONAL;
        }
        break;

        //=======================================================================

      case OP_CREATE_ALIAS:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        struct AddressList *al = NULL;
        if (pview->mode == PAGER_MODE_ATTACH_E)
          al = mutt_get_address(pview->pdata->body->email->env, NULL);
        else
          al = mutt_get_address(pview->pdata->email->env, NULL);
        alias_create(al, NeoMutt->sub);
        break;

        //=======================================================================

      case OP_PURGE_MESSAGE:
      case OP_DELETE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(m, MUTT_ACL_DELETE, _("Can't delete message")))
        {
          break;
        }

        mutt_set_flag(m, pview->pdata->email, MUTT_DELETE, true);
        mutt_set_flag(m, pview->pdata->email, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
        const bool c_delete_untag =
            cs_subset_bool(NeoMutt->sub, "delete_untag");
        if (c_delete_untag)
          mutt_set_flag(m, pview->pdata->email, MUTT_TAG, false);
        menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);

        if (mutt_change_flag(m, &el, (op == OP_MAIN_SET_FLAG)) == 0)
          menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (pview->pdata->email->deleted && c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        emaillist_clear(&el);
        break;
      }

        //=======================================================================

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           delete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.  */
        if (!assert_mailbox_permissions(m, MUTT_ACL_DELETE, _("Can't delete messages")))
        {
          break;
        }

        int subthread = (op == OP_DELETE_SUBTHREAD);
        int r = mutt_thread_set_flag(m, pview->pdata->email, MUTT_DELETE, 1, subthread);
        if (r == -1)
          break;
        if (op == OP_PURGE_THREAD)
        {
          r = mutt_thread_set_flag(m, pview->pdata->email, MUTT_PURGE, true, subthread);
          if (r == -1)
            break;
        }

        const bool c_delete_untag =
            cs_subset_bool(NeoMutt->sub, "delete_untag");
        if (c_delete_untag)
          mutt_thread_set_flag(m, pview->pdata->email, MUTT_TAG, 0, subthread);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }

        if (!c_resolve && (c_pager_index_lines != 0))
          menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        else
          menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);

        break;
      }

        //=======================================================================

      case OP_DISPLAY_ADDRESS:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH_E)
          mutt_display_address(pview->pdata->body->email->env);
        else
          mutt_display_address(pview->pdata->email->env);
        break;

        //=======================================================================

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);

        if (OptNeedResort)
        {
          OptNeedResort = false;
          if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
            break;
          OptNeedResort = true;
        }

        if ((pager_menu->redraw & MENU_REDRAW_FLOW) && (pview->flags & MUTT_PAGER_RETWINCH))
        {
          op = -1;
          rc = OP_REFORMAT_WINCH;
          continue;
        }

        op = 0;
        break;

        //=======================================================================

      case OP_FLAG_MESSAGE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(m, MUTT_ACL_WRITE, "Can't flag message"))
          break;

        mutt_set_flag(m, pview->pdata->email, MUTT_FLAG, !pview->pdata->email->flagged);
        menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_PIPE:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_pipe_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                                    pview->pdata->body, false);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, pview->pdata->ctx, pview->pdata->email, false);
          mutt_pipe_message(m, &el);
          emaillist_clear(&el);
        }
        break;

        //=======================================================================

      case OP_PRINT:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_print_attachment_list(pview->pdata->actx, pview->pdata->fp,
                                     false, pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, pview->pdata->ctx, pview->pdata->email, false);
          mutt_print_message(m, &el);
          emaillist_clear(&el);
        }
        break;

        //=======================================================================

      case OP_MAIL:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        mutt_send_message(SEND_NO_FLAGS, NULL, NULL,
                          ctx_mailbox(pview->pdata->ctx), NULL, NeoMutt->sub);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

#ifdef USE_NNTP
      case OP_POST:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        const enum QuadOption c_post_moderated =
            cs_subset_quad(NeoMutt->sub, "post_moderated");
        if ((m->type == MUTT_NNTP) &&
            !((struct NntpMboxData *) m->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
        {
          break;
        }

        mutt_send_message(SEND_NEWS, NULL, NULL, ctx_mailbox(pview->pdata->ctx),
                          NULL, NeoMutt->sub);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FORWARD_TO_GROUP:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        const enum QuadOption c_post_moderated =
            cs_subset_quad(NeoMutt->sub, "post_moderated");
        if ((m->type == MUTT_NNTP) &&
            !((struct NntpMboxData *) m->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_forward(pview->pdata->fp, pview->pdata->email,
                              pview->pdata->actx, pview->pdata->body, SEND_NEWS);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, pview->pdata->email);

          mutt_send_message(SEND_NEWS | SEND_FORWARD, NULL, NULL,
                            ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FOLLOWUP:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        char *followup_to = NULL;
        if (pview->mode == PAGER_MODE_ATTACH_E)
          followup_to = pview->pdata->body->email->env->followup_to;
        else
          followup_to = pview->pdata->email->env->followup_to;

        const enum QuadOption c_followup_to_poster =
            cs_subset_quad(NeoMutt->sub, "followup_to_poster");
        if (!followup_to || !mutt_istr_equal(followup_to, "poster") ||
            (query_quadoption(c_followup_to_poster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          const enum QuadOption c_post_moderated =
              cs_subset_quad(NeoMutt->sub, "post_moderated");
          if ((m->type == MUTT_NNTP) &&
              !((struct NntpMboxData *) m->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
          {
            break;
          }
          if (pview->mode == PAGER_MODE_ATTACH_E)
          {
            mutt_attach_reply(pview->pdata->fp, m, pview->pdata->email,
                              pview->pdata->actx, pview->pdata->body, SEND_NEWS | SEND_REPLY);
          }
          else
          {
            struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
            emaillist_add_email(&el, pview->pdata->email);
            mutt_send_message(SEND_NEWS | SEND_REPLY, NULL, NULL,
                              ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
            emaillist_clear(&el);
          }
          menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
          break;
        }

        //=======================================================================

#endif
      /* fallthrough */
      case OP_REPLY:
      case OP_GROUP_REPLY:
      case OP_GROUP_CHAT_REPLY:
      case OP_LIST_REPLY:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        SendFlags replyflags = SEND_REPLY;
        if (op == OP_GROUP_REPLY)
          replyflags |= SEND_GROUP_REPLY;
        else if (op == OP_GROUP_CHAT_REPLY)
          replyflags |= SEND_GROUP_CHAT_REPLY;
        else if (op == OP_LIST_REPLY)
          replyflags |= SEND_LIST_REPLY;

        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_reply(pview->pdata->fp, m, pview->pdata->email,
                            pview->pdata->actx, pview->pdata->body, replyflags);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, pview->pdata->email);
          mutt_send_message(replyflags, NULL, NULL,
                            ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_RECALL_MESSAGE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);

        mutt_send_message(SEND_POSTPONED, NULL, NULL,
                          ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
        emaillist_clear(&el);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FORWARD_MESSAGE:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_forward(pview->pdata->fp, pview->pdata->email,
                              pview->pdata->actx, pview->pdata->body, SEND_NO_FLAGS);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, pview->pdata->email);

          mutt_send_message(SEND_FORWARD, NULL, NULL,
                            ctx_mailbox(pview->pdata->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_DECRYPT_SAVE:
        if (!WithCrypto)
        {
          op = -1;
          break;
        }
        /* fallthrough */
        //=======================================================================

      case OP_SAVE:
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_save_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                                    pview->pdata->body, pview->pdata->email, NULL);
          break;
        }
        /* fallthrough */
        //=======================================================================

      case OP_COPY_MESSAGE:
      case OP_DECODE_SAVE:
      case OP_DECODE_COPY:
      case OP_DECRYPT_COPY:
      {
        if (!(WithCrypto != 0) && (op == OP_DECRYPT_COPY))
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);

        const enum MessageSaveOpt save_opt =
            ((op == OP_SAVE) || (op == OP_DECODE_SAVE) || (op == OP_DECRYPT_SAVE)) ?
                SAVE_MOVE :
                SAVE_COPY;

        enum MessageTransformOpt transform_opt =
            ((op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY)) ? TRANSFORM_DECODE :
            ((op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY)) ? TRANSFORM_DECRYPT :
                                                                   TRANSFORM_NONE;

        const int rc2 = mutt_save_message(m, &el, save_opt, transform_opt);
        if ((rc2 == 0) && (save_opt == SAVE_MOVE))
        {
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
          if (c_resolve)
          {
            op = -1;
            rc = OP_MAIN_NEXT_UNDELETED;
          }
          else
            menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        }
        emaillist_clear(&el);
        break;
      }

        //=======================================================================

      case OP_SHELL_ESCAPE:
        if (mutt_shell_escape())
        {
          mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_FORCE);
        }
        break;

        //=======================================================================

      case OP_TAG:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        mutt_set_flag(m, pview->pdata->email, MUTT_TAG, !pview->pdata->email->tagged);

        menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;
      }

        //=======================================================================

      case OP_TOGGLE_NEW:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(m, MUTT_ACL_SEEN, _("Can't toggle new")))
          break;

        if (pview->pdata->email->read || pview->pdata->email->old)
          mutt_set_flag(m, pview->pdata->email, MUTT_NEW, true);
        else if (!first)
          mutt_set_flag(m, pview->pdata->email, MUTT_READ, true);
        first = false;
        pview->pdata->ctx->msg_in_pager = -1;
        priv->win_pbar->actions |= WA_RECALC;
        menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_UNDELETE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(m, MUTT_ACL_DELETE, _("Can't undelete message")))
        {
          break;
        }

        mutt_set_flag(m, pview->pdata->email, MUTT_DELETE, false);
        mutt_set_flag(m, pview->pdata->email, MUTT_PURGE, false);
        menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;
      }

        //=======================================================================

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(m))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           undelete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this. */
        if (!assert_mailbox_permissions(m, MUTT_ACL_DELETE, _("Can't undelete messages")))
        {
          break;
        }

        int r = mutt_thread_set_flag(m, pview->pdata->email, MUTT_DELETE, false,
                                     (op != OP_UNDELETE_THREAD));
        if (r != -1)
        {
          r = mutt_thread_set_flag(m, pview->pdata->email, MUTT_PURGE, false,
                                   (op != OP_UNDELETE_THREAD));
        }
        if (r != -1)
        {
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
          if (c_resolve)
          {
            rc = (op == OP_DELETE_THREAD) ? OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
            op = -1;
          }

          if (!c_resolve && (c_pager_index_lines != 0))
            menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
          else
            menu_queue_redraw(pager_menu, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        }
        break;
      }

        //=======================================================================

      case OP_VERSION:
        mutt_message(mutt_make_version());
        break;

        //=======================================================================

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

        //=======================================================================

      case OP_VIEW_ATTACHMENTS:
        if (pview->flags & MUTT_PAGER_ATTACHMENT)
        {
          op = -1;
          rc = OP_ATTACH_COLLAPSE;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        dlg_select_attachment(NeoMutt->sub, ctx_mailbox(Context),
                              pview->pdata->email, pview->pdata->fp);
        if (pview->pdata->email->attach_del)
          m->changed = true;
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_MAIL_KEY:
      {
        if (!(WithCrypto & APPLICATION_PGP))
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);

        mutt_send_message(SEND_KEY, NULL, NULL, ctx_mailbox(pview->pdata->ctx),
                          &el, NeoMutt->sub);
        emaillist_clear(&el);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_EDIT_LABEL:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);
        rc = mutt_label_message(m, &el);
        emaillist_clear(&el);

        if (rc > 0)
        {
          m->changed = true;
          menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
          mutt_message(ngettext("%d label changed", "%d labels changed", rc), rc);
        }
        else
        {
          mutt_message(_("No labels changed"));
        }
        break;
      }

        //=======================================================================

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

        //=======================================================================

      case OP_EXTRACT_KEYS:
      {
        if (!WithCrypto)
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, pview->pdata->email);
        crypt_extract_keys_from_messages(m, &el);
        emaillist_clear(&el);
        menu_queue_redraw(pager_menu, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

        //=======================================================================

      case OP_CHECK_STATS:
        mutt_check_stats(m);
        break;

        //=======================================================================

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_FIRST:
      case OP_SIDEBAR_LAST:
      case OP_SIDEBAR_NEXT:
      case OP_SIDEBAR_NEXT_NEW:
      case OP_SIDEBAR_PAGE_DOWN:
      case OP_SIDEBAR_PAGE_UP:
      case OP_SIDEBAR_PREV:
      case OP_SIDEBAR_PREV_NEW:
      {
        struct MuttWindow *win_sidebar =
            mutt_window_find(dialog_find(rd.pview->win_pager), WT_SIDEBAR);
        if (!win_sidebar)
          break;
        sb_change_mailbox(win_sidebar, op);
        break;
      }

        //=======================================================================

      case OP_SIDEBAR_TOGGLE_VISIBLE:
        bool_str_toggle(NeoMutt->sub, "sidebar_visible", NULL);
        mutt_window_reflow(dialog_find(rd.pview->win_pager));
        break;

        //=======================================================================
#endif

      default:
        op = -1;
        break;
    }
  }
  //-------------------------------------------------------------------------
  // END OF ACT 3: Read user input loop - while (op != -1)
  //-------------------------------------------------------------------------

  pager_check_read_delay(pview, m, delay_read_timestamp, true);
  mutt_file_fclose(&rd.fp);
  if (pview->mode == PAGER_MODE_EMAIL)
  {
    pview->pdata->ctx->msg_in_pager = -1;
    priv->win_pbar->actions |= WA_RECALC;
    switch (rc)
    {
      case -1:
      case OP_DISPLAY_HEADERS:
        mutt_clear_pager_position();
        break;
      default:
        TopLine = rd.topline;
        OldEmail = pview->pdata->email;
        break;
    }
  }

  cleanup_quote(&rd.quote_list);

  for (size_t i = 0; i < rd.max_line; i++)
  {
    FREE(&(rd.line_info[i].syntax));
    if (rd.search_compiled && rd.line_info[i].search)
      FREE(&(rd.line_info[i].search));
  }
  if (rd.search_compiled)
  {
    regfree(&rd.search_re);
    rd.search_compiled = false;
  }
  FREE(&rd.line_info);

  if (rd.pview->win_index)
  {
    rd.pview->win_index->size = MUTT_WIN_SIZE_MAXIMISE;
    rd.pview->win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    rd.pview->win_index->parent->size = MUTT_WIN_SIZE_MAXIMISE;
    rd.pview->win_index->parent->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    window_set_visible(rd.pview->win_index->parent, true);
  }
  window_set_visible(rd.pview->win_pager->parent, false);
  mutt_window_reflow(dialog_find(rd.pview->win_pager));

  return (rc != -1) ? rc : 0;
}

/**
 * create_panel_pager - Create the Windows for the Pager panel
 * @param status_on_top true, if the Pager bar should be on top
 * @param shared        Shared Index data
 * @retval ptr New Pager Panel
 */
struct MuttWindow *create_panel_pager(bool status_on_top, struct IndexSharedData *shared)
{
  struct MuttWindow *panel_pager =
      mutt_window_new(WT_PAGER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  panel_pager->state.visible = false; // The Pager and Pager Bar are initially hidden

  struct MuttWindow *win_pager = menu_new_window(MENU_PAGER, NeoMutt->sub);
  panel_pager->focus = win_pager;

  struct PagerPrivateData *priv = pager_private_data_new();
  panel_pager->wdata = priv;
  panel_pager->wdata_free = pager_private_data_free;

  struct MuttWindow *win_pbar = pbar_new(panel_pager, shared, priv);
  if (status_on_top)
  {
    mutt_window_add_child(panel_pager, win_pbar);
    mutt_window_add_child(panel_pager, win_pager);
  }
  else
  {
    mutt_window_add_child(panel_pager, win_pager);
    mutt_window_add_child(panel_pager, win_pbar);
  }

  return panel_pager;
}
