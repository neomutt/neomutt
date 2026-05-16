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

#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "module_data.h"
#include "mutt_logging.h"
#include "mx.h"

/// Help Bar for the Mailing-list action dialog
static const struct Mapping ListHelp[] = {
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
 * struct ListAction - A mailing-list action in the dialog
 */
struct ListAction
{
  const char *name; ///< Label for the action
  int op;           ///< Op code
  size_t offset;    ///< Offset into struct Rfc2369ListHeaders
};

/// Mailing-list actions shown in the dialog
static const struct ListAction ListActions[] = {
  // clang-format off
  { N_("Help"),        OP_LIST_HELP,        offsetof(struct Rfc2369ListHeaders, help)        },
  { N_("Post"),        OP_LIST_POST,        offsetof(struct Rfc2369ListHeaders, post)        },
  { N_("Subscribe"),   OP_LIST_SUBSCRIBE,   offsetof(struct Rfc2369ListHeaders, subscribe)   },
  { N_("Unsubscribe"), OP_LIST_UNSUBSCRIBE, offsetof(struct Rfc2369ListHeaders, unsubscribe) },
  { N_("Archives"),    OP_LIST_ARCHIVE,     offsetof(struct Rfc2369ListHeaders, archive)     },
  { N_("Owner"),       OP_LIST_OWNER,       offsetof(struct Rfc2369ListHeaders, owner)       },
  // clang-format on
};

/**
 * struct ListData - Private data for the Mailing-list action dialog
 */
struct ListData
{
  struct Menu *menu;                 ///< Dialog menu
  struct Mailbox *mailbox;           ///< Source mailbox
  struct Rfc2369ListHeaders headers; ///< Parsed List-* headers
  int label_width;                   ///< Width of the longest action label
  bool done;                         ///< Exit the dialog
};

/**
 * get_action_value - Get the stored value for a mailing-list action
 * @param headers Parsed List-* headers
 * @param action  Action definition
 * @retval ptr Pointer to the stored header value
 */
static char **get_action_value(struct Rfc2369ListHeaders *headers,
                               const struct ListAction *action)
{
  return (char **) (((char *) headers) + action->offset);
}

/**
 * list_data_free - Free list dialog data - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void list_data_free(struct Menu *menu, void **ptr)
{
  struct ListData *ld = *ptr;
  if (!ld)
    return;

  mutt_rfc2369_list_headers_free(&ld->headers);
  FREE(ptr);
}

/**
 * list_make_entry - Format a mailing-list action for the dialog - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static int list_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  struct ListData *ld = menu->mdata;
  if (!ld || (line < 0) || (line >= (int) countof(ListActions)))
    return 0;

  const char *name = _(ListActions[line].name);
  const char *value = *get_action_value(&ld->headers, &ListActions[line]);

  buf_strcpy(buf, name);
  for (int i = mutt_strwidth(name); i < ld->label_width; i++)
    buf_addch(buf, ' ');
  buf_addstr(buf, ": ");
  buf_addstr(buf, value ? value : "--");

  return mutt_strwidth(buf_string(buf));
}

/**
 * compose_list_action - Compose a message for a mailing-list action
 * @param ld      Dialog state
 * @param action  Mailing-list action
 * @retval true The dialog should exit
 * @retval false The dialog should remain open
 */
static bool compose_list_action(struct ListData *ld, const struct ListAction *action)
{
  char *mailto = *get_action_value(&ld->headers, action);
  if (!mailto)
  {
    mutt_error(_("No list action available for %s"), _(action->name));
    return false;
  }

  if (url_check_scheme(mailto) != U_MAILTO)
  {
    mutt_error(_("List actions only support mailto: URIs. (Try a browser?)"));
    return true;
  }

  struct Email *e = email_new();
  e->env = mutt_env_new();

  char *body = NULL;
  if (!mutt_parse_mailto(e->env, &body, mailto))
  {
    mutt_error(_("Could not parse mailto: URI"));
    FREE(&body);
    email_free(&e);
    return true;
  }

  e->body = mutt_body_new();
  char ctype[] = "text/plain";
  mutt_parse_content_type(ctype, e->body);
  e->body->use_disp = false;
  e->body->disposition = DISP_INLINE;

  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp_draft(tempfile);
  FILE *fp = mutt_file_fopen(buf_string(tempfile), "w+");
  if (!fp)
  {
    mutt_perror("%s", buf_string(tempfile));
    FREE(&body);
    email_free(&e);
    buf_pool_release(&tempfile);
    return true;
  }

  if (body)
    fprintf(fp, "%s\n", body);
  mutt_file_fclose(&fp);
  FREE(&body);

  e->body->filename = buf_strdup(tempfile);
  e->body->unlink = true;
  buf_pool_release(&tempfile);

  (void) mutt_send_message(SEND_DRAFT_FILE, e, NULL, ld->mailbox, NULL, NeoMutt->sub);
  return true;
}

/**
 * op_exit - Exit the list dialog
 * @param ld    Dialog state
 * @param event Event being handled
 * @retval enum #FunctionRetval
 */
static int op_exit(struct ListData *ld, const struct KeyEvent *event)
{
  ld->done = true;
  return FR_SUCCESS;
}

/**
 * op_select_action - Execute the selected mailing-list action
 * @param ld    Dialog state
 * @param event Event being handled
 * @retval enum #FunctionRetval
 */
static int op_select_action(struct ListData *ld, const struct KeyEvent *event)
{
  const int index = menu_get_index(ld->menu);
  if ((index < 0) || (index >= (int) countof(ListActions)))
    return FR_NO_ACTION;

  ld->done = compose_list_action(ld, &ListActions[index]);
  return FR_SUCCESS;
}

/**
 * struct ListFunction - A list dialog function
 */
struct ListFunction
{
  int op; ///< Op code
  int (*function)(struct ListData *ld, const struct KeyEvent *event); ///< Function
};

/// Functions handled by the list dialog
static const struct ListFunction ListFunctions[] = {
  // clang-format off
  { OP_EXIT,                 op_exit          },
  { OP_GENERIC_SELECT_ENTRY, op_select_action },
  { OP_LIST_ARCHIVE,         op_select_action },
  { OP_LIST_HELP,            op_select_action },
  { OP_LIST_OWNER,           op_select_action },
  { OP_LIST_POST,            op_select_action },
  { OP_LIST_SUBSCRIBE,       op_select_action },
  { OP_LIST_UNSUBSCRIBE,     op_select_action },
  { 0, NULL },
  // clang-format on
};

/**
 * list_function_dispatcher - Perform a List dialog function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
static int list_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  struct MuttWindow *dlg = dialog_find(win);
  if (!event || !dlg || !dlg->wdata)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  struct Menu *menu = dlg->wdata;
  struct ListData *ld = menu->mdata;
  if (!ld)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  int rc = FR_UNKNOWN;
  for (size_t i = 0; ListFunctions[i].op != OP_NULL; i++)
  {
    if (ListFunctions[i].op == event->op)
    {
      rc = ListFunctions[i].function(ld, event);
      break;
    }
  }

  if (rc == FR_UNKNOWN)
    return rc;

  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(event->op),
             event->op, NONULL(dispatcher_get_retval_name(rc)));
  dispatcher_flush_on_error(rc);
  return rc;
}

/**
 * dlg_list - Display mailing-list actions for an email - @ingroup gui_dlg
 * @param m Mailbox containing the email
 * @param e Email to inspect
 */
void dlg_list(struct Mailbox *m, struct Email *e)
{
  if (!m || !e)
    return;

  struct IndexModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_INDEX);
  ASSERT(mod_data);

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

  for (size_t i = 0; i < countof(ListActions); i++)
    ld->label_width = MAX(ld->label_width, mutt_strwidth(_(ListActions[i].name)));

  struct SimpleDialogWindows sdw = simple_dialog_new(mod_data->menu_list,
                                                     WT_DLG_LIST, ListHelp);
  struct Menu *menu = sdw.menu;
  ld->menu = menu;
  menu->max = countof(ListActions);
  menu->make_entry = list_make_entry;
  menu->mdata = ld;
  menu->mdata_free = list_data_free;

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

    event = km_dokey(mod_data->menu_list, GETCH_NONE);
    op = event.op;
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(mod_data->menu_list);
      continue;
    }
    mutt_clear_error();

    int rc = list_function_dispatcher(sdw.dlg, &event);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, &event);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(menu->win, &event);
  } while (!ld->done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&sdw.dlg);
}
