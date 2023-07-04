/**
 * @file
 * Dump all notifications
 *
 * @authors
 * Copyright (C) 2019-2020 Richard Russon <rich@flatcap.org>
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
 * @page debug_notify Dump all notifications
 *
 * Dump all notifications
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "mview.h"

extern const struct Mapping ColorFields[];
extern const struct Mapping ComposeColorFields[];

static void notify_dump_account(struct NotifyCallback *nc)
{
  struct EventAccount *ev_a = nc->event_data;
  struct Account *a = ev_a->account;
  if (!a)
    return;

  mutt_debug(LL_DEBUG1, "    Account: %p (%s) %s\n", a,
             name_mailbox_type(a->type), NONULL(a->name));
}

static void notify_dump_color(struct NotifyCallback *nc)
{
  struct EventColor *ev_c = nc->event_data;

  const char *color = NULL;
  const char *scope = "";

  if (ev_c->cid == MT_COLOR_MAX)
    color = "ALL";

  if (!color)
    color = mutt_map_get_name(ev_c->cid, ColorFields);

  if (!color)
  {
    color = mutt_map_get_name(ev_c->cid, ComposeColorFields);
    scope = "compose ";
  }

  if (!color)
    color = "UNKNOWN";

  mutt_debug(LL_DEBUG1, "    Color: %s %s%s (%d)\n",
             (nc->event_subtype == NT_COLOR_SET) ? "set" : "reset", scope,
             color, ev_c->cid);
}

static void notify_dump_command(struct NotifyCallback *nc)
{
  struct Command *cmd = nc->event_data;

  if (cmd->data < 4096)
    mutt_debug(LL_DEBUG1, "    Command: %s, data: %ld\n", cmd->name, cmd->data);
  else
    mutt_debug(LL_DEBUG1, "    Command: %s, data: %p\n", cmd->name, (void *) cmd->data);
}

static void notify_dump_config(struct NotifyCallback *nc)
{
  struct EventConfig *ev_c = nc->event_data;

  struct Buffer value = buf_make(128);
  cs_he_string_get(ev_c->sub->cs, ev_c->he, &value);
  mutt_debug(LL_DEBUG1, "    Config: %s %s = %s\n",
             name_notify_config(nc->event_subtype), ev_c->name, buf_string(&value));
  buf_dealloc(&value);
}

static void notify_dump_mview(struct NotifyCallback *nc)
{
  struct EventMview *ev_m = nc->event_data;

  const char *path = "NONE";
  if (ev_m->mv && ev_m->mv->mailbox)
    path = mailbox_path(ev_m->mv->mailbox);

  mutt_debug(LL_DEBUG1, "    MailboxView: %s %s\n",
             name_notify_mview(nc->event_subtype), path);
}

static void notify_dump_email(struct NotifyCallback *nc)
{
  struct EventEmail *ev_e = nc->event_data;

  mutt_debug(LL_DEBUG1, "    Email: %d\n", ev_e->num_emails);
  for (size_t i = 0; i < ev_e->num_emails; i++)
  {
    mutt_debug(LL_DEBUG1, "        : %p\n", ev_e->emails[i]);
  }
}

static void notify_dump_global(struct NotifyCallback *nc)
{
  mutt_debug(LL_DEBUG1, "    Global: %s\n", name_notify_global(nc->event_subtype));
}

static void notify_dump_mailbox(struct NotifyCallback *nc)
{
  struct EventMailbox *ev_m = nc->event_data;

  struct Mailbox *m = ev_m->mailbox;
  const char *path = m ? mailbox_path(m) : "";
  mutt_debug(LL_DEBUG1, "    Mailbox: %s %s\n",
             name_notify_mailbox(nc->event_subtype), path);
}

static void notify_dump_window_state(struct NotifyCallback *nc)
{
  struct EventWindow *ev_w = nc->event_data;
  const struct MuttWindow *win = ev_w->win;
  WindowNotifyFlags flags = ev_w->flags;

  struct Buffer buf = buf_make(128);

  buf_add_printf(&buf, "[%s] ", mutt_window_win_name(win));

  if (flags & WN_VISIBLE)
    buf_addstr(&buf, "visible ");
  if (flags & WN_HIDDEN)
    buf_addstr(&buf, "hidden ");

  if (flags & WN_MOVED)
  {
    buf_add_printf(&buf, "moved (C%d,R%d)->(C%d,R%d) ", win->old.col_offset,
                   win->old.row_offset, win->state.col_offset, win->state.row_offset);
  }

  if (flags & WN_TALLER)
    buf_add_printf(&buf, "taller [%d->%d] ", win->old.rows, win->state.rows);
  if (flags & WN_SHORTER)
    buf_add_printf(&buf, "shorter [%d->%d] ", win->old.rows, win->state.rows);
  if (flags & WN_WIDER)
    buf_add_printf(&buf, "wider [%d->%d] ", win->old.cols, win->state.cols);
  if (flags & WN_NARROWER)
    buf_add_printf(&buf, "narrower [%d->%d] ", win->old.cols, win->state.cols);

  mutt_debug(LL_DEBUG1, "    Window: %s\n", buf_string(&buf));

  buf_dealloc(&buf);
}

static void notify_dump_window_focus(struct NotifyCallback *nc)
{
  struct EventWindow *ev_w = nc->event_data;
  struct MuttWindow *win = ev_w->win;

  struct Buffer buf = buf_make(128);

  buf_addstr(&buf, "Focus: ");

  if (win)
  {
    struct MuttWindow *dlg = dialog_find(win);
    if (dlg && (dlg != win))
      buf_add_printf(&buf, "%s:", mutt_window_win_name(dlg));

    buf_add_printf(&buf, "%s ", mutt_window_win_name(win));

    buf_add_printf(&buf, "(C%d,R%d) [%dx%d]", win->state.col_offset,
                   win->state.row_offset, win->state.cols, win->state.rows);
  }
  else
  {
    buf_addstr(&buf, "NONE");
  }

  mutt_debug(LL_DEBUG1, "    Window: %s\n", buf_string(&buf));

  buf_dealloc(&buf);
}

int debug_all_observer(struct NotifyCallback *nc)
{
  mutt_debug(LL_DEBUG1, "\033[1;31mNotification:\033[0m %s\n",
             name_notify_type(nc->event_type));

  switch (nc->event_type)
  {
    case NT_ACCOUNT:
      notify_dump_account(nc);
      break;
    case NT_COLOR:
      notify_dump_color(nc);
      break;
    case NT_COMMAND:
      notify_dump_command(nc);
      break;
    case NT_CONFIG:
      notify_dump_config(nc);
      break;
    case NT_MVIEW:
      notify_dump_mview(nc);
      break;
    case NT_EMAIL:
      notify_dump_email(nc);
      break;
    case NT_GLOBAL:
      notify_dump_global(nc);
      break;
    case NT_MAILBOX:
      notify_dump_mailbox(nc);
      break;
    case NT_WINDOW:
      if (nc->event_subtype == NT_WINDOW_STATE)
        notify_dump_window_state(nc);
      else if (nc->event_subtype == NT_WINDOW_FOCUS)
        notify_dump_window_focus(nc);
      break;
    default:
      mutt_debug(LL_DEBUG1, "    Event Type: %d\n", nc->event_type);
      mutt_debug(LL_DEBUG1, "    Event Sub-type: %d\n", nc->event_subtype);
      mutt_debug(LL_DEBUG1, "    Event Data: %p\n", nc->event_data);
      break;
  }

  mutt_debug(LL_DEBUG1, "    Global Data: %p\n", nc->global_data);

  mutt_debug(LL_DEBUG5, "debug done\n");
  return 0;
}
