/**
 * @file
 * Autocrypt functions
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page autocrypt_functions Autocrypt functions
 *
 * Autocrypt functions
 */

#include "config.h"
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "lib.h"
#include "menu/lib.h"
#include "question/lib.h"
#include "opcodes.h"

/**
 * toggle_active - Toggle whether an Autocrypt account is active
 * @param entry Menu Entry for the account
 */
static void toggle_active(struct AccountEntry *entry)
{
  entry->account->enabled = !entry->account->enabled;
  if (mutt_autocrypt_db_account_update(entry->account) != 0)
  {
    entry->account->enabled = !entry->account->enabled;
    /* L10N: This error message is displayed if a database update of an
       account record fails for some odd reason.  */
    mutt_error(_("Error updating account record"));
  }
}

/**
 * toggle_prefer_encrypt - Toggle whether an Autocrypt account prefers encryption
 * @param entry Menu Entry for the account
 */
static void toggle_prefer_encrypt(struct AccountEntry *entry)
{
  entry->account->prefer_encrypt = !entry->account->prefer_encrypt;
  if (mutt_autocrypt_db_account_update(entry->account))
  {
    entry->account->prefer_encrypt = !entry->account->prefer_encrypt;
    mutt_error(_("Error updating account record"));
  }
}

// -----------------------------------------------------------------------------

/**
 * op_autocrypt_create_acct - Create a new autocrypt account - Implements ::autocrypt_function_t - @ingroup autocrypt_function_api
 */
static int op_autocrypt_create_acct(struct AutocryptData *ad, int op)
{
  if (mutt_autocrypt_account_init(false) == 0)
    populate_menu(ad->menu);

  return FR_SUCCESS;
}

/**
 * op_autocrypt_delete_acct - Delete the current account - Implements ::autocrypt_function_t - @ingroup autocrypt_function_api
 */
static int op_autocrypt_delete_acct(struct AutocryptData *ad, int op)
{
  if (!ad->menu->mdata)
    return FR_ERROR;

  const int index = menu_get_index(ad->menu);
  struct AccountEntry *entry = ((struct AccountEntry *) ad->menu->mdata) + index;
  char msg[128];
  snprintf(msg, sizeof(msg),
           // L10N: Confirmation message when deleting an autocrypt account
           _("Really delete account \"%s\"?"), entry->addr->mailbox);
  if (mutt_yesorno(msg, MUTT_NO) != MUTT_YES)
    return FR_NO_ACTION;

  if (mutt_autocrypt_db_account_delete(entry->account) == 0)
    populate_menu(ad->menu);

  return FR_SUCCESS;
}

/**
 * op_autocrypt_toggle_active - Toggle the current account active/inactive - Implements ::autocrypt_function_t - @ingroup autocrypt_function_api
 */
static int op_autocrypt_toggle_active(struct AutocryptData *ad, int op)
{
  if (!ad->menu->mdata)
    return FR_ERROR;

  const int index = menu_get_index(ad->menu);
  struct AccountEntry *entry = ((struct AccountEntry *) ad->menu->mdata) + index;
  toggle_active(entry);
  menu_queue_redraw(ad->menu, MENU_REDRAW_FULL);

  return FR_SUCCESS;
}

/**
 * op_autocrypt_toggle_prefer - Toggle the current account prefer-encrypt flag - Implements ::autocrypt_function_t - @ingroup autocrypt_function_api
 */
static int op_autocrypt_toggle_prefer(struct AutocryptData *ad, int op)
{
  if (!ad->menu->mdata)
    return FR_ERROR;

  const int index = menu_get_index(ad->menu);
  struct AccountEntry *entry = (struct AccountEntry *) (ad->menu->mdata) + index;
  toggle_prefer_encrypt(entry);
  menu_queue_redraw(ad->menu, MENU_REDRAW_FULL);

  return FR_SUCCESS;
}

/**
 * op_exit - Exit this menu - Implements ::autocrypt_function_t - @ingroup autocrypt_function_api
 */
static int op_exit(struct AutocryptData *ad, int op)
{
  ad->done = true;
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * AutocryptFunctions - All the NeoMutt functions that the Autocrypt supports
 */
struct AutocryptFunction AutocryptFunctions[] = {
  // clang-format off
  { OP_AUTOCRYPT_CREATE_ACCT,   op_autocrypt_create_acct },
  { OP_AUTOCRYPT_DELETE_ACCT,   op_autocrypt_delete_acct },
  { OP_AUTOCRYPT_TOGGLE_ACTIVE, op_autocrypt_toggle_active },
  { OP_AUTOCRYPT_TOGGLE_PREFER, op_autocrypt_toggle_prefer },
  { OP_EXIT,                    op_exit },
  { 0, NULL },
  // clang-format on
};

/**
 * autocrypt_function_dispatcher - Perform a Autocrypt function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int autocrypt_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct AutocryptData *ad = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; AutocryptFunctions[i].op != OP_NULL; i++)
  {
    const struct AutocryptFunction *fn = &AutocryptFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(ad, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
