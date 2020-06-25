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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "debug/lib.h"
#include "index.h"
#include "browser.h"
#include "commands.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "init.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pattern.h"
#include "progress.h"
#include "protos.h"
#include "recvattach.h"
#include "score.h"
#include "sort.h"
#include "status.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_POP
#include "pop/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_INOTIFY
#include "monitor.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/* These Config Variables are only used in index.c */
bool C_ChangeFolderNext; ///< Config: Suggest the next folder, rather than the first when using '<change-folder>'
bool C_CollapseAll; ///< Config: Collapse all threads when entering a folder
bool C_CollapseFlagged; ///< Config: Prevent the collapse of threads with flagged emails
bool C_CollapseUnread; ///< Config: Prevent the collapse of threads with unread emails
char *C_MarkMacroPrefix; ///< Config: Prefix for macros using '<mark-message>'
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

  struct Email *e_cur = mutt_get_virt_email(ctx->mailbox, menu->current);
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
  if (!ctx || !ctx->mailbox || !menu)
    return;

  struct Email *e_cur = mutt_get_virt_email(ctx->mailbox, menu->current);

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
          ; // do nothing

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
 * struct CurrentEmail - Keep track of the currently selected Email
 */
struct CurrentEmail
{
  struct Email *e;  ///< The current Email
  time_t received;  ///< From Email.received
  char *message_id; ///< From Email.Envelope.message_id
};

/**
 * is_current_email - Check whether an email is the currently selected Email
 * @param cur  Currently selected Email
 * @param e    Email to check
 * @retval true e is current
 * @retval false e is not current
 */
static bool is_current_email(const struct CurrentEmail *cur, const struct Email *e)
{
  return (e->received == cur->received) &&
         mutt_str_equal(e->env->message_id, cur->message_id);
}

/**
 * set_current_email - Keep track of the currently selected Email
 * @param cur Currently selected Email
 * @param e   Email to set as current
 */
static void set_current_email(struct CurrentEmail *cur, struct Email *e)
{
  *cur = (struct CurrentEmail){
    .e = e,
    .received = e ? e->received : 0,
    .message_id = mutt_str_replace(&cur->message_id, e ? e->env->message_id : NULL),
  };
}

/**
 * update_index - Update the index
 * @param menu       Current Menu
 * @param ctx        Mailbox
 * @param check      Flags, e.g. #MUTT_REOPENED
 * @param oldcount   How many items are currently in the index
 * @param cur        Remember our place in the index
 */
static void update_index(struct Menu *menu, struct Context *ctx, int check,
                         int oldcount, const struct CurrentEmail *cur)
{
  if (!menu || !ctx)
    return;

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
      if (is_current_email(cur, e))
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
 * mutt_update_index - Update the index
 * @param menu      Current Menu
 * @param ctx       Mailbox
 * @param check     Flags, e.g. #MUTT_REOPENED
 * @param oldcount  How many items are currently in the index
 * @param cur_email Currently selected email
 *
 * @note cur_email cannot be NULL
 */
void mutt_update_index(struct Menu *menu, struct Context *ctx, int check,
                       int oldcount, const struct Email *cur_email)
{
  struct CurrentEmail se = { .received = cur_email->received,
                             .message_id = cur_email->env->message_id };
  update_index(menu, ctx, check, oldcount, &se);
}

/**
 * mailbox_index_observer - Listen for Mailbox changes - Implements ::observer_t
 *
 * If a Mailbox is closed, then set a pointer to NULL.
 */
static int mailbox_index_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if ((nc->event_type != NT_MAILBOX) || (nc->event_subtype != NT_MAILBOX_CLOSED))
    return 0;

  struct Mailbox **ptr = nc->global_data;
  if (!ptr || !*ptr)
    return 0;

  *ptr = NULL;
  return 0;
}

/**
 * change_folder_mailbox - Change to a different Mailbox by pointer
 * @param menu      Current Menu
 * @param m         Mailbox
 * @param oldcount  How many items are currently in the index
 * @param cur       Remember our place in the index
 * @param read_only Open Mailbox in read-only mode
 */
static void change_folder_mailbox(struct Menu *menu, struct Mailbox *m, int *oldcount,
                                  const struct CurrentEmail *cur, bool read_only)
{
  if (!m)
    return;

  /* keepalive failure in mutt_enter_fname may kill connection. */
  if (Context && Context->mailbox && (mutt_buffer_is_empty(&Context->mailbox->pathbuf)))
    ctx_free(&Context);

  if (Context && Context->mailbox)
  {
    char *new_last_folder = NULL;
#ifdef USE_INOTIFY
    int monitor_remove_rc = mutt_monitor_remove(NULL);
#endif
#ifdef USE_COMP_MBOX
    if (Context->mailbox->compress_info && (Context->mailbox->realpath[0] != '\0'))
      new_last_folder = mutt_str_dup(Context->mailbox->realpath);
    else
#endif
      new_last_folder = mutt_str_dup(mailbox_path(Context->mailbox));
    *oldcount = Context->mailbox->msg_count;

    int check = mx_mbox_close(&Context);
    if (check != 0)
    {
#ifdef USE_INOTIFY
      if (monitor_remove_rc == 0)
        mutt_monitor_add(NULL);
#endif
      if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
        update_index(menu, Context, check, *oldcount, cur);

      FREE(&new_last_folder);
      OptSearchInvalid = true;
      menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
      return;
    }
    FREE(&LastFolder);
    LastFolder = new_last_folder;
  }
  mutt_str_replace(&CurrentFolder, mailbox_path(m));

  /* If the `folder-hook` were to call `unmailboxes`, then the Mailbox (`m`)
   * could be deleted, leaving `m` dangling. */
  // TODO: Refactor this function to avoid the need for an observer
  notify_observer_add(m->notify, mailbox_index_observer, &m);
  char *dup_path = mutt_str_dup(mailbox_path(m));

  mutt_folder_hook(mailbox_path(m), m ? m->name : NULL);
  if (m)
  {
    /* `m` is still valid, but we won't need the observer again before the end
     * of the function. */
    notify_observer_remove(m->notify, mailbox_index_observer, &m);
  }
  else
  {
    // Recreate the Mailbox (probably because a hook has done `unmailboxes *`)
    m = mx_path_resolve(dup_path);
  }
  FREE(&dup_path);

  if (!m)
    return;

  const int flags = read_only ? MUTT_READONLY : MUTT_OPEN_NO_FLAGS;
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
  }

  if (((C_Sort & SORT_MASK) == SORT_THREADS) && C_CollapseAll)
    collapse_all(Context, menu, 0);

#ifdef USE_SIDEBAR
  struct MuttWindow *dlg = mutt_window_dialog(menu->win_index);
  struct MuttWindow *win_sidebar = mutt_window_find(dlg, WT_SIDEBAR);
  sb_set_open_mailbox(win_sidebar, Context ? Context->mailbox : NULL);
#endif

  mutt_clear_error();
  mutt_mailbox_check(Context ? Context->mailbox : NULL, MUTT_MAILBOX_CHECK_FORCE); /* force the mailbox check after we have changed the folder */
  menu->redraw = REDRAW_FULL;
  OptSearchInvalid = true;
}

#ifdef USE_NOTMUCH
/**
 * change_folder_notmuch - Change to a different Notmuch Mailbox by string
 * @param menu      Current Menu
 * @param buf       Folder to change to
 * @param buflen    Length of buffer
 * @param oldcount  How many items are currently in the index
 * @param cur       Remember our place in the index
 * @param read_only Open Mailbox in read-only mode
 */
static struct Mailbox *change_folder_notmuch(struct Menu *menu, char *buf,
                                             int buflen, int *oldcount,
                                             const struct CurrentEmail *cur, bool read_only)
{
  if (!nm_url_from_query(NULL, buf, buflen))
  {
    mutt_message(_("Failed to create query, aborting"));
    return NULL;
  }

  struct Mailbox *m_query = mx_path_resolve(buf);
  change_folder_mailbox(menu, m_query, oldcount, cur, read_only);
  return m_query;
}
#endif

/**
 * change_folder_string - Change to a different Mailbox by string
 * @param menu         Current Menu
 * @param buf          Folder to change to
 * @param buflen       Length of buffer
 * @param oldcount     How many items are currently in the index
 * @param cur          Remember our place in the index
 * @param pager_return Return to the pager afterwards
 * @param read_only    Open Mailbox in read-only mode
 */
static void change_folder_string(struct Menu *menu, char *buf, size_t buflen,
                                 int *oldcount, const struct CurrentEmail *cur,
                                 bool *pager_return, bool read_only)
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

  enum MailboxType type = mx_path_probe(buf);
  if ((type == MUTT_MAILBOX_ERROR) || (type == MUTT_UNKNOWN))
  {
    // Look for a Mailbox by its description, before failing
    struct Mailbox *m = mailbox_find_name(buf);
    if (m)
    {
      change_folder_mailbox(menu, m, oldcount, cur, read_only);
      *pager_return = false;
    }
    else
      mutt_error(_("%s is not a mailbox"), buf);
    return;
  }

  /* past this point, we don't return to the pager on error */
  *pager_return = false;

  struct Mailbox *m = mx_path_resolve(buf);
  change_folder_mailbox(menu, m, oldcount, cur, read_only);
}

/**
 * index_make_entry - Format a menu item for the index list - Implements Menu::make_entry()
 */
void index_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  buf[0] = '\0';

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

  mutt_make_string_flags(buf, buflen, menu->win_index->state.cols,
                         NONULL(C_IndexFormat), Context, Context->mailbox, e, flags);
}

/**
 * index_color - Calculate the colour for a line of the index - Implements Menu::color()
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
  if (!buf || !stdscr)
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
 * index_custom_redraw - Redraw the index - Implements Menu::custom_redraw()
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
    mutt_draw_statusline(menu->win_ibar->state.cols, buf, sizeof(buf));
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
 * @param dlg Dialog containing Windows to draw on
 * @retval num How the menu was finished, e.g. OP_QUIT, OP_EXIT
 *
 * This function handles the message index window as well as commands returned
 * from the pager (MENU_PAGER).
 */
int mutt_index_menu(struct MuttWindow *dlg)
{
  char buf[PATH_MAX], helpstr[1024];
  int op = OP_NULL;
  bool done = false; /* controls when to exit the "event" loop */
  bool tag = false;  /* has the tag-prefix command been pressed? */
  int newcount = -1;
  int oldcount = -1;
  struct CurrentEmail cur = { 0 };
  bool do_mailbox_notify = true;
  int close = 0; /* did we OP_QUIT or OP_EXIT out of this menu? */
  int attach_msg = OptAttachMsg;
  bool in_pager = false; /* set when pager redirects a function through the index */

  struct MuttWindow *win_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *win_ibar = mutt_window_find(dlg, WT_INDEX_BAR);
  struct MuttWindow *win_pager = mutt_window_find(dlg, WT_PAGER);
  struct MuttWindow *win_pbar = mutt_window_find(dlg, WT_PAGER_BAR);

  struct Menu *menu = mutt_menu_new(MENU_MAIN);
  menu->pagelen = win_index->state.rows;
  menu->win_index = win_index;
  menu->win_ibar = win_ibar;

  menu->make_entry = index_make_entry;
  menu->color = index_color;
  menu->current = ci_first_message(Context);
  menu->help = mutt_compile_help(
      helpstr, sizeof(helpstr), MENU_MAIN,
#ifdef USE_NNTP
      (Context && Context->mailbox && (Context->mailbox->type == MUTT_NNTP)) ?
          IndexNewsHelp :
#endif
          IndexHelp);
  menu->custom_redraw = index_custom_redraw;
  mutt_menu_push_current(menu);
  mutt_window_reflow(NULL);

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
    {
      resort_index(Context, menu);
    }

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

      set_current_email(&cur, mutt_get_virt_email(Context->mailbox, menu->current));

      int check = mx_mbox_check(Context->mailbox);
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
          bool verbose = Context->mailbox->verbose;
          Context->mailbox->verbose = false;
          update_index(menu, Context, check, oldcount, &cur);
          Context->mailbox->verbose = verbose;
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

    if (in_pager)
    {
      if (menu->current < menu->max)
        menu->oldcurrent = menu->current;
      else
        menu->oldcurrent = -1;

      mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE); /* fallback from the pager */
    }
    else
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
        mutt_window_move(menu->win_index, 2, menu->current - menu->top);
      else if (C_BrailleFriendly)
        mutt_window_move(menu->win_index, 0, menu->current - menu->top);
      else
      {
        mutt_window_move(menu->win_index, menu->win_index->state.cols - 1,
                         menu->current - menu->top);
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

      /* either user abort or timeout */
      if (op < 0)
      {
        mutt_timeout_hook();
        if (tag)
          mutt_window_clearline(MuttMessageWindow, 0);
        continue;
      }

      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);

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
      {
        tag = true;
      }

      mutt_clear_error();
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
        if (Context->mailbox->type == MUTT_NNTP)
        {
          if (op == OP_GET_MESSAGE)
          {
            buf[0] = '\0';
            if ((mutt_get_field(_("Enter Message-Id: "), buf, sizeof(buf),
                                MUTT_COMP_NO_FLAGS) != 0) ||
                (buf[0] == '\0'))
            {
              break;
            }
          }
          else
          {
            if (!cur.e || STAILQ_EMPTY(&cur.e->env->references))
            {
              mutt_error(_("Article has no parent reference"));
              break;
            }
            mutt_str_copy(buf, STAILQ_FIRST(&cur.e->env->references)->data, sizeof(buf));
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
        if (Context->mailbox->type != MUTT_NNTP)
          break;

        if (!cur.e)
          break;

        int oldmsgcount = Context->mailbox->msg_count;
        int oldindex = cur.e->index;
        int rc = 0;

        if (!cur.e->env->message_id)
        {
          mutt_error(_("No Message-Id. Unable to perform operation."));
          break;
        }

        mutt_message(_("Fetching message headers..."));
        if (!Context->mailbox->id_hash)
          Context->mailbox->id_hash = mutt_make_id_hash(Context->mailbox);
        mutt_str_copy(buf, cur.e->env->message_id, sizeof(buf));

        /* trying to find msgid of the root message */
        if (op == OP_RECONSTRUCT_THREAD)
        {
          struct ListNode *ref = NULL;
          STAILQ_FOREACH(ref, &cur.e->env->references, entries)
          {
            if (!mutt_hash_find(Context->mailbox->id_hash, ref->data))
            {
              rc = nntp_check_msgid(Context->mailbox, ref->data);
              if (rc < 0)
                break;
            }

            /* the last msgid in References is the root message */
            if (!STAILQ_NEXT(ref, entries))
              mutt_str_copy(buf, ref->data, sizeof(buf));
          }
        }

        /* fetching all child messages */
        if (rc >= 0)
          rc = nntp_check_children(Context->mailbox, buf);

        /* at least one message has been loaded */
        if (Context->mailbox->msg_count > oldmsgcount)
        {
          struct Email *e_oldcur = mutt_get_virt_email(Context->mailbox, menu->current);
          bool verbose = Context->mailbox->verbose;

          if (rc < 0)
            Context->mailbox->verbose = false;
          mutt_sort_headers(Context, (op == OP_RECONSTRUCT_THREAD));
          Context->mailbox->verbose = verbose;

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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        if (isdigit(LastKey))
          mutt_unget_event(LastKey, 0);
        buf[0] = '\0';
        if ((mutt_get_field(_("Jump to message: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_READONLY | CHECK_ATTACH))
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
#ifdef USE_DEBUG_GRAPHVIZ
        dump_graphviz("index");
#endif
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
        mutt_help(MENU_MAIN, win_index->state.cols);
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
        menu->oldcurrent = cur.e ? cur.e->index : -1;
        if (op == OP_TOGGLE_READ)
        {
          char buf2[1024];

          if (!Context->pattern || !mutt_strn_equal(Context->pattern, "!~R!~D~s", 8))
          {
            snprintf(buf2, sizeof(buf2), "!~R!~D~s%s",
                     Context->pattern ? Context->pattern : ".*");
          }
          else
          {
            mutt_str_copy(buf2, Context->pattern + 8, sizeof(buf2));
            if ((*buf2 == '\0') || mutt_strn_equal(buf2, ".*", 2))
              snprintf(buf2, sizeof(buf2), "~A");
          }
          FREE(&Context->pattern);
          Context->pattern = mutt_str_dup(buf2);
          mutt_pattern_func(MUTT_LIMIT, NULL);
        }

        if (((op == OP_LIMIT_CURRENT_THREAD) && mutt_limit_current_thread(cur.e)) ||
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
          notify_send(NeoMutt->notify, NT_GLOBAL, NT_GLOBAL_SHUTDOWN, NULL);

          if (!Context || ((check = mx_mbox_close(&Context)) == 0))
            done = true;
          else
          {
            if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
              update_index(menu, Context, check, oldcount, &cur);

            menu->redraw = REDRAW_FULL; /* new mail arrived? */
            OptSearchInvalid = true;
          }
        }
        break;

      case OP_REDRAW:
        mutt_window_reflow(NULL);
        clearok(stdscr, true);
        menu->redraw = REDRAW_FULL;
        break;

      // Initiating a search can happen on an empty mailbox, but
      // searching for next/previous/... needs to be on a message and
      // thus a non-empty mailbox
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        /* fallthrough */
      case OP_SEARCH:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        menu->current = mutt_search_command(menu->current, op);
        if (menu->current == -1)
          menu->current = menu->oldcurrent;
        else
          menu->redraw |= REDRAW_MOTION;
        break;

      case OP_SORT:
      case OP_SORT_REVERSE:
        if (mutt_select_sort((op == OP_SORT_REVERSE)) != 0)
          break;

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
          if (!cur.e)
            break;
          mutt_set_flag(Context->mailbox, cur.e, MUTT_TAG, !cur.e->tagged);

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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        mutt_pattern_func(MUTT_TAG, _("Tag messages matching: "));
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_MAIN_UNDELETE_PATTERN:
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_READONLY))
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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;
        if (mutt_pattern_func(MUTT_UNTAG, _("Untag messages matching: ")) == 0)
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        break;

      case OP_COMPOSE_TO_SENDER:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        mutt_send_message(SEND_TO_SENDER, NULL, NULL, Context, &el, NeoMutt->sub);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      /* --------------------------------------------------------------------
       * The following operations can be performed inside of the pager.
       */

#ifdef USE_IMAP
      case OP_MAIN_IMAP_FETCH:
        if (Context && Context->mailbox && (Context->mailbox->type == MUTT_IMAP))
          imap_check_mailbox(Context->mailbox, true);
        break;

      case OP_MAIN_IMAP_LOGOUT_ALL:
        if (Context && Context->mailbox && (Context->mailbox->type == MUTT_IMAP))
        {
          int check = mx_mbox_close(&Context);
          if (check != 0)
          {
            if ((check == MUTT_NEW_MAIL) || (check == MUTT_REOPENED))
              update_index(menu, Context, check, oldcount, &cur);
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
            if (!cur.e)
              break;
            if (cur.e->deleted)
              newidx = ci_next_undeleted(Context, menu->current);
            if (newidx < 0)
              newidx = ci_previous_undeleted(Context, menu->current);
            if (newidx >= 0)
              e = mutt_get_virt_email(Context->mailbox, newidx);
          }

          int check = mx_mbox_sync(Context->mailbox);
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
            update_index(menu, Context, check, oc, &cur);

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
          if (!cur.e)
            break;
          cur.e->quasi_deleted = true;
          Context->mailbox->changed = true;
        }
        break;

#ifdef USE_NOTMUCH
      case OP_MAIN_ENTIRE_THREAD:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (Context->mailbox->type != MUTT_NOTMUCH)
        {
          if (((Context->mailbox->type != MUTT_MH) && (Context->mailbox->type != MUTT_MAILDIR)) ||
              (!cur.e || !cur.e->env || !cur.e->env->message_id))
          {
            mutt_message(_("No virtual folder and no Message-Id, aborting"));
            break;
          } // no virtual folder, but we have message-id, reconstruct thread on-the-fly
          strncpy(buf, "id:", sizeof(buf));
          int msg_id_offset = 0;
          if ((cur.e->env->message_id)[0] == '<')
            msg_id_offset = 1;
          mutt_str_cat(buf, sizeof(buf), (cur.e->env->message_id) + msg_id_offset);
          if (buf[strlen(buf) - 1] == '>')
            buf[strlen(buf) - 1] = '\0';

          change_folder_notmuch(menu, buf, sizeof(buf), &oldcount, &cur, false);

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
        struct Email *e_oldcur = mutt_get_virt_email(Context->mailbox, menu->current);
        if (nm_read_entire_thread(Context->mailbox, cur.e) < 0)
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
            menu->current = mutt_uncollapse_thread(Context, cur.e);
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
        if (!cur.e)
          break;
        char *tags = NULL;
        if (!tag)
          tags = driver_tags_get_with_hidden(&cur.e->tags);
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

          if (m->verbose)
          {
            mutt_progress_init(&progress, _("Update tags..."),
                               MUTT_PROGRESS_WRITE, m->msg_tagged);
          }

#ifdef USE_NOTMUCH
          if (m->type == MUTT_NOTMUCH)
            nm_db_longrun_init(m, true);
#endif
          for (int px = 0, i = 0; i < m->msg_count; i++)
          {
            struct Email *e = m->emails[i];
            if (!e)
              break;
            if (!message_is_tagged(Context, e))
              continue;

            if (m->verbose)
              mutt_progress_update(&progress, ++px, -1);
            mx_tags_commit(m, e, buf);
            if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
            {
              bool still_queried = false;
#ifdef USE_NOTMUCH
              if (m->type == MUTT_NOTMUCH)
                still_queried = nm_message_is_still_queried(m, e);
#endif
              e->quasi_deleted = !still_queried;
              m->changed = true;
            }
          }
#ifdef USE_NOTMUCH
          if (m->type == MUTT_NOTMUCH)
            nm_db_longrun_done(m);
#endif
          menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
        }
        else
        {
          if (mx_tags_commit(m, cur.e, buf))
          {
            mutt_message(_("Failed to modify tags, aborting"));
            break;
          }
          if (op == OP_MAIN_MODIFY_TAGS_THEN_HIDE)
          {
            bool still_queried = false;
#ifdef USE_NOTMUCH
            if (m->type == MUTT_NOTMUCH)
              still_queried = nm_message_is_still_queried(m, cur.e);
#endif
            cur.e->quasi_deleted = !still_queried;
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
      {
        buf[0] = '\0';
        if ((mutt_get_field("Query: ", buf, sizeof(buf), MUTT_NM_QUERY) != 0) ||
            (buf[0] == '\0'))
        {
          mutt_message(_("No query, aborting"));
          break;
        }

        // Keep copy of user's query to name the mailbox
        char *query_unencoded = mutt_str_dup(buf);

        struct Mailbox *m_query =
            change_folder_notmuch(menu, buf, sizeof(buf), &oldcount, &cur,
                                  (op == OP_MAIN_VFOLDER_FROM_QUERY_READONLY));
        if (m_query)
        {
          m_query->name = query_unencoded;
          query_unencoded = NULL;
        }
        else
        {
          FREE(&query_unencoded);
        }

        break;
      }

      case OP_MAIN_WINDOWED_VFOLDER_BACKWARD:
      {
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
        nm_query_window_backward();
        mutt_str_copy(buf, C_NmQueryWindowCurrentSearch, sizeof(buf));
        change_folder_notmuch(menu, buf, sizeof(buf), &oldcount, &cur, false);
        break;
      }

      case OP_MAIN_WINDOWED_VFOLDER_FORWARD:
      {
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
        mutt_str_copy(buf, C_NmQueryWindowCurrentSearch, sizeof(buf));
        change_folder_notmuch(menu, buf, sizeof(buf), &oldcount, &cur, false);
        break;
      }
#endif

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_OPEN:
      {
        struct MuttWindow *win_sidebar = mutt_window_find(dlg, WT_SIDEBAR);
        change_folder_mailbox(menu, sb_get_highlight(win_sidebar), &oldcount, &cur, false);
        break;
      }
#endif

      case OP_MAIN_NEXT_UNREAD_MAILBOX:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
          break;

        struct Mailbox *m = Context->mailbox;

        struct Buffer *folderbuf = mutt_buffer_pool_get();
        mutt_buffer_strcpy(folderbuf, mailbox_path(m));
        m = mutt_mailbox_next(m, folderbuf);
        mutt_buffer_pool_release(&folderbuf);

        if (!m)
        {
          mutt_error(_("No mailboxes have new mail"));
          break;
        }

        change_folder_mailbox(menu, m, &oldcount, &cur, false);
        break;
      }

      case OP_MAIN_CHANGE_FOLDER:
      case OP_MAIN_CHANGE_FOLDER_READONLY:
#ifdef USE_NOTMUCH
      case OP_MAIN_CHANGE_VFOLDER: // now an alias for OP_MAIN_CHANGE_FOLDER
#endif
      {
        bool pager_return = true; /* return to display message in pager */
        struct Buffer *folderbuf = mutt_buffer_pool_get();
        mutt_buffer_alloc(folderbuf, PATH_MAX);

        char *cp = NULL;
        bool read_only;
        if (attach_msg || C_ReadOnly || (op == OP_MAIN_CHANGE_FOLDER_READONLY))
        {
          cp = _("Open mailbox in read-only mode");
          read_only = true;
        }
        else
        {
          cp = _("Open mailbox");
          read_only = false;
        }

        if (C_ChangeFolderNext && Context && Context->mailbox &&
            !mutt_buffer_is_empty(&Context->mailbox->pathbuf))
        {
          mutt_buffer_strcpy(folderbuf, mailbox_path(Context->mailbox));
          mutt_buffer_pretty_mailbox(folderbuf);
        }
        /* By default, fill buf with the next mailbox that contains unread mail */
        mutt_mailbox_next(Context ? Context->mailbox : NULL, folderbuf);

        if (mutt_buffer_enter_fname(cp, folderbuf, true) == -1)
          goto changefoldercleanup;

        /* Selected directory is okay, let's save it. */
        mutt_browser_select_dir(mutt_b2s(folderbuf));

        if (mutt_buffer_is_empty(folderbuf))
        {
          mutt_window_clearline(MuttMessageWindow, 0);
          goto changefoldercleanup;
        }

        struct Mailbox *m = mx_mbox_find2(mutt_b2s(folderbuf));
        if (m)
        {
          change_folder_mailbox(menu, m, &oldcount, &cur, read_only);
          pager_return = false;
        }
        else
        {
          change_folder_string(menu, folderbuf->data, folderbuf->dsize,
                               &oldcount, &cur, &pager_return, read_only);
        }

      changefoldercleanup:
        mutt_buffer_pool_release(&folderbuf);
        if (in_pager && pager_return)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;
      }

#ifdef USE_NNTP
      case OP_MAIN_CHANGE_GROUP:
      case OP_MAIN_CHANGE_GROUP_READONLY:
      {
        bool pager_return = true; /* return to display message in pager */
        struct Buffer *folderbuf = mutt_buffer_pool_get();
        mutt_buffer_alloc(folderbuf, PATH_MAX);

        OptNews = false;
        bool read_only;
        char *cp = NULL;
        if (attach_msg || C_ReadOnly || (op == OP_MAIN_CHANGE_GROUP_READONLY))
        {
          cp = _("Open newsgroup in read-only mode");
          read_only = true;
        }
        else
        {
          cp = _("Open newsgroup");
          read_only = false;
        }

        if (C_ChangeFolderNext && Context && Context->mailbox &&
            !mutt_buffer_is_empty(&Context->mailbox->pathbuf))
        {
          mutt_buffer_strcpy(folderbuf, mailbox_path(Context->mailbox));
          mutt_buffer_pretty_mailbox(folderbuf);
        }

        OptNews = true;
        CurrentNewsSrv =
            nntp_select_server(Context ? Context->mailbox : NULL, C_NewsServer, false);
        if (!CurrentNewsSrv)
          goto changefoldercleanup2;

        nntp_mailbox(Context ? Context->mailbox : NULL, folderbuf->data,
                     folderbuf->dsize);

        if (mutt_buffer_enter_fname(cp, folderbuf, true) == -1)
          goto changefoldercleanup2;

        /* Selected directory is okay, let's save it. */
        mutt_browser_select_dir(mutt_b2s(folderbuf));

        if (mutt_buffer_is_empty(folderbuf))
        {
          mutt_window_clearline(MuttMessageWindow, 0);
          goto changefoldercleanup2;
        }

        struct Mailbox *m = mx_mbox_find2(mutt_b2s(folderbuf));
        if (m)
        {
          change_folder_mailbox(menu, m, &oldcount, &cur, read_only);
          pager_return = false;
        }
        else
        {
          change_folder_string(menu, folderbuf->data, folderbuf->dsize,
                               &oldcount, &cur, &pager_return, read_only);
        }
        menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_MAIN, IndexNewsHelp);

      changefoldercleanup2:
        mutt_buffer_pool_release(&folderbuf);
        if (in_pager && pager_return)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        break;
      }
#endif

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        if (!cur.e)
          break;
        /* toggle the weeding of headers so that a user can press the key
         * again while reading the message.  */
        if (op == OP_DISPLAY_HEADERS)
          bool_str_toggle(NeoMutt->sub, "weed", NULL);

        OptNeedResort = false;

        if (((C_Sort & SORT_MASK) == SORT_THREADS) && cur.e->collapsed)
        {
          mutt_uncollapse_thread(Context, cur.e);
          mutt_set_vnum(Context);
          if (C_UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, cur.e);
        }

        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, cur.e, tag);
          mutt_check_traditional_pgp(&el, &menu->redraw);
          emaillist_clear(&el);
        }
        set_current_email(&cur, mutt_get_virt_email(Context->mailbox, menu->current));

        op = mutt_display_message(win_index, win_ibar, win_pager, win_pbar,
                                  Context->mailbox, cur.e);
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
          update_index(menu, Context, MUTT_NEW_MAIL, Context->mailbox->msg_count, &cur);
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
        if (!cur.e)
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled"));
        else if (!STAILQ_EMPTY(&cur.e->env->in_reply_to) ||
                 !STAILQ_EMPTY(&cur.e->env->references))
        {
          {
            mutt_break_thread(cur.e);
            mutt_sort_headers(Context, true);
            menu->current = cur.e->vnum;
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
        if (!cur.e)
          break;

        if ((C_Sort & SORT_MASK) != SORT_THREADS)
          mutt_error(_("Threading is not enabled"));
        else if (!cur.e->env->message_id)
          mutt_error(_("No Message-ID: header available to link thread"));
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, NULL, true);

          if (mutt_link_threads(cur.e, &el, Context->mailbox))
          {
            mutt_sort_headers(Context, true);
            menu->current = cur.e->vnum;

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
        if (!cur.e)
          break;
        mutt_edit_content_type(cur.e, cur.e->content, NULL);
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
        el_add_tagged(&el, Context, cur.e, tag);

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
        int mcur = menu->current;
        menu->current = -1;
        for (size_t i = 0; i != Context->mailbox->vcount; i++)
        {
          if ((op == OP_MAIN_NEXT_NEW) || (op == OP_MAIN_NEXT_UNREAD) ||
              (op == OP_MAIN_NEXT_NEW_THEN_UNREAD))
          {
            mcur++;
            if (mcur > (Context->mailbox->vcount - 1))
            {
              mcur = 0;
            }
          }
          else
          {
            mcur--;
            if (mcur < 0)
            {
              mcur = Context->mailbox->vcount - 1;
            }
          }

          struct Email *e = mutt_get_virt_email(Context->mailbox, mcur);
          if (!e)
            break;
          if (e->collapsed && ((C_Sort & SORT_MASK) == SORT_THREADS))
          {
            if ((UNREAD(e) != 0) && (first_unread == -1))
              first_unread = mcur;
            if ((UNREAD(e) == 1) && (first_new == -1))
              first_new = mcur;
          }
          else if (!e->deleted && !e->read)
          {
            if (first_unread == -1)
              first_unread = mcur;
            if (!e->old && (first_new == -1))
              first_new = mcur;
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
          if (!cur.e)
            break;
          mutt_set_flag(m, cur.e, MUTT_FLAG, !cur.e->flagged);
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
          if (!cur.e)
            break;
          if (cur.e->read || cur.e->old)
            mutt_set_flag(m, cur.e, MUTT_NEW, true);
          else
            mutt_set_flag(m, cur.e, MUTT_READ, true);

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

        switch (op)
        {
          case OP_MAIN_NEXT_THREAD:
            menu->current = mutt_next_thread(cur.e);
            break;

          case OP_MAIN_NEXT_SUBTHREAD:
            menu->current = mutt_next_subthread(cur.e);
            break;

          case OP_MAIN_PREV_THREAD:
            menu->current = mutt_previous_thread(cur.e);
            break;

          case OP_MAIN_PREV_SUBTHREAD:
            menu->current = mutt_previous_subthread(cur.e);
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

        menu->current = mutt_parent_message(Context, cur.e, op == OP_MAIN_ROOT_MESSAGE);
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
        el_add_tagged(&el, Context, cur.e, tag);

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

        if (!cur.e)
          break;

        if (cur.e->collapsed)
        {
          menu->current = mutt_uncollapse_thread(Context, cur.e);
          mutt_set_vnum(Context);
          if (C_UncollapseJump)
            menu->current = mutt_thread_next_unread(Context, cur.e);
        }
        else if (CAN_COLLAPSE(cur.e))
        {
          menu->current = mutt_collapse_thread(Context, cur.e);
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
        if (!prereq(Context, menu, CHECK_IN_MAILBOX))
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
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        ci_bounce_message(Context->mailbox, &el);
        emaillist_clear(&el);
        break;
      }

      case OP_CREATE_ALIAS:
      {
        struct AddressList *al = NULL;
        if (cur.e && cur.e->env)
          al = mutt_get_address(cur.e->env, NULL);
        alias_create(al);
        menu->redraw |= REDRAW_CURRENT;
        break;
      }

      case OP_QUERY:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        query_index();
        break;

      case OP_PURGE_MESSAGE:
      case OP_DELETE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;
        /* L10N: CHECK_ACL */
        if (!check_acl(Context, MUTT_ACL_DELETE, _("Can't delete message")))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);

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
        if (!cur.e)
          break;

        int subthread = (op == OP_DELETE_SUBTHREAD);
        int rc = mutt_thread_set_flag(cur.e, MUTT_DELETE, true, subthread);
        if (rc == -1)
          break;
        if (op == OP_PURGE_THREAD)
        {
          rc = mutt_thread_set_flag(cur.e, MUTT_PURGE, true, subthread);
          if (rc == -1)
            break;
        }

        if (C_DeleteUntag)
          mutt_thread_set_flag(cur.e, MUTT_TAG, false, subthread);
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
        if (Context && (Context->mailbox->type == MUTT_NNTP))
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
        if (!cur.e)
          break;
        mutt_display_address(cur.e->env);
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

        if (!cur.e)
          break;
        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, cur.e, tag);
          mutt_check_traditional_pgp(&el, &menu->redraw);
          emaillist_clear(&el);
        }
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        mutt_ev_message(Context->mailbox, &el, edit ? EVM_EDIT : EVM_VIEW);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;

        break;
      }

      case OP_FORWARD_MESSAGE:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_ATTACH))
          break;
        if (!cur.e)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        mutt_send_message(SEND_FORWARD, NULL, NULL, Context, &el, NeoMutt->sub);
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
        if (!cur.e)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        mutt_send_message(replyflags, NULL, NULL, Context, &el, NeoMutt->sub);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_EDIT_LABEL:
      {
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE | CHECK_READONLY))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
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
        if (!cur.e)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        mutt_send_message(SEND_REPLY | SEND_LIST_REPLY, NULL, NULL, Context,
                          &el, NeoMutt->sub);
        emaillist_clear(&el);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_MAIL:
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        mutt_send_message(SEND_NO_FLAGS, NULL, NULL, Context, NULL, NeoMutt->sub);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_MAIL_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!prereq(Context, menu, CHECK_ATTACH))
          break;
        mutt_send_message(SEND_KEY, NULL, NULL, NULL, NULL, NeoMutt->sub);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EXTRACT_KEYS:
      {
        if (!WithCrypto)
          break;
        if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT | CHECK_VISIBLE))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
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
        if (!cur.e)
          break;
        if (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED))
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, Context, cur.e, tag);
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
        el_add_tagged(&el, Context, cur.e, tag);
        mutt_pipe_message(Context->mailbox, &el);
        emaillist_clear(&el);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, piping could change
         * new or old messages status to read. Redraw what's needed.  */
        if ((Context->mailbox->type == MUTT_IMAP) && !C_ImapPeek)
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
        el_add_tagged(&el, Context, cur.e, tag);
        mutt_print_message(Context->mailbox, &el);
        emaillist_clear(&el);

#ifdef USE_IMAP
        /* in an IMAP folder index with imap_peek=no, printing could change
         * new or old messages status to read. Redraw what's needed.  */
        if ((Context->mailbox->type == MUTT_IMAP) && !C_ImapPeek)
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

        int rc = mutt_thread_set_flag(cur.e, MUTT_READ, true, (op != OP_MAIN_READ_THREAD));
        if (rc != -1)
        {
          if (C_Resolve)
          {
            menu->current = ((op == OP_MAIN_READ_THREAD) ? mutt_next_thread(cur.e) :
                                                           mutt_next_subthread(cur.e));
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
        if (!cur.e)
          break;
        if (cur.e->env->message_id)
        {
          char buf2[128];

          buf2[0] = '\0';
          /* L10N: This is the prompt for <mark-message>.  Whatever they
             enter will be prefixed by $mark_macro_prefix and will become
             a macro hotkey to jump to the currently selected message. */
          if (!mutt_get_field(_("Enter macro stroke: "), buf2, sizeof(buf2), MUTT_COMP_NO_FLAGS) &&
              buf2[0])
          {
            char str[256], macro[256];
            snprintf(str, sizeof(str), "%s%s", C_MarkMacroPrefix, buf2);
            snprintf(macro, sizeof(macro), "<search>~i \"%s\"\n", cur.e->env->message_id);
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
        mutt_send_message(SEND_POSTPONED, NULL, NULL, Context, NULL, NeoMutt->sub);
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
              mutt_resend_message(NULL, Context, e, NeoMutt->sub);
          }
        }
        else
        {
          mutt_resend_message(NULL, Context, cur.e, NeoMutt->sub);
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
        if (!cur.e)
          break;
        if ((op != OP_FOLLOWUP) || !cur.e->env->followup_to ||
            !mutt_istr_equal(cur.e->env->followup_to, "poster") ||
            (query_quadoption(C_FollowupToPoster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          if (Context && (Context->mailbox->type == MUTT_NNTP) &&
              !((struct NntpMboxData *) Context->mailbox->mdata)->allowed && (query_quadoption(C_PostModerated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
          {
            break;
          }
          if (op == OP_POST)
            mutt_send_message(SEND_NEWS, NULL, NULL, Context, NULL, NeoMutt->sub);
          else
          {
            if (!prereq(Context, menu, CHECK_IN_MAILBOX | CHECK_MSGCOUNT))
              break;
            struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
            el_add_tagged(&el, Context, cur.e, tag);
            mutt_send_message(((op == OP_FOLLOWUP) ? SEND_REPLY : SEND_FORWARD) | SEND_NEWS,
                              NULL, NULL, Context, &el, NeoMutt->sub);
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
        if (!cur.e)
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        el_add_tagged(&el, Context, cur.e, tag);
        if (C_PgpAutoDecode && (tag || !(cur.e->security & PGP_TRADITIONAL_CHECKED)))
        {
          mutt_check_traditional_pgp(&el, &menu->redraw);
        }
        mutt_send_message(SEND_REPLY, NULL, NULL, Context, &el, NeoMutt->sub);
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
        if (!cur.e)
          break;

        int rc = mutt_thread_set_flag(cur.e, MUTT_TAG, !cur.e->tagged, (op != OP_TAG_THREAD));
        if (rc != -1)
        {
          if (C_Resolve)
          {
            if (op == OP_TAG_THREAD)
              menu->current = mutt_next_thread(cur.e);
            else
              menu->current = mutt_next_subthread(cur.e);

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
        el_add_tagged(&el, Context, cur.e, tag);

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

        int rc = mutt_thread_set_flag(cur.e, MUTT_DELETE, false, (op != OP_UNDELETE_THREAD));
        if (rc != -1)
        {
          rc = mutt_thread_set_flag(cur.e, MUTT_PURGE, false, (op != OP_UNDELETE_THREAD));
        }
        if (rc != -1)
        {
          if (C_Resolve)
          {
            if (op == OP_UNDELETE_THREAD)
              menu->current = mutt_next_thread(cur.e);
            else
              menu->current = mutt_next_subthread(cur.e);

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
        if (!cur.e)
          break;
        mutt_view_attachments(cur.e);
        if (cur.e->attach_del)
          Context->mailbox->changed = true;
        menu->redraw = REDRAW_FULL;
        break;

      case OP_END_COND:
        break;

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

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
        struct MuttWindow *win_sidebar = mutt_window_find(dlg, WT_SIDEBAR);
        if (!win_sidebar)
          break;
        sb_change_mailbox(win_sidebar, op);
        break;
      }

      case OP_SIDEBAR_TOGGLE_VISIBLE:
        bool_str_toggle(NeoMutt->sub, "sidebar_visible", NULL);
        mutt_window_reflow(NULL);
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
  FREE(&cur.message_id);
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
 * mutt_reply_observer - Listen for config changes to "reply_regex" - Implements ::observer_t
 */
int mutt_reply_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_str_equal(ec->name, "reply_regex"))
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

/**
 * create_panel_index - Create the Windows for the Index panel
 * @param parent        Parent Window
 * @param status_on_top true, if the Index bar should be on top
 * @retval ptr Nested Windows
 */
static struct MuttWindow *create_panel_index(struct MuttWindow *parent, bool status_on_top)
{
  struct MuttWindow *panel_index =
      mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *win_index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *win_ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (status_on_top)
  {
    mutt_window_add_child(panel_index, win_ibar);
    mutt_window_add_child(panel_index, win_index);
  }
  else
  {
    mutt_window_add_child(panel_index, win_index);
    mutt_window_add_child(panel_index, win_ibar);
  }

  return panel_index;
}

/**
 * create_panel_pager - Create the Windows for the Pager panel
 * @param parent        Parent Window
 * @param status_on_top true, if the Pager bar should be on top
 * @retval ptr Nested Windows
 */
static struct MuttWindow *create_panel_pager(struct MuttWindow *parent, bool status_on_top)
{
  struct MuttWindow *panel_pager =
      mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  panel_pager->state.visible = false; // The Pager and Pager Bar are initially hidden

  struct MuttWindow *win_pager =
      mutt_window_new(WT_PAGER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  win_pager->state.visible = false;

  struct MuttWindow *win_pbar =
      mutt_window_new(WT_PAGER_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);
  win_pbar->state.visible = false;

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

/**
 * index_pager_init - Allocate the Windows for the Index/Pager
 * @retval ptr Dialog containing nested Windows
 */
struct MuttWindow *index_pager_init(void)
{
  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_INDEX, MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  notify_observer_add(NeoMutt->notify, mutt_dlgindex_observer, dlg);

  mutt_window_add_child(dlg, create_panel_index(dlg, C_StatusOnTop));
  mutt_window_add_child(dlg, create_panel_pager(dlg, C_StatusOnTop));

  return dlg;
}

/**
 * index_pager_shutdown - Clear up any non-Window parts
 * @param dlg Dialog
 */
void index_pager_shutdown(struct MuttWindow *dlg)
{
  notify_observer_remove(NeoMutt->notify, mutt_dlgindex_observer, dlg);
}

/**
 * mutt_dlgindex_observer - Listen for config changes affecting the Index/Pager - Implements ::observer_t
 */
int mutt_dlgindex_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  struct MuttWindow *win_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *win_pager = mutt_window_find(dlg, WT_PAGER);
  if (!win_index || !win_pager)
    return -1;

  if (mutt_str_equal(ec->name, "status_on_top"))
  {
    struct MuttWindow *parent = win_index->parent;
    if (!parent)
      return -1;
    struct MuttWindow *first = TAILQ_FIRST(&parent->children);
    if (!first)
      return -1;

    if ((C_StatusOnTop && (first == win_index)) || (!C_StatusOnTop && (first != win_index)))
    {
      // Swap the Index and the Index Bar Windows
      TAILQ_REMOVE(&parent->children, first, entries);
      TAILQ_INSERT_TAIL(&parent->children, first, entries);
    }

    parent = win_pager->parent;
    first = TAILQ_FIRST(&parent->children);

    if ((C_StatusOnTop && (first == win_pager)) || (!C_StatusOnTop && (first != win_pager)))
    {
      // Swap the Pager and Pager Bar Windows
      TAILQ_REMOVE(&parent->children, first, entries);
      TAILQ_INSERT_TAIL(&parent->children, first, entries);
    }
    goto reflow;
  }

  if (mutt_str_equal(ec->name, "pager_index_lines"))
  {
    struct MuttWindow *parent = win_pager->parent;
    if (parent->state.visible)
    {
      int vcount = (Context && Context->mailbox) ? Context->mailbox->vcount : 0;
      win_index->req_rows = MIN(C_PagerIndexLines, vcount);
      win_index->size = MUTT_WIN_SIZE_FIXED;

      win_index->parent->size = MUTT_WIN_SIZE_MINIMISE;
      win_index->parent->state.visible = (C_PagerIndexLines != 0);
    }
    else
    {
      win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
      win_index->size = MUTT_WIN_SIZE_MAXIMISE;

      win_index->parent->size = MUTT_WIN_SIZE_MAXIMISE;
      win_index->parent->state.visible = true;
    }
  }

reflow:
  mutt_window_reflow(dlg);
  return 0;
}
