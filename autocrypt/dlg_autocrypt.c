/**
 * @file
 * Autocrypt account menu
 *
 * @authors
 * Copyright (C) 2019 Kevin J. McCarthy <kevin@8t8.us>
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
 * @page autocrypt_dlg_autocrypt Autocrypt account dialog
 *
 * The Autocrypt Account Dialog lets the user set up or update an Autocrypt Account.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                     | Type             | See Also        |
 * | :----------------------- | :--------------- | :-------------- |
 * | Autocrypt Account Dialog | WT_DLG_AUTOCRYPT | dlg_autocrypt() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #AccountEntry
 *
 * The @ref gui_simple holds a Menu.  The Autocrypt Account Dialog stores its
 * data (#AccountEntry) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                     |
 * | :---------- | :-------------------------- |
 * | #NT_CONFIG  | autocrypt_config_observer() |
 * | #NT_WINDOW  | autocrypt_window_observer() |
 *
 * The Autocrypt Account Dialog doesn't have any specific colours, so it doesn't
 * need to support #NT_COLOR.
 *
 * The Autocrypt Account Dialog does not implement MuttWindow::recalc() or MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "format_flags.h"
#include "functions.h"
#include "mutt_logging.h"
#include "muttlib.h"

/// Help Bar for the Autocrypt Account selection dialog
static const struct Mapping AutocryptHelp[] = {
  // clang-format off
  { N_("Exit"),       OP_EXIT },
  /* L10N: Autocrypt Account Menu Help line:
     create new account */
  { N_("Create"),     OP_AUTOCRYPT_CREATE_ACCT },
  /* L10N: Autocrypt Account Menu Help line:
     delete account */
  { N_("Delete"),     OP_AUTOCRYPT_DELETE_ACCT },
  /* L10N: Autocrypt Account Menu Help line:
     toggle an account active/inactive
     The words here are abbreviated to keep the help line compact.
     It currently has the content:
     q:Exit  c:Create  D:Delete  a:Tgl Active  p:Prf Encr  ?:Help */
  { N_("Tgl Active"), OP_AUTOCRYPT_TOGGLE_ACTIVE },
  /* L10N: Autocrypt Account Menu Help line:
     toggle "prefer-encrypt" on an account
     The words here are abbreviated to keep the help line compact.
     It currently has the content:
     q:Exit  c:Create  D:Delete  a:Tgl Active  p:Prf Encr  ?:Help */
  { N_("Prf Encr"),   OP_AUTOCRYPT_TOGGLE_PREFER },
  { N_("Help"),       OP_HELP },
  { NULL, 0 }
  // clang-format on
};

/**
 * autocrypt_format_str - Format a string for the Autocrypt account list - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :----------------------------------------------------------------
 * | \%a     | Email address
 * | \%k     | Gpg keyid
 * | \%n     | Current entry number
 * | \%p     | Prefer-encrypt flag
 * | \%s     | Status flag (active/inactive)
 */
static const char *autocrypt_format_str(char *buf, size_t buflen, size_t col, int cols,
                                        char op, const char *src, const char *prec,
                                        const char *if_str, const char *else_str,
                                        intptr_t data, MuttFormatFlags flags)
{
  struct AccountEntry *entry = (struct AccountEntry *) data;
  char tmp[128] = { 0 };

  switch (op)
  {
    case 'a':
      mutt_format_s(buf, buflen, prec, buf_string(entry->addr->mailbox));
      break;
    case 'k':
      mutt_format_s(buf, buflen, prec, entry->account->keyid);
      break;
    case 'n':
      snprintf(tmp, sizeof(tmp), "%%%sd", prec);
      snprintf(buf, buflen, tmp, entry->num);
      break;
    case 'p':
      if (entry->account->prefer_encrypt)
      {
        /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt set */
        mutt_format_s(buf, buflen, prec, _("prefer encrypt"));
      }
      else
      {
        /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt unset;
           thus encryption will need to be manually enabled.  */
        mutt_format_s(buf, buflen, prec, _("manual encrypt"));
      }
      break;
    case 's':
      if (entry->account->enabled)
      {
        /* L10N: Autocrypt Account menu.
           flag that an account is enabled/active */
        mutt_format_s(buf, buflen, prec, _("active"));
      }
      else
      {
        /* L10N: Autocrypt Account menu.
           flag that an account is disabled/inactive */
        mutt_format_s(buf, buflen, prec, _("inactive"));
      }
      break;
  }

  return (src);
}

/**
 * autocrypt_make_entry - Format an Autocrypt Account for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $autocrypt_acct_format, autocrypt_format_str()
 */
static void autocrypt_make_entry(struct Menu *menu, char *buf, size_t buflen, int num)
{
  struct AccountEntry *entry = &((struct AccountEntry *) menu->mdata)[num];

  const char *const c_autocrypt_acct_format = cs_subset_string(NeoMutt->sub, "autocrypt_acct_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                      NONULL(c_autocrypt_acct_format), autocrypt_format_str,
                      (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * autocrypt_menu_free - Free the Autocrypt account Menu - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void autocrypt_menu_free(struct Menu *menu, void **ptr)
{
  struct AccountEntry *entries = *ptr;

  for (size_t i = 0; i < menu->max; i++)
  {
    mutt_autocrypt_db_account_free(&entries[i].account);
    mutt_addr_free(&entries[i].addr);
  }

  FREE(ptr);
}

/**
 * populate_menu - Add the Autocrypt data to a Menu
 * @param menu Menu to populate
 * @retval true Success
 */
bool populate_menu(struct Menu *menu)
{
  // Clear out any existing data
  autocrypt_menu_free(menu, &menu->mdata);
  menu->max = 0;

  struct AutocryptAccount **accounts = NULL;
  int num_accounts = 0;

  if (mutt_autocrypt_db_account_get_all(&accounts, &num_accounts) < 0)
    return false;

  struct AccountEntry *entries = mutt_mem_calloc(num_accounts, sizeof(struct AccountEntry));
  menu->mdata = entries;
  menu->mdata_free = autocrypt_menu_free;
  menu->max = num_accounts;

  for (int i = 0; i < num_accounts; i++)
  {
    entries[i].num = i + 1;
    /* note: we are transferring the account pointer to the entries
     * array, and freeing the accounts array below.  the account
     * will be freed in autocrypt_menu_free().  */
    entries[i].account = accounts[i];

    entries[i].addr = mutt_addr_new();
    entries[i].addr->mailbox = buf_new(accounts[i]->email_addr);
    mutt_addr_to_local(entries[i].addr);
  }
  FREE(&accounts);

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  return true;
}

/**
 * autocrypt_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_autocrypt`.
 */
static int autocrypt_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "autocrypt_acct_format"))
    return 0;

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * autocrypt_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int autocrypt_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->sub->notify, autocrypt_config_observer, menu);
  notify_observer_remove(win_menu->notify, autocrypt_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_autocrypt - Display the Autocrypt account Menu - @ingroup gui_dlg
 *
 * The Autocrypt Dialog lets the user select an Autocrypt Account to use.
 */
void dlg_autocrypt(void)
{
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (!c_autocrypt)
    return;

  if (mutt_autocrypt_init(false))
    return;

  struct MuttWindow *dlg = simple_dialog_new(MENU_AUTOCRYPT, WT_DLG_AUTOCRYPT, AutocryptHelp);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = autocrypt_make_entry;

  populate_menu(menu);

  struct AutocryptData ad = { false, menu };
  dlg->wdata = &ad;

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  // L10N: Autocrypt Account Management Menu title
  sbar_set_title(sbar, _("Autocrypt Accounts"));

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, autocrypt_config_observer, menu);
  notify_observer_add(menu->win->notify, NT_WINDOW, autocrypt_window_observer, menu->win);

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_AUTOCRYPT, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_AUTOCRYPT);
      continue;
    }
    mutt_clear_error();

    int rc = autocrypt_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!ad.done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
}
