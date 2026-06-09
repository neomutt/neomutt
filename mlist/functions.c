/**
 * @file
 * Mailing-list functions
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
 * @page mlist_functions Mailing-list functions
 *
 * Mailing-list functions
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "module_data.h"

/// Mailing-list actions shown in the dialog
const struct ListAction ListActions[] = {
  // clang-format off
  { N_("Help"),        OP_LIST_HELP,        offsetof(struct Rfc2369ListHeaders, help)        },
  { N_("Post"),        OP_LIST_POST,        offsetof(struct Rfc2369ListHeaders, post)        },
  { N_("Subscribe"),   OP_LIST_SUBSCRIBE,   offsetof(struct Rfc2369ListHeaders, subscribe)   },
  { N_("Unsubscribe"), OP_LIST_UNSUBSCRIBE, offsetof(struct Rfc2369ListHeaders, unsubscribe) },
  { N_("Archives"),    OP_LIST_ARCHIVE,     offsetof(struct Rfc2369ListHeaders, archive)     },
  { N_("Owner"),       OP_LIST_OWNER,       offsetof(struct Rfc2369ListHeaders, owner)       },
  // clang-format on
};

/// Number of entries in #ListActions
const int ListActionsCount = countof(ListActions);

// clang-format off
/**
 * OpList - Functions for the List Dialog
 */
static const struct MenuFuncOp OpList[] = { /* map: list */
  { "exit",                          OP_EXIT },
  { "list-archive",                  OP_LIST_ARCHIVE },
  { "list-help",                     OP_LIST_HELP },
  { "list-owner",                    OP_LIST_OWNER },
  { "list-post",                     OP_LIST_POST },
  { "list-subscribe",                OP_LIST_SUBSCRIBE },
  { "list-unsubscribe",              OP_LIST_UNSUBSCRIBE },
  { NULL, 0 },
};

/**
 * ListDefaultBindings - Key bindings for the List Dialog
 */
static const struct MenuOpSeq ListDefaultBindings[] = { /* map: list */
  { OP_EXIT,                               "q" },
  { OP_LIST_ARCHIVE,                       "a" },
  { OP_LIST_HELP,                          "h" },
  { OP_LIST_OWNER,                         "o" },
  { OP_LIST_POST,                          "p" },
  { OP_LIST_SUBSCRIBE,                     "s" },
  { OP_LIST_UNSUBSCRIBE,                   "u" },
  { 0, NULL },
};
// clang-format on

/**
 * mlist_init_keys - Initialise the Mlist Keybindings - Implements ::init_keys_api
 */
void mlist_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct MenuDefinition *md_list = NULL;
  struct SubMenu *sm_list = NULL;

  sm_list = km_register_submenu(OpList);
  md_list = km_register_menu(MENU_LIST, "list");
  km_menu_add_submenu(md_list, sm_list);
  km_menu_add_submenu(md_list, sm_generic);
  km_menu_add_bindings(md_list, ListDefaultBindings);

  struct MlistModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_MLIST);
  ASSERT(mod_data);
  mod_data->menu_mlist = md;
}

/**
 * mlist_action_value - Get the stored value for a mailing-list action
 * @param headers Parsed List-* headers
 * @param action  Action definition
 * @retval ptr Pointer to the stored header values
 */
struct ListHead *mlist_action_value(struct Rfc2369ListHeaders *headers,
                                    const struct ListAction *action)
{
  return (struct ListHead *) (((char *) headers) + action->offset);
}

/**
 * compose_list_action - Compose a message for a mailing-list action
 * @param ld      Dialog state
 * @param entry   Mailing-list entry
 * @retval true The dialog should exit
 * @retval false The dialog should remain open
 */
static bool compose_list_action(struct ListData *ld, const struct ListEntry *entry)
{
  if (!entry || !entry->value)
  {
    mutt_error(_("No list action available"));
    return false;
  }

  const char *uri = entry->value;
  if (!mutt_istr_startswith(uri, "mailto:"))
    return false;

  struct Email *e = email_new();
  e->env = mutt_env_new();

  char *body = NULL;
  if (!mutt_parse_mailto(e->env, &body, uri))
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
 *
 * For a specific list opcode (e.g. OP_LIST_UNSUBSCRIBE) the matching action is used.
 * If there are several matches (e.g. mailto: and https: links), the first match
 * is selected, but the action is not performed.
 * For a generic selection the currently highlighted action is used.
 */
static int op_select_action(struct ListData *ld, const struct KeyEvent *event)
{
  const struct ListEntry *entry = NULL;

  if (event && (event->op != OP_GENERIC_SELECT_ENTRY))
  {
    int first_match = -1;
    int num_matches = 0;
    for (int i = 0; i < ARRAY_SIZE(&ld->entries); i++)
    {
      const struct ListEntry *candidate = ARRAY_GET(&ld->entries, i);
      if (candidate && (candidate->action->op == event->op))
      {
        if (first_match < 0)
          first_match = i;
        num_matches++;
      }
    }

    if (num_matches == 0)
      return FR_NO_ACTION;

    menu_set_index(ld->menu, first_match);

    // Several matches: just select the first, let the user choose
    if (num_matches > 1)
      return FR_SUCCESS;

    entry = ARRAY_GET(&ld->entries, first_match);
  }
  else
  {
    entry = ARRAY_GET(&ld->entries, menu_get_index(ld->menu));
  }

  if (!entry)
    return FR_NO_ACTION;

  ld->done = compose_list_action(ld, entry);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * MlistFunctions - All the NeoMutt functions that the List Dialog supports
 */
static const struct MlistFunction MlistFunctions[] = {
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
 * mlist_function_dispatcher - Perform a List dialog function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int mlist_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
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
  for (size_t i = 0; MlistFunctions[i].op != OP_NULL; i++)
  {
    if (MlistFunctions[i].op == event->op)
    {
      rc = MlistFunctions[i].function(ld, event);
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
