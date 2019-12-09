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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "index.h"
#include "alias.h"
#include "browser.h"
#include "color.h"
#include "commands.h"
#include "context.h"
#include "curs_lib.h"
#include "format_flags.h"
#include "globals.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
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
#ifdef USE_AUTOCRYPT
#include "autocrypt/autocrypt.h"
#endif

/* These Config Variables are only used in index.c */
bool C_ChangeFolderNext; ///< Config: Suggest the next folder, rather than the first when using '<change-folder>'
bool C_CollapseAll; ///< Config: Collapse all threads when entering a folder
bool C_CollapseFlagged; ///< Config: Prevent the collapse of threads with flagged emails
bool C_CollapseUnread; ///< Config: Prevent the collapse of threads with unread emails
char *C_MarkMacroPrefix; ///< Config: Prefix for macros using '<mark-message>'
bool C_PgpAutoDecode;    ///< Config: Automatically decrypt PGP messages
bool C_UncollapseJump; ///< Config: When opening a thread, jump to the next unread message
bool C_UncollapseNew; ///< Config: Open collapsed threads when new mail arrives

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

#define UNREAD(email) mutt_thread_contains_unread(Context, email)
#define FLAGGED(email) mutt_thread_contains_flagged(Context, email)

#define CAN_COLLAPSE(email)                                                    \
  ((C_CollapseUnread || !UNREAD(email)) && (C_CollapseFlagged || !FLAGGED(email)))

// clang-format off
/**
 * typedef CheckFlags - Checks to perform before running a function
 */
typedef uint8_t CheckFlags;       ///< Flags, e.g. #CHECK_IN_MAILBOX
#define CHECK_NO_FLAGS         0  ///< No flags are set
#define CHECK_IN_MAILBOX (1 << 0) ///< Is there a mailbox open?
#define CHECK_MSGCOUNT   (1 << 1) ///< Are there any messages?
#define CHECK_VISIBLE    (1 << 2) ///< Is the selected message visible in the index?
#define CHECK_READONLY   (1 << 3) ///< Is the mailbox readonly?
#define CHECK_ATTACH     (1 << 4) ///< Is the user in message-attach mode?
// clang-format on

/**
 * get_cur_email - Get the currently-selected Email
 * @param ctx  Context
 * @param menu Menu
 * @retval ptr  Email
 * @retval NULL No Email selected, or bad index values
 */
static struct Email *get_cur_email(struct Context *ctx, struct Menu *menu)
{
  if (!ctx || !ctx->mailbox || !menu)
    return NULL;

  return mutt_get_virt_email(ctx->mailbox, menu->current);
}

/**
 * prereq - Check the pre-requisites for a function
 * @param ctx    Mailbox
 * @param menu   Current Menu
 * @param checks Checks to perform, see #CheckFlags
 * @retval bool true if the checks pass successfully
 */
static bool prereq(struct Context *ctx, struct Menu *menu, CheckFlags checks)
{
  bool result = true;

  if (checks & (CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
    checks |= CHECK_IN_MAILBOX;

  if ((checks & CHECK_IN_MAILBOX) && (!ctx || !ctx->mailbox))
  {
    mutt_error(_("No mailbox is open"));
    result = false;
  }

  if (result && (checks & CHECK_MSGCOUNT) && (ctx->mailbox->msg_count == 0))
  {
    mutt_error(_("There are no messages"));
    result = false;
  }

  if (result && (checks & CHECK_VISIBLE) && (menu->current >= ctx->mailbox->vcount))
  {
    mutt_error(_("No visible messages"));
    result = false;
  }

  if (result && (checks & CHECK_READONLY) && ctx->mailbox->readonly)
  {
    mutt_error(_("Mailbox is read-only"));
    result = false;
  }

  if (result && (checks & CHECK_ATTACH) && OptAttachMsg)
  {
    mutt_error(_("Function not permitted in attach-message mode"));
    result = false;
  }

  if (!result)
    mutt_flushinp();

  return result;
}

/**
 * check_acl - Check the ACLs for a function
 * @param ctx Mailbox
 * @param acl ACL, see #AclFlags
 * @param msg Error message for failure
 * @retval bool true if the function is permitted
 */
static bool check_acl(struct Context *ctx, AclFlags acl, const char *msg)
{
  if (!ctx || !ctx->mailbox)
    return false;

  if (!(ctx->mailbox->rights & acl))
  {
    /* L10N: %s is one of the CHECK_ACL entries below. */
    mutt_error(_("%s: Operation not permitted by ACL"), msg);
    return false;
  }

  return true;
}

/**
 * collapse_all - Collapse/uncollapse all threads
 * @param ctx    Context
 * @param menu   current menu
 * @param toggle toggle collapsed state
 *
 * This function is called by the OP_MAIN_COLLAPSE_ALL command and on folder
 * enter if the #C_CollapseAll option is set. In the first case, the @a toggle
 * parameter is 1 to actually toggle collapsed/uncollapsed state on all
 * threads. In the second case, the @a toggle parameter is 0, actually turning
 * this function into a one-way collapse.
 */
static void collapse_all(struct Context *ctx, struct Menu *menu, int toggle)
{
  if (!ctx || !ctx->mailbox || (ctx->mailbox->msg_count == 0) || !menu)
    return;

  struct Email *e_cur = get_cur_email(ctx, menu);
  if (!e_cur)
    return;

  struct MuttThread *thread = NULL, *top = NULL;
  int final;

  /* Figure out what the current message would be after folding / unfolding,
   * so that we can restore the cursor in a sane way afterwards. */
  if (e_cur->collapsed && toggle)
    final = mutt_uncollapse_thread(ctx, e_cur);
  else if (CAN_COLLAPSE(e_cur))
    final = mutt_collapse_thread(ctx, e_cur);
  else
    final = e_cur->vnum;

  if (final == -1)
    return;

  struct Email *base = mutt_get_virt_email(ctx->mailbox, final);
  if (!base)
    return;

  /* Iterate all threads, perform collapse/uncollapse as needed */
  top = ctx->tree;
  ctx->collapsed = toggle ? !ctx->collapsed : true;
  struct Email *e = NULL;
  while ((thread = top))
  {
    while (!thread->message)
      thread = thread->child;
    e = thread->message;

    if (e->collapsed != ctx->collapsed)
    {
      if (e->collapsed)
        mutt_uncollapse_thread(ctx, e);
      else if (CAN_COLLAPSE(e))
        mutt_collapse_thread(ctx, e);
    }
    top = top->next;
  }

  /* Restore the cursor */
  mutt_set_vnum(ctx);
  for (int i = 0; i < ctx->mailbox->vcount; i++)
  {
    e = mutt_get_virt_email(ctx->mailbox, i);
    if (!e)
      break;
    if (e->index == base->index)
    {
      menu->current = i;
      break;
    }
  }

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * ci_next_undeleted - Find the next undeleted email
 * @param ctx   Context
 * @param msgno Message number to start at
 * @retval >=0 Message number of next undeleted email
 * @retval  -1 No more undeleted messages
 */
static int ci_next_undeleted(struct Context *ctx, int msgno)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  for (int i = msgno + 1; i < ctx->mailbox->vcount; i++)
  {
    struct Email *e = mutt_get_virt_email(ctx->mailbox, i);
    if (!e)
      continue;
    if (!e->deleted)
      return i;
  }
  return -1;
}

/**
 * ci_previous_undeleted - Find the previous undeleted email
 * @param ctx   Context
 * @param msgno Message number to start at
 * @retval >=0 Message number of next undeleted email
 * @retval  -1 No more undeleted messages
 */
static int ci_previous_undeleted(struct Context *ctx, int msgno)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  for (int i = msgno - 1; i >= 0; i--)
  {
    struct Email *e = mutt_get_virt_email(ctx->mailbox, i);
    if (!e)
      continue;
    if (!e->deleted)
      return i;
  }
  return -1;
}

/**
 * ci_first_message - Get index of first new message
 * @param ctx Context
 * @retval num Index of first new message
 *
 * Return the index of the first new message, or failing that, the first
 * unread message.
 */
static int ci_first_message(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox || (ctx->mailbox->msg_count == 0))
    return 0;

  int old = -1;
  for (int i = 0; i < ctx->mailbox->vcount; i++)
  {
    struct Email *e = mutt_get_virt_email(ctx->mailbox, i);
    if (!e)
      continue;
    if (!e->read && !e->deleted)
    {
      if (!e->old)
        return i;
      if (old == -1)
        old = i;
    }
  }
  if (old != -1)
    return old;

  /* If C_Sort is reverse and not threaded, the latest message is first.
   * If C_Sort is threaded, the latest message is first if exactly one
   * of C_Sort and C_SortAux are reverse.  */
  if (((C_Sort & SORT_REVERSE) && ((C_Sort & SORT_MASK) != SORT_THREADS)) ||
      (((C_Sort & SORT_MASK) == SORT_THREADS) && ((C_Sort ^ C_SortAux) & SORT_REVERSE)))
  {
    return 0;
  }
  else
  {
    return ctx->mailbox->vcount ? ctx->mailbox->vcount - 1 : 0;
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
    mutt_error(_("Can't toggle write on a readonly mailbox"));
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
 * @param ctx  Context
 * @param menu Current Menu
 */
static void resort_index(struct Context *ctx, struct Menu *menu)
{
  if (!ctx || !ctx->mailbox)
    return;

  struct Email *e_cur = get_cur_email(ctx, menu);

  menu->current = -1;
  mutt_sort_headers(ctx, false);
  /* Restore the current message */

  for (int i = 0; i < ctx->mailbox->vcount; i++)
  {
    struct Email *e = mutt_get_virt_email(ctx->mailbox, i);
    if (!e)
      continue;
    if (e == e_cur)
    {
      menu->current = i;
      break;
    }
  }

  if (((C_Sort & SORT_MASK) == SORT_THREADS) && (menu->current < 0))
    menu->current = mutt_parent_message(ctx, e_cur, false);

  if (menu->current < 0)
    menu->current = ci_first_message(ctx);

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
  if ((check != MUTT_REOPENED) && oldcount && (ctx->pattern || C_UncollapseNew))
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

      if (mutt_pattern_exec(SLIST_FIRST(ctx->limit_pattern),
                            MUTT_MATCH_FULL_ADDRESS, ctx->mailbox, e, NULL))
      {
        /* vnum will get properly set by mutt_set_vnum(), which
         * is called by mutt_sort_headers() just below. */
        e->vnum = 1;
        e->limited = true;
      }
    }
    /* Need a second sort to set virtual numbers and redraw the tree */
    mutt_sort_headers(ctx, false);
  }

  /* uncollapse threads with new mail */
  if (C_UncollapseNew)
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
      mutt_set_vnum(ctx);
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
      mutt_set_vnum(ctx);
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
      if (i == 0)
      {
        ctx->mailbox->vcount = 0;
        ctx->vsize = 0;
      }

      struct Email *e = ctx->mailbox->emails[i];
      if (!e)
        break;
      if (mutt_pattern_exec(SLIST_FIRST(ctx->limit_pattern),
                            MUTT_MATCH_FULL_ADDRESS, ctx->mailbox, e, NULL))
      {
        assert(ctx->mailbox->vcount < ctx->mailbox->msg_count);
        e->vnum = ctx->mailbox->vcount;
        ctx->mailbox->v2r[ctx->mailbox->vcount] = i;
        e->limited = true;
        ctx->mailbox->vcount++;
        struct Body *b = e->content;
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

  if ((C_Sort & SORT_MASK) == SORT_THREADS)
    update_index_threaded(ctx, check, oldcount);
  else
    update_index_unthreaded(ctx, check, oldcount);

  menu->current = -1;
  if (oldcount)
  {
    /* restore the current message to the message it was pointing to */
    for (int i = 0; i < ctx->mailbox->vcount; i++)
    {
      struct Email *e = mutt_get_virt_email(ctx->mailbox, i);
      if (!e)
        continue;
      if (e->index == menu->oldcurrent)
      {
        menu->current = i;
        break;
      }
    }
  }

  if (menu->current < 0)
    menu->current = ci_first_message(Context);
}

/**
 * mailbox_index_observer - Listen for Mailbox changes - Implements ::observer_t()
 *
 * If a Mailbox is closed, then set a pointer to NULL.
 */
static int mailbox_index_observer(struct NotifyCallback *nc)
{
  if (!nc)
    return -1;

  if ((nc->event_type != NT_MAILBOX) || (nc->event_subtype != MBN_CLOSED))
    return 0;

  struct Mailbox **ptr = (struct Mailbox **) nc->data;
  if (!ptr || !*ptr)
    return 0;

  *ptr = NULL;
  return 0;
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
 * @param pager_return Return to the pager afterwards
 * @retval  0 Success
 * @retval -1 Error
 */
static int main_change_folder(struct Menu *menu, int op, struct Mailbox *m,
                              char *buf, size_t buflen, int *oldcount,
                              int *index_hint, bool *pager_return)
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
    mx_path_canon(buf, buflen, C_Folder, NULL);
  }

  enum MailboxType magic = mx_path_probe(buf, NULL);
  if ((magic == MUTT_MAILBOX_ERROR) || (magic == MUTT_UNKNOWN))
  {
    // Try to see if the buffer matches a description before we bail.
    // We'll receive a non-null pointer if there is a corresponding mailbox.
    m = mailbox_find_name(buf);
    if (m)
    {
      mutt_str_strfcpy(buf, mailbox_path(m), buflen);
    }
    else
    {
      // Bail.
      mutt_error(_("%s is not a mailbox"), buf);
      return -1;
    }
  }

  /* past this point, we don't return to the pager on error */
  if (pager_return)
    *pager_return = false;

  /* keepalive failure in mutt_enter_fname may kill connection. */
  if (Context && Context->mailbox && (mutt_buffer_is_empty(&Context->mailbox->pathbuf)))
    ctx_free(&Context);

  if (Context && Context->mailbox)
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
      new_last_folder = mutt_str_strdup(mailbox_path(Context->mailbox));
    *oldcount = Context->mailbox->msg_count;

    int check = mx_mbox_close(&Context);
    if (check != 0)
    {
#ifdef USE_INOTIFY
      if (monitor_remove_rc == 0)
        mutt_monitor_add(NULL);
#endif
      if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
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

  if (m)
  {
    /* If the `folder-hook` were to call `unmailboxes`, then the Mailbox (`m`)
     * could be deleted, leaving `m` dangling. */
    // TODO: Refactor this function to avoid the need for an observer
    notify_observer_add(m->notify, NT_MAILBOX, 0, mailbox_index_observer, IP & m);
  }
  mutt_folder_hook(buf, m ? m->name : NULL);
  if (m)
  {
    /* `m` is still valid, but we won't need the observer again before the end
     * of the function. */
    notify_observer_remove(m->notify, mailbox_index_observer, IP & m);
  }

  int flags = MUTT_OPEN_NO_FLAGS;
  if (C_ReadOnly || (op == OP_MAIN_CHANGE_FOLDER_READONLY))
    flags = MUTT_READONLY;
#ifdef USE_NOTMUCH
  if (op == OP_MAIN_VFOLDER_FROM_QUERY_READONLY)
    flags = MUTT_READONLY;
#endif

  bool free_m = false;
  if (!m)
  {
    m = mx_path_resolve(buf);
    free_m = true;
  }
  Context = mx_mbox_open(m, flags);
  if (Context)
  {
    menu->current = ci_first_message(Context);
#ifdef USE_INOTIFY
    mutt_monitor_add(NULL);
#endif
  }
  else
  {
    menu->current = 0;
    if (free_m)
      mailbox_free(&m);
  }

  if (((C_Sort & SORT_MASK) == SORT_THREADS) && C_CollapseAll)
    collapse_all(Context, menu, 0);

#ifdef USE_SIDEBAR
  mutt_sb_set_open_mailbox(Context ? Context->mailbox : NULL);
#endif

  mutt_clear_error();
  mutt_mailbox_check(Context ? Context->mailbox : NULL, MUTT_MAILBOX_CHECK_FORCE); /* force the mailbox check after we have changed the folder */
  menu->redraw = REDRAW_FULL;
  OptSearchInvalid = true;

  return 0;
}

/**
 * index_make_entry - Format a menu item for the index list - Implements Menu::menu_make_entry()
 */
void index_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  if (!Context || !Context->mailbox || !menu || (line < 0) ||
      (line >= Context->mailbox->email_max))
    return;

  struct Email *e = mutt_get_virt_email(Context->mailbox, line);
  if (!e)
    return;

  MuttFormatFlags flags = MUTT_FORMAT_ARROWCURSOR | MUTT_FORMAT_INDEX;
  struct MuttThread *tmp = NULL;

  if (((C_Sort & SORT_MASK) == SORT_THREADS) && e->tree)
  {
    flags |= MUTT_FORMAT_TREE; /* display the thread tree */
    if (e->display_subject)
      flags |= MUTT_FORMAT_FORCESUBJ;
    else
    {
      const int reverse = C_Sort & SORT_REVERSE;
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
        if (reverse ? (tmp->message->msgno > edgemsgno) : (tmp->message->msgno < edgemsgno))
        {
          flags |= MUTT_FORMAT_FORCESUBJ;
          break;
        }
        else if (tmp->message->vnum >= 0)
          break;
      }
      if (flags & MUTT_FORMAT_FORCESUBJ)
      {
        for (tmp = e->thread->prev; tmp; tmp = tmp->prev)
        {
          if (!tmp->message)
            continue;

          /* ...but if a previous sibling is available, don't force it */
          if (reverse ? (tmp->message->msgno > edgemsgno) : (tmp->message->msgno < edgemsgno))
            break;
          else if (tmp->message->vnum >= 0)
          {
            flags &= ~MUTT_FORMAT_FORCESUBJ;
            break;
          }
        }
      }
    }
  }

  mutt_make_string_flags(buf, buflen, menu->win_index->cols, NONULL(C_IndexFormat),
                         Context, Context->mailbox, e, flags);
}

/**
 * index_color - Calculate the colour for a line of the index - Implements Menu::menu_color()
 */
int index_color(int line)
{
  if (!Context || !Context->mailbox || (line < 0))
    return 0;

  struct Email *e = mutt_get_virt_email(Context->mailbox, line);
  if (!e)
    return 0;

  if (e->pair)
    return e->pair;

  mutt_set_header_color(Context->mailbox, e);
  return e->pair;
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
  if (!buf)
    return;

  size_t i = 0;
  size_t offset = 0;
  bool found = false;
  size_t chunks = 0;
  size_t len = 0;

  struct StatusSyntax
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
    STAILQ_FOREACH(cl, &Colors->status_list, entries)
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
        mutt_mem_realloc(&syntax, chunks * sizeof(struct StatusSyntax));
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
    mutt_window_addnstr(buf, MIN(len, syntax[0].first));
    attrset(Colors->defs[MT_COLOR_STATUS]);
    if (len <= syntax[0].first)
      goto dsl_finish; /* no more room */

    offset = syntax[0].first;
  }

  for (i = 0; i < chunks; i++)
  {
    /* Highlighted text */
    attrset(syntax[i].color);
    mutt_window_addnstr(buf + offset, MIN(len, syntax[i].last) - offset);
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

    attrset(Colors->defs[MT_COLOR_STATUS]);
    offset = syntax[i].last;
    mutt_window_addnstr(buf + offset, next - offset);

    offset = next;
    if (offset >= len)
      goto dsl_finish; /* no more room */
  }

  attrset(Colors->defs[MT_COLOR_STATUS]);
  if (offset < len)
  {
    /* Text after the last highlight */
    mutt_window_addnstr(buf + offset, len - offset);
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

  if (Context && Context->mailbox && Context->mailbox->emails &&
      !(menu->current >= Context->mailbox->vcount))
  {
    menu_check_recenter(menu);

    if (menu->redraw & REDRAW_INDEX)
    {
      menu_redraw_index(menu);
      menu->redraw |= REDRAW_STATUS;
    }
    else if (menu->redraw & (REDRAW_MOTION_RESYNC | REDRAW_MOTION))
      menu_redraw_motion(menu);
    else if (menu->redraw & REDRAW_CURRENT)
      menu_redraw_current(menu);
  }

  if (menu->redraw & REDRAW_STATUS)
  {
    char buf[1024];
    menu_status_line(buf, sizeof(buf), menu, NONULL(C_StatusFormat));
    mutt_window_move(menu->win_ibar, 0, 0);
    mutt_curses_set_color(MT_COLOR_STATUS);
    mutt_draw_statusline(menu->win_ibar->cols, buf, sizeof(buf));
    mutt_curses_set_color(MT_COLOR_NORMAL);
    menu->redraw &= ~REDRAW_STATUS;
    if (C_TsEnabled && TsSupported)
    {
      menu_status_line(buf, sizeof(buf), menu, NONULL(C_TsStatusFormat));
      mutt_ts_status(buf);
      menu_status_line(buf, sizeof(buf), menu, NONULL(C_TsIconFormat));
      mutt_ts_icon(buf);
    }
  }

  menu->redraw = REDRAW_NO_FLAGS;
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
  char buf[PATH_MAX], helpstr[1024];
  OpenMailboxFlags flags;
  int op = OP_NULL;
  bool done = false; /* controls when to exit the "event" loop */
  bool tag = false;  /* has the tag-prefix command been pressed? */
  int newcount = -1;
  int oldcount = -1;
  int index_hint = 0; /* used to restore cursor position */
  bool do_mailbox_notify = true;
  int close = 0; /* did we OP_QUIT or OP_EXIT out of this menu? */
  int attach_msg = OptAttachMsg;
  bool in_pager = false; /* set when pager redirects a function through the index */

  struct Menu *menu = mutt_menu_new(MENU_MAIN);
  menu->menu_make_entry = index_make_entry;
  menu->menu_color = index_color;
  menu->current = ci_first_message(Context);
  menu->help = mutt_compile_help(
      helpstr, sizeof(helpstr), MENU_MAIN,
#ifdef USE_NNTP
      (Context && Context->mailbox && (Context->mailbox->magic == MUTT_NNTP)) ?
          IndexNewsHelp :
#endif
          IndexHelp);
  menu->menu_custom_redraw = index_custom_redraw;
  mutt_menu_push_current(menu);
  mutt_window_reflow();

  if (!attach_msg)
  {
    /* force the mailbox check after we enter the folder */
    mutt_mailbox_check(Context ? Context->mailbox : NULL, MUTT_MAILBOX_CHECK_FORCE);
  }
#ifdef USE_INOTIFY
  mutt_monitor_add(NULL);
#endif

  if (((C_Sort & SORT_MASK) == SORT_THREADS) && C_CollapseAll)
  {
    collapse_all(Context, menu, 0);
    menu->redraw = REDRAW_FULL;
  }

  while (true)
  {
    /* Clear the tag prefix unless we just started it.  Don't clear
     * the prefix on a timeout (op==-2), but do clear on an abort (op==-1) */
    if (tag && (op != OP_TAG_PREFIX) && (op != OP_TAG_PREFIX_COND) && (op != -2))
      tag = false;

    /* check if we need to resort the index because just about
     * any 'op' below could do mutt_enter_command(), either here or
     * from any new menu launched, and change $sort/$sort_aux */
    if (OptNeedResort && Context && Context->mailbox &&
        (Context->mailbox->msg_count != 0) && (menu->current >= 0))
      resort_index(Context, menu);

    menu->max = (Context && Context->mailbox) ? Context->mailbox->vcount : 0;
    oldcount = (Context && Context->mailbox) ? Context->mailbox->msg_count : 0;

    if (OptRedrawTree && Context && Context->mailbox &&
        (Context->mailbox->msg_count != 0) && ((C_Sort & SORT_MASK) == SORT_THREADS))
    {
      mutt_draw_tree(Context);
      menu->redraw |= REDRAW_STATUS;
      OptRedrawTree = false;
    }

    if (Context)
      Context->menu = menu;

    if (Context && Context->mailbox && !attach_msg)
    {
      /* check for new mail in the mailbox.  If nonzero, then something has
       * changed about the file (either we got new mail or the file was
       * modified underneath us.) */

      struct Email *e_cur = get_cur_email(Context, menu);
      index_hint = e_cur ? e_cur->index : 0;

      int check = mx_mbox_check(Context->mailbox, &index_hint);
      if (check < 0)
      {
        if (!Context->mailbox || (mutt_buffer_is_empty(&Context->mailbox->pathbuf)))
        {
          /* fatal error occurred */
          ctx_free(&Context);
          menu->redraw = REDRAW_FULL;
        }

        OptSearchInvalid = true;
      }
      else if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED) || (check == MUTT_FLAGS))
      {
        /* notify the user of new mail */
        if (check == MUTT_REOPENED)
        {
          mutt_error(
              _("Mailbox was externally modified.  Flags may be wrong."));
        }
        else if (check == MUTT_NEW_MAIL)
        {
          for (size_t i = 0; i < Context->mailbox->msg_count; i++)
          {
            const struct Email *e = Context->mailbox->emails[i];
            if (e && !e->read && !e->old)
            {
              mutt_message(_("New mail in this mailbox"));
              if (C_BeepNew)
                mutt_beep(true);
              if (C_NewMailCommand)
              {
                char cmd[1024];
                menu_status_line(cmd, sizeof(cmd), menu, NONULL(C_NewMailCommand));
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

        if (Context && Context->mailbox)
        {
          bool q = Context->mailbox->quiet;
          Context->mailbox->quiet = true;
          update_index(menu, Context, check, oldcount, index_hint);
          Context->mailbox->quiet = q;
          menu->max = Context->mailbox->vcount;
        }
        else
        {
          menu->max = 0;
        }

        menu->redraw = REDRAW_FULL;

        OptSearchInvalid = true;
      }
    }

    if (!attach_msg)
    {
      /* check for new mail in the incoming folders */
      oldcount = newcount;
      newcount = mutt_mailbox_check(Context ? Context->mailbox : NULL, 0);
      if (newcount != oldcount)
        menu->redraw |= REDRAW_STATUS;
      if (do_mailbox_notify)
      {
        if (mutt_mailbox_notify(Context ? Context->mailbox : NULL))
        {
          menu->redraw |= REDRAW_STATUS;
          if (C_BeepNew)
            mutt_beep(true);
          if (C_NewMailCommand)
          {
            char cmd[1024];
            menu_status_line(cmd, sizeof(cmd), menu, NONULL(C_NewMailCommand));
            if (mutt_system(cmd) != 0)
              mutt_error(_("Error running \"%s\""), cmd);
          }
        }
      }
      else
        do_mailbox_notify = true;
    }

    if (op >= 0)
      mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    if (!in_pager)
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

      if (C_ArrowCursor)
        mutt_window_move(menu->win_index, menu->current - menu->top + menu->offset, 2);
      else if (C_BrailleFriendly)
        mutt_window_move(menu->win_index, menu->current - menu->top + menu->offset, 0);
      else
      {
        mutt_window_move(menu->win_index, menu->current - menu->top + menu->offset,
                         menu->win_index->cols - 1);
      }
      mutt_refresh();

      if (SigWinch)
      {
        SigWinch = 0;
        mutt_resize_screen();
        menu->top = 0; /* so we scroll the right amount */
        /* force a real complete redraw.  clrtobot() doesn't seem to be able
         * to handle every case without this.  */
        clearok(stdscr, true);
        continue;
      }

      op = km_dokey(MENU_MAIN);

      mutt_debug(LL_DEBUG3, "[%d]: Got op %d\n", __LINE__, op);

      /* either user abort or timeout */
      if (op < 0)
      {
        mutt_timeout_hook();
        if (tag)
          mutt_window_clearline(MuttMessageWindow, 0);
        continue;
      }

      mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

      /* special handling for the tag-prefix function */
      if ((op == OP_TAG_PREFIX) || (op == OP_TAG_PREFIX_COND))
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

        if (Context->mailbox->msg_tagged == 0)
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
      else if (C_AutoTag && Context && Context->mailbox &&
               (Context->mailbox->msg_tagged != 0))
        tag = true;

      mutt_clear_error();
    }
    else
    {
      if (menu->current < menu->max)
        menu->oldcurrent = menu->current;
      else
        menu->oldcurrent = -1;

      mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE); /* fallback from the pager */
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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        /* fallthrough */

      case OP_GET_MESSAGE:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_READONLY | CHECK_ATTACH))
          break;
        if (Context->mailbox->magic == MUTT_NNTP)
        {
          if (op == OP_GET_MESSAGE)
          {
            buf[0] = '\0';
            if ((mutt_get_field(_("Enter Message-Id: "), buf, sizeof(buf), 0) != 0) ||
                !buf[0])
            {
              break;
            }
          }
          else
          {
            struct Email *e_cur = get_cur_email(Context, menu);
            if (!e_cur || STAILQ_EMPTY(&e_cur->env->references))
            {
              mutt_error(_("Article has no parent reference"));
              break;
            }
            mutt_str_strfcpy(buf, STAILQ_FIRST(&e_cur->env->references)->data, sizeof(buf));
          }
          if (!Context->mailbox->id_hash)
            Context->mailbox->id_hash = mutt_make_id_hash(Context->mailbox);
          struct Email *e = mutt_hash_find(Context->mailbox->id_hash, buf);
          if (e)
          {
            if (e->vnum != -1)
            {
              menu->current = e->vnum;
              menu->redraw = REDRAW_MOTION_RESYNC;
            }
            else if (e->collapsed)
            {
              mutt_uncollapse_thread(Context, e);
              mutt_set_vnum(Context);
              menu->current = e->vnum;
              menu->redraw = REDRAW_MOTION_RESYNC;
            }
            else
              mutt_error(_("Message is not visible in limited view"));
          }
          else
          {
            mutt_message(_("Fetching %s from server..."), buf);
            int rc = nntp_check_msgid(Context->mailbox, buf);
            if (rc == 0)
            {
              e = Context->mailbox->emails[Context->mailbox->msg_count - 1];
              mutt_sort_headers(Context, false);
              menu->current = e->vnum;
              menu->redraw = REDRAW_FULL;
            }
            else if (rc > 0)
              mutt_error(_("Article %s not found on the server"), buf);
          }
        }
        break;

      case OP_GET_CHILDREN:
      case OP_RECONSTRUCT_THREAD:
      {
        if (!prereq(Context, menu,
                    CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY | CHECK_ATTACH))
        {
          break;
        }
        if (Context->mailbox->magic != MUTT_NNTP)
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        int oldmsgcount = Context->mailbox->msg_count;
        int oldindex = e_cur->index;
        int rc = 0;

        if (!e_cur->env->message_id)
        {
          mutt_error(_("No Message-Id. Unable to perform operation."));
          break;
        }

        mutt_message(_("Fetching message headers..."));
        if (!Context->mailbox->id_hash)
          Context->mailbox->id_hash = mutt_make_id_hash(Context->mailbox);
        mutt_str_strfcpy(buf, e_cur->env->message_id, sizeof(buf));

        /* trying to find msgid of the root message */
        if (op == OP_RECONSTRUCT_THREAD)
        {
          struct ListNode *ref = NULL;
          STAILQ_FOREACH(ref, &e_cur->env->references, entries)
          {
            if (!mutt_hash_find(Context->mailbox->id_hash, ref->data))
            {
              rc = nntp_check_msgid(Context->mailbox, ref->data);
              if (rc < 0)
                break;
            }

            /* the last msgid in References is the root message */
            if (!STAILQ_NEXT(ref, entries))
              mutt_str_strfcpy(buf, ref->data, sizeof(buf));
          }
        }

        /* fetching all child messages */
        if (rc >= 0)
          rc = nntp_check_children(Context->mailbox, buf);

        /* at least one message has been loaded */
        if (Context->mailbox->msg_count > oldmsgcount)
        {
          struct Email *e_oldcur = get_cur_email(Context, menu);
          bool quiet = Context->mailbox->quiet;

          if (rc < 0)
            Context->mailbox->quiet = true;
          mutt_sort_headers(Context, (op == OP_RECONSTRUCT_THREAD));
          Context->mailbox->quiet = quiet;

          /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
            * update the index */
          if (in_pager)
          {
            menu->current = e_oldcur->vnum;
            menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
            op = OP_DISPLAY_MESSAGE;
            continue;
          }

          /* if the root message was retrieved, move to it */
          struct Email *e = mutt_hash_find(Context->mailbox->id_hash, buf);
          if (e)
            menu->current = e->vnum;
          else
          {
            /* try to restore old position */
            for (int i = 0; i < Context->mailbox->msg_count; i++)
            {
              e = Context->mailbox->emails[i];
              if (!e)
                break;
              if (e->index == oldindex)
              {
                menu->current = e->vnum;
                /* as an added courtesy, recenter the menu
                  * with the current entry at the middle of the screen */
                menu_check_recenter(menu);
                menu_current_middle(menu);
              }
            }
          }
          menu->redraw = REDRAW_FULL;
        }
        else if (rc >= 0)
        {
          mutt_error(_("No deleted messages found in the thread"));
          /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
            * update the index */
          if (in_pager)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
        }
        break;
      }
#endif

      case OP_JUMP:
      {
        int msg_num = 0;
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (isdigit(LastKey))
          mutt_unget_event(LastKey, 0);
        buf[0] = '\0';
        if ((mutt_get_field(_("Jump to message: "), buf, sizeof(buf), 0) != 0) ||
            (buf[0] == '\0'))
        {
          mutt_error(_("Nothing to do"));
        }
        else if (mutt_str_atoi(buf, &msg_num) < 0)
          mutt_error(_("Argument must be a message number"));
        else if ((msg_num < 1) || (msg_num > Context->mailbox->msg_count))
          mutt_error(_("Invalid message number"));
        else if (!message_is_visible(Context, Context->mailbox->emails[msg_num - 1]))
          mutt_error(_("That message is not visible"));
        else
        {
          struct Email *e = Context->mailbox->emails[msg_num - 1];

          if (mutt_messages_in_thread(Context->mailbox, e, 1) > 1)
          {
            mutt_uncollapse_thread(Context, e);
            mutt_set_vnum(Context);
          }
          menu->current = e->vnum;
        }

        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_FULL;

        break;
      }

        /* --------------------------------------------------------------------
         * 'index' specific commands
         */

      case OP_MAIN_DELETE_PATTERN:
        if (!prereq(Context, menu,
                    CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY | CHECK_ATTACH))
        {
          break;
        }
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
            delete zero, 1, 12, ... messages. So in English we use
            "messages". Your language might have other means to express this.  */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't delete messages")))
          break;

        mutt_pattern_func(MUTT_DELETE, _("Delete messages matching: "));
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

#ifdef USE_POP
      case OP_MAIN_FETCH_MAIL:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
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
        mutt_help(MENU_MAIN, MuttIndexWindow->cols);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIN_SHOW_LIMIT:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        if (!Context->pattern)
          mutt_message(_("No limit pattern is in effect"));
        else
        {
          char buf2[256];
          /* L10N: ask for a limit to apply */
          snprintf(buf2, sizeof(buf2), _("Limit: %s"), Context->pattern);
          mutt_message("%s", buf2);
        }
        break;

      case OP_LIMIT_CURRENT_THREAD:
      case OP_MAIN_LIMIT:
      case OP_TOGGLE_READ:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        menu->oldcurrent = e_cur ? e_cur->index : -1;
        if (op == OP_TOGGLE_READ)
        {
          char buf2[1024];

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

        if (((op == OP_LIMIT_CURRENT_THREAD) && mutt_limit_current_thread(e_cur)) ||
            (op == OP_TOGGLE_READ) ||
            ((op == OP_MAIN_LIMIT) &&
             (mutt_pattern_func(MUTT_LIMIT,
                                _("Limit to messages matching: ")) == 0)))
        {
          if (menu->oldcurrent >= 0)
          {
            /* try to find what used to be the current message */
            menu->current = -1;
            for (size_t i = 0; i < Context->mailbox->vcount; i++)
            {
              struct Email *e = mutt_get_virt_email(Context->mailbox, i);
              if (!e)
                continue;
              if (e->index == menu->oldcurrent)
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
          if ((Context->mailbox->msg_count != 0) && ((C_Sort & SORT_MASK) == SORT_THREADS))
          {
            if (C_CollapseAll)
              collapse_all(Context, menu, 0);
            mutt_draw_tree(Context);
          }
          menu->redraw = REDRAW_FULL;
        }
        if (Context->pattern)
          mutt_message(_("To view all messages, limit to \"all\""));
        break;
      }

      case OP_QUIT:
        close = op;
        if (attach_msg)
        {
          done = true;
          break;
        }

        if (query_quadoption(C_Quit, _("Quit NeoMutt?")) == MUTT_YES)
        {
          int check;

          oldcount = (Context && Context->mailbox) ? Context->mailbox->msg_count : 0;

          mutt_startup_shutdown_hook(MUTT_SHUTDOWN_HOOK);
          notify_send(NeoMutt->notify, NT_GLOBAL, NT_GLOBAL_SHUTDOWN, 0);

          if (!Context || ((check = mx_mbox_close(&Context)) == 0))
            done = true;
          else
          {
            if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
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
          if (Context && Context->mailbox && (Context->mailbox->msg_count != 0))
          {
            resort_index(Context, menu);
            OptSearchInvalid = true;
          }
          if (in_pager)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_TAG:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (tag && !C_AutoTag)
        {
          struct Mailbox *m = Context->mailbox;
          for (size_t i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (message_is_visible(Context, e))
              mutt_set_flag(m, e, MUTT_TAG, false);
          }
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          struct Email *e_cur = get_cur_email(Context, menu);
          if (!e_cur)
            break;
          mutt_set_flag(Context->mailbox, e_cur, MUTT_TAG, !e_cur->tagged);

          Context->last_tag = e_cur->tagged ?
                                  e_cur :
                                  (((Context->last_tag == e_cur) && !e_cur->tagged) ?
                                       NULL :
                                       Context->last_tag);

          menu->redraw |= REDRAW_STATUS;
          if (C_Resolve && (menu->current < Context->mailbox->vcount - 1))
          {
            menu->current++;
            menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        break;
      }

      case OP_MAIN_TAG_PATTERN:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        mutt_pattern_func(MUTT_TAG, _("Tag messages matching: "));
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_MAIN_UNDELETE_PATTERN:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
            undelete zero, 1, 12, ... messages. So in English we use
            "messages". Your language might have other means to express this. */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't undelete messages")))
          break;

        if (mutt_pattern_func(MUTT_UNDELETE,
                              _("Undelete messages matching: ")) == 0)
        {
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;

      case OP_MAIN_UNTAG_PATTERN:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (mutt_pattern_func(MUTT_UNTAG, _("Untag messages matching: ")) == 0)
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_COMPOSE_TO_SENDER:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);
        ci_send_message(SEND_TO_SENDER, NULL, NULL, Context, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      /* --------------------------------------------------------------------
       * The following operations can be performed inside of the pager.
       */

#ifdef USE_IMAP
      case OP_MAIN_IMAP_FETCH:
        if (Context && Context->mailbox && (Context->mailbox->magic == MUTT_IMAP))
          imap_check_mailbox(Context->mailbox, true);
        break;

      case OP_MAIN_IMAP_LOGOUT_ALL:
        if (Context && Context->mailbox && (Context->mailbox->magic == MUTT_IMAP))
        {
          int check = mx_mbox_close(&Context);
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
        if (!Context || !Context->mailbox || (Context->mailbox->msg_count == 0))
          break;

        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_READONLY))
          break;
        {
          int ovc = Context->mailbox->vcount;
          int oc = Context->mailbox->msg_count;
          struct Email *e = NULL;

          /* don't attempt to move the cursor if there are no visible messages in the current limit */
          if (menu->current < Context->mailbox->vcount)
          {
            /* threads may be reordered, so figure out what header the cursor
             * should be on. */
            int newidx = menu->current;
            struct Email *e_cur = get_cur_email(Context, menu);
            if (!e_cur)
              break;
            if (e_cur->deleted)
              newidx = ci_next_undeleted(Context, menu->current);
            if (newidx < 0)
              newidx = ci_previous_undeleted(Context, menu->current);
            if (newidx >= 0)
              e = mutt_get_virt_email(Context->mailbox, newidx);
          }

          int check = mx_mbox_sync(Context->mailbox, &index_hint);
          if (check == 0)
          {
            if (e && (Context->mailbox->vcount != ovc))
            {
              for (size_t i = 0; i < Context->mailbox->vcount; i++)
              {
                struct Email *e2 = mutt_get_virt_email(Context->mailbox, i);
                if (e2 == e)
                {
                  menu->current = i;
                  break;
                }
              }
            }
            OptSearchInvalid = true;
          }
          else if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
            update_index(menu, Context, check, oc, index_hint);

          /* do a sanity check even if mx_mbox_sync failed.  */

          if ((menu->current < 0) || (Context && Context->mailbox &&
                                      (menu->current >= Context->mailbox->vcount)))
          {
            menu->current = ci_first_message(Context);
          }
        }

        /* check for a fatal error, or all messages deleted */
        if (Context && Context->mailbox &&
            mutt_buffer_is_empty(&Context->mailbox->pathbuf))
          ctx_free(&Context);

        /* if we were in the pager, redisplay the message */
        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIN_QUASI_DELETE:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (tag)
        {
          struct Mailbox *m = Context->mailbox;
          for (size_t i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (message_is_tagged(Context, e))
            {
              e->quasi_deleted = true;
              m->changed = true;
            }
          }
        }
        else
        {
          struct Email *e_cur = get_cur_email(Context, menu);
          if (!e_cur)
            break;
          e_cur->quasi_deleted = true;
          Context->mailbox->changed = true;
        }
        break;

#ifdef USE_NOTMUCH
      case OP_MAIN_ENTIRE_THREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (Context->mailbox->magic != MUTT_NOTMUCH)
        {
          if ((Context->mailbox->magic != MUTT_MH && Context->mailbox->magic != MUTT_MAILDIR) ||
              (!e_cur || !e_cur->env || !e_cur->env->message_id))
          {
            mutt_message(_("No virtual folder and no Message-Id, aborting"));
            break;
          } // no virtual folder, but we have message-id, reconstruct thread on-the-fly
          strncpy(buf, "id:", sizeof(buf));
          int msg_id_offset = 0;
          if ((e_cur->env->message_id)[0] == '<')
            msg_id_offset = 1;
          mutt_str_strcat(buf, sizeof(buf), (e_cur->env->message_id) + msg_id_offset);
          if (buf[strlen(buf) - 1] == '>')
            buf[strlen(buf) - 1] = '\0';
          if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          {
            mutt_message(_("Failed to create query, aborting"));
            break;
          }

          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint, NULL);

          // If notmuch doesn't contain the message, we're left in an empty
          // vfolder. No messages are found, but nm_read_entire_thread assumes
          // a valid message-id and will throw a segfault.
          //
          // To prevent that, stay in the empty vfolder and print an error.
          if (Context->mailbox->msg_count == 0)
          {
            mutt_error(_("failed to find message in notmuch database. try "
                         "running 'notmuch new'."));
            break;
          }
        }
        oldcount = Context->mailbox->msg_count;
        struct Email *e_oldcur = e_cur;
        if (nm_read_entire_thread(Context->mailbox, e_cur) < 0)
        {
          mutt_message(_("Failed to read thread, aborting"));
          break;
        }
        if (oldcount < Context->mailbox->msg_count)
        {
          /* nm_read_entire_thread() triggers mutt_sort_headers() if necessary */
          menu->current = e_oldcur->vnum;
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;

          if (e_oldcur->collapsed || Context->collapsed)
          {
            menu->current = mutt_uncollapse_thread(Context, e_cur);
            mutt_set_vnum(Context);
          }
        }
        if (in_pager)
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
        if (!Context || !Context->mailbox)
          break;
        struct Mailbox *m = Context->mailbox;
        if (!mx_tags_is_supported(m))
        {
          mutt_message(_("Folder doesn't support tagging, aborting"));
          break;
        }
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        char *tags = NULL;
        if (!tag)
          tags = driver_tags_get_with_hidden(&e_cur->tags);
        int rc = mx_tags_edit(m, tags, buf, sizeof(buf));
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

          if (!m->quiet)
          {
            mutt_progress_init(&progress, _("Update tags..."),
                               MUTT_PROGRESS_WRITE, m->msg_tagged);
          }

#ifdef USE_NOTMUCH
          if (m->magic == MUTT_NOTMUCH)
            nm_db_longrun_init(m, true);
#endif
          for (int px = 0, i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (!message_is_tagged(Context, e))
              continue;

            if (!m->quiet)
              mutt_progress_update(&progress, ++px, -1);
            mx_tags_commit(m, e, buf);
            if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
            {
              bool still_queried = false;
#ifdef USE_NOTMUCH
              if (m->magic == MUTT_NOTMUCH)
                still_queried = nm_message_is_still_queried(m, e);
#endif
              e->quasi_deleted = !still_queried;
              m->changed = true;
            }
          }
#ifdef USE_NOTMUCH
          if (m->magic == MUTT_NOTMUCH)
            nm_db_longrun_done(m);
#endif
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (mx_tags_commit(m, e_cur, buf))
          {
            mutt_message(_("Failed to modify tags, aborting"));
            break;
          }
          if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
          {
            bool still_queried = false;
#ifdef USE_NOTMUCH
            if (m->magic == MUTT_NOTMUCH)
              still_queried = nm_message_is_still_queried(m, e_cur);
#endif
            e_cur->quasi_deleted = !still_queried;
            m->changed = true;
          }
          if (in_pager)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw = REDRAW_CURRENT;
            }
            else
              menu->redraw = REDRAW_MOTION_RESYNC;
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
      case OP_MAIN_VFOLDER_FROM_QUERY_READONLY:
        buf[0] = '\0';
        if ((mutt_get_field("Query: ", buf, sizeof(buf), MUTT_NM_QUERY) != 0) || !buf[0])
        {
          mutt_message(_("No query, aborting"));
          break;
        }
        if (!nm_uri_from_query(NULL, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint, NULL);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_BACKWARD:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        mutt_debug(LL_DEBUG2, "OP_MAIN_WINDOWED_VFOLDER_BACKWARD\n");
        if (C_NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled"));
          break;
        }
        if (!C_NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded"));
          break;
        }
        nm_query_window_backward();
        mutt_str_strfcpy(buf, C_NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint, NULL);
        break;

      case OP_MAIN_WINDOWED_VFOLDER_FORWARD:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        if (C_NmQueryWindowDuration <= 0)
        {
          mutt_message(_("Windowed queries disabled"));
          break;
        }
        if (!C_NmQueryWindowCurrentSearch)
        {
          mutt_message(_("No notmuch vfolder currently loaded"));
          break;
        }
        nm_query_window_forward();
        mutt_str_strfcpy(buf, C_NmQueryWindowCurrentSearch, sizeof(buf));
        if (!nm_uri_from_query(Context->mailbox, buf, sizeof(buf)))
          mutt_message(_("Failed to create query, aborting"));
        else
        {
          mutt_debug(LL_DEBUG2, "nm: + windowed query (%s)\n", buf);
          main_change_folder(menu, op, NULL, buf, sizeof(buf), &oldcount, &index_hint, NULL);
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
#endif
      {
        bool pager_return = true; /* return to display message in pager */

        struct Buffer *folderbuf = mutt_buffer_pool_get();
        mutt_buffer_alloc(folderbuf, PATH_MAX);
        struct Mailbox *m = NULL;
        char *cp = NULL;
#ifdef USE_NNTP
        OptNews = false;
#endif
        if (attach_msg || C_ReadOnly ||
#ifdef USE_NNTP
            (op == OP_MAIN_CHANGE_GROUP_READONLY) ||
#endif
            (op == OP_MAIN_CHANGE_FOLDER_READONLY))
        {
          flags = MUTT_READONLY;
        }
        else
          flags = MUTT_OPEN_NO_FLAGS;

        if (flags)
          cp = _("Open mailbox in read-only mode");
        else
          cp = _("Open mailbox");

        if ((op == OP_MAIN_NEXT_UNREAD_MAILBOX) && Context && Context->mailbox &&
            !mutt_buffer_is_empty(&Context->mailbox->pathbuf))
        {
          mutt_buffer_strcpy(folderbuf, mailbox_path(Context->mailbox));
          mutt_buffer_pretty_mailbox(folderbuf);
          mutt_mailbox_next_buffer(Context ? Context->mailbox : NULL, folderbuf);
          if (mutt_buffer_is_empty(folderbuf))
          {
            mutt_error(_("No mailboxes have new mail"));
            goto changefoldercleanup;
          }
        }
#ifdef USE_SIDEBAR
        else if (op == OP_SIDEBAR_OPEN)
        {
          m = mutt_sb_get_highlight();
          if (!m)
            goto changefoldercleanup;
          mutt_buffer_strcpy(folderbuf, mailbox_path(m));

          /* Mark the selected dir for the neomutt browser */
          mutt_browser_select_dir(mailbox_path(m));
        }
#endif
        else
        {
          if (C_ChangeFolderNext && Context && Context->mailbox &&
              !mutt_buffer_is_empty(&Context->mailbox->pathbuf))
          {
            mutt_buffer_strcpy(folderbuf, mailbox_path(Context->mailbox));
            mutt_buffer_pretty_mailbox(folderbuf);
          }
#ifdef USE_NNTP
          if ((op == OP_MAIN_CHANGE_GROUP) || (op == OP_MAIN_CHANGE_GROUP_READONLY))
          {
            OptNews = true;
            CurrentNewsSrv = nntp_select_server(Context ? Context->mailbox : NULL,
                                                C_NewsServer, false);
            if (!CurrentNewsSrv)
              goto changefoldercleanup;
            if (flags)
              cp = _("Open newsgroup in read-only mode");
            else
              cp = _("Open newsgroup");
            nntp_mailbox(Context ? Context->mailbox : NULL, folderbuf->data,
                         folderbuf->dsize);
          }
          else
#endif
          {
            /* By default, fill buf with the next mailbox that contains unread
             * mail */
            mutt_mailbox_next_buffer(Context ? Context->mailbox : NULL, folderbuf);
          }

          if (mutt_buffer_enter_fname(cp, folderbuf, true) == -1)
          {
            goto changefoldercleanup;
          }

          /* Selected directory is okay, let's save it. */
          mutt_browser_select_dir(mutt_b2s(folderbuf));

          if (mutt_buffer_is_empty(folderbuf))
          {
            mutt_window_clearline(MuttMessageWindow, 0);
            goto changefoldercleanup;
          }
        }

        if (!m)
          m = mx_mbox_find2(mutt_b2s(folderbuf));

        main_change_folder(menu, op, m, folderbuf->data, folderbuf->dsize,
                           &oldcount, &index_hint, &pager_return);
#ifdef USE_NNTP
        /* mutt_mailbox_check() must be done with mail-reader mode! */
        menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_MAIN,
                                       (Context && Context->mailbox &&
                                        (Context->mailbox->magic == MUTT_NNTP)) ?
                                           IndexNewsHelp :
                                           IndexHelp);
#endif
        mutt_buffer_expand_path(folderbuf);
#ifdef USE_SIDEBAR
        mutt_sb_set_open_mailbox(Context ? Context->mailbox : NULL);
#endif
        goto changefoldercleanup;

      changefoldercleanup:
        mutt_buffer_pool_release(&folderbuf);
        if (in_pager && pager_return)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;
      }

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        /* toggle the weeding of headers so that a user can press the key
         * again while reading the message.  */
        if (op == OP_DISPLAY_HEADERS)
          bool_str_toggle(Config, "weed", NULL);

        OptNeedResort = false;

        if (((C_Sort & SORT_MASK) == SORT_THREADS) && e_cur->collapsed)
        {
          mutt_uncollapse_thread(Context, e_cur);
          mutt_set_vnum(Context);
          if (C_UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, e_cur);
        }

        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, e_cur, tag);
          mutt_check_traditional_pgp(&el, &menu->redraw);
          emaillist_clear(&el);
        }
        e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        int hint = e_cur->index;

        op = mutt_display_message(MuttIndexWindow, Context->mailbox, e_cur);
        if (op < 0)
        {
          OptNeedResort = false;
          break;
        }

        /* This is used to redirect a single operation back here afterwards.  If
         * mutt_display_message() returns 0, then this flag and pager state will
         * be cleaned up after this switch statement. */
        in_pager = true;
        menu->oldcurrent = menu->current;
        if (Context && Context->mailbox)
          update_index(menu, Context, MUTT_NEW_MAIL, Context->mailbox->msg_count, hint);
        continue;
      }

      case OP_EXIT:
        close = op;
        if ((!in_pager) && attach_msg)
        {
          done = true;
          break;
        }

        if ((!in_pager) &&
            (query_quadoption(C_Quit, _("Exit NeoMutt without saving?")) == MUTT_YES))
        {
          if (Context)
          {
            mx_fastclose_mailbox(Context->mailbox);
            ctx_free(&Context);
          }
          done = true;
        }
        break;

      case OP_MAIN_BREAK_THREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_WRITE, _("Can't break thread")))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled"));
        else if (!STAILQ_EMPTY(&e_cur->env->in_reply_to) ||
                 !STAILQ_EMPTY(&e_cur->env->references))
        {
          {
            struct Email *e_oldcur = e_cur;

            mutt_break_thread(e_cur);
            mutt_sort_headers(Context, true);
            menu->current = e_oldcur->vnum;
          }

          Context->mailbox->changed = true;
          mutt_message(_("Thread broken"));

          if (in_pager)
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
              _("Thread can't be broken, message is not part of a thread"));
        }
        break;
      }

      case OP_MAIN_LINK_THREADS:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_WRITE, _("Can't link threads")))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled"));
        else if (!e_cur->env->message_id)
          mutt_error(_("No Message-ID: header available to link thread"));
        else if (!tag && (!Context->last_tag || !Context->last_tag->tagged))
          mutt_error(_("First, please tag a message to be linked here"));
        else
        {
          struct Email *e_oldcur = e_cur;
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, Context->last_tag, tag);

          if (mutt_link_threads(e_cur, &el, Context->mailbox))
          {
            mutt_sort_headers(Context, true);
            menu->current = e_oldcur->vnum;

            Context->mailbox->changed = true;
            mutt_message(_("Threads linked"));
          }
          else
            mutt_error(_("No thread linked"));

          emaillist_clear(&el);
        }

        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;

        break;
      }

      case OP_EDIT_TYPE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        mutt_edit_content_type(e_cur, e_cur->content, NULL);
        /* if we were in the pager, redisplay the message */
        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_CURRENT;
        break;
      }

      case OP_MAIN_NEXT_UNDELETED:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (menu->current >= (Context->mailbox->vcount - 1))
        {
          if (!in_pager)
            mutt_message(_("You are on the last message"));
          break;
        }
        menu->current = ci_next_undeleted(Context, menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (!in_pager)
            mutt_error(_("No undeleted messages"));
        }
        else if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_NEXT_ENTRY:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (menu->current >= (Context->mailbox->vcount - 1))
        {
          if (!in_pager)
            mutt_message(_("You are on the last message"));
          break;
        }
        menu->current++;
        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_MAIN_PREV_UNDELETED:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (menu->current < 1)
        {
          mutt_message(_("You are on the first message"));
          break;
        }
        menu->current = ci_previous_undeleted(Context, menu->current);
        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if (!in_pager)
            mutt_error(_("No undeleted messages"));
        }
        else if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;

      case OP_PREV_ENTRY:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (menu->current < 1)
        {
          if (!in_pager)
            mutt_message(_("You are on the first message"));
          break;
        }
        menu->current--;
        if (in_pager)
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
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);

        const bool delete_original =
            (op == OP_SAVE) || (op == OP_DECODE_SAVE) || (op == OP_DECRYPT_SAVE);
        const bool decode = (op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY);
        const bool decrypt = (op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY);

        if ((mutt_save_message(Context->mailbox, &el, delete_original, decode, decrypt) == 0) &&
            delete_original)
        {
          menu->redraw |= REDRAW_STATUS;
          if (tag)
            menu->redraw |= REDRAW_INDEX;
          else if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        emaillist_clear(&el);
        break;
      }

      case OP_MAIN_NEXT_NEW:
      case OP_MAIN_NEXT_UNREAD:
      case OP_MAIN_PREV_NEW:
      case OP_MAIN_PREV_UNREAD:
      case OP_MAIN_NEXT_NEW_THEN_UNREAD:
      case OP_MAIN_PREV_NEW_THEN_UNREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;

        int first_unread = -1;
        int first_new = -1;

        const int saved_current = menu->current;
        int cur = menu->current;
        menu->current = -1;
        for (size_t i = 0; i != Context->mailbox->vcount; i++)
        {
          if ((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_NEXT_UNREAD) ||
              (op == OP_MAIN_NEXT_NEW_THEN_UNREAD))
          {
            cur++;
            if (cur > (Context->mailbox->vcount - 1))
            {
              cur = 0;
            }
          }
          else
          {
            cur--;
            if (cur < 0)
            {
              cur = Context->mailbox->vcount - 1;
            }
          }

          struct Email *e = mutt_get_virt_email(Context->mailbox, cur);
          if (!e)
            break;
          if (e->collapsed && ((C_Sort & SORT_MASK) == SORT_THREADS))
          {
            if ((UNREAD(e) != 0) && (first_unread == -1))
              first_unread = cur;
            if ((UNREAD(e) == 1) && (first_new == -1))
              first_new = cur;
          }
          else if (!e->deleted && !e->read)
          {
            if (first_unread == -1)
              first_unread = cur;
            if (!e->old && (first_new == -1))
              first_new = cur;
          }

          if (((op == OP_MAIN_NEXT_UNREAD) || (op == OP_MAIN_PREV_UNREAD)) &&
              (first_unread != -1))
            break;
          if (((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_PREV_NEW) ||
               (op == OP_MAIN_NEXT_NEW_THEN_UNREAD) || (op == OP_MAIN_PREV_NEW_THEN_UNREAD)) &&
              (first_new != -1))
          {
            break;
          }
        }
        if (((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_PREV_NEW) ||
             (op == OP_MAIN_NEXT_NEW_THEN_UNREAD) || (op == OP_MAIN_PREV_NEW_THEN_UNREAD)) &&
            (first_new != -1))
        {
          menu->current = first_new;
        }
        else if (((op == OP_MAIN_NEXT_UNREAD) || (op == OP_MAIN_PREV_UNREAD) ||
                  (op == OP_MAIN_NEXT_NEW_THEN_UNREAD) || (op == OP_MAIN_PREV_NEW_THEN_UNREAD)) &&
                 (first_unread != -1))
        {
          menu->current = first_unread;
        }

        if (menu->current == -1)
        {
          menu->current = menu->oldcurrent;
          if ((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_PREV_NEW))
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

        if ((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_NEXT_UNREAD) ||
            (op == OP_MAIN_NEXT_NEW_THEN_UNREAD))
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

        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;
      }
      case OP_FLAG_MESSAGE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_WRITE, _("Can't flag message")))
          break;

        struct Mailbox *m = Context->mailbox;
        if (tag)
        {
          for (size_t i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (message_is_tagged(Context, e))
              mutt_set_flag(m, e, MUTT_FLAG, !e->flagged);
          }

          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          struct Email *e_cur = get_cur_email(Context, menu);
          if (!e_cur)
            break;
          mutt_set_flag(m, e_cur, MUTT_FLAG, !e_cur->flagged);
          if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;
      }

      case OP_TOGGLE_NEW:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_SEEN, _("Can't toggle new")))
          break;

        struct Mailbox *m = Context->mailbox;
        if (tag)
        {
          for (size_t i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (!message_is_tagged(Context, e))
              continue;

            if (e->read || e->old)
              mutt_set_flag(m, e, MUTT_NEW, true);
            else
              mutt_set_flag(m, e, MUTT_READ, true);
          }
          menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          struct Email *e_cur = get_cur_email(Context, menu);
          if (!e_cur)
            break;
          if (e_cur->read || e_cur->old)
            mutt_set_flag(m, e_cur, MUTT_NEW, true);
          else
            mutt_set_flag(m, e_cur, MUTT_READ, true);

          if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
          menu->redraw |= REDRAW_STATUS;
        }
        break;
      }

      case OP_TOGGLE_WRITE:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        if (mx_toggle_write(Context->mailbox) == 0)
        {
          if (in_pager)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
          else
            menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_MAIN_NEXT_THREAD:
      case OP_MAIN_NEXT_SUBTHREAD:
      case OP_MAIN_PREV_THREAD:
      case OP_MAIN_PREV_SUBTHREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        switch (op)
        {
          case OP_MAIN_NEXT_THREAD:
            menu->current = mutt_next_thread(e_cur);
            break;

          case OP_MAIN_NEXT_SUBTHREAD:
            menu->current = mutt_next_subthread(e_cur);
            break;

          case OP_MAIN_PREV_THREAD:
            menu->current = mutt_previous_thread(e_cur);
            break;

          case OP_MAIN_PREV_SUBTHREAD:
            menu->current = mutt_previous_subthread(e_cur);
            break;
        }

        if (menu->current < 0)
        {
          menu->current = menu->oldcurrent;
          if ((op == OP_MAIN_NEXT_THREAD) || (op == OP_MAIN_NEXT_SUBTHREAD))
            mutt_error(_("No more threads"));
          else
            mutt_error(_("You are on the first thread"));
        }
        else if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;
      }

      case OP_MAIN_ROOT_MESSAGE:
      case OP_MAIN_PARENT_MESSAGE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        menu->current = mutt_parent_message(Context, e_cur, op == OP_MAIN_ROOT_MESSAGE);
        if (menu->current < 0)
        {
          menu->current = menu->oldcurrent;
        }
        else if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
        break;
      }

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* check_acl(MUTT_ACL_WRITE); */

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);

        if (mutt_change_flag(Context->mailbox, &el, (op == OP_MAIN_SET_FLAG)) == 0)
        {
          menu->redraw |= REDRAW_STATUS;
          if (tag)
            menu->redraw |= REDRAW_INDEX;
          else if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        emaillist_clear(&el);
        break;
      }

      case OP_MAIN_COLLAPSE_THREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error(_("Threading is not enabled"));
          break;
        }

        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        if (e_cur->collapsed)
        {
          menu->current = mutt_uncollapse_thread(Context, e_cur);
          mutt_set_vnum(Context);
          if (C_UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, e_cur);
        }
        else if (CAN_COLLAPSE(e_cur))
        {
          menu->current = mutt_collapse_thread(Context, e_cur);
          mutt_set_vnum(Context);
        }
        else
        {
          mutt_error(_("Thread contains unread or flagged messages"));
          break;
        }

        menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

        break;
      }

      case OP_MAIN_COLLAPSE_ALL:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error(_("Threading is not enabled"));
          break;
        }
        collapse_all(Context, menu, 1);
        break;

        /* --------------------------------------------------------------------
         * These functions are invoked directly from the internal-pager
         */

      case OP_BOUNCE_MESSAGE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        ci_bounce_message(Context->mailbox, &el);
        emaillist_clear(&el);
        break;
      }

      case OP_CREATE_ALIAS:
      {
        struct Email *e_cur = get_cur_email(Context, menu);

        mutt_alias_create(e_cur ? e_cur->env : NULL, NULL);
        menu->redraw |= REDRAW_CURRENT;
        break;
      }

      case OP_QUERY:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        mutt_query_menu(NULL, 0);
        break;

      case OP_PURGE_MESSAGE:
      case OP_DELETE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't delete message")))
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);

        mutt_emails_set_flag(Context->mailbox, &el, MUTT_DELETE, 1);
        mutt_emails_set_flag(Context->mailbox, &el, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
        if (C_DeleteUntag)
          mutt_emails_set_flag(Context->mailbox, &el, MUTT_TAG, 0);
        emaillist_clear(&el);

        if (tag)
        {
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          if (C_Resolve)
          {
            menu->current = ci_next_undeleted(Context, menu->current);
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
              menu->redraw |= REDRAW_CURRENT;
            }
            else if (in_pager)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
            else
              menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }
        menu->redraw |= REDRAW_STATUS;
        break;
      }

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           delete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this. */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't delete messages")))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        int subthread = (op == OP_DELETE_SUBTHREAD);
        int rc = mutt_thread_set_flag(e_cur, MUTT_DELETE, true, subthread);
        if (rc == -1)
          break;
        if (op == OP_PURGE_THREAD)
        {
          rc = mutt_thread_set_flag(e_cur, MUTT_PURGE, true, subthread);
          if (rc == -1)
            break;
        }

        if (C_DeleteUntag)
          mutt_thread_set_flag(e_cur, MUTT_TAG, false, subthread);
        if (C_Resolve)
        {
          menu->current = ci_next_undeleted(Context, menu->current);
          if (menu->current == -1)
            menu->current = menu->oldcurrent;
        }
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;
      }

#ifdef USE_NNTP
      case OP_CATCHUP:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_READONLY | CHECK_ATTACH))
          break;
        if (Context && (Context->mailbox->magic == MUTT_NNTP))
        {
          struct NntpMboxData *mdata = Context->mailbox->mdata;
          if (mutt_newsgroup_catchup(Context->mailbox, mdata->adata, mdata->group))
            menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
#endif

      case OP_DISPLAY_ADDRESS:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        mutt_display_address(e_cur->env);
        break;
      }

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        if (Context)
          mutt_check_rescore(Context->mailbox);
        break;

      case OP_EDIT_OR_VIEW_RAW_MESSAGE:
      case OP_EDIT_RAW_MESSAGE:
      case OP_VIEW_RAW_MESSAGE:
      {
        /* TODO split this into 3 cases? */
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        bool edit;
        if (op == OP_EDIT_RAW_MESSAGE)
        {
          if (!prereq(Context, menu, CHECK_READONLY))
            break;
          /* L10N: CHECK_ACL */
          if (!check_acl(Context, MUTT_ACL_INSERT, _("Can't edit message")))
            break;
          edit = true;
        }
        else if (op == OP_EDIT_OR_VIEW_RAW_MESSAGE)
          edit = !Context->mailbox->readonly && (Context->mailbox->rights & MUTT_ACL_INSERT);
        else
          edit = false;

        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, e_cur, tag);
          mutt_check_traditional_pgp(&el, &menu->redraw);
          emaillist_clear(&el);
        }
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        mutt_ev_message(Context->mailbox, &el, edit ? EVM_EDIT : EVM_VIEW);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;

        break;
      }

      case OP_FORWARD_MESSAGE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        ci_send_message(SEND_FORWARD, NULL, NULL, Context, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_GROUP_REPLY:
      case OP_GROUP_CHAT_REPLY:
      {
        SendFlags replyflags = SEND_REPLY;
        if (op == OP_GROUP_REPLY)
          replyflags |= SEND_GROUP_REPLY;
        else
          replyflags |= SEND_GROUP_CHAT_REPLY;
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        ci_send_message(replyflags, NULL, NULL, Context, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_EDIT_LABEL:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);
        int num_changed = mutt_label_message(Context->mailbox, &el);
        emaillist_clear(&el);

        if (num_changed > 0)
        {
          Context->mailbox->changed = true;
          menu->redraw = REDRAW_FULL;
          /* L10N: This is displayed when the x-label on one or more
             messages is edited. */
          mutt_message(ngettext("%d label changed", "%d labels changed", num_changed),
                       num_changed);
        }
        else
        {
          /* L10N: This is displayed when editing an x-label, but no messages
             were updated.  Possibly due to canceling at the prompt or if the new
             label is the same as the old label. */
          mutt_message(_("No labels changed"));
        }
        break;
      }

      case OP_LIST_REPLY:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        ci_send_message(SEND_REPLY | SEND_LIST_REPLY, NULL, NULL, Context, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_MAIL:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        ci_send_message(SEND_NO_FLAGS, NULL, NULL, Context, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIL_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        ci_send_message(SEND_KEY, NULL, NULL, NULL, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EXTRACT_KEYS:
      {
        if (!WithCrypto)
          break;
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);
        crypt_extract_keys_from_messages(Context->mailbox, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_CHECK_TRADITIONAL:
      {
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        if (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, e_cur, tag);
          mutt_check_traditional_pgp(&el, &menu->redraw);
          emaillist_clear(&el);
        }

        if (in_pager)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;
      }

      case OP_PIPE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);
        mutt_pipe_message(Context->mailbox, &el);
        emaillist_clear(&el);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, piping could change
         * new or old messages status to read. Redraw what's needed.  */
        if ((Context->mailbox->magic == MUTT_IMAP) && !C_ImapPeek)
        {
          menu->redraw |= (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
        }
#endif
        break;
      }

      case OP_PRINT:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);
        mutt_print_message(Context->mailbox, &el);
        emaillist_clear(&el);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, printing could change
         * new or old messages status to read. Redraw what's needed.  */
        if ((Context->mailbox->magic == MUTT_IMAP) && !C_ImapPeek)
        {
          menu->redraw |= (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
        }
#endif
        break;
      }

      case OP_MAIN_READ_THREAD:
      case OP_MAIN_READ_SUBTHREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           mark zero, 1, 12, ... messages as read. So in English we use
           "messages". Your language might have other means to express this. */
        if (!check_acl(Context, MUTT_ACL_SEEN, _("Can't mark messages as read")))
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        int rc = mutt_thread_set_flag(e_cur, MUTT_READ, true, (op != OP_MAIN_READ_THREAD));
        if (rc != -1)
        {
          if (C_Resolve)
          {
            menu->current = ((op == OP_MAIN_READ_THREAD) ? mutt_next_thread(e_cur) :
                                                           mutt_next_subthread(e_cur));
            if (menu->current == -1)
            {
              menu->current = menu->oldcurrent;
            }
            else if (in_pager)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
      }

      case OP_MARK_MSG:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        if (e_cur->env->message_id)
        {
          char buf2[128];

          buf2[0] = '\0';
          /* L10N: This is the prompt for <mark-message>.  Whatever they
             enter will be prefixed by $mark_macro_prefix and will become
             a macro hotkey to jump to the currently selected message. */
          if (!mutt_get_field(_("Enter macro stroke: "), buf2, sizeof(buf2), MUTT_CLEAR) &&
              buf2[0])
          {
            char str[256], macro[256];
            snprintf(str, sizeof(str), "%s%s", C_MarkMacroPrefix, buf2);
            snprintf(macro, sizeof(macro), "<search>~i \"%s\"\n", e_cur->env->message_id);
            /* L10N: "message hotkey" is the key bindings menu description of a
               macro created by <mark-message>. */
            km_bind(str, MENU_MAIN, OP_MACRO, macro, _("message hotkey"));

            /* L10N: This is echoed after <mark-message> creates a new hotkey
               macro.  %s is the hotkey string ($mark_macro_prefix followed
               by whatever they typed at the prompt.) */
            snprintf(buf2, sizeof(buf2), _("Message bound to %s"), str);
            mutt_message(buf2);
            mutt_debug(LL_DEBUG1, "Mark: %s => %s\n", str, macro);
          }
        }
        else
        {
          /* L10N: This error is printed if <mark-message> can't find a
             Message-ID for the currently selected message in the index. */
          mutt_error(_("No message ID to macro"));
        }
        break;
      }

      case OP_RECALL_MESSAGE:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        ci_send_message(SEND_POSTPONED, NULL, NULL, Context, NULL);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_RESEND:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;

        if (tag)
        {
          struct Mailbox *m = Context->mailbox;
          for (size_t i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (message_is_tagged(Context, e))
              mutt_resend_message(NULL, Context, e);
          }
        }
        else
        {
          struct Email *e_cur = get_cur_email(Context, menu);
          mutt_resend_message(NULL, Context, e_cur);
        }

        menu->redraw = REDRAW_FULL;
        break;

#ifdef USE_NNTP
      case OP_FOLLOWUP:
      case OP_FORWARD_TO_GROUP:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        /* fallthrough */

      case OP_POST:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        if ((op != OP_FOLLOWUP) || !e_cur->env->followup_to ||
            (mutt_str_strcasecmp(e_cur->env->followup_to, "poster") != 0) ||
            (query_quadoption(C_FollowupToPoster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          if (Context && (Context->mailbox->magic == MUTT_NNTP) &&
              !((struct NntpMboxData *) Context->mailbox->mdata)->allowed && (query_quadoption(C_PostModerated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
          {
            break;
          }
          if (op == OP_POST)
            ci_send_message(SEND_NEWS, NULL, NULL, Context, NULL);
          else
          {
            if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT))
              break;
            struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
            el_add_tagged(&el, Context, e_cur, tag);
            ci_send_message(((op == OP_FOLLOWUP) ? SEND_REPLY : SEND_FORWARD) | SEND_NEWS,
                            NULL, NULL, Context, &el);
            emaillist_clear(&el);
          }
          menu->redraw = REDRAW_FULL;
          break;
        }
      }
#endif
      /* fallthrough */
      case OP_REPLY:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, e_cur, tag);
        if (C_PgpAutoDecode && (tag || !(e_cur->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        ci_send_message(SEND_REPLY, NULL, NULL, Context, &el);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_SHELL_ESCAPE:
        mutt_shell_escape();
        break;

      case OP_TAG_THREAD:
      case OP_TAG_SUBTHREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;

        int rc = mutt_thread_set_flag(e_cur, MUTT_TAG, !e_cur->tagged, (op != OP_TAG_THREAD));
        if (rc != -1)
        {
          if (C_Resolve)
          {
            if (op == OP_TAG_THREAD)
              menu->current = mutt_next_thread(e_cur);
            else
              menu->current = mutt_next_subthread(e_cur);

            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
      }

      case OP_UNDELETE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't undelete message")))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        struct Email *e_cur = get_cur_email(Context, menu);
        el_add_tagged(&el, Context, e_cur, tag);

        mutt_emails_set_flag(Context->mailbox, &el, MUTT_DELETE, 0);
        mutt_emails_set_flag(Context->mailbox, &el, MUTT_PURGE, 0);
        emaillist_clear(&el);

        if (tag)
        {
          menu->redraw |= REDRAW_INDEX;
        }
        else
        {
          if (C_Resolve && (menu->current < (Context->mailbox->vcount - 1)))
          {
            menu->current++;
            menu->redraw |= REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw |= REDRAW_CURRENT;
        }

        menu->redraw |= REDRAW_STATUS;
        break;
      }

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
            undelete zero, 1, 12, ... messages. So in English we use
            "messages". Your language might have other means to express this. */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't undelete messages")))
          break;

        struct Email *e_cur = get_cur_email(Context, menu);
        int rc = mutt_thread_set_flag(e_cur, MUTT_DELETE, false, (op != OP_UNDELETE_THREAD));
        if (rc != -1)
        {
          rc = mutt_thread_set_flag(e_cur, MUTT_PURGE, false, (op != OP_UNDELETE_THREAD));
        }
        if (rc != -1)
        {
          if (C_Resolve)
          {
            if (op == OP_UNDELETE_THREAD)
              menu->current = mutt_next_thread(e_cur);
            else
              menu->current = mutt_next_subthread(e_cur);

            if (menu->current == -1)
              menu->current = menu->oldcurrent;
          }
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
        break;
      }

      case OP_VERSION:
        mutt_message(mutt_make_version());
        break;

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

      case OP_VIEW_ATTACHMENTS:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct Email *e_cur = get_cur_email(Context, menu);
        if (!e_cur)
          break;
        mutt_view_attachments(e_cur);
        if (e_cur->attach_del)
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

#ifdef USE_AUTOCRYPT
      case OP_AUTOCRYPT_ACCT_MENU:
        mutt_autocrypt_account_menu();
        break;
#endif

      default:
        if (!in_pager)
          km_error_key(MENU_MAIN);
    }

#ifdef USE_NOTMUCH
    if (Context)
      nm_db_debug_check(Context->mailbox);
#endif

    if (in_pager)
    {
      mutt_clear_pager_position();
      in_pager = false;
      menu->redraw = REDRAW_FULL;
    }

    if (done)
      break;
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  return close;
}

/**
 * mutt_set_header_color - Select a colour for a message
 * @param m Mailbox
 * @param e Current Email
 */
void mutt_set_header_color(struct Mailbox *m, struct Email *e)
{
  if (!e)
    return;

  struct ColorLine *color = NULL;
  struct PatternCache cache = { 0 };

  STAILQ_FOREACH(color, &Colors->index_list, entries)
  {
    if (mutt_pattern_exec(SLIST_FIRST(color->color_pattern),
                          MUTT_MATCH_FULL_ADDRESS, m, e, &cache))
    {
      e->pair = color->pair;
      return;
    }
  }
  e->pair = Colors->defs[MT_COLOR_NORMAL];
}

/**
 * mutt_reply_observer - Listen for config changes to "reply_regex" - Implements ::observer_t()
 */
int mutt_reply_observer(struct NotifyCallback *nc)
{
  if (!nc)
    return -1;

  struct EventConfig *ec = (struct EventConfig *) nc->event;

  if (mutt_str_strcmp(ec->name, "reply_regex") != 0)
    return 0;

  if (!Context || !Context->mailbox)
    return 0;

  regmatch_t pmatch[1];
  struct Mailbox *m = Context->mailbox;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    struct Envelope *env = e->env;
    if (!env || !env->subject)
      continue;

    if (mutt_regex_capture(C_ReplyRegex, env->subject, 1, pmatch))
    {
      env->real_subj = env->subject + pmatch[0].rm_eo;
      continue;
    }

    env->real_subj = env->subject;
  }

  OptResortInit = true; /* trigger a redraw of the index */
  return 0;
}
