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

/**
 * @page index2 GUI manage the main index (list of emails)
 *
 * GUI manage the main index (list of emails)
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "index.h"
#include "account.h"
#include "alias.h"
#include "browser.h"
#include "commands.h"
#include "context.h"
#include "curs_lib.h"
#include "format_flags.h"
#include "globals.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_menu.h"
#include "mutt_account.h"
#include "mutt_curses.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pattern.h"
#include "progress.h"
#include "protos.h"
#include "query.h"
#include "recvattach.h"
#include "score.h"
#include "send.h"
#include "sort.h"
#include "status.h"
#include "terminal.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_POP
#include "pop/pop.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif

/* These Config Variables are only used in index.c */
bool ChangeFolderNext; ///< Config: Suggest the next folder, rather than the first when using '<change-folder>'
bool CollapseAll;      ///< Config: Collapse all threads when entering a folder
bool CollapseFlagged; ///< Config: Prevent the collapse of threads with flagged emails
bool CollapseUnread; ///< Config: Prevent the collapse of threads with unread emails
char *MarkMacroPrefix; ///< Config: Prefix for macros using '<mark-message>'
bool PgpAutoDecode;    ///< Config: Automatically decrypt PGP messages
bool UncollapseJump; ///< Config: When opening a thread, jump to the next unread message
bool UncollapseNew; ///< Config: Open collapsed threads when new mail arrives

static const char *No_mailbox_is_open = N_("No mailbox is open");
static const char *There_are_no_messages = N_("There are no messages");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode");
static const char *NoVisible = N_("No visible messages");

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
  else if (!Context->mailbox->msg_count)                                       \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(There_are_no_messages));                                      \
    break;                                                                     \
  }

#define CHECK_VISIBLE                                                          \
  if (Context && menu->current >= Context->mailbox->vcount)                    \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(NoVisible));                                                  \
    break;                                                                     \
  }

#define CHECK_READONLY                                                         \
  if (Context->mailbox->readonly)                                              \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Mailbox_is_read_only));                                       \
    break;                                                                     \
  }

#define CHECK_ACL(aclbit, action)                                              \
  if (!mutt_bit_isset(Context->mailbox->rights, aclbit))                       \
  {                                                                            \
    mutt_flushinp();                                                           \
    /* L10N: %s is one of the CHECK_ACL entries below. */                      \
    mutt_error(_("%s: Operation not permitted by ACL"), action);               \
    break;                                                                     \
  }

#define CHECK_ATTACH                                                           \
  if (OptAttachMsg)                                                            \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Function_not_permitted_in_attach_message_mode));              \
    break;                                                                     \
  }

#define CUR_EMAIL Context->mailbox->emails[Context->mailbox->v2r[menu->current]]
#define UNREAD(email) mutt_thread_contains_unread(Context, email)
#define FLAGGED(email) mutt_thread_contains_flagged(Context, email)

#define CAN_COLLAPSE(email)                                                    \
  ((CollapseUnread || !UNREAD(email)) && (CollapseFlagged || !FLAGGED(email)))

/**
 * collapse_all - Collapse/uncollapse all threads
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
  struct Email *e = NULL, *base = NULL;
  struct MuttThread *thread = NULL, *top = NULL;
  int final;

  if (!Context || (Context->mailbox->msg_count == 0))
    return;

  /* Figure out what the current message would be after folding / unfolding,
   * so that we can restore the cursor in a sane way afterwards. */
  if (CUR_EMAIL->collapsed && toggle)
    final = mutt_uncollapse_thread(Context, CUR_EMAIL);
  else if (CAN_COLLAPSE(CUR_EMAIL))
    final = mutt_collapse_thread(Context, CUR_EMAIL);
  else
    final = CUR_EMAIL->virtual;

  base = Context->mailbox->emails[Context->mailbox->v2r[final]];

  /* Iterate all threads, perform collapse/uncollapse as needed */
  top = Context->tree;
  Context->collapsed = toggle ? !Context->collapsed : true;
  while ((thread = top))
  {
    while (!thread->message)
      thread = thread->child;
    e = thread->message;

    if (e->collapsed != Context->collapsed)
    {
      if (e->collapsed)
        mutt_uncollapse_thread(Context, e);
      else if (CAN_COLLAPSE(e))
        mutt_collapse_thread(Context, e);
    }
    top = top->next;
  }

  /* Restore the cursor */
  mutt_set_virtual(Context);
  for (int j = 0; j < Context->mailbox->vcount; j++)
  {
    if (Context->mailbox->emails[Context->mailbox->v2r[j]]->index == base->index)
    {
      menu->current = j;
      break;
    }
  }

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * ci_next_undeleted - Find the next undeleted email
 * @param msgno Message number to start at
 * @retval >=0 Message number of next undeleted email
 * @retval  -1 No more undeleted messages
 */
static int ci_next_undeleted(int msgno)
{
  for (int i = msgno + 1; i < Context->mailbox->vcount; i++)
    if (!Context->mailbox->emails[Context->mailbox->v2r[i]]->deleted)
      return i;
  return -1;
}

/**
 * ci_previous_undeleted - Find the previous undeleted email
 * @param msgno Message number to start at
 * @retval >=0 Message number of next undeleted email
 * @retval  -1 No more undeleted messages
 */
static int ci_previous_undeleted(int msgno)
{
  for (int i = msgno - 1; i >= 0; i--)
    if (!Context->mailbox->emails[Context->mailbox->v2r[i]]->deleted)
      return i;
  return -1;
}

/**
 * ci_first_message - Get index of first new message
 * @retval num Index of first new message
 *
 * Return the index of the first new message, or failing that, the first
 * unread message.
 */
static int ci_first_message(void)
{
  if (!Context || !Context->mailbox->msg_count)
    return 0;

  int old = -1;
  for (int i = 0; i < Context->mailbox->vcount; i++)
  {
    if (!Context->mailbox->emails[Context->mailbox->v2r[i]]->read &&
        !Context->mailbox->emails[Context->mailbox->v2r[i]]->deleted)
    {
      if (!Context->mailbox->emails[Context->mailbox->v2r[i]]->old)
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
    return Context->mailbox->vcount ? Context->mailbox->vcount - 1 : 0;
  }

  return 0;
}

/**
 * mx_toggle_write - Toggle the mailbox's readonly flag
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error
 *
 * This should be in mx.c, but it only gets used here.
 */
static int mx_toggle_write(struct Mailbox *m)
{
  if (!m)
    return -1;

  if (m->readonly)
  {
    mutt_error(_("Cannot toggle write on a readonly mailbox"));
    return -1;
  }

  if (m->dontwrite)
  {
    m->dontwrite = false;
    mutt_message(_("Changes to folder will be written on folder exit"));
  }
  else
  {
    m->dontwrite = true;
    mutt_message(_("Changes to folder will not be written"));
  }

  return 0;
}

/**
 * resort_index - Resort the index
 * @param menu Current Menu
 */
static void resort_index(struct Menu *menu)
{
  struct Email *current = CUR_EMAIL;

  menu->current = -1;
  mutt_sort_headers(Context, false);
  /* Restore the current message */

  for (int i = 0; i < Context->mailbox->vcount; i++)
  {
    if (Context->mailbox->emails[Context->mailbox->v2r[i]] == current)
    {
      menu->current = i;
      break;
    }
  }

  if ((Sort & SORT_MASK) == SORT_THREADS && menu->current < 0)
    menu->current = mutt_parent_message(Context, current, false);

  if (menu->current < 0)
    menu->current = ci_first_message();

  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * update_index_threaded - Update the index (if threaded)
 * @param ctx      Mailbox
 * @param check    Flags, e.g. #MUTT_REOPENED
 * @param oldcount How many items are currently in the index
 */
static void update_index_threaded(struct Context *ctx, int check, int oldcount)
{
  struct Email **save_new = NULL;

  /* save the list of new messages */
  if ((check != MUTT_REOPENED) && oldcount && (ctx->pattern || UncollapseNew))
  {
    save_new = mutt_mem_malloc(sizeof(struct Email *) * (ctx->mailbox->msg_count - oldcount));
    for (int i = oldcount; i < ctx->mailbox->msg_count; i++)
      save_new[i - oldcount] = ctx->mailbox->emails[i];
  }

  /* Sort first to thread the new messages, because some patterns
   * require the threading information.
   *
   * If the mailbox was reopened, need to rethread from scratch. */
  mutt_sort_headers(ctx, (check == MUTT_REOPENED));

  if (ctx->pattern)
  {
    for (int i = (check == MUTT_REOPENED) ? 0 : oldcount; i < ctx->mailbox->msg_count; i++)
    {
      struct Email *e = NULL;

      if ((check != MUTT_REOPENED) && oldcount)
        e = save_new[i - oldcount];
      else
        e = ctx->mailbox->emails[i];

      if (mutt_pattern_exec(ctx->limit_pattern, MUTT_MATCH_FULL_ADDRESS, ctx->mailbox, e, NULL))
      {
        /* virtual will get properly set by mutt_set_virtual(), which
         * is called by mutt_sort_headers() just below. */
        e->virtual = 1;
        e->limited = true;
      }
    }
    /* Need a second sort to set virtual numbers and redraw the tree */
    mutt_sort_headers(ctx, false);
  }

  /* uncollapse threads with new mail */
  if (UncollapseNew)
  {
    if (check == MUTT_REOPENED)
    {
      ctx->collapsed = false;

      for (struct MuttThread *h = ctx->tree; h; h = h->next)
      {
        struct MuttThread *j = h;
        for (; !j->message; j = j->child)
          ;
        mutt_uncollapse_thread(ctx, j->message);
      }
      mutt_set_virtual(ctx);
    }
    else if (oldcount)
    {
      for (int j = 0; j < (ctx->mailbox->msg_count - oldcount); j++)
      {
        if (!ctx->pattern || save_new[j]->limited)
        {
          mutt_uncollapse_thread(ctx, save_new[j]);
        }
      }
      mutt_set_virtual(ctx);
    }
  }

  FREE(&save_new);
}

/**
 * update_index_unthreaded - Update the index (if unthreaded)
 * @param ctx      Mailbox
 * @param check    Flags, e.g. #MUTT_REOPENED
 * @param oldcount How many items are currently in the index
 */
static void update_index_unthreaded(struct Context *ctx, int check, int oldcount)
{
  /* We are in a limited view. Check if the new message(s) satisfy
   * the limit criteria. If they do, set their virtual msgno so that
   * they will be visible in the limited view */
  if (ctx->pattern)
  {
    int padding = mx_msg_padding_size(ctx->mailbox);
    for (int i = (check == MUTT_REOPENED) ? 0 : oldcount; i < ctx->mailbox->msg_count; i++)
    {
      if (!i)
      {
        ctx->mailbox->vcount = 0;
        ctx->vsize = 0;
      }

      if (mutt_pattern_exec(ctx->limit_pattern, MUTT_MATCH_FULL_ADDRESS,
                            ctx->mailbox, ctx->mailbox->emails[i], NULL))
      {
        assert(ctx->mailbox->vcount < ctx->mailbox->msg_count);
        ctx->mailbox->emails[i]->virtual = ctx->mailbox->vcount;
        ctx->mailbox->v2r[ctx->mailbox->vcount] = i;
        ctx->mailbox->emails[i]->limited = true;
        ctx->mailbox->vcount++;
        struct Body *b = ctx->mailbox->emails[i]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset + padding;
      }
    }
  }

  /* if the mailbox was reopened, need to rethread from scratch */
  mutt_sort_headers(ctx, (check == MUTT_REOPENED));
}

/**
 * update_index - Update the index
 * @param menu       Current Menu
 * @param ctx        Mailbox
 * @param check      Flags, e.g. #MUTT_REOPENED
 * @param oldcount   How many items are currently in the index
 * @param index_hint Remember our place in the index
 */
void update_index(struct Menu *menu, struct Context *ctx, int check, int oldcount, int index_hint)
{
  if (!menu || !ctx)
    return;

  /* take note of the current message */
  if (oldcount)
  {
    if (menu->current < ctx->mailbox->vcount)
      menu->oldcurrent = index_hint;
    else
      oldcount = 0; /* invalid message number! */
  }

  if ((Sort & SORT_MASK) == SORT_THREADS)
    update_index_threaded(ctx, check, oldcount);
  else
    update_index_unthreaded(ctx, check, oldcount);

  menu->current = -1;
  if (oldcount)
  {
    /* restore the current message to the message it was pointing to */
    for (int i = 0; i < ctx->mailbox->vcount; i++)
    {
      if (ctx->mailbox->emails[ctx->mailbox->v2r[i]]->index == menu->oldcurrent)
      {
        menu->current = i;
        break;
      }
    }
  }

  if (menu->current < 0)
    menu->current = ci_first_message();
}

/**
 * main_change_folder - Change to a different mailbox
 * @param menu       Current Menu
 * @param m          Mailbox
 * @param op         Operation, e.g. OP_MAIN_CHANGE_FOLDER_READONLY
 * @param buf        Folder to change to
 * @param buflen     Length of buffer
 * @param oldcount   How many items are currently in the index
 * @param index_hint Remember our place in the index
 * @retval  0 Success
 * @retval -1 Error
 */
static int main_change_folder(struct Menu *menu, int op, struct Mailbox *m,
                              char *buf, size_t buflen, int *oldcount, int *index_hint)
{
#ifdef USE_NNTP
  if (OptNews)
  {
    OptNews = false;
    nntp_expand_path(buf, buflen, &CurrentNewsSrv->conn->account);
  }
  else
#endif
  {
    mx_path_canon(buf, buflen, Folder, NULL);
  }

  enum MailboxType magic = mx_path_probe(buf, NULL);
  if ((magic == MUTT_MAILBOX_ERROR) || (magic == MUTT_UNKNOWN))
  {
    mutt_error(_("%s is not a mailbox"), buf);
    return -1;
  }

  /* keepalive failure in mutt_enter_fname may kill connection. #3028 */
  if (Context && (Context->mailbox->path[0] == '\0'))
    mutt_context_free(&Context);

  if (Context)
  {
    char *new_last_folder = NULL;
#ifdef USE_INOTIFY
    int monitor_remove_rc = mutt_monitor_remove(NULL);
#endif
#ifdef USE_COMPRESSED
    if (Context->mailbox->compress_info && (Context->mailbox->realpath[0] != '\0'))
      new_last_folder = mutt_str_strdup(Context->mailbox->realpath);
    else
#endif
      new_last_folder = mutt_str_strdup(Context->mailbox->path);
    *oldcount = Context ? Context->mailbox->msg_count : 0;

    int check = mx_mbox_close(&Context, index_hint);
    if (check != 0)
    {
#ifdef USE_INOTIFY
      if (monitor_remove_rc == 0)
        mutt_monitor_add(NULL);
#endif
      if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
        update_index(menu, Context, check, *oldcount, *index_hint);

      FREE(&new_last_folder);
      OptSearchInvalid = true;
      menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
      return 0;
    }
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

  const int flags =
      (ReadOnly || (op == OP_MAIN_CHANGE_FOLDER_READONLY)) ? MUTT_READONLY : 0;
  Context = mx_mbox_open(m, buf, flags);
  if (Context)
  {
    menu->current = ci_first_message();
#ifdef USE_INOTIFY
    mutt_monitor_add(NULL);
#endif
  }
  else
    menu->current = 0;

  if (((Sort & SORT_MASK) == SORT_THREADS) && CollapseAll)
    collapse_all(menu, 0);

#ifdef USE_SIDEBAR
  mutt_sb_set_open_mailbox();
#endif

  mutt_clear_error();
  mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE); /* force the mailbox check after we have changed the folder */
  menu->redraw = REDRAW_FULL;
  OptSearchInvalid = true;

  return 0;
}

/**
 * index_make_entry - Format a menu item for the index list - Implements Menu::menu_make_entry()
 */
void index_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  if (!Context || !menu || (line < 0) || (line >= Context->mailbox->email_max))
    return;

  struct Email *e = Context->mailbox->emails[Context->mailbox->v2r[line]];
  if (!e)
    return;

  enum FormatFlag flag = MUTT_FORMAT_MAKEPRINT | MUTT_FORMAT_ARROWCURSOR | MUTT_FORMAT_INDEX;
  struct MuttThread *tmp = NULL;

  if ((Sort & SORT_MASK) == SORT_THREADS && e->tree)
  {
    flag |= MUTT_FORMAT_TREE; /* display the thread tree */
    if (e->display_subject)
      flag |= MUTT_FORMAT_FORCESUBJ;
    else
    {
      const int reverse = Sort & SORT_REVERSE;
      int edgemsgno;
      if (reverse)
      {
        if (menu->top + menu->pagelen > menu->max)
          edgemsgno = Context->mailbox->v2r[menu->max - 1];
        else
          edgemsgno = Context->mailbox->v2r[menu->top + menu->pagelen - 1];
      }
      else
        edgemsgno = Context->mailbox->v2r[menu->top];

      for (tmp = e->thread->parent; tmp; tmp = tmp->parent)
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
        for (tmp = e->thread->prev; tmp; tmp = tmp->prev)
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

  mutt_make_string_flags(buf, buflen, NONULL(IndexFormat), Context, e, flag);
}

/**
 * index_color - Calculate the colour for a line of the index - Implements Menu::menu_color()
 */
int index_color(int line)
{
  if (!Context || (line < 0))
    return 0;

  struct Email *e = Context->mailbox->emails[Context->mailbox->v2r[line]];

  if (e && e->pair)
    return e->pair;

  mutt_set_header_color(Context->mailbox, e);
  if (e)
    return e->pair;

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
void mutt_draw_statusline(int cols, const char *buf, size_t buflen)
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

/**
 * index_custom_redraw - Redraw the index - Implements Menu::menu_custom_redraw()
 */
static void index_custom_redraw(struct Menu *menu)
{
  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);
    mutt_show_error();
  }

#ifdef USE_SIDEBAR
  if (menu->redraw & REDRAW_SIDEBAR)
    menu_redraw_sidebar(menu);
#endif

  if (Context && Context->mailbox->emails && !(menu->current >= Context->mailbox->vcount))
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
      menu_status_line(buf, sizeof(buf), menu, NONULL(TsStatusFormat));
      mutt_ts_status(buf);
      menu_status_line(buf, sizeof(buf), menu, NONULL(TsIconFormat));
      mutt_ts_icon(buf);
    }
  }

  menu->redraw = 0;
}

/**
 * mutt_index_menu - Display a list of emails
 * @retval num How the menu was finished, e.g. OP_QUIT, OP_EXIT
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
  bool do_mailbox_notify = true;
  int close = 0; /* did we OP_QUIT or OP_EXIT out of this menu? */
  int attach_msg = OptAttachMsg;

  struct Menu *menu = mutt_menu_new(MENU_MAIN);
  menu->menu_make_entry = index_make_entry;
  menu->menu_color = index_color;
  menu->current = ci_first_message();
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_MAIN,
#ifdef USE_NNTP
                                 (Context && (Context->mailbox->magic == MUTT_NNTP)) ?
                                     IndexNewsHelp :
#endif
                                     IndexHelp);
  menu->menu_custom_redraw = index_custom_redraw;
  mutt_menu_push_current(menu);

  if (!attach_msg)
  {
    /* force the mailbox check after we enter the folder */
    mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE);
  }
#ifdef USE_INOTIFY
  mutt_monitor_add(NULL);
#endif

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
    if (OptNeedResort && Context && Context->mailbox->msg_count && menu->current >= 0)
      resort_index(menu);

    menu->max = Context ? Context->mailbox->vcount : 0;
    oldcount = Context ? Context->mailbox->msg_count : 0;

    if (OptRedrawTree && Context && Context->mailbox->msg_count && (Sort & SORT_MASK) == SORT_THREADS)
    {
      mutt_draw_tree(Context);
      menu->redraw |= REDRAW_STATUS;
      OptRedrawTree = false;
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

      index_hint = (Context->mailbox->vcount && menu->current >= 0 &&
                    menu->current < Context->mailbox->vcount) ?
                       CUR_EMAIL->index :
                       0;

      check = mx_mbox_check(Context, &index_hint);
      if (check < 0)
      {
        if (!Context->mailbox || Context->mailbox->path[0] == '\0')
        {
          /* fatal error occurred */
          mutt_context_free(&Context);
          menu->redraw = REDRAW_FULL;
        }

        OptSearchInvalid = true;
      }
      else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED || check == MUTT_FLAGS)
      {
        /* notify the user of new mail */
        if (check == MUTT_REOPENED)
        {
          mutt_error(
              _("Mailbox was externally modified.  Flags may be wrong."));
        }
        else if (check == MUTT_NEW_MAIL)
        {
          for (i = oldcount; i < Context->mailbox->msg_count; i++)
          {
            if (!Context->mailbox->emails[i]->read)
            {
              mutt_message(_("New mail in this mailbox"));
              if (BeepNew)
                beep();
              if (NewMailCommand)
              {
                char cmd[LONG_STRING];
                menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCommand));
                if (mutt_system(cmd) != 0)
                  mutt_error(_("Error running \"%s\""), cmd);
              }
              break;
            }
          }
        }
        else if (check == MUTT_FLAGS)
          mutt_message(_("Mailbox was externally modified"));

        /* avoid the message being overwritten by mailbox */
        do_mailbox_notify = false;

        bool q = Context->mailbox->quiet;
        Context->mailbox->quiet = true;
        update_index(menu, Context, check, oldcount, index_hint);
        Context->mailbox->quiet = q;

        menu->redraw = REDRAW_FULL;
        menu->max = Context->mailbox->vcount;

        OptSearchInvalid = true;
      }
    }

    if (!attach_msg)
    {
      /* check for new mail in the incoming folders */
      oldcount = newcount;
      newcount = mutt_mailbox_check(0);
      if (newcount != oldcount)
        menu->redraw |= REDRAW_STATUS;
      if (do_mailbox_notify)
      {
        if (mutt_mailbox_notify())
        {
          menu->redraw |= REDRAW_STATUS;
          if (BeepNew)
            beep();
          if (NewMailCommand)
          {
            char cmd[LONG_STRING];
            menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCommand));
            if (mutt_system(cmd) != 0)
              mutt_error(_("Error running \"%s\""), cmd);
          }
        }
      }
      else
        do_mailbox_notify = true;
    }

    if (op >= 0)
      mutt_curs_set(0);

    if (menu->menu == MENU_MAIN)
    {
      index_custom_redraw(menu);

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
      {
        mutt_window_move(MuttIndexWindow, menu->current - menu->top + menu->offset,
                         MuttIndexWindow->cols - 1);
      }
      mutt_refresh();

      if (SigWinch)
      {
        mutt_flushinp();
        mutt_resize_screen();
        SigWinch = 0;
        menu->top = 0; /* so we scroll the right amount */
        /* force a real complete redraw.  clrtobot() doesn't seem to be able
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

        if (!Context || !Context->mailbox)
        {
          mutt_error(_("No mailbox is open"));
          continue;
        }

        if (!Context->mailbox->msg_tagged)
        {
          if (op == OP_TAG_PREFIX)
            mutt_error(_("No tagged messages"));
          else if (op == OP_TAG_PREFIX_COND)
          {
            mutt_flush_macro_to_endcond();
            mutt_message(_("Nothing to do"));
          }
          continue;
        }

        /* get the real command */
        tag = true;
        continue;
      }
      else if (AutoTag && Context && Context->mailbox && Context->mailbox->msg_tagged)
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
    OptNews = false; /* for any case */
#endif

#ifdef USE_NOTMUCH
    if (Context)
      nm_db_debug_check(Context->mailbox);
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
        if (Context->mailbox->magic == MUTT_NNTP)
        {
          struct Email *e = NULL;

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
            if (STAILQ_EMPTY(&CUR_EMAIL->env->references))
            {
              mutt_error(_("Article has no parent reference"));
              break;
            }
            mutt_str_strfcpy(buf, STAILQ_FIRST(&CUR_EMAIL->env->references)->data,
                             sizeof(buf));
          }
          if (!Context->mailbox->id_hash)
            Context->mailbox->id_hash = mutt_make_id_hash(Context->mailbox);
          e = mutt_hash_find(Context->mailbox->id_hash, buf);
          if (e)
          {
            if (e->virtual != -1)
            {
              menu->current = e->virtual;
              menu->redraw = REDRAW_MOTION_RESYNCH;
            }
            else if (e->collapsed)
            {
              mutt_uncollapse_thread(Context, e);
              mutt_set_virtual(Context);
              menu->current = e->virtual;
              menu->redraw = REDRAW_MOTION_RESYNCH;
            }
            else
              mutt_error(_("Message is not visible in limited view"));
          }
          else
          {
            int rc2;

            mutt_message(_("Fetching %s from server..."), buf);
            rc2 = nntp_check_msgid(Context, buf);
            if (rc2 == 0)
            {
              e = Context->mailbox->emails[Context->mailbox->msg_count - 1];
              mutt_sort_headers(Context, false);
              menu->current = e->virtual;
              menu->redraw = REDRAW_FULL;
            }
            else if (rc2 > 0)
              mutt_error(_("Article %s not found on the server"), buf);
          }
        }
        break;

      case OP_GET_CHILDREN:
      case OP_RECONSTRUCT_THREAD:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        CHECK_ATTACH;
        if (Context->mailbox->magic == MUTT_NNTP)
        {
          int oldmsgcount = Context->mailbox->msg_count;
          int oldindex = CUR_EMAIL->index;
          int rc2 = 0;

          if (!CUR_EMAIL->env->message_id)
          {
            mutt_error(_("No Message-Id. Unable to perform operation."));
            break;
          }

          mutt_message(_("Fetching message headers..."));
          if (!Context->mailbox->id_hash)
            Context->mailbox->id_hash = mutt_make_id_hash(Context->mailbox);
          mutt_str_strfcpy(buf, CUR_EMAIL->env->message_id, sizeof(buf));

          /* trying to find msgid of the root message */
          if (op == OP_RECONSTRUCT_THREAD)
          {
            struct ListNode *ref = NULL;
            STAILQ_FOREACH(ref, &CUR_EMAIL->env->references, entries)
            {
              if (!mutt_hash_find(Context->mailbox->id_hash, ref->data))
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
          if (Context->mailbox->msg_count > oldmsgcount)
          {
            struct Email *oldcur = CUR_EMAIL;
            struct Email *e = NULL;
            bool quiet = Context->mailbox->quiet;

            if (rc2 < 0)
              Context->mailbox->quiet = true;
            mutt_sort_headers(Context, (op == OP_RECONSTRUCT_THREAD));
            Context->mailbox->quiet = quiet;

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
            e = mutt_hash_find(Context->mailbox->id_hash, buf);
            if (e)
              menu->current = e->virtual;

            /* try to restore old position */
            else
            {
              for (int k = 0; k < Context->mailbox->msg_count; k++)
              {
                if (Context->mailbox->emails[k]->index == oldindex)
                {
                  menu->current = Context->mailbox->emails[k]->virtual;
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
            mutt_error(_("No deleted messages found in the thread"));
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
        if ((mutt_get_field(_("Jump to message: "), buf, sizeof(buf), 0) != 0) ||
            (buf[0] == '\0'))
        {
          mutt_error(_("Nothing to do"));
        }
        else if (mutt_str_atoi(buf, &i) < 0)
          mutt_error(_("Argument must be a message number"));
        else if ((i < 1) || (i > Context->mailbox->msg_count))
          mutt_error(_("Invalid message number"));
        else if (!message_is_visible(Context, i - 1))
          mutt_error(_("That message is not visible"));
        else
        {
          struct Email *e = Context->mailbox->emails[i - 1];

          if (mutt_messages_in_thread(Context->mailbox, e, 1) > 1)
          {
            mutt_uncollapse_thread(Context, e);
            mutt_set_virtual(Context);
          }
          menu->current = e->virtual;
        }

        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_FULL;

        break;

        /* --------------------------------------------------------------------
         * `index' specific commands
         */

      case OP_MAIN_DELETE_PATTERN:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           delete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.
         */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete messages"));

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

      case OP_SHOW_LOG_MESSAGES:
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
          mutt_message(_("No limit pattern is in effect"));
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
        menu->oldcurrent = (Context->mailbox->vcount && menu->current >= 0 &&
                            menu->current < Context->mailbox->vcount) ?
                               CUR_EMAIL->index :
                               -1;
        if (op == OP_TOGGLE_READ)
        {
          char buf2[LONG_STRING];

          if (!Context->pattern || (strncmp(Context->pattern, "!~R!~D~s", 8) != 0))
          {
            snprintf(buf2, sizeof(buf2), "!~R!~D~s%s",
                     Context->pattern ? Context->pattern : ".*");
          }
          else
          {
            mutt_str_strfcpy(buf2, Context->pattern + 8, sizeof(buf2));
            if (!*buf2 || (strncmp(buf2, ".*", 2) == 0))
              snprintf(buf2, sizeof(buf2), "~A");
          }
          FREE(&Context->pattern);
          Context->pattern = mutt_str_strdup(buf2);
          mutt_pattern_func(MUTT_LIMIT, NULL);
        }

        if (((op == OP_LIMIT_CURRENT_THREAD) && mutt_limit_current_thread(CUR_EMAIL)) ||
            (op == OP_TOGGLE_READ) ||
            ((op == OP_MAIN_LIMIT) &&
             (mutt_pattern_func(MUTT_LIMIT,
                                _("Limit to messages matching: ")) == 0)))
        {
          if (menu->oldcurrent >= 0)
          {
            /* try to find what used to be the current message */
            menu->current = -1;
            for (i = 0; i < Context->mailbox->vcount; i++)
            {
              if (Context->mailbox->emails[Context->mailbox->v2r[i]]->index == menu->oldcurrent)
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
          if (Context->mailbox->msg_count && (Sort & SORT_MASK) == SORT_THREADS)
            mutt_draw_tree(Context);
          menu->redraw = REDRAW_FULL;
        }
        if (Context->pattern)
          mutt_message(_("To view all messages, limit to \"all\""));
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

          oldcount = Context ? Context->mailbox->msg_count : 0;

          mutt_startup_shutdown_hook(MUTT_SHUTDOWN_HOOK);

          if (!Context || (check = mx_mbox_close(&Context, &index_hint)) == 0)
            done = true;
          else
          {
            if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
              update_index(menu, Context, check, oldcount, index_hint);

            menu->redraw = REDRAW_FULL; /* new mail arrived? */
            OptSearchInvalid = true;
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
          if (Context && Context->mailbox->msg_count)
          {
            resort_index(menu);
            OptSearchInvalid = true;
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
          for (j = 0; j < Context->mailbox->msg_count; j++)
            if (message_is_visible(Context, j))
              mutt_set_flag(Context->mailbox, Context->mailbox->emails[j], MUTT_TAG, 0);
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_TAG, !CUR_EMAIL->tagged);

          Context->last_tag = CUR_EMAIL->tagged ?
                                  CUR_EMAIL :
                                  ((Context->last_tag == CUR_EMAIL && !CUR_EMAIL->tagged) ?
                                       NULL :
                                       Context->last_tag);

          menu->redraw |= REDRAW_STATUS;
          if (Resolve && menu->current < Context->mailbox->vcount - 1)
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
        /* L10N: Due to the implementation details we do not know whether we
           undelete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.
         */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete messages"));

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
        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        ci_send_message(SEND_TO_SENDER, NULL, NULL, Context, tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;
        break;

        /* --------------------------------------------------------------------
         * The following operations can be performed inside of the pager.
         */

#ifdef USE_IMAP
      case OP_MAIN_IMAP_FETCH:
        if (Context && Context->mailbox->magic == MUTT_IMAP)
          imap_check_mailbox(Context->mailbox, true);
        break;

      case OP_MAIN_IMAP_LOGOUT_ALL:
        if (Context && Context->mailbox->magic == MUTT_IMAP)
        {
          int check = mx_mbox_close(&Context, &index_hint);
          if (check != 0)
          {
            if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
              update_index(menu, Context, check, oldcount, index_hint);
            OptSearchInvalid = true;
            menu->redraw = REDRAW_FULL;
            break;
          }
        }
        imap_logout_all();
        mutt_message(_("Logged out of IMAP servers"));
        OptSearchInvalid = true;
        menu->redraw = REDRAW_FULL;
        break;
#endif

      case OP_MAIN_SYNC_FOLDER:

        if (Context && !Context->mailbox->msg_count)
          break;

        CHECK_MSGCOUNT;
        CHECK_READONLY;
        {
          int ovc = Context->mailbox->vcount;
          int oc = Context->mailbox->msg_count;
          int check;
          struct Email *newhdr = NULL;

          /* don't attempt to move the cursor if there are no visible messages in the current limit */
          if (menu->current < Context->mailbox->vcount)
          {
            /* threads may be reordered, so figure out what header the cursor
             * should be on. #3092 */
            int newidx = menu->current;
            if (CUR_EMAIL->deleted)
              newidx = ci_next_undeleted(menu->current);
            if (newidx < 0)
              newidx = ci_previous_undeleted(menu->current);
            if (newidx >= 0)
              newhdr = Context->mailbox->emails[Context->mailbox->v2r[newidx]];
          }

          check = mx_mbox_sync(Context, &index_hint);
          if (check == 0)
          {
            if (newhdr && Context->mailbox->vcount != ovc)
            {
              for (j = 0; j < Context->mailbox->vcount; j++)
              {
                if (Context->mailbox->emails[Context->mailbox->v2r[j]] == newhdr)
                {
                  menu->current = j;
                  break;
                }
              }
            }
            OptSearchInvalid = true;
          }
          else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
            update_index(menu, Context, check, oc, index_hint);

          /* do a sanity check even if mx_mbox_sync failed.  */

          if ((menu->current < 0) || (Context && Context->mailbox &&
                                      (menu->current >= Context->mailbox->vcount)))
          {
            menu->current = ci_first_message();
          }
        }

        /* check for a fatal error, or all messages deleted */
        if (Context->mailbox->path[0] == '\0')
          mutt_context_free(&Context);

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
          for (j = 0; j < Context->mailbox->msg_count; j++)
          {
            if (message_is_tagged(Context, j))
            {
              Context->mailbox->emails[j]->quasi_deleted = true;
              Context->mailbox->changed = true;
            }
          }
        }
        else
        {
          CUR_EMAIL->quasi_deleted = true;
          Context->mailbox->changed = true;
        }
        break;

#ifdef USE_NOTMUCH
      case OP_MAIN_ENTIRE_THREAD:
      {
        if (!Context || (Context->mailbox->magic != MUTT_NOTMUCH))
        {
          if (!CUR_EMAIL || !CUR_EMAIL->env || !CUR_EMAIL->env->message_id)
          {
            mutt_message(_("No virtual folder and no Message-Id, aborting"));
            break;
          } // no virtual folder, but we have message-id, reconstruct thread on-the-fly
          strncpy(buf, "id:", sizeof(buf));
          int msg_id_offset = 0;
          if ((CUR_EMAIL->env->message_id)[0] == '<')
            msg_id_offset = 1;
          mutt_str_strcat(buf, sizeof(buf), (CUR_EMAIL->env->message_id) + msg_id_offset);
          if (buf[strlen(buf) - 1] == '>')
            buf[strlen(buf) - 1] = '\0';
          if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          {
            mutt_message(_("Failed to create query, aborting"));
            break;
          }
          else
            main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint);
        }
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        int oc = Context->mailbox->msg_count;
        if (nm_read_entire_thread(Context, CUR_EMAIL) < 0)
        {
          mutt_message(_("Failed to read thread, aborting"));
          break;
        }
        if (oc < Context->mailbox->msg_count)
        {
          struct Email *oldcur = CUR_EMAIL;

          if ((Sort & SORT_MASK) == SORT_THREADS)
            mutt_sort_headers(Context, false);
          menu->current = oldcur->virtual;
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;

          if (oldcur->collapsed || Context->collapsed)
          {
            menu->current = mutt_uncollapse_thread(Context, CUR_EMAIL);
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
        if (!Context || !mx_tags_is_supported(Context->mailbox))
        {
          mutt_message(_("Folder doesn't support tagging, aborting"));
          break;
        }
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        char *tags = NULL;
        if (!tag)
          tags = driver_tags_get_with_hidden(&CUR_EMAIL->tags);
        rc = mx_tags_edit(Context->mailbox, tags, buf, sizeof(buf));
        FREE(&tags);
        if (rc < 0)
          break;
        else if (rc == 0)
        {
          mutt_message(_("No tag specified, aborting"));
          break;
        }

        if (tag)
        {
          struct Progress progress;
          int px;

          if (!Context->mailbox->quiet)
          {
            char msgbuf[STRING];
            snprintf(msgbuf, sizeof(msgbuf), _("Update tags..."));
            mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, 1,
                               Context->mailbox->msg_tagged);
          }

#ifdef USE_NOTMUCH
          if (Context->mailbox->magic == MUTT_NOTMUCH)
            nm_db_longrun_init(Context->mailbox, true);
#endif
          for (px = 0, j = 0; j < Context->mailbox->msg_count; j++)
          {
            if (!message_is_tagged(Context, j))
              continue;

            if (!Context->mailbox->quiet)
              mutt_progress_update(&progress, ++px, -1);
            mx_tags_commit(Context->mailbox, Context->mailbox->emails[j], buf);
            if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
            {
              bool still_queried = false;
#ifdef USE_NOTMUCH
              if (Context->mailbox->magic == MUTT_NOTMUCH)
                still_queried = nm_message_is_still_queried(
                    Context->mailbox, Context->mailbox->emails[j]);
#endif
              Context->mailbox->emails[j]->quasi_deleted = !still_queried;
              Context->mailbox->changed = true;
            }
          }
#ifdef USE_NOTMUCH
          if (Context->mailbox->magic == MUTT_NOTMUCH)
            nm_db_longrun_done(Context->mailbox);
#endif
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (mx_tags_commit(Context->mailbox, CUR_EMAIL, buf))
          {
            mutt_message(_("Failed to modify tags, aborting"));
            break;
          }
          if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
          {
            bool still_queried = false;
#ifdef USE_NOTMUCH
            if (Context->mailbox->magic == MUTT_NOTMUCH)
              still_queried = nm_message_is_still_queried(Context->mailbox, CUR_EMAIL);
#endif
            CUR_EMAIL->quasi_deleted = !still_queried;
            Context->mailbox->changed = true;
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

      case OP_CHECK_STATS:
        mutt_check_stats();
        break;

#ifdef USE_NOTMUCH
      case OP_MAIN_VFOLDER_FROM_QUERY:
        buf[0] = '\0';
        if (mutt_get_field("Query: ", buf, sizeof(buf), MUTT_NM_QUERY) != 0 || !buf[0])
        {
          mutt_message(_("No query, aborting"));
          break;
        }
        if (!nm_uri_from_query(NULL, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_BACKWARD:
        CHECK_IN_MAILBOX;
        mutt_debug(2, "OP_MAIN_WINDOWED_VFOLDER_BACKWARD\n");
        if (NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled"));
          break;
        }
        if (!NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded"));
          break;
        }
        nm_query_window_backward();
        mutt_str_strfcpy(buf, NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_FORWARD:
        CHECK_IN_MAILBOX;
        if (NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled"));
          break;
        }
        if (!NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded"));
          break;
        }
        nm_query_window_forward();
        mutt_str_strfcpy(buf, NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
        {
          mutt_debug(2, "nm: + windowed query (%s)\n", buf);
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint);
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
      {
        struct Mailbox *m = NULL;
        OptNews = false;
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
        else
          cp = _("Open mailbox");

        buf[0] = '\0';
        if ((op == OP_MAIN_NEXT_UNREAD_MAILBOX) && Context &&
            (Context->mailbox->path[0] != '\0'))
        {
          mutt_str_strfcpy(buf, Context->mailbox->path, sizeof(buf));
          mutt_pretty_mailbox(buf, sizeof(buf));
          mutt_mailbox(buf, sizeof(buf));
          if (!buf[0])
          {
            mutt_error(_("No mailboxes have new mail"));
            break;
          }
        }
#ifdef USE_SIDEBAR
        else if (op == OP_SIDEBAR_OPEN)
        {
          m = mutt_sb_get_highlight();
          if (!m)
            break;
          mutt_str_strfcpy(buf, m->path, sizeof(buf));

          /* Mark the selected dir for the neomutt browser */
          mutt_browser_select_dir(m->path);
        }
#endif
        else
        {
          if (ChangeFolderNext && Context && (Context->mailbox->path[0] != '\0'))
          {
            mutt_str_strfcpy(buf, Context->mailbox->path, sizeof(buf));
            mutt_pretty_mailbox(buf, sizeof(buf));
          }
#ifdef USE_NNTP
          if (op == OP_MAIN_CHANGE_GROUP || op == OP_MAIN_CHANGE_GROUP_READONLY)
          {
            OptNews = true;
            m = Context ? Context->mailbox : NULL;
            CurrentNewsSrv = nntp_select_server(m, NewsServer, false);
            if (!CurrentNewsSrv)
              break;
            if (flags)
              cp = _("Open newsgroup in read-only mode");
            else
              cp = _("Open newsgroup");
            nntp_mailbox(m, buf, sizeof(buf));
          }
          else
#endif
          {
            /* By default, fill buf with the next mailbox that contains unread
             * mail */
            mutt_mailbox(buf, sizeof(buf));
          }

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

        if (!m)
          m = mx_mbox_find2(buf);

        main_change_folder(menu, op, m, buf, sizeof(buf), &oldcount, &index_hint);
#ifdef USE_NNTP
        /* mutt_mailbox_check() must be done with mail-reader mode! */
        menu->help = mutt_compile_help(
            helpstr, sizeof(helpstr), MENU_MAIN,
            (Context && (Context->mailbox->magic == MUTT_NNTP)) ? IndexNewsHelp : IndexHelp);
#endif
        mutt_expand_path(buf, sizeof(buf));
#ifdef USE_SIDEBAR
        mutt_sb_set_open_mailbox();
#endif
        break;
      }

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        /* toggle the weeding of headers so that a user can press the key
         * again while reading the message.
         */
        if (op == OP_DISPLAY_HEADERS)
          bool_str_toggle(Config, "weed", NULL);

        OptNeedResort = false;

        if ((Sort & SORT_MASK) == SORT_THREADS && CUR_EMAIL->collapsed)
        {
          mutt_uncollapse_thread(Context, CUR_EMAIL);
          mutt_set_virtual(Context);
          if (UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, CUR_EMAIL);
        }

        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        int hint =
            Context->mailbox->emails[Context->mailbox->v2r[menu->current]]->index;

        /* If we are returning to the pager via an index menu redirection, we
         * need to reset the menu->menu.  Otherwise mutt_menu_pop_current() will
         * set CurrentMenu incorrectly when we return back to the index menu. */
        menu->menu = MENU_MAIN;

        op = mutt_display_message(CUR_EMAIL);
        if (op < 0)
        {
          OptNeedResort = false;
          break;
        }

        /* This is used to redirect a single operation back here afterwards.  If
         * mutt_display_message() returns 0, then the menu and pager state will
         * be cleaned up after this switch statement. */
        menu->menu = MENU_PAGER;
        menu->oldcurrent = menu->current;
        if (Context)
          update_index(menu, Context, MUTT_NEW_MAIL, Context->mailbox->msg_count, hint);

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
            mutt_context_free(&Context);
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
          mutt_error(_("Threading is not enabled"));
        else if (!STAILQ_EMPTY(&CUR_EMAIL->env->in_reply_to) ||
                 !STAILQ_EMPTY(&CUR_EMAIL->env->references))
        {
          {
            struct Email *oldcur = CUR_EMAIL;

            mutt_break_thread(CUR_EMAIL);
            mutt_sort_headers(Context, true);
            menu->current = oldcur->virtual;
          }

          Context->mailbox->changed = true;
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
        {
          mutt_error(
              _("Thread cannot be broken, message is not part of a thread"));
        }

        break;

      case OP_MAIN_LINK_THREADS:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        /* L10N: CHECK_ACL */
        CHECK_ACL(MUTT_ACL_WRITE, _("Cannot link threads"));

        if ((Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled"));
        else if (!CUR_EMAIL->env->message_id)
          mutt_error(_("No Message-ID: header available to link thread"));
        else if (!tag && (!Context->last_tag || !Context->last_tag->tagged))
          mutt_error(_("First, please tag a message to be linked here"));
        else
        {
          struct Email *oldcur = CUR_EMAIL;

          if (mutt_link_threads(CUR_EMAIL, tag ? NULL : Context->last_tag, Context))
          {
            mutt_sort_headers(Context, true);
            menu->current = oldcur->virtual;

            Context->mailbox->changed = true;
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
        mutt_edit_content_type(CUR_EMAIL, CUR_EMAIL->content, NULL);
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
        if (menu->current >= Context->mailbox->vcount - 1)
        {
          if (menu->menu == MENU_MAIN)
            mutt_error(_("You are on the last message"));
          break;
        }
        menu->current = ci_next_undeleted(menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (menu->menu == MENU_MAIN)
            mutt_error(_("No undeleted messages"));
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
        if (menu->current >= Context->mailbox->vcount - 1)
        {
          if (menu->menu == MENU_MAIN)
            mutt_error(_("You are on the last message"));
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
          mutt_error(_("You are on the first message"));
          break;
        }
        menu->current = ci_previous_undeleted(menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (menu->menu == MENU_MAIN)
            mutt_error(_("No undeleted messages"));
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
            mutt_error(_("You are on the first message"));
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
        if ((mutt_save_message(tag ? NULL : CUR_EMAIL,
                               (op == OP_DECRYPT_SAVE) || (op == OP_SAVE) || (op == OP_DECODE_SAVE),
                               (op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY),
                               (op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY)) == 0) &&
            ((op == OP_SAVE) || (op == OP_DECODE_SAVE) || (op == OP_DECRYPT_SAVE)))
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

        const int saved_current = menu->current;
        i = menu->current;
        menu->current = -1;
        for (j = 0; j != Context->mailbox->vcount; j++)
        {
          if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_NEXT_NEW_THEN_UNREAD)
          {
            i++;
            if (i > Context->mailbox->vcount - 1)
            {
              i = 0;
            }
          }
          else
          {
            i--;
            if (i < 0)
            {
              i = Context->mailbox->vcount - 1;
            }
          }

          struct Email *e = Context->mailbox->emails[Context->mailbox->v2r[i]];
          if (e->collapsed && (Sort & SORT_MASK) == SORT_THREADS)
          {
            if (UNREAD(e) && first_unread == -1)
              first_unread = i;
            if (UNREAD(e) == 1 && first_new == -1)
              first_new = i;
          }
          else if ((!e->deleted && !e->read))
          {
            if (first_unread == -1)
              first_unread = i;
            if ((!e->old) && first_new == -1)
              first_new = i;
          }

          if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD) && first_unread != -1)
            break;
          if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
               op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
              first_new != -1)
          {
            break;
          }
        }
        if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
             op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
            first_new != -1)
        {
          menu->current = first_new;
        }
        else if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD ||
                  op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD) &&
                 first_unread != -1)
        {
          menu->current = first_unread;
        }

        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW)
          {
            if (Context->pattern)
              mutt_error(_("No new messages in this limited view"));
            else
              mutt_error(_("No new messages"));
          }
          else
          {
            if (Context->pattern)
              mutt_error(_("No unread messages in this limited view"));
            else
              mutt_error(_("No unread messages"));
          }
          break;
        }

        if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_NEXT_NEW_THEN_UNREAD)
        {
          if (saved_current > menu->current)
          {
            mutt_message(_("Search wrapped to top"));
          }
        }
        else if (saved_current < menu->current)
        {
          mutt_message(_("Search wrapped to bottom"));
        }

        if (menu->menu == MENU_PAGER)
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
          for (j = 0; j < Context->mailbox->msg_count; j++)
          {
            if (message_is_tagged(Context, j))
            {
              mutt_set_flag(Context->mailbox, Context->mailbox->emails[j],
                            MUTT_FLAG, !Context->mailbox->emails[j]->flagged);
            }
          }

          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_FLAG, !CUR_EMAIL->flagged);
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
          for (j = 0; j < Context->mailbox->msg_count; j++)
          {
            if (!message_is_tagged(Context, j))
              continue;

            if (Context->mailbox->emails[j]->read || Context->mailbox->emails[j]->old)
              mutt_set_flag(Context->mailbox, Context->mailbox->emails[j], MUTT_NEW, 1);
            else
              mutt_set_flag(Context->mailbox, Context->mailbox->emails[j], MUTT_READ, 1);
          }
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (CUR_EMAIL->read || CUR_EMAIL->old)
            mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_NEW, 1);
          else
            mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_READ, 1);

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
        if (mx_toggle_write(Context->mailbox) == 0)
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
            menu->current = mutt_next_thread(CUR_EMAIL);
            break;

          case OP_MAIN_NEXT_SUBTHREAD:
            menu->current = mutt_next_subthread(CUR_EMAIL);
            break;

          case OP_MAIN_PREV_THREAD:
            menu->current = mutt_previous_thread(CUR_EMAIL);
            break;

          case OP_MAIN_PREV_SUBTHREAD:
            menu->current = mutt_previous_subthread(CUR_EMAIL);
            break;
        }

        if (menu->current < 0)
        {
          menu->current = menu->oldcurrent;
          if (op == OP_MAIN_NEXT_THREAD || op == OP_MAIN_NEXT_SUBTHREAD)
            mutt_error(_("No more threads"));
          else
            mutt_error(_("You are on the first thread"));
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

        menu->current = mutt_parent_message(Context, CUR_EMAIL, op == OP_MAIN_ROOT_MESSAGE);
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

        if (mutt_change_flag(tag ? NULL : CUR_EMAIL, (op == OP_MAIN_SET_FLAG)) == 0)
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
          mutt_error(_("Threading is not enabled"));
          break;
        }

        if (CUR_EMAIL->collapsed)
        {
          menu->current = mutt_uncollapse_thread(Context, CUR_EMAIL);
          mutt_set_virtual(Context);
          if (UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, CUR_EMAIL);
        }
        else if (CAN_COLLAPSE(CUR_EMAIL))
        {
          menu->current = mutt_collapse_thread(Context, CUR_EMAIL);
          mutt_set_virtual(Context);
        }
        else
        {
          mutt_error(_("Thread contains unread or flagged messages"));
          break;
        }

        menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

        break;

      case OP_MAIN_COLLAPSE_ALL:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if ((Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error(_("Threading is not enabled"));
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
        ci_bounce_message(tag ? NULL : CUR_EMAIL);
        break;

      case OP_CREATE_ALIAS:

        mutt_alias_create(Context && Context->mailbox->vcount ? CUR_EMAIL->env : NULL, NULL);
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
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_DELETE, 1);
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
          if (DeleteUntag)
            mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_TAG, 0);
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
        /* L10N: Due to the implementation details we do not know whether we
           delete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.
         */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete messages"));

        {
          int subthread = (op == OP_DELETE_SUBTHREAD);
          rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_DELETE, 1, subthread);
          if (rc == -1)
            break;
          if (op == OP_PURGE_THREAD)
          {
            rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_PURGE, 1, subthread);
            if (rc == -1)
              break;
          }

          if (DeleteUntag)
            mutt_thread_set_flag(CUR_EMAIL, MUTT_TAG, 0, subthread);
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
        if (Context && Context->mailbox->magic == MUTT_NNTP)
        {
          struct NntpMboxData *mdata = Context->mailbox->mdata;
          if (mutt_newsgroup_catchup(Context->mailbox, mdata->adata, mdata->group))
            menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
#endif

      case OP_DISPLAY_ADDRESS:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_display_address(CUR_EMAIL->env);
        break;

      case OP_ENTER_COMMAND:

        mutt_enter_command();
        if (Context)
          mutt_check_rescore(Context->mailbox);
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
          edit = !Context->mailbox->readonly &&
                 mutt_bit_isset(Context->mailbox->rights, MUTT_ACL_INSERT);
        else
          edit = false;

        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        if (edit)
          mutt_edit_message(Context, tag ? NULL : CUR_EMAIL);
        else
          mutt_view_message(Context, tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;

        break;

      case OP_FORWARD_MESSAGE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        ci_send_message(SEND_FORWARD, NULL, NULL, Context, tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_GROUP_REPLY:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_ATTACH;
        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        ci_send_message(SEND_REPLY | SEND_GROUP_REPLY, NULL, NULL, Context,
                        tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EDIT_LABEL:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        CHECK_READONLY;
        rc = mutt_label_message(tag ? NULL : CUR_EMAIL);
        if (rc > 0)
        {
          Context->mailbox->changed = true;
          menu->redraw = REDRAW_FULL;
          /* L10N: This is displayed when the x-label on one or more
           * messages is edited. */
          mutt_message(ngettext("%d label changed", "%d labels changed", rc), rc);
        }
        else
        {
          /* L10N: This is displayed when editing an x-label, but no messages
           * were updated.  Possibly due to canceling at the prompt or if the new
           * label is the same as the old label. */
          mutt_message(_("No labels changed"));
        }
        break;

      case OP_LIST_REPLY:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        ci_send_message(SEND_REPLY | SEND_LIST_REPLY, NULL, NULL, Context,
                        tag ? NULL : CUR_EMAIL);
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
        ci_send_message(SEND_KEY, NULL, NULL, NULL, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EXTRACT_KEYS:
        if (!WithCrypto)
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        crypt_extract_keys_from_messages(tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_CHECK_TRADITIONAL:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED))
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);

        if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;

      case OP_PIPE:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_pipe_message(tag ? NULL : CUR_EMAIL);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, piping could change
         * new or old messages status to read. Redraw what's needed.
         */
        if (Context->mailbox->magic == MUTT_IMAP && !ImapPeek)
        {
          menu->redraw |= (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
        }
#endif

        break;

      case OP_PRINT:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_print_message(tag ? NULL : CUR_EMAIL);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, printing could change
         * new or old messages status to read. Redraw what's needed.
         */
        if (Context->mailbox->magic == MUTT_IMAP && !ImapPeek)
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
        /* L10N: Due to the implementation details we do not know whether we
           mark zero, 1, 12, ... messages as read. So in English we use
           "messages". Your language might have other means to express this.
         */
        CHECK_ACL(MUTT_ACL_SEEN, _("Cannot mark messages as read"));

        rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_READ, 1, op == OP_MAIN_READ_THREAD ? 0 : 1);

        if (rc != -1)
        {
          if (Resolve)
          {
            menu->current = (op == OP_MAIN_READ_THREAD ? mutt_next_thread(CUR_EMAIL) :
                                                         mutt_next_subthread(CUR_EMAIL));
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
        if (CUR_EMAIL->env->message_id)
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
            snprintf(macro, sizeof(macro), "<search>~i \"%s\"\n", CUR_EMAIL->env->message_id);
            /* L10N: "message hotkey" is the key bindings menu description of a
               macro created by <mark-message>. */
            km_bind(str, MENU_MAIN, OP_MACRO, macro, _("message hotkey"));

            /* L10N: This is echoed after <mark-message> creates a new hotkey
               macro.  %s is the hotkey string ($mark_macro_prefix followed
               by whatever they typed at the prompt.) */
            snprintf(buf2, sizeof(buf2), _("Message bound to %s"), str);
            mutt_message(buf2);
            mutt_debug(1, "Mark: %s => %s\n", str, macro);
          }
        }
        else
        {
          /* L10N: This error is printed if <mark-message> cannot find a
             Message-ID for the currently selected message in the index. */
          mutt_error(_("No message ID to macro"));
        }
        break;

      case OP_RECALL_MESSAGE:

        CHECK_ATTACH;
        ci_send_message(SEND_POSTPONED, NULL, NULL, Context, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_RESEND:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if (tag)
        {
          for (j = 0; j < Context->mailbox->msg_count; j++)
          {
            if (message_is_tagged(Context, j))
              mutt_resend_message(NULL, Context, Context->mailbox->emails[j]);
          }
        }
        else
          mutt_resend_message(NULL, Context, CUR_EMAIL);

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
        if (op != OP_FOLLOWUP || !CUR_EMAIL->env->followup_to ||
            (mutt_str_strcasecmp(CUR_EMAIL->env->followup_to, "poster") != 0) ||
            query_quadoption(FollowupToPoster,
                             _("Reply by mail as poster prefers?")) != MUTT_YES)
        {
          if (Context && Context->mailbox->magic == MUTT_NNTP &&
              !((struct NntpMboxData *) Context->mailbox->mdata)->allowed && query_quadoption(PostModerated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
          {
            break;
          }
          if (op == OP_POST)
            ci_send_message(SEND_NEWS, NULL, NULL, Context, NULL);
          else
          {
            CHECK_MSGCOUNT;
            ci_send_message((op == OP_FOLLOWUP ? SEND_REPLY : SEND_FORWARD) | SEND_NEWS,
                            NULL, NULL, Context, tag ? NULL : CUR_EMAIL);
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
        if (PgpAutoDecode && (tag || !(CUR_EMAIL->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(tag ? NULL : CUR_EMAIL, &menu->redraw);
        }
        ci_send_message(SEND_REPLY, NULL, NULL, Context, tag ? NULL : CUR_EMAIL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_SHELL_ESCAPE:

        mutt_shell_escape();
        break;

      case OP_TAG_THREAD:
      case OP_TAG_SUBTHREAD:

        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_TAG, !CUR_EMAIL->tagged,
                                  op == OP_TAG_THREAD ? 0 : 1);

        if (rc != -1)
        {
          if (Resolve)
          {
            if (op == OP_TAG_THREAD)
              menu->current = mutt_next_thread(CUR_EMAIL);
            else
              menu->current = mutt_next_subthread(CUR_EMAIL);

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
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_DELETE, 0);
          mutt_set_flag(Context->mailbox, CUR_EMAIL, MUTT_PURGE, 0);
          if (Resolve && menu->current < Context->mailbox->vcount - 1)
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
        /* L10N: Due to the implementation details we do not know whether we
           undelete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.
         */
        CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete messages"));

        rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_DELETE, 0, op == OP_UNDELETE_THREAD ? 0 : 1);
        if (rc != -1)
        {
          rc = mutt_thread_set_flag(CUR_EMAIL, MUTT_PURGE, 0,
                                    op == OP_UNDELETE_THREAD ? 0 : 1);
        }
        if (rc != -1)
        {
          if (Resolve)
          {
            if (op == OP_UNDELETE_THREAD)
              menu->current = mutt_next_thread(CUR_EMAIL);
            else
              menu->current = mutt_next_subthread(CUR_EMAIL);

            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_VERSION:
        mutt_message(mutt_make_version());
        break;

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

      case OP_VIEW_ATTACHMENTS:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        mutt_view_attachments(CUR_EMAIL);
        if (Context && CUR_EMAIL->attach_del)
          Context->mailbox->changed = true;
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
        bool_str_toggle(Config, "sidebar_visible", NULL);
        mutt_window_reflow();
        break;
#endif
      default:
        if (menu->menu == MENU_MAIN)
          km_error_key(MENU_MAIN);
    }

#ifdef USE_NOTMUCH
    if (Context)
      nm_db_debug_check(Context->mailbox);
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

/**
 * mutt_set_header_color - Select a colour for a message
 * @param m      Mailbox
 * @param curhdr Header of message
 */
void mutt_set_header_color(struct Mailbox *m, struct Email *curhdr)
{
  struct ColorLine *color = NULL;
  struct PatternCache cache = { 0 };

  if (!curhdr)
    return;

  STAILQ_FOREACH(color, &ColorIndexList, entries)
  {
    if (mutt_pattern_exec(color->color_pattern, MUTT_MATCH_FULL_ADDRESS, m, curhdr, &cache))
    {
      curhdr->pair = color->pair;
      return;
    }
  }
  curhdr->pair = ColorDefs[MT_COLOR_NORMAL];
}

/**
 * mutt_reply_listener - Listen for config changes to "reply_regex" - Implements ::cs_listener()
 */
bool mutt_reply_listener(const struct ConfigSet *cs, struct HashElem *he,
                         const char *name, enum ConfigEvent ev)
{
  if (mutt_str_strcmp(name, "reply_regex") != 0)
    return true;

  if (!Context)
    return true;

  regmatch_t pmatch[1];

  for (int i = 0; i < Context->mailbox->msg_count; i++)
  {
    struct Envelope *e = Context->mailbox->emails[i]->env;
    if (!e || !e->subject)
      continue;

    if (ReplyRegex && ReplyRegex->regex &&
        (regexec(ReplyRegex->regex, e->subject, 1, pmatch, 0) == 0))
    {
      e->real_subj = e->subject + pmatch[0].rm_eo;
      continue;
    }

    e->real_subj = e->subject;
  }

  OptResortInit = true; /* trigger a redraw of the index */
  return true;
}
