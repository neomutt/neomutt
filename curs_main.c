/**
 * @file
 * GUI manage the main index (list of emails)
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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
#include <assert.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "alias.h"
#include "body.h"
#include "buffy.h"
#include "context.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pattern.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#include "tags.h"
#include "terminal.h"
#include "thread.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_POP
#include "pop.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif
#ifdef USE_NNTP
#include "nntp.h"
#endif

static const char *No_mailbox_is_open = N_("No mailbox is open.");
static const char *There_are_no_messages = N_("There are no messages.");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only.");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode.");
static const char *NoVisible = N_("No visible messages.");

#define CHECK_IN_MAILBOX                                                       \
  if (!Context)                                                                \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(No_mailbox_is_open));                                         \
    break;                                                                     \
  }

#define CHECK_MSGCOUNT                                                         \
  if (!Context)                                                                \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(No_mailbox_is_open));                                         \
    break;                                                                     \
  }                                                                            \
  else if (!Context->msgcount)                                                 \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(There_are_no_messages));                                      \
    break;                                                                     \
  }

#define CHECK_VISIBLE                                                          \
  if (Context && menu->current >= Context->vcount)                             \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(NoVisible));                                                  \
    break;                                                                     \
  }

#define CHECK_READONLY                                                         \
  if (Context->readonly)                                                       \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Mailbox_is_read_only));                                       \
    break;                                                                     \
  }

#define CHECK_ACL(aclbit, action)                                              \
  if (!mutt_bit_isset(Context->rights, aclbit))                                \
  {                                                                            \
    mutt_flushinp();                                                           \
    /* L10N: %s is one of the CHECK_ACL entries below. */                      \
    mutt_error(_("%s: Operation not permitted by ACL"), action);               \
    break;                                                                     \
  }

#define CHECK_ATTACH                                                           \
  if (OPT_ATTACH_MSG)                                                          \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Function_not_permitted_in_attach_message_mode));              \
    break;                                                                     \
  }

#define CURHDR Context->hdrs[Context->v2r[menu->current]]
#define UNREAD(h) mutt_thread_contains_unread(Context, h)
#define FLAGGED(h) mutt_thread_contains_flagged(Context, h)

#define CAN_COLLAPSE(header)                                                   \
  ((CollapseUnread || !UNREAD(header)) && (CollapseFlagged || !FLAGGED(header)))

/**
 * collapse/uncollapse all threads
 * @param menu   current menu
 * @param toggle toggle collapsed state
 *
 * This function is called by the OP_MAIN_COLLAPSE_ALL command and on folder
 * enter if the CollapseAll option is set. In the first case, the @a toggle
 * parameter is 1 to actually toggle collapsed/uncollapsed state on all
 * threads. In the second case, the @a toggle parameter is 0, actually turning
 * this function into a one-way collapse.
 */
static void collapse_all(struct Menu *menu, int toggle)
{
  struct Header *h = NULL, *base = NULL;
  struct MuttThread *thread = NULL, *top = NULL;
  int final;

  if (!Context || (Context->msgcount == 0))
    return;

  /* Figure out what the current message would be after folding / unfolding,
   * so that we can restore the cursor in a sane way afterwards. */
  if (CURHDR->collapsed && toggle)
    final = mutt_uncollapse_thread(Context, CURHDR);
  else if (CAN_COLLAPSE(CURHDR))
    final = mutt_collapse_thread(Context, CURHDR);
  else
    final = CURHDR->virtual;

  base = Context->hdrs[Context->v2r[final]];

  /* Iterate all threads, perform collapse/uncollapse as needed */
  top = Context->tree;
  Context->collapsed = toggle ? !Context->collapsed : true;
  while ((thread = top) != NULL)
  {
    while (!thread->message)
      thread = thread->child;
    h = thread->message;

    if (h->collapsed != Context->collapsed)
    {
      if (h->collapsed)
        mutt_uncollapse_thread(Context, h);
      else if (CAN_COLLAPSE(h))
        mutt_collapse_thread(Context, h);
    }
    top = top->next;
  }

  /* Restore the cursor */
  mutt_set_virtual(Context);
  for (int j = 0; j < Context->vcount; j++)
  {
    if (Context->hdrs[Context->v2r[j]]->index == base->index)
    {
      menu->current = j;
      break;
    }
  }

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

static int ci_next_undeleted(int msgno)
{
  for (int i = msgno + 1; i < Context->vcount; i++)
    if (!Context->hdrs[Context->v2r[i]]->deleted)
      return i;
  return -1;
}

static int ci_previous_undeleted(int msgno)
{
  for (int i = msgno - 1; i >= 0; i--)
    if (!Context->hdrs[Context->v2r[i]]->deleted)
      return i;
  return -1;
}

/**
 * ci_first_message - Get index of first new message
 *
 * Return the index of the first new message, or failing that, the first
 * unread message.
 */
static int ci_first_message(void)
{
  if (Context && Context->msgcount)
  {
    int old = -1;
    for (int i = 0; i < Context->vcount; i++)
    {
      if (!Context->hdrs[Context->v2r[i]]->read &&
          !Context->hdrs[Context->v2r[i]]->deleted)
      {
        if (!Context->hdrs[Context->v2r[i]]->old)
          return i;
        else if (old == -1)
          old = i;
      }
    }
    if (old != -1)
      return old;

    /* If Sort is reverse and not threaded, the latest message is first.
     * If Sort is threaded, the latest message is first iff exactly one
     * of Sort and SortAux are reverse.
     */
    if (((Sort & SORT_REVERSE) && (Sort & SORT_MASK) != SORT_THREADS) ||
        ((Sort & SORT_MASK) == SORT_THREADS && ((Sort ^ SortAux) & SORT_REVERSE)))
    {
      return 0;
    }
    else
    {
      return (Context->vcount ? Context->vcount - 1 : 0);
    }
  }
  return 0;
}

/**
 * mx_toggle_write - Toggle the mailbox's readonly flag
 *
 * This should be in mx.c, but it only gets used here.
 */
static int mx_toggle_write(struct Context *ctx)
{
  if (!ctx)
    return -1;

  if (ctx->readonly)
  {
    mutt_error(_("Cannot toggle write on a readonly mailbox!"));
    return -1;
  }

  if (ctx->dontwrite)
  {
    ctx->dontwrite = false;
    mutt_message(_("Changes to folder will be written on folder exit."));
  }
  else
  {
    ctx->dontwrite = true;
    mutt_message(_("Changes to folder will not be written."));
  }

  return 0;
}

static void resort_index(struct Menu *menu)
{
  struct Header *current = CURHDR;

  menu->current = -1;
  mutt_sort_headers(Context, 0);
  /* Restore the current message */

  for (int i = 0; i < Context->vcount; i++)
  {
    if (Context->hdrs[Context->v2r[i]] == current)
    {
      menu->current = i;
      break;
    }
  }

  if ((Sort & SORT_MASK) == SORT_THREADS && menu->current < 0)
    menu->current = mutt_parent_message(Context, current, 0);

  if (menu->current < 0)
    menu->current = ci_first_message();

  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
}

void update_index(struct Menu *menu, struct Context *ctx, int check, int oldcount, int index_hint)
{
  /* store pointers to the newly added messages */
  struct Header **save_new = NULL;

  if (!menu || !ctx)
    return;

  /* take note of the current message */
  if (oldcount)
  {
    if (menu->current < ctx->vcount)
      menu->oldcurrent = index_hint;
    else
      oldcount = 0; /* invalid message number! */
  }

  /* We are in a limited view. Check if the new message(s) satisfy
   * the limit criteria. If they do, set their virtual msgno so that
   * they will be visible in the limited view */
  if (ctx->pattern)
  {
    for (int i = (check == MUTT_REOPENED) ? 0 : oldcount; i < ctx->msgcount; i++)
    {
      if (!i)
        ctx->vcount = 0;

      if (mutt_pattern_exec(ctx->limit_pattern, MUTT_MATCH_FULL_ADDRESS, ctx,
                            ctx->hdrs[i], NULL))
      {
        assert(ctx->vcount < ctx->msgcount);
        ctx->hdrs[i]->virtual = ctx->vcount;
        ctx->v2r[ctx->vcount] = i;
        ctx->hdrs[i]->limited = true;
        ctx->vcount++;
        struct Body *b = ctx->hdrs[i]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset;
      }
    }
  }

  /* save the list of new messages */
  if (UncollapseNew && oldcount && check != MUTT_REOPENED && ((Sort & SORT_MASK) == SORT_THREADS))
  {
    save_new = mutt_mem_malloc(sizeof(struct Header *) * (ctx->msgcount - oldcount));
    for (int i = oldcount; i < ctx->msgcount; i++)
      save_new[i - oldcount] = ctx->hdrs[i];
  }

  /* if the mailbox was reopened, need to rethread from scratch */
  mutt_sort_headers(ctx, (check == MUTT_REOPENED));

  /* uncollapse threads with new mail */
  if (UncollapseNew && ((Sort & SORT_MASK) == SORT_THREADS))
  {
    if (check == MUTT_REOPENED)
    {
      struct MuttThread *h = NULL, *k = NULL;

      ctx->collapsed = false;

      for (h = ctx->tree; h; h = h->next)
      {
        for (k = h; !k->message; k = k->child)
          ;
        mutt_uncollapse_thread(ctx, k->message);
      }
      mutt_set_virtual(ctx);
    }
    else if (oldcount)
    {
      for (int i = 0; i < ctx->msgcount - oldcount; i++)
      {
        for (int k = 0; k < ctx->msgcount; k++)
        {
          struct Header *h = ctx->hdrs[k];
          if (h == save_new[i] && (!ctx->pattern || h->limited))
            mutt_uncollapse_thread(ctx, h);
        }
      }
      FREE(&save_new);
      mutt_set_virtual(ctx);
    }
  }

  menu->current = -1;
  if (oldcount)
  {
    /* restore the current message to the message it was pointing to */
    for (int i = 0; i < ctx->vcount; i++)
    {
      if (ctx->hdrs[ctx->v2r[i]]->index == menu->oldcurrent)
      {
        menu->current = i;
        break;
      }
    }
  }

  if (menu->current < 0)
    menu->current = ci_first_message();
}

static int main_change_folder(struct Menu *menu, int op, char *buf,
                              size_t buflen, int *oldcount, int *index_hint)
{
#ifdef USE_NNTP
  if (OPT_NEWS)
  {
    OPT_NEWS = false;
    nntp_expand_path(buf, buflen, &CurrentNewsSrv->conn->account);
  }
  else
#endif
    mutt_expand_path(buf, buflen);
  if (mx_get_magic(buf) <= 0)
  {
    mutt_error(_("%s is not a mailbox."), buf);
    return -1;
  }

  /* keepalive failure in mutt_enter_fname may kill connection. #3028 */
  if (Context && !Context->path)
    FREE(&Context);

  if (Context)
  {
    char *new_last_folder = NULL;

#ifdef USE_COMPRESSED
    if (Context->compress_info && Context->realpath)
      new_last_folder = mutt_str_strdup(Context->realpath);
    else
#endif
      new_last_folder = mutt_str_strdup(Context->path);
    *oldcount = Context ? Context->msgcount : 0;

    int check = mx_close_mailbox(Context, index_hint);
    if (check != 0)
    {
      if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
        update_index(menu, Context, check, *oldcount, *index_hint);

      FREE(&new_last_folder);
      OPT_SEARCH_INVALID = true;
      menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
      return 0;
    }
    FREE(&Context);
    FREE(&LastFolder);
    LastFolder = new_last_folder;
  }
  mutt_str_replace(&CurrentFolder, buf);

  mutt_sleep(0);

  /* Note that menu->menu may be MENU_PAGER if the change folder
   * operation originated from the pager.
   *
   * However, exec commands currently use CurrentMenu to determine what
   * functions are available, which is automatically set by the
   * mutt_push/pop_current_menu() functions.  If that changes, the menu
   * would need to be reset here, and the pager cleanup code after the
   * switch statement would need to be run. */
  mutt_folder_hook(buf);

  Context = mx_open_mailbox(
      buf, (ReadOnly || (op == OP_MAIN_CHANGE_FOLDER_READONLY)) ? MUTT_READONLY : 0, NULL);
  if (Context)
  {
    menu->current = ci_first_message();
  }
  else
    menu->current = 0;

  if (((Sort & SORT_MASK) == SORT_THREADS) && CollapseAll)
    collapse_all(menu, 0);

#ifdef USE_SIDEBAR
  mutt_sb_set_open_buffy();
#endif

  mutt_clear_error();
  mutt_buffy_check(true); /* force the buffy check after we have changed the folder */
  menu->redraw = REDRAW_FULL;
  OPT_SEARCH_INVALID = true;

  return 0;
}

/**
 * index_make_entry - Format a menu item for the index list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
void index_make_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  if (!Context || !menu || (num < 0) || (num >= Context->hdrmax))
    return;

  struct Header *h = Context->hdrs[Context->v2r[num]];
  if (!h)
    return;

  enum FormatFlag flag = MUTT_FORMAT_MAKEPRINT | MUTT_FORMAT_ARROWCURSOR | MUTT_FORMAT_INDEX;
  struct MuttThread *tmp = NULL;

  if ((Sort & SORT_MASK) == SORT_THREADS && h->tree)
  {
    flag |= MUTT_FORMAT_TREE; /* display the thread tree */
    if (h->display_subject)
      flag |= MUTT_FORMAT_FORCESUBJ;
    else
    {
      const int reverse = Sort & SORT_REVERSE;
      int edgemsgno;
      if (reverse)
      {
        if (menu->top + menu->pagelen > menu->max)
          edgemsgno = Context->v2r[menu->max - 1];
        else
          edgemsgno = Context->v2r[menu->top + menu->pagelen - 1];
      }
      else
        edgemsgno = Context->v2r[menu->top];

      for (tmp = h->thread->parent; tmp; tmp = tmp->parent)
      {
        if (!tmp->message)
          continue;

        /* if no ancestor is visible on current screen, provisionally force
         * subject... */
        if (reverse ? tmp->message->msgno > edgemsgno : tmp->message->msgno < edgemsgno)
        {
          flag |= MUTT_FORMAT_FORCESUBJ;
          break;
        }
        else if (tmp->message->virtual >= 0)
          break;
      }
      if (flag & MUTT_FORMAT_FORCESUBJ)
      {
        for (tmp = h->thread->prev; tmp; tmp = tmp->prev)
        {
          if (!tmp->message)
            continue;

          /* ...but if a previous sibling is available, don't force it */
          if (reverse ? tmp->message->msgno > edgemsgno : tmp->message->msgno < edgemsgno)
            break;
          else if (tmp->message->virtual >= 0)
          {
            flag &= ~MUTT_FORMAT_FORCESUBJ;
            break;
          }
        }
      }
    }
  }

  mutt_make_string_flags(buf, buflen, NONULL(IndexFormat), Context, h, flag);
}

int index_color(int index_no)
{
  if (!Context || (index_no < 0))
    return 0;

  struct Header *h = Context->hdrs[Context->v2r[index_no]];

  if (h && h->pair)
    return h->pair;

  mutt_set_header_color(Context, h);
  if (h)
    return h->pair;

  return 0;
}

/**
 * mutt_draw_statusline - Draw a highlighted status bar
 * @param cols   Maximum number of screen columns
 * @param buf    Message to be displayed
 * @param buflen Length of the buffer
 *
 * Users configure the highlighting of the status bar, e.g.
 *     color status red default "[0-9][0-9]:[0-9][0-9]"
 *
 * Where regexes overlap, the one nearest the start will be used.
 * If two regexes start at the same place, the longer match will be used.
 */
void mutt_draw_statusline(int cols, const char *buf, int buflen)
{
  size_t i = 0;
  size_t offset = 0;
  bool found = false;
  size_t chunks = 0;
  size_t len = 0;

  struct Syntax
  {
    int color;
    int first;
    int last;
  } *syntax = NULL;

  if (!buf || !stdscr)
    return;

  do
  {
    struct ColorLine *cl = NULL;
    found = false;

    if (!buf[offset])
      break;

    /* loop through each "color status regex" */
    STAILQ_FOREACH(cl, &ColorStatusList, entries)
    {
      regmatch_t pmatch[cl->match + 1];

      if (regexec(&cl->regex, buf + offset, cl->match + 1, pmatch, 0) != 0)
        continue; /* regex doesn't match the status bar */

      int first = pmatch[cl->match].rm_so + offset;
      int last = pmatch[cl->match].rm_eo + offset;

      if (first == last)
        continue; /* ignore an empty regex */

      if (!found)
      {
        chunks++;
        mutt_mem_realloc(&syntax, chunks * sizeof(struct Syntax));
      }

      i = chunks - 1;
      if (!found || (first < syntax[i].first) ||
          ((first == syntax[i].first) && (last > syntax[i].last)))
      {
        syntax[i].color = cl->pair;
        syntax[i].first = first;
        syntax[i].last = last;
      }
      found = true;
    }

    if (syntax)
    {
      offset = syntax[i].last;
    }
  } while (found);

  /* Only 'len' bytes will fit into 'cols' screen columns */
  len = mutt_wstr_trunc(buf, buflen, cols, NULL);

  offset = 0;

  if ((chunks > 0) && (syntax[0].first > 0))
  {
    /* Text before the first highlight */
    addnstr(buf, MIN(len, syntax[0].first));
    attrset(ColorDefs[MT_COLOR_STATUS]);
    if (len <= syntax[0].first)
      goto dsl_finish; /* no more room */

    offset = syntax[0].first;
  }

  for (i = 0; i < chunks; i++)
  {
    /* Highlighted text */
    attrset(syntax[i].color);
    addnstr(buf + offset, MIN(len, syntax[i].last) - offset);
    if (len <= syntax[i].last)
      goto dsl_finish; /* no more room */

    size_t next;
    if ((i + 1) == chunks)
    {
      next = len;
    }
    else
    {
      next = MIN(len, syntax[i + 1].first);
    }

    attrset(ColorDefs[MT_COLOR_STATUS]);
    offset = syntax[i].last;
    addnstr(buf + offset, next - offset);

    offset = next;
    if (offset >= len)
      goto dsl_finish; /* no more room */
  }

  attrset(ColorDefs[MT_COLOR_STATUS]);
  if (offset < len)
  {
    /* Text after the last highlight */
    addnstr(buf + offset, len - offset);
  }

  int width = mutt_strwidth(buf);
  if (width < cols)
  {
    /* Pad the rest of the line with whitespace */
    mutt_paddstr(cols - width, "");
  }
dsl_finish:
  FREE(&syntax);
}

static const struct Mapping IndexHelp[] = {
  { N_("Quit"), OP_QUIT },
  { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Save"), OP_SAVE },
  { N_("Mail"), OP_MAIL },
  { N_("Reply"), OP_REPLY },
  { N_("Group"), OP_GROUP_REPLY },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

#ifdef USE_NNTP
struct Mapping IndexNewsHelp[] = {
  { N_("Quit"), OP_QUIT },
  { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Save"), OP_SAVE },
  { N_("Post"), OP_POST },
  { N_("Followup"), OP_FOLLOWUP },
  { N_("Catchup"), OP_CATCHUP },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};
#endif

static void index_menu_redraw(struct Menu *menu)
{
  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);
    mutt_show_error();
  }

#ifdef USE_SIDEBAR
  if (menu->redraw & REDRAW_SIDEBAR)
  {
    mutt_sb_set_buffystats(Context);
    menu_redraw_sidebar(menu);
  }
#endif

  if (Context && Context->hdrs && !(menu->current >= Context->vcount))
  {
    menu_check_recenter(menu);

    if (menu->redraw & REDRAW_INDEX)
    {
      menu_redraw_index(menu);
      menu->redraw |= REDRAW_STATUS;
    }
    else if (menu->redraw & (REDRAW_MOTION_RESYNCH | REDRAW_MOTION))
      menu_redraw_motion(menu);
    else if (menu->redraw & REDRAW_CURRENT)
      menu_redraw_current(menu);
  }

  if (menu->redraw & REDRAW_STATUS)
  {
    char buf[LONG_STRING];
    menu_status_line(buf, sizeof(buf), menu, NONULL(StatusFormat));
    mutt_window_move(MuttStatusWindow, 0, 0);
    SETCOLOR(MT_COLOR_STATUS);
    mutt_draw_statusline(MuttStatusWindow->cols, buf, sizeof(buf));
    NORMAL_COLOR;
    menu->redraw &= ~REDRAW_STATUS;
    if (TsEnabled && TsSupported)
    {
      menu_status_line(buf, sizeof(buf), menu, NONULL(TSStatusFormat));
      mutt_ts_status(buf);
      menu_status_line(buf, sizeof(buf), menu, NONULL(TSIconFormat));
      mutt_ts_icon(buf);
    }
  }

  menu->redraw = 0;
}

/**
 * mutt_index_menu - Display a list of emails
 *
 * This function handles the message index window as well as commands returned
 * from the pager (MENU_PAGER).
 */
int mutt_index_menu(void)
{
  char buf[LONG_STRING], helpstr[LONG_STRING];
  int flags;
  int op = OP_NULL;
  bool done = false; /* controls when to exit the "event" loop */
  int i = 0, j;
  bool tag = false; /* has the tag-prefix command been pressed? */
  int newcount = -1;
  int oldcount = -1;
  int rc = -1;
  char *cp = NULL; /* temporary variable. */
  int index_hint;  /* used to restore cursor position */
  bool do_buffy_notify = true;
  int close = 0; /* did we OP_QUIT or OP_EXIT out of this menu? */
  int attach_msg = OPT_ATTACH_MSG;

  struct Menu *menu = mutt_menu_new(MENU_MAIN);
  menu->make_entry = index_make_entry;
  menu->color = index_color;
  menu->current = ci_first_message();
  menu->help =
      mutt_compile_help(helpstr, sizeof(helpstr), MENU_MAIN,
#ifdef USE_NNTP
                        (Context && (Context->magic == MUTT_NNTP)) ? IndexNewsHelp :
#endif
                                                                     IndexHelp);
  menu->custom_menu_redraw = index_menu_redraw;
  mutt_menu_push_current(menu);

  if (!attach_msg)
    mutt_buffy_check(true); /* force the buffy check after we enter the folder */

  if (((Sort & SORT_MASK) == SORT_THREADS) && CollapseAll)
  {
    collapse_all(menu, 0);
    menu->redraw = REDRAW_FULL;
  }

  while (true)
  {
    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (op==-2), but do clear on an abort (op==-1)
     */
    if (tag && op != OP_TAG_PREFIX && op != OP_TAG_PREFIX_COND && op != -2)
      tag = false;

    /* check if we need to resort the index because just about
     * any 'op' below could do mutt_enter_command(), either here or
     * from any new menu launched, and change $sort/$sort_aux
     */
    if (OPT_NEED_RESORT && Context && Context->msgcount && menu->current >= 0)
      resort_index(menu);

    menu->max = Context ? Context->vcount : 0;
    oldcount = Context ? Context->msgcount : 0;

    if (OPT_REDRAW_TREE && Context && Context->msgcount && (Sort & SORT_MASK) == SORT_THREADS)
    {
      mutt_draw_tree(Context);
      menu->redraw |= REDRAW_STATUS;
      OPT_REDRAW_TREE = false;
    }

    if (Context)
      Context->menu = menu;

    if (Context && !attach_msg)
    {
      int check;
      /* check for new mail in the mailbox.  If nonzero, then something has
       * changed about the file (either we got new mail or the file was
       * modified underneath us.)
       */

      index_hint =
          (Context->vcount && menu->current >= 0 && menu->current < Context->vcount) ?
              CURHDR->index :
              0;

      check = mx_check_mailbox(Context, &index_hint);
      if (check < 0)
      {
        if (!Context->path)
        {
          /* fatal error occurred */
          FREE(&Context);
          menu->redraw = REDRAW_FULL;
        }

        OPT_SEARCH_INVALID = true;
      }
      else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED || check == MUTT_FLAGS)
      {
        /* notify the user of new mail */
        if (check == MUTT_REOPENED)
          mutt_error(
              _("Mailbox was externally modified.  Flags may be wrong."));
        else if (check == MUTT_NEW_MAIL)
        {
          for (i = oldcount; i < Context->msgcount; i++)
          {
            if (!Context->hdrs[i]->read)
            {
              mutt_message(_("New mail in this mailbox."));
              if (BeepNew)
                beep();
              if (NewMailCommand)
              {
                char cmd[LONG_STRING];
                menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCommand));
                if (mutt_system(cmd) != 0)
                  mutt_error(_("Error running \"%s\"!"), cmd);
              }
              break;
            }
          }
        }
        else if (check == MUTT_FLAGS)
          mutt_message(_("Mailbox was externally modified."));

        /* avoid the message being overwritten by buffy */
        do_buffy_notify = false;

        bool q = Context->quiet;
        Context->quiet = true;
        update_index(menu, Context, check, oldcount, index_hint);
        Context->quiet = q;

        menu->redraw = REDRAW_FULL;
        menu->max = Context->vcount;

        OPT_SEARCH_INVALID = true;
      }
    }

    if (!attach_msg)
    {
      /* check for new mail in the incoming folders */
      oldcount = newcount;
      newcount = mutt_buffy_check(false);
      if (newcount != oldcount)
        menu->redraw |= REDRAW_STATUS;
      if (do_buffy_notify)
      {
        if (mutt_buffy_notify())
        {
          menu->redraw |= REDRAW_STATUS;
          if (BeepNew)
            beep();
          if (NewMailCommand)
          {
            char cmd[LONG_STRING];
            menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCommand));
            if (mutt_system(cmd) != 0)
              mutt_error(_("Error running \"%s\"!"), cmd);
          }
        }
      }
      else
        do_buffy_notify = true;
    }

    if (op >= 0)
      mutt_curs_set(0);

    if (menu->menu == MENU_MAIN)
    {
      index_menu_redraw(menu);

      /* give visual indication that the next command is a tag- command */
      if (tag)
      {
        mutt_window_mvaddstr(MuttMessageWindow, 0, 0, "tag-");
        mutt_window_clrtoeol(MuttMessageWindow);
      }

      if (menu->current < menu->max)
        menu->oldcurrent = menu->current;
      else
        menu->oldcurrent = -1;

      if (ArrowCursor)
        mutt_window_move(MuttIndexWindow, menu->current - menu->top + menu->offset, 2);
      else if (BrailleFriendly)
        mutt_window_move(MuttIndexWindow, menu->current - menu->top + menu->offset, 0);
      else
        mutt_window_move(MuttIndexWindow, menu->current - menu->top + menu->offset,
                         MuttIndexWindow->cols - 1);
      mutt_refresh();

      if (SigWinch)
      {
        mutt_flushinp();
        mutt_resize_screen();
        SigWinch = 0;
        menu->top = 0; /* so we scroll the right amount */
        /*
         * force a real complete redraw.  clrtobot() doesn't seem to be able
         * to handle every case without this.
         */
        clearok(stdscr, true);
        continue;
      }

      op = km_dokey(MENU_MAIN);

      mutt_debug(4, "[%d]: Got op %d\n", __LINE__, op);

      /* either user abort or timeout */
      if (op < 0)
      {
        mutt_timeout_hook();
        if (tag)
          mutt_window_clearline(MuttMessageWindow, 0);
        continue;
      }

      mutt_curs_set(1);

      /* special handling for the tag-prefix function */
      if (op == OP_TAG_PREFIX || op == OP_TAG_PREFIX_COND)
      {
        /* A second tag-prefix command aborts */
        if (tag)
        {
          tag = false;
          mutt_window_clearline(MuttMessageWindow, 0);
          continue;
        }

        if (!Context)
        {
          mutt_error(_("No mailbox is open."));
          continue;
        }

        if (!Context->tagged)
        {
          if (op == OP_TAG_PREFIX)
            mutt_error(_("No tagged messages."));
          else if (op == OP_TAG_PREFIX_COND)
          {
            mutt_flush_macro_to_endcond();
            mutt_message(_("Nothing to do."));
          }
          continue;
        }

        /* get the real command */
        tag = true;
        continue;
      }
      else if (AutoTag && Context && Context->tagged)
        tag = true;

      mutt_clear_error();
    }
    else
    {
      if (menu->current < menu->max)
        menu->oldcurrent = menu->current;
      else
        menu->oldcurrent = -1;

      mutt_curs_set(1); /* fallback from the pager */
    }

#ifdef USE_NNTP
    OPT_NEWS = false; /* for any case */
#endif

#ifdef USE_NOTMUCH
    if (Context)
      nm_debug_check(Context);
#endif

    switch (op)
    {
        /* ----------------------------------------------------------------------
         * movement commands
         */

      case OP_BOTTOM_PAGE:
        menu_bottom_page(menu);
        break;
      case OP_CURRENT_BOTTOM:
        menu_current_bottom(menu);
        break;
      case OP_CURRENT_MIDDLE:
        menu_current_middle(menu);
        break;
      case OP_CURRENT_TOP:
        menu_current_top(menu);
        break;
      case OP_FIRST_ENTRY:
        menu_first_entry(menu);
        break;
      case OP_HALF_DOWN:
        menu_half_down(menu);
        break;
      case OP_HALF_UP:
        menu_half_up(menu);
        break;
      case OP_LAST_ENTRY:
        menu_last_entry(menu);
        break;
      case OP_MIDDLE_PAGE:
        menu_middle_page(menu);
        break;
      case OP_NEXT_LINE:
        menu_next_line(menu);
        break;
      case OP_NEXT_PAGE:
        menu_next_page(menu);
        break;
      case OP_PREV_LINE:
        menu_prev_line(menu);
        break;
      case OP_PREV_PAGE:
        menu_prev_page(menu);
        break;
      case OP_TOP_PAGE:
        menu_top_page(menu);
        break;

#ifdef USE_NNTP
      case OP_GET_PARENT:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        /* fallthrough */

      case OP_GET_MESSAGE:
        CHECK_IN_MAILBOX;
        CHECK_READONLY;
        CHECK_ATTACH;
        if (Context->magic == MUTT_NNTP)
        {
          struct Header *hdr = NULL;

          if (op == OP_GET_MESSAGE)
          {
            buf[0] = 0;
            if (mutt_get_field(_("Enter Message-Id: "), buf, sizeof(buf), 0) != 0 ||
                !buf[0])
            {
              break;
            }
          }
          else
          {
            if (STAILQ_EMPTY(&CURHDR->env->references))
            {
              mutt_error(_("Article has no parent reference."));
              break;
            }
            mutt_str_strfcpy(buf, STAILQ_FIRST(&CURHDR->env->references)->data,
                             sizeof(buf));
          }
          if (!Context->id_hash)
            Context->id_hash = mutt_make_id_hash(Context);
          hdr = mutt_hash_find(Context->id_hash, buf);
          if (hdr)
          {
            if (hdr->virtual != -1)
            {
              menu->current = hdr->virtual;
              menu->redraw = REDRAW_MOTION_RESYNCH;
            }
            else if (hdr->collapsed)
            {
              mutt_uncollapse_thread(Context, hdr);
              mutt_set_virtual(Context);
              menu->current = hdr->virtual;
              menu->redraw = REDRAW_MOTION_RESYNCH;
            }
            else
              mutt_error(_("Message is not visible in limited view."));
          }
          else
          {
            int rc2;

            mutt_message(_("Fetching %s from server..."), buf);
            rc2 = nntp_check_msgid(Context, buf);
            if (rc2 == 0)
            {
              hdr = Context->hdrs[Context->msgcount - 1];
              mutt_sort_headers(Context, 0);
              menu->current = hdr->virtual;
              menu->redraw = REDRAW_FULL;
            }
            else if (rc2 > 0)
              mutt_error(_("Article %s not found on the server."), buf);
          }
        }
        break;

      case OP_GET_CHILDREN:
      case OP_RECONSTRUCT_THREAD:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        CHECK_ATTACH;
        if (Context->magic == MUTT_NNTP)
        {
          int oldmsgcount = Context->msgcount;
          int oldindex = CURHDR->index;
          int rc2 = 0;

          if (!CURHDR->env->message_id)
          {
            mutt_error(_("No Message-Id. Unable to perform operation."));
            break;
          }

          mutt_message(_("Fetching message headers..."));
          if (!Context->id_hash)
            Context->id_hash = mutt_make_id_hash(Context);
          mutt_str_strfcpy(buf, CURHDR->env->message_id, sizeof(buf));

          /* trying to find msgid of the root message */
          if (op == OP_RECONSTRUCT_THREAD)
          {
            struct ListNode *ref;
            STAILQ_FOREACH(ref, &CURHDR->env->references, entries)
            {
              if (mutt_hash_find(Context->id_hash, ref->data) == NULL)
              {
                rc2 = nntp_check_msgid(Context, ref->data);
                if (rc2 < 0)
                  break;
              }

              /* the last msgid in References is the root message */
              if (!STAILQ_NEXT(ref, entries))
                mutt_str_strfcpy(buf, ref->data, sizeof(buf));
            }
          }

          /* fetching all child messages */
          if (rc2 >= 0)
            rc2 = nntp_check_children(Context, buf);

          /* at least one message has been loaded */
          if (Context->msgcount > oldmsgcount)
          {
            struct Header *oldcur = CURHDR;
            struct Header *hdr = NULL;
            bool quiet = Context->quiet;

            if (rc2 < 0)
              Context->quiet = true;
            mutt_sort_headers(Context, (op == OP_RECONSTRUCT_THREAD));
            Context->quiet = quiet;

            /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
               update the index */
            if (menu->menu == MENU_PAGER)
            {
              menu->current = oldcur->virtual;
              menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
              op = OP_DISPLAY_MESSAGE;
              continue;
            }

            /* if the root message was retrieved, move to it */
            hdr = mutt_hash_find(Context->id_hash, buf);
            if (hdr)
              menu->current = hdr->virtual;

            /* try to restore old position */
            else
            {
              for (int k = 0; k < Context->msgcount; k++)
              {
                if (Context->hdrs[k]->index == oldindex)
                {
                  menu->current = Context->hdrs[k]->virtual;
                  /* as an added courtesy, recenter the menu
                   * with the current entry at the middle of the screen */
                  menu_check_recenter(menu);
                  menu_current_middle(menu);
                }
              }
            }
            menu->redraw = REDRAW_FULL;
          }
          else if (rc2 >= 0)
          {
            mutt_error(_("No deleted messages found in the thread."));
            /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
               update the index */
            if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
          }
        }
        break;
#endif

      case OP_JUMP:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (isdigit(LastKey))
          mutt_unget_event(LastKey, 0);
        buf[0] = 0;
        if (mutt_get_field(_("Jump to message: "), buf, sizeof(buf), 0) != 0 || !buf[0])
        {
          if (menu->menu == MENU_PAGER)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          break;
        }

        if (mutt_str_atoi(buf, &i) < 0)
        {
          mutt_error(_("Argument must be a message number."));
          break;
        }

        if (i > 0 && i <= Context->msgcount)
        {
          for (j = i - 1; j < Context->msgcount; j++)
          {
            if (Context->hdrs[j]->virtual != -1)
              break;
          }
          if (j >= Context->msgcount)
          {
            for (j = i - 2; j >= 0; j--)
            {
              if (Context->hdrs[j]->virtual != -1)
                break;
            }
          }

          if (j >= 0)
          {
            menu->current = Context->hdrs[j]->virtual;
            if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
            else
              menu->redraw = REDRAW_MOTION;
          }
          else
            mutt_error(_("That message is not visible."));
        }
        else
          mutt_error(_("Invalid message number."));

        break;

        /* --------------------------------------------------------------------
         * `index' specific commands
         */

      case OP_MAIN_DELETE_PATTERN:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message(s)"));

        CHECK_ATTACH;
        mutt_pattern_func(MUTT_DELETE, _("Delete messages matching: "));
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

#ifdef USE_POP
      case OP_MAIN_FETCH_MAIL:

        CHECK_ATTACH;
        pop_fetch_mail();
        menu->redraw = REDRAW_FULL;
        break;
#endif /* USE_POP */

      case OP_SHOW_MESSAGES:
      {
        char tempfile[PATH_MAX];
        mutt_mktemp(tempfile, sizeof(tempfile));

        FILE *fp = mutt_file_fopen(tempfile, "a+");
        if (!fp)
        {
          mutt_perror("fopen");
          break;
        }

        log_queue_save(fp);
        mutt_file_fclose(&fp);

        mutt_do_pager("messages", tempfile, MUTT_PAGER_LOGS, NULL);
        break;
      }

      case OP_HELP:

        mutt_help(MENU_MAIN);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIN_SHOW_LIMIT:
        CHECK_IN_MAILBOX;
        if (!Context->pattern)
          mutt_message(_("No limit pattern is in effect."));
        else
        {
          char buf2[STRING];
          /* L10N: ask for a limit to apply */
          snprintf(buf2, sizeof(buf2), _("Limit: %s"), Context->pattern);
          mutt_message("%s", buf2);
        }
        break;

      case OP_LIMIT_CURRENT_THREAD:
      case OP_MAIN_LIMIT:
      case OP_TOGGLE_READ:

        CHECK_IN_MAILBOX;
        menu->oldcurrent =
            (Context->vcount && menu->current >= 0 && menu->current < Context->vcount) ?
                CURHDR->index :
                -1;
        if (op == OP_TOGGLE_READ)
        {
          char buf2[LONG_STRING];

          if (!Context->pattern || (strncmp(Context->pattern, "!~R!~D~s", 8) != 0))
          {
            snprintf(buf2, sizeof(buf2), "!~R!~D~s%s",
                     Context->pattern ? Context->pattern : ".*");
            OPT_HIDE_READ = true;
          }
          else
          {
            mutt_str_strfcpy(buf2, Context->pattern + 8, sizeof(buf2));
            if (!*buf2 || (strncmp(buf2, ".*", 2) == 0))
              snprintf(buf2, sizeof(buf2), "~A");
            OPT_HIDE_READ = false;
          }
          FREE(&Context->pattern);
          Context->pattern = mutt_str_strdup(buf2);
        }

        if (((op == OP_LIMIT_CURRENT_THREAD) && mutt_limit_current_thread(CURHDR)) ||
            ((op == OP_MAIN_LIMIT) &&
             (mutt_pattern_func(MUTT_LIMIT,
                                _("Limit to messages matching: ")) == 0)))
        {
          if (menu->oldcurrent >= 0)
          {
            /* try to find what used to be the current message */
            menu->current = -1;
            for (i = 0; i < Context->vcount; i++)
            {
              if (Context->hdrs[Context->v2r[i]]->index == menu->oldcurrent)
              {
                menu->current = i;
                break;
              }
            }
            if (menu->current < 0)
              menu->current = 0;
          }
          else
            menu->current = 0;
          if (Context->msgcount && (Sort & SORT_MASK) == SORT_THREADS)
            mutt_draw_tree(Context);
          menu->redraw = REDRAW_FULL;
        }
        if (Context->pattern)
          mutt_message(_("To view all messages, limit to \"all\"."));
        break;

      case OP_QUIT:

        close = op;
        if (attach_msg)
        {
          done = true;
          break;
        }

        if (query_quadoption(Quit, _("Quit NeoMutt?")) == MUTT_YES)
        {
          int check;

          oldcount = Context ? Context->msgcount : 0;

          mutt_startup_shutdown_hook(MUTT_SHUTDOWNHOOK);

          if (!Context || (check = mx_close_mailbox(Context, &index_hint)) == 0)
            done = true;
          else
          {
            if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
              update_index(menu, Context, check, oldcount, index_hint);

            menu->redraw = REDRAW_FULL; /* new mail arrived? */
            OPT_SEARCH_INVALID = true;
          }
        }
        break;

      case OP_REDRAW:

        clearok(stdscr, true);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        menu->current = mutt_search_command(menu->current, op);
        if (menu->current == -1)
          menu->current = menu->oldcurrent;
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_SORT:
      case OP_SORT_REVERSE:

        if (mutt_select_sort((op == OP_SORT_REVERSE)) == 0)
        {
          if (Context && Context->msgcount)
          {
            resort_index(menu);
            OPT_SEARCH_INVALID = true;
          }
          if (menu->menu == MENU_PAGER)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_TAG:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (tag && !AutoTag)
        {
          for (j = 0; j < Context->msgcount; j++)
            if (message_is_visible(Context, j))
              mutt_set_flag(Context, Context->hdrs[j], MUTT_TAG, 0);
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context, CURHDR, MUTT_TAG, !CURHDR->tagged);

          Context->last_tag = CURHDR->tagged ?
                                  CURHDR :
                                  ((Context->last_tag == CURHDR && !CURHDR->tagged) ?
                                       NULL :
                                       Context->last_tag);

          menu->redraw |= REDRAW_STATUS;
          if (Resolve && menu->current < Context->vcount - 1)
          {
            menu->current++;
            menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        break;

      case OP_MAIN_TAG_PATTERN:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_pattern_func(MUTT_TAG, _("Tag messages matching: "));
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_MAIN_UNDELETE_PATTERN:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message(s)"));

        if (mutt_pattern_func(MUTT_UNDELETE,
                              _("Undelete messages matching: ")) == 0)
        {
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_MAIN_UNTAG_PATTERN:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (mutt_pattern_func(MUTT_UNTAG, _("Untag messages matching: ")) == 0)
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_COMPOSE_TO_SENDER:
        if (Context)
        {
          mutt_compose_to_sender(tag ? NULL : CURHDR);
          menu->redraw = REDRAW_FULL;
        }
        break;

        /* --------------------------------------------------------------------
         * The following operations can be performed inside of the pager.
         */

#ifdef USE_IMAP
      case OP_MAIN_IMAP_FETCH:
        if (Context && Context->magic == MUTT_IMAP)
          imap_check_mailbox(Context, 1);
        break;

      case OP_MAIN_IMAP_LOGOUT_ALL:
        if (Context && Context->magic == MUTT_IMAP)
        {
          if (mx_close_mailbox(Context, &index_hint) != 0)
          {
            OPT_SEARCH_INVALID = true;
            menu->redraw = REDRAW_FULL;
            break;
          }
          FREE(&Context);
        }
        imap_logout_all();
        mutt_message(_("Logged out of IMAP servers."));
        OPT_SEARCH_INVALID = true;
        menu->redraw = REDRAW_FULL;
        break;
#endif

      case OP_MAIN_SYNC_FOLDER:

        if (Context && !Context->msgcount)
          break;

        CHECK_MSGCOUNT;
        CHECK_READONLY;
        {
          int ovc = Context->vcount;
          int oc = Context->msgcount;
          int check;
          struct Header *newhdr = NULL;

          /* don't attempt to move the cursor if there are no visible messages in the current limit */
          if (menu->current < Context->vcount)
          {
            /* threads may be reordered, so figure out what header the cursor
             * should be on. #3092 */
            int newidx = menu->current;
            if (CURHDR->deleted)
              newidx = ci_next_undeleted(menu->current);
            if (newidx < 0)
              newidx = ci_previous_undeleted(menu->current);
            if (newidx >= 0)
              newhdr = Context->hdrs[Context->v2r[newidx]];
          }

          check = mx_sync_mailbox(Context, &index_hint);
          if (check == 0)
          {
            if (newhdr && Context->vcount != ovc)
            {
              for (j = 0; j < Context->vcount; j++)
              {
                if (Context->hdrs[Context->v2r[j]] == newhdr)
                {
                  menu->current = j;
                  break;
                }
              }
            }
            OPT_SEARCH_INVALID = true;
          }
          else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
            update_index(menu, Context, check, oc, index_hint);

          /*
           * do a sanity check even if mx_sync_mailbox failed.
           */

          if (menu->current < 0 || menu->current >= Context->vcount)
            menu->current = ci_first_message();
        }

        /* check for a fatal error, or all messages deleted */
        if (!Context->path)
          FREE(&Context);

        /* if we were in the pager, redisplay the message */
        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIN_QUASI_DELETE:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (tag)
        {
          for (j = 0; j < Context->msgcount; j++)
          {
            if (message_is_tagged(Context, j))
            {
              Context->hdrs[j]->quasi_deleted = true;
              Context->changed = true;
            }
          }
        }
        else
        {
          CURHDR->quasi_deleted = true;
          Context->changed = true;
        }
        break;

#ifdef USE_NOTMUCH
      case OP_MAIN_ENTIRE_THREAD:
      {
        if (!Context || (Context->magic != MUTT_NOTMUCH))
        {
          mutt_message(_("No virtual folder, aborting."));
          break;
        }
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        int oc = Context->msgcount;
        if (nm_read_entire_thread(Context, CURHDR) < 0)
        {
          mutt_message(_("Failed to read thread, aborting."));
          break;
        }
        if (oc < Context->msgcount)
        {
          struct Header *oldcur = CURHDR;

          if ((Sort & SORT_MASK) == SORT_THREADS)
            mutt_sort_headers(Context, 0);
          menu->current = oldcur->virtual;
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;

          if (oldcur->collapsed || Context->collapsed)
          {
            menu->current = mutt_uncollapse_thread(Context, CURHDR);
            mutt_set_virtual(Context);
          }
        }
        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;
      }

#endif
      case OP_MAIN_MODIFY_TAGS:
      case OP_MAIN_MODIFY_TAGS_THEN_HIDE:
      {
        if (!Context || !mx_tags_is_supported(Context))
        {
          mutt_message(_("Folder doesn't support tagging, aborting."));
          break;
        }
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        char *tags = NULL;
        if (!tag)
          tags = driver_tags_get_with_hidden(&CURHDR->tags);
        rc = mx_tags_editor(Context, tags, buf, sizeof(buf));
        FREE(&tags);
        if (rc < 0)
          break;
        else if (rc == 0)
        {
          mutt_message(_("No tag specified, aborting."));
          break;
        }

        if (tag)
        {
          struct Progress progress;
          int px;

          if (!Context->quiet)
          {
            char msgbuf[STRING];
            snprintf(msgbuf, sizeof(msgbuf), _("Update tags..."));
            mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, 1, Context->tagged);
          }

#ifdef USE_NOTMUCH
          if (Context->magic == MUTT_NOTMUCH)
            nm_longrun_init(Context, true);
#endif
          for (px = 0, j = 0; j < Context->msgcount; j++)
          {
            if (!message_is_tagged(Context, j))
              continue;

            if (!Context->quiet)
              mutt_progress_update(&progress, ++px, -1);
            mx_tags_commit(Context, Context->hdrs[j], buf);
            if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
            {
              bool still_queried = false;
#ifdef USE_NOTMUCH
              if (Context->magic == MUTT_NOTMUCH)
                still_queried = nm_message_is_still_queried(Context, Context->hdrs[j]);
#endif
              Context->hdrs[j]->quasi_deleted = !still_queried;
              Context->changed = true;
            }
          }
#ifdef USE_NOTMUCH
          if (Context->magic == MUTT_NOTMUCH)
            nm_longrun_done(Context);
#endif
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (mx_tags_commit(Context, CURHDR, buf))
          {
            mutt_message(_("Failed to modify tags, aborting."));
            break;
          }
          if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
          {
            bool still_queried = false;
#ifdef USE_NOTMUCH
            if (Context->magic == MUTT_NOTMUCH)
              still_queried = nm_message_is_still_queried(Context, CURHDR);
#endif
            CURHDR->quasi_deleted = !still_queried;
            Context->changed = true;
          }
          if (menu->menu == MENU_PAGER)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw = REDRAW_CURRENT;
            }
            else
              menu->redraw = REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw = REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;
      }

#ifdef USE_NOTMUCH
      case OP_MAIN_VFOLDER_FROM_QUERY:
        buf[0] = '\0';
        if (mutt_get_field("Query: ", buf, sizeof(buf), MUTT_NM_QUERY) != 0 || !buf[0])
        {
          mutt_message(_("No query, aborting."));
          break;
        }
        if (!nm_uri_from_query(Context, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting."));
        else
          main_change_folder(menu, op, buf, sizeof(buf), &oldcount, &index_hint);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_BACKWARD:
        mutt_debug(2, "OP_MAIN_WINDOWED_VFOLDER_BACKWARD\n");
        if (NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled."));
          break;
        }
        if (!NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded."));
          break;
        }
        nm_query_window_backward();
        strncpy(buf, NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting."));
        else
          main_change_folder(menu, op, buf, sizeof(buf), &oldcount, &index_hint);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_FORWARD:
        if (NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled."));
          break;
        }
        if (!NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded."));
          break;
        }
        nm_query_window_forward();
        strncpy(buf, NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting."));
        else
        {
          mutt_debug(2, "nm: + windowed query (%s)\n", buf);
          main_change_folder(menu, op, buf, sizeof(buf), &oldcount, &index_hint);
        }
        break;

      case OP_MAIN_CHANGE_VFOLDER:
#endif

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_OPEN:
#endif
      case OP_MAIN_CHANGE_FOLDER:
      case OP_MAIN_NEXT_UNREAD_MAILBOX:
      case OP_MAIN_CHANGE_FOLDER_READONLY:
#ifdef USE_NNTP
      case OP_MAIN_CHANGE_GROUP:
      case OP_MAIN_CHANGE_GROUP_READONLY:
        OPT_NEWS = false;
#endif
        if (attach_msg || ReadOnly ||
#ifdef USE_NNTP
            op == OP_MAIN_CHANGE_GROUP_READONLY ||
#endif
            op == OP_MAIN_CHANGE_FOLDER_READONLY)
          flags = MUTT_READONLY;
        else
          flags = 0;

        if (flags)
          cp = _("Open mailbox in read-only mode");
#ifdef USE_NOTMUCH
        else if (op == OP_MAIN_CHANGE_VFOLDER)
          cp = _("Open virtual folder");
#endif
        else
          cp = _("Open mailbox");

        buf[0] = '\0';
        if ((op == OP_MAIN_NEXT_UNREAD_MAILBOX) && Context && Context->path)
        {
          mutt_str_strfcpy(buf, Context->path, sizeof(buf));
          mutt_pretty_mailbox(buf, sizeof(buf));
          mutt_buffy(buf, sizeof(buf));
          if (!buf[0])
          {
            mutt_error(_("No mailboxes have new mail"));
            break;
          }
        }
#ifdef USE_SIDEBAR
        else if (op == OP_SIDEBAR_OPEN)
        {
          const char *path = mutt_sb_get_highlight();
          if (!path || !*path)
            break;
          strncpy(buf, path, sizeof(buf));

          /* Mark the selected dir for the neomutt browser */
          mutt_browser_select_dir(buf);
        }
#endif
#ifdef USE_NOTMUCH
        else if (op == OP_MAIN_CHANGE_VFOLDER)
        {
          if (Context && (Context->magic == MUTT_NOTMUCH))
          {
            mutt_str_strfcpy(buf, Context->path, sizeof(buf));
            mutt_buffy_vfolder(buf, sizeof(buf));
          }
          mutt_enter_vfolder(cp, buf, sizeof(buf), 1);
          if (!buf[0])
          {
            mutt_window_clearline(MuttMessageWindow, 0);
            break;
          }
        }
#endif
        else
        {
          if (ChangeFolderNext && Context && Context->path)
          {
            mutt_str_strfcpy(buf, Context->path, sizeof(buf));
            mutt_pretty_mailbox(buf, sizeof(buf));
          }
#ifdef USE_NNTP
          if (op == OP_MAIN_CHANGE_GROUP || op == OP_MAIN_CHANGE_GROUP_READONLY)
          {
            OPT_NEWS = true;
            CurrentNewsSrv = nntp_select_server(NewsServer, false);
            if (!CurrentNewsSrv)
              break;
            if (flags)
              cp = _("Open newsgroup in read-only mode");
            else
              cp = _("Open newsgroup");
            nntp_buffy(buf, sizeof(buf));
          }
          else
#endif
            /* By default, fill buf with the next mailbox that contains unread
             * mail */
            mutt_buffy(buf, sizeof(buf));

          if (mutt_enter_fname(cp, buf, sizeof(buf), 1) == -1)
          {
            if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
            else
              break;
          }

          /* Selected directory is okay, let's save it. */
          mutt_browser_select_dir(buf);

          if (!buf[0])
          {
            mutt_window_clearline(MuttMessageWindow, 0);
            break;
          }
        }

        main_change_folder(menu, op, buf, sizeof(buf), &oldcount, &index_hint);
#ifdef USE_NNTP
        /* mutt_buffy_check() must be done with mail-reader mode! */
        menu->help = mutt_compile_help(
            helpstr, sizeof(helpstr), MENU_MAIN,
            (Context && (Context->magic == MUTT_NNTP)) ? IndexNewsHelp : IndexHelp);
#endif
        mutt_expand_path(buf, sizeof(buf));
#ifdef USE_SIDEBAR
        mutt_sb_set_open_buffy();
#endif
        break;

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        /*
         * toggle the weeding of headers so that a user can press the key
         * again while reading the message.
         */
        if (op == OP_DISPLAY_HEADERS)
          Weed = !Weed;

        OPT_NEED_RESORT = false;

        if ((Sort & SORT_MASK) == SORT_THREADS && CURHDR->collapsed)
        {
          mutt_uncollapse_thread(Context, CURHDR);
          mutt_set_virtual(Context);
          if (UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, CURHDR);
        }

        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        int hint = Context->hdrs[Context->v2r[menu->current]]->index;

        /* If we are returning to the pager via an index menu redirection, we
         * need to reset the menu->menu.  Otherwise mutt_menu_pop_current() will
         * set CurrentMenu incorrectly when we return back to the index menu. */
        menu->menu = MENU_MAIN;

        op = mutt_display_message(CURHDR);
        if (op < 0)
        {
          OPT_NEED_RESORT = false;
          break;
        }

        /* This is used to redirect a single operation back here afterwards.  If
         * mutt_display_message() returns 0, then the menu and pager state will
         * be cleaned up after this switch statement. */
        menu->menu = MENU_PAGER;
        menu->oldcurrent = menu->current;
        if (Context)
          update_index(menu, Context, MUTT_NEW_MAIL, Context->msgcount, hint);

        continue;

      case OP_EXIT:

        close = op;
        if (menu->menu == MENU_MAIN && attach_msg)
        {
          done = true;
          break;
        }

        if ((menu->menu == MENU_MAIN) &&
            (query_quadoption(Quit, _("Exit NeoMutt without saving?")) == MUTT_YES))
        {
          if (Context)
          {
            mx_fastclose_mailbox(Context);
            FREE(&Context);
          }
          done = true;
        }
        break;

      case OP_MAIN_BREAK_THREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        CHECK_ACL(MUTT_ACL_WRITE, _("Cannot break thread"));

        if ((Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled."));
        else if (!STAILQ_EMPTY(&CURHDR->env->in_reply_to) ||
                 !STAILQ_EMPTY(&CURHDR->env->references))
        {
          {
            struct Header *oldcur = CURHDR;

            mutt_break_thread(CURHDR);
            mutt_sort_headers(Context, 1);
            menu->current = oldcur->virtual;
          }

          Context->changed = true;
          mutt_message(_("Thread broken"));

          if (menu->menu == MENU_PAGER)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          else
            menu->redraw |= REDRAW_INDEX;
        }
        else
          mutt_error(
              _("Thread cannot be broken, message is not part of a thread"));

        break;

      case OP_MAIN_LINK_THREADS:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_WRITE, _("Cannot link threads"));

        if ((Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled."));
        else if (!CURHDR->env->message_id)
          mutt_error(_("No Message-ID: header available to link thread"));
        else if (!tag && (!Context->last_tag || !Context->last_tag->tagged))
          mutt_error(_("First, please tag a message to be linked here"));
        else
        {
          struct Header *oldcur = CURHDR;

          if (mutt_link_threads(CURHDR, tag ? NULL : Context->last_tag, Context))
          {
            mutt_sort_headers(Context, 1);
            menu->current = oldcur->virtual;

            Context->changed = true;
            mutt_message(_("Threads linked"));
          }
          else
            mutt_error(_("No thread linked"));
        }

        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;

        break;

      case OP_EDIT_TYPE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        mutt_edit_content_type(CURHDR, CURHDR->content, NULL);
        /* if we were in the pager, redisplay the message */
        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_CURRENT;
        break;

      case OP_MAIN_NEXT_UNDELETED:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (menu->current >= Context->vcount - 1)
        {
          if (menu->menu == MENU_MAIN)
            mutt_error(_("You are on the last message."));
          break;
        }
        menu->current = ci_next_undeleted(menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (menu->menu == MENU_MAIN)
            mutt_error(_("No undeleted messages."));
        }
        else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_NEXT_ENTRY:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (menu->current >= Context->vcount - 1)
        {
          if (menu->menu == MENU_MAIN)
            mutt_error(_("You are on the last message."));
          break;
        }
        menu->current++;
        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_MAIN_PREV_UNDELETED:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (menu->current < 1)
        {
          mutt_error(_("You are on the first message."));
          break;
        }
        menu->current = ci_previous_undeleted(menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (menu->menu == MENU_MAIN)
            mutt_error(_("No undeleted messages."));
        }
        else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_PREV_ENTRY:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (menu->current < 1)
        {
          if (menu->menu == MENU_MAIN)
            mutt_error(_("You are on the first message."));
          break;
        }
        menu->current--;
        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_DECRYPT_COPY:
      case OP_DECRYPT_SAVE:
        if (!WithCrypto)
          break;
      /* fallthrough */
      case OP_COPY_MESSAGE:
      case OP_SAVE:
      case OP_DECODE_COPY:
      case OP_DECODE_SAVE:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (mutt_save_message(tag ? NULL : CURHDR,
                              (op == OP_DECRYPT_SAVE) || (op == OP_SAVE) || (op == OP_DECODE_SAVE),
                              (op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY),
                              (op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY) || 0) == 0 &&
            (op == OP_SAVE || op == OP_DECODE_SAVE || op == OP_DECRYPT_SAVE))
        {
          menu->redraw |= REDRAW_STATUS;
          if (tag)
            menu->redraw |= REDRAW_INDEX;
          else if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        break;

      case OP_MAIN_NEXT_NEW:
      case OP_MAIN_NEXT_UNREAD:
      case OP_MAIN_PREV_NEW:
      case OP_MAIN_PREV_UNREAD:
      case OP_MAIN_NEXT_NEW_THEN_UNREAD:
      case OP_MAIN_PREV_NEW_THEN_UNREAD:

      {
        int first_unread = -1;
        int first_new = -1;

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        i = menu->current;
        menu->current = -1;
        for (j = 0; j != Context->vcount; j++)
        {
          if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_NEXT_NEW_THEN_UNREAD)
          {
            i++;
            if (i > Context->vcount - 1)
            {
              mutt_message(_("Search wrapped to top."));
              i = 0;
            }
          }
          else
          {
            i--;
            if (i < 0)
            {
              mutt_message(_("Search wrapped to bottom."));
              i = Context->vcount - 1;
            }
          }

          struct Header *h = Context->hdrs[Context->v2r[i]];
          if (h->collapsed && (Sort & SORT_MASK) == SORT_THREADS)
          {
            if (UNREAD(h) && first_unread == -1)
              first_unread = i;
            if (UNREAD(h) == 1 && first_new == -1)
              first_new = i;
          }
          else if ((!h->deleted && !h->read))
          {
            if (first_unread == -1)
              first_unread = i;
            if ((!h->old) && first_new == -1)
              first_new = i;
          }

          if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD) && first_unread != -1)
            break;
          if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
               op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
              first_new != -1)
            break;
        }
        if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
             op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
            first_new != -1)
          menu->current = first_new;
        else if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD ||
                  op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
                 first_unread != -1)
          menu->current = first_unread;

        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW)
          {
            if (Context->pattern)
              mutt_error(_("No new messages in this limited view."));
            else
              mutt_error(_("No new messages."));
          }
          else
          {
            if (Context->pattern)
              mutt_error(_("No unread messages in this limited view."));
            else
              mutt_error(_("No unread messages."));
          }
        }
        else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;
      }
      case OP_FLAG_MESSAGE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_WRITE, _("Cannot flag message"));

        if (tag)
        {
          for (j = 0; j < Context->msgcount; j++)
          {
            if (message_is_tagged(Context, j))
              mutt_set_flag(Context, Context->hdrs[j], MUTT_FLAG,
                            !Context->hdrs[j]->flagged);
          }

          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context, CURHDR, MUTT_FLAG, !CURHDR->flagged);
          if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;

      case OP_TOGGLE_NEW:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_SEEN, _("Cannot toggle new"));

        if (tag)
        {
          for (j = 0; j < Context->msgcount; j++)
          {
            if (!message_is_tagged(Context, j))
              continue;

            if (Context->hdrs[j]->read || Context->hdrs[j]->old)
              mutt_set_flag(Context, Context->hdrs[j], MUTT_NEW, 1);
            else
              mutt_set_flag(Context, Context->hdrs[j], MUTT_READ, 1);
          }
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (CURHDR->read || CURHDR->old)
            mutt_set_flag(Context, CURHDR, MUTT_NEW, 1);
          else
            mutt_set_flag(Context, CURHDR, MUTT_READ, 1);

          if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
          menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_TOGGLE_WRITE:

        CHECK_IN_MAILBOX;
        if (mx_toggle_write(Context) == 0)
          menu->redraw |= REDRAW_STATUS;
        break;

      case OP_MAIN_NEXT_THREAD:
      case OP_MAIN_NEXT_SUBTHREAD:
      case OP_MAIN_PREV_THREAD:
      case OP_MAIN_PREV_SUBTHREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        switch (op)
        {
          case OP_MAIN_NEXT_THREAD:
            menu->current = mutt_next_thread(CURHDR);
            break;

          case OP_MAIN_NEXT_SUBTHREAD:
            menu->current = mutt_next_subthread(CURHDR);
            break;

          case OP_MAIN_PREV_THREAD:
            menu->current = mutt_previous_thread(CURHDR);
            break;

          case OP_MAIN_PREV_SUBTHREAD:
            menu->current = mutt_previous_subthread(CURHDR);
            break;
        }

        if (menu->current < 0)
        {
          menu->current = menu->oldcurrent;
          if (op == OP_MAIN_NEXT_THREAD || op == OP_MAIN_NEXT_SUBTHREAD)
            mutt_error(_("No more threads."));
          else
            mutt_error(_("You are on the first thread."));
        }
        else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_MAIN_ROOT_MESSAGE:
      case OP_MAIN_PARENT_MESSAGE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        menu->current = mutt_parent_message(Context, CURHDR, op == OP_MAIN_ROOT_MESSAGE);
        if (menu->current < 0)
        {
          menu->current = menu->oldcurrent;
        }
        else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* CHECK_ACL(MUTT_ACL_WRITE); */

        if (mutt_change_flag(tag ? NULL : CURHDR, (op == OP_MAIN_SET_FLAG)) == 0)
        {
          menu->redraw |= REDRAW_STATUS;
          if (tag)
            menu->redraw |= REDRAW_INDEX;
          else if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        break;

      case OP_MAIN_COLLAPSE_THREAD:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if ((Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error(_("Threading is not enabled."));
          break;
        }

        if (CURHDR->collapsed)
        {
          menu->current = mutt_uncollapse_thread(Context, CURHDR);
          mutt_set_virtual(Context);
          if (UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, CURHDR);
        }
        else if (CAN_COLLAPSE(CURHDR))
        {
          menu->current = mutt_collapse_thread(Context, CURHDR);
          mutt_set_virtual(Context);
        }
        else
        {
          mutt_error(_("Thread contains unread or flagged messages."));
          break;
        }

        menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

        break;

      case OP_MAIN_COLLAPSE_ALL:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if ((Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error(_("Threading is not enabled."));
          break;
        }
        collapse_all(menu, 1);
        break;

        /* --------------------------------------------------------------------
         * These functions are invoked directly from the internal-pager
         */

      case OP_BOUNCE_MESSAGE:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        ci_bounce_message(tag ? NULL : CURHDR);
        break;

      case OP_CREATE_ALIAS:

        mutt_alias_create(Context && Context->vcount ? CURHDR->env : NULL, NULL);
        menu->redraw |= REDRAW_CURRENT;
        break;

      case OP_QUERY:
        CHECK_ATTACH;
        mutt_query_menu(NULL, 0);
        break;

      case OP_PURGE_MESSAGE:
      case OP_DELETE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message"));

        if (tag)
        {
          mutt_tag_set_flag(MUTT_DELETE, 1);
          mutt_tag_set_flag(MUTT_PURGE, (op == OP_PURGE_MESSAGE));
          if (DeleteUntag)
            mutt_tag_set_flag(MUTT_TAG, 0);
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context, CURHDR, MUTT_DELETE, 1);
          mutt_set_flag(Context, CURHDR, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
          if (DeleteUntag)
            mutt_set_flag(Context, CURHDR, MUTT_TAG, 0);
          if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message(s)"));

        {
          int subthread = (op == OP_DELETE_SUBTHREAD);
          rc = mutt_thread_set_flag(CURHDR, MUTT_DELETE, 1, subthread);
          if (rc == -1)
            break;
          if (op == OP_PURGE_THREAD)
          {
            rc = mutt_thread_set_flag(CURHDR, MUTT_PURGE, 1, subthread);
            if (rc == -1)
              break;
          }

          if (DeleteUntag)
            mutt_thread_set_flag(CURHDR, MUTT_TAG, 0, subthread);
          if (Resolve)
          {
            menu->current = ci_next_undeleted(menu->current);
            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

#ifdef USE_NNTP
      case OP_CATCHUP:
        CHECK_MSGCOUNT;
        CHECK_READONLY;
        CHECK_ATTACH
        if (Context && Context->magic == MUTT_NNTP)
        {
          struct NntpData *nntp_data = Context->data;
          if (mutt_newsgroup_catchup(nntp_data->nserv, nntp_data->group))
            menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
#endif

      case OP_DISPLAY_ADDRESS:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_display_address(CURHDR->env);
        break;

      case OP_ENTER_COMMAND:

        mutt_enter_command();
        mutt_check_rescore(Context);
        break;

      case OP_EDIT_OR_VIEW_RAW_MESSAGE:
      case OP_EDIT_RAW_MESSAGE:
      case OP_VIEW_RAW_MESSAGE:

        /* TODO split this into 3 cases? */
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        bool edit;
        if (op == OP_EDIT_RAW_MESSAGE)
        {
          CHECK_READONLY;
          /* L10N: CHECK_ACL */
          CHECK_ACL(MUTT_ACL_INSERT, _("Cannot edit message"));
          edit = true;
        }
        else if (op == OP_EDIT_OR_VIEW_RAW_MESSAGE)
          edit = !Context->readonly && mutt_bit_isset(Context->rights, MUTT_ACL_INSERT);
        else
          edit = false;

        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        if (edit)
          mutt_edit_message(Context, tag ? NULL : CURHDR);
        else
          mutt_view_message(Context, tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;

        break;

      case OP_FORWARD_MESSAGE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        ci_send_message(SENDFORWARD, NULL, NULL, Context, tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_GROUP_REPLY:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        ci_send_message(SENDREPLY | SENDGROUPREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EDIT_LABEL:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        rc = mutt_label_message(tag ? NULL : CURHDR);
        if (rc > 0)
        {
          Context->changed = true;
          menu->redraw = REDRAW_FULL;
          /* L10N: This is displayed when the x-label on one or more
           * messages is edited. */
          mutt_message(_("%d labels changed."), rc);
        }
        else
        {
          /* L10N: This is displayed when editing an x-label, but no messages
           * were updated.  Possibly due to canceling at the prompt or if the new
           * label is the same as the old label. */
          mutt_message(_("No labels changed."));
        }
        break;

      case OP_LIST_REPLY:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        ci_send_message(SENDREPLY | SENDLISTREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIL:

        CHECK_ATTACH;
        ci_send_message(0, NULL, NULL, Context, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIL_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        CHECK_ATTACH;
        ci_send_message(SENDKEY, NULL, NULL, NULL, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EXTRACT_KEYS:
        if (!WithCrypto)
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        crypt_extract_keys_from_messages(tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_CHECK_TRADITIONAL:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED))
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);

        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;

      case OP_PIPE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_pipe_message(tag ? NULL : CURHDR);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, piping could change
         * new or old messages status to read. Redraw what's needed.
         */
        if (Context->magic == MUTT_IMAP && !ImapPeek)
        {
          menu->redraw |= (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
        }
#endif

        break;

      case OP_PRINT:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_print_message(tag ? NULL : CURHDR);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, printing could change
         * new or old messages status to read. Redraw what's needed.
         */
        if (Context->magic == MUTT_IMAP && !ImapPeek)
        {
          menu->redraw |= (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
        }
#endif

        break;

      case OP_MAIN_READ_THREAD:
      case OP_MAIN_READ_SUBTHREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_SEEN, _("Cannot mark message(s) as read"));

        rc = mutt_thread_set_flag(CURHDR, MUTT_READ, 1, op == OP_MAIN_READ_THREAD ? 0 : 1);

        if (rc != -1)
        {
          if (Resolve)
          {
            menu->current = (op == OP_MAIN_READ_THREAD ? mutt_next_thread(CURHDR) :
                                                         mutt_next_subthread(CURHDR));
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
            }
            else if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_MARK_MSG:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (CURHDR->env->message_id)
        {
          char buf2[128];

          buf2[0] = '\0';
          /* L10N: This is the prompt for <mark-message>.  Whatever they
             enter will be prefixed by $mark_macro_prefix and will become
             a macro hotkey to jump to the currently selected message. */
          if (!mutt_get_field(_("Enter macro stroke: "), buf2, sizeof(buf2), MUTT_CLEAR) &&
              buf2[0])
          {
            char str[STRING], macro[STRING];
            snprintf(str, sizeof(str), "%s%s", MarkMacroPrefix, buf2);
            snprintf(macro, sizeof(macro), "<search>~i \"%s\"\n", CURHDR->env->message_id);
            /* L10N: "message hotkey" is the key bindings menu description of a
               macro created by <mark-message>. */
            km_bind(str, MENU_MAIN, OP_MACRO, macro, _("message hotkey"));

            /* L10N: This is echoed after <mark-message> creates a new hotkey
               macro.  %s is the hotkey string ($mark_macro_prefix followed
               by whatever they typed at the prompt.) */
            snprintf(buf2, sizeof(buf2), _("Message bound to %s."), str);
            mutt_message(buf2);
            mutt_debug(1, "Mark: %s => %s\n", str, macro);
          }
        }
        else
          /* L10N: This error is printed if <mark-message> cannot find a
             Message-ID for the currently selected message in the index. */
          mutt_error(_("No message ID to macro."));
        break;

      case OP_RECALL_MESSAGE:

        CHECK_ATTACH;
        ci_send_message(SENDPOSTPONED, NULL, NULL, Context, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_RESEND:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if (tag)
        {
          for (j = 0; j < Context->msgcount; j++)
          {
            if (message_is_tagged(Context, j))
              mutt_resend_message(NULL, Context, Context->hdrs[j]);
          }
        }
        else
          mutt_resend_message(NULL, Context, CURHDR);

        menu->redraw = REDRAW_FULL;
        break;

#ifdef USE_NNTP
      case OP_FOLLOWUP:
      case OP_FORWARD_TO_GROUP:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        /* fallthrough */

      case OP_POST:
        CHECK_ATTACH;
        if (op != OP_FOLLOWUP || !CURHDR->env->followup_to ||
            (mutt_str_strcasecmp(CURHDR->env->followup_to, "poster") != 0) ||
            query_quadoption(FollowupToPoster,
                             _("Reply by mail as poster prefers?")) != MUTT_YES)
        {
          if (Context && Context->magic == MUTT_NNTP &&
              !((struct NntpData *) Context->data)->allowed && query_quadoption(PostModerated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
          {
            break;
          }
          if (op == OP_POST)
            ci_send_message(SENDNEWS, NULL, NULL, Context, NULL);
          else
          {
            CHECK_MSGCOUNT;
            ci_send_message((op == OP_FOLLOWUP ? SENDREPLY : SENDFORWARD) | SENDNEWS,
                            NULL, NULL, Context, tag ? NULL : CURHDR);
          }
          menu->redraw = REDRAW_FULL;
          break;
        }
#endif
      /* fallthrough */
      case OP_REPLY:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (PgpAutoDecode && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CURHDR, &menu->redraw);
        }
        ci_send_message(SENDREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_SHELL_ESCAPE:

        mutt_shell_escape();
        break;

      case OP_TAG_THREAD:
      case OP_TAG_SUBTHREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        rc = mutt_thread_set_flag(CURHDR, MUTT_TAG, !CURHDR->tagged,
                                  op == OP_TAG_THREAD ? 0 : 1);

        if (rc != -1)
        {
          if (Resolve)
          {
            if (op == OP_TAG_THREAD)
              menu->current = mutt_next_thread(CURHDR);
            else
              menu->current = mutt_next_subthread(CURHDR);

            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_UNDELETE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message"));

        if (tag)
        {
          mutt_tag_set_flag(MUTT_DELETE, 0);
          mutt_tag_set_flag(MUTT_PURGE, 0);
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context, CURHDR, MUTT_DELETE, 0);
          mutt_set_flag(Context, CURHDR, MUTT_PURGE, 0);
          if (Resolve && menu->current < Context->vcount - 1)
          {
            menu->current++;
            menu->redraw |= REDRAW_MOTION_RESYNCH;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message(s)"));

        rc = mutt_thread_set_flag(CURHDR, MUTT_DELETE, 0, op == OP_UNDELETE_THREAD ? 0 : 1);
        if (rc != -1)
          rc = mutt_thread_set_flag(CURHDR, MUTT_PURGE, 0,
                                    op == OP_UNDELETE_THREAD ? 0 : 1);
        if (rc != -1)
        {
          if (Resolve)
          {
            if (op == OP_UNDELETE_THREAD)
              menu->current = mutt_next_thread(CURHDR);
            else
              menu->current = mutt_next_subthread(CURHDR);

            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_VERSION:
        mutt_version();
        break;

      case OP_BUFFY_LIST:
        mutt_buffy_list();
        break;

      case OP_VIEW_ATTACHMENTS:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_view_attachments(CURHDR);
        if (Context && CURHDR->attach_del)
          Context->changed = true;
        menu->redraw = REDRAW_FULL;
        break;

      case OP_END_COND:
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
        mutt_sb_change_mailbox(op);
        break;

      case OP_SIDEBAR_TOGGLE_VISIBLE:
        SidebarVisible = !SidebarVisible;
        mutt_window_reflow();
        break;

      case OP_SIDEBAR_TOGGLE_VIRTUAL:
        mutt_sb_toggle_virtual();
        break;
#endif
      default:
        if (menu->menu == MENU_MAIN)
          km_error_key(MENU_MAIN);
    }

#ifdef USE_NOTMUCH
    if (Context)
      nm_debug_check(Context);
#endif

    if (menu->menu == MENU_PAGER)
    {
      mutt_clear_pager_position();
      menu->menu = MENU_MAIN;
      menu->redraw = REDRAW_FULL;
    }

    if (done)
      break;
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);
  return close;
}

void mutt_set_header_color(struct Context *ctx, struct Header *curhdr)
{
  struct ColorLine *color = NULL;
  struct PatternCache cache;

  if (!curhdr)
    return;

  memset(&cache, 0, sizeof(cache));

  STAILQ_FOREACH(color, &ColorIndexList, entries)
  {
    if (mutt_pattern_exec(color->color_pattern, MUTT_MATCH_FULL_ADDRESS, ctx, curhdr, &cache))
    {
      curhdr->pair = color->pair;
      return;
    }
  }
  curhdr->pair = ColorDefs[MT_COLOR_NORMAL];
}
