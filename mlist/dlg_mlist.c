/**
 * @file
 * Mailing-list action dialog
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page mlist_dlg_mlist Mailing-list action dialog
 *
 * Mailing-list action dialog
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "functions.h"
#include "mutt_logging.h"
#include "mx.h"

/// Help Bar for the Mailing-list action dialog
static const struct Mapping MlistHelp[] = {
  // clang-format off
  { N_("Exit"),        OP_EXIT },
  { N_("Archive"),     OP_LIST_ARCHIVE },
  { N_("Help"),        OP_LIST_HELP },
  { N_("Owner"),       OP_LIST_OWNER },
  { N_("Post"),        OP_LIST_POST },
  { N_("Subscribe"),   OP_LIST_SUBSCRIBE },
  { N_("Unsubscribe"), OP_LIST_UNSUBSCRIBE },
  { NULL, 0 },
  // clang-format on
};

/**
 * mlist_data_free - Free list dialog data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void mlist_data_free(struct Menu *menu, void **ptr)
{
  struct ListData *ld = *ptr;
  if (!ld)
    return;

  ARRAY_FREE(&ld->entries);
  mutt_rfc2369_list_headers_free(&ld->headers);
  FREE(ptr);
}

/**
 * mlist_data_add_entries - Add menu rows for one mailing-list action
 * @param ld     Dialog state
 * @param action Mailing-list action
 */
static void mlist_data_add_entries(struct ListData *ld, const struct ListAction *action)
{
  struct ListHead *values = mlist_action_value(&ld->headers, action);
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, values, entries)
  {
    const struct ListEntry entry = { action, np->data };
    ARRAY_ADD(&ld->entries, entry);
  }
}

/**
 * mlist_make_entry - Format a mailing-list action for the dialog - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static int mlist_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  struct ListData *ld = menu->mdata;
  if (!ld)
    return 0;

  const struct ListEntry *entry = ARRAY_GET(&ld->entries, line);
  if (!entry)
    return 0;

  const char *name = _(entry->action->name);

  buf_strcpy(buf, name);
  for (int i = mutt_strwidth(name); i < ld->label_width; i++)
    buf_addch(buf, ' ');
  buf_addstr(buf, ": ");
  buf_addstr(buf, entry->value);

  return mutt_strwidth(buf_string(buf));
}

/**
 * dlg_mlist - Display mailing-list actions for an email - @ingroup gui_dlg
 * @param m Mailbox containing the email
 * @param e Email to inspect
 */
void dlg_mlist(struct Mailbox *m, struct Email *e)
{
  if (!m || !e)
    return;

  struct MenuDefinition *md_list = menu_find(MENU_LIST);
  ASSERT(md_list);

  struct ListData *ld = MUTT_MEM_CALLOC(1, struct ListData);
  ld->mailbox = m;

  struct Message *msg = mx_msg_open(m, e);
  if (!msg)
  {
    FREE(&ld);
    return;
  }

  if (!mutt_file_seek(msg->fp, e->offset, SEEK_SET))
  {
    mutt_error(_("Unable to read mailing list headers"));
    mx_msg_close(m, &msg);
    FREE(&ld);
    return;
  }

  mutt_rfc2369_read_headers(msg->fp, &ld->headers);
  mx_msg_close(m, &msg);

  for (int i = 0; i < ListActionsCount; i++)
    mlist_data_add_entries(ld, &ListActions[i]);

  if (ARRAY_EMPTY(&ld->entries))
  {
    mutt_warning(_("No mailing list actions available"));
    mutt_rfc2369_list_headers_free(&ld->headers);
    FREE(&ld);
    return;
  }

  for (int i = 0; i < ListActionsCount; i++)
    ld->label_width = MAX(ld->label_width, mutt_strwidth(_(ListActions[i].name)));

  struct SimpleDialogWindows sdw = simple_dialog_new(md_list, WT_DLG_MLIST, MlistHelp);
  struct Menu *menu = sdw.menu;
  ld->menu = menu;
  menu->max = ARRAY_SIZE(&ld->entries);
  menu->make_entry = mlist_make_entry;
  menu->mdata = ld;
  menu->mdata_free = mlist_data_free;

  sbar_set_title(sdw.sbar, _("Available mailing list actions"));

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  struct KeyEvent event = { 0, OP_NULL };
  do
  {
    menu_tagging_dispatcher(menu->win, &event);
    window_redraw(NULL);

    event = km_dokey(md_list, GETCH_NONE);
    op = event.op;
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(md_list);
      continue;
    }
    mutt_clear_error();

    int rc = mlist_function_dispatcher(sdw.dlg, &event);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, &event);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(menu->win, &event);
  } while (!ld->done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&sdw.dlg);
}
