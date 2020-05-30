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
 * @page autocrypt_account Autocrypt account menu
 *
 * Autocrypt account menu
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "format_flags.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "autocrypt/lib.h"

/**
 * struct AccountEntry - An entry in the Autocrypt account Menu
 */
struct AccountEntry
{
  int tagged; /* TODO */
  int num;
  struct AutocryptAccount *account;
  struct Address *addr;
};

static const struct Mapping AutocryptAcctHelp[] = {
  { N_("Exit"), OP_EXIT },
  /* L10N: Autocrypt Account Menu Help line:
     create new account */
  { N_("Create"), OP_AUTOCRYPT_CREATE_ACCT },
  /* L10N: Autocrypt Account Menu Help line:
     delete account */
  { N_("Delete"), OP_AUTOCRYPT_DELETE_ACCT },
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
  { N_("Prf Encr"), OP_AUTOCRYPT_TOGGLE_PREFER },
  { N_("Help"), OP_HELP },
  { NULL, 0 }
};

/**
 * account_format_str - Format a string for the Autocrypt account list - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:-----------------------------------------------------------------
 * | \%a     | Email address
 * | \%k     | Gpg keyid
 * | \%n     | Current entry number
 * | \%p     | Prefer-encrypt flag
 * | \%s     | Status flag (active/inactive)
 */
static const char *account_format_str(char *dest, size_t destlen, size_t col, int cols,
                                      char op, const char *src, const char *fmt,
                                      const char *ifstring, const char *elsestring,
                                      intptr_t data, MuttFormatFlags flags)
{
  struct AccountEntry *entry = (struct AccountEntry *) data;
  char tmp[128];

  switch (op)
  {
    case 'a':
      mutt_format_s(dest, destlen, fmt, entry->addr->mailbox);
      break;
    case 'k':
      mutt_format_s(dest, destlen, fmt, entry->account->keyid);
      break;
    case 'n':
      snprintf(tmp, sizeof(tmp), "%%%sd", fmt);
      snprintf(dest, destlen, tmp, entry->num);
      break;
    case 'p':
      if (entry->account->prefer_encrypt)
      {
        /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt set */
        mutt_format_s(dest, destlen, fmt, _("prefer encrypt"));
      }
      else
      {
        /* L10N: Autocrypt Account menu.
           flag that an account has prefer-encrypt unset;
           thus encryption will need to be manually enabled.  */
        mutt_format_s(dest, destlen, fmt, _("manual encrypt"));
      }
      break;
    case 's':
      if (entry->account->enabled)
      {
        /* L10N: Autocrypt Account menu.
           flag that an account is enabled/active */
        mutt_format_s(dest, destlen, fmt, _("active"));
      }
      else
      {
        /* L10N: Autocrypt Account menu.
           flag that an account is disabled/inactive */
        mutt_format_s(dest, destlen, fmt, _("inactive"));
      }
      break;
  }

  return (src);
}

/**
 * account_make_entry - Create a line for the Autocrypt account menu - Implements Menu::make_entry()
 * @param buf    Buffer to save the string
 * @param buflen Length of the buffer
 * @param menu   Menu to use
 * @param num    Line in the Menu
 */
static void account_make_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct AccountEntry *entry = &((struct AccountEntry *) menu->mdata)[num];

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_AutocryptAcctFormat), account_format_str,
                      IP entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * create_menu - Create the Autocrypt account Menu
 * @retval ptr New Menu
 */
static struct Menu *create_menu(void)
{
  struct AutocryptAccount **accounts = NULL;
  int num_accounts = 0;

  if (mutt_autocrypt_db_account_get_all(&accounts, &num_accounts) < 0)
    return NULL;

  struct Menu *menu = mutt_menu_new(MENU_AUTOCRYPT_ACCT);
  menu->make_entry = account_make_entry;
  /* menu->tag = account_tag; */
  // L10N: Autocrypt Account Management Menu title
  menu->title = _("Autocrypt Accounts");
  char *helpstr = mutt_mem_malloc(256);
  menu->help = mutt_compile_help(helpstr, 256, MENU_AUTOCRYPT_ACCT, AutocryptAcctHelp);

  struct AccountEntry *entries =
      mutt_mem_calloc(num_accounts, sizeof(struct AccountEntry));
  menu->mdata = entries;
  menu->max = num_accounts;

  for (int i = 0; i < num_accounts; i++)
  {
    entries[i].num = i + 1;
    /* note: we are transferring the account pointer to the entries
     * array, and freeing the accounts array below.  the account
     * will be freed in menu_free().  */
    entries[i].account = accounts[i];

    entries[i].addr = mutt_addr_new();
    entries[i].addr->mailbox = mutt_str_dup(accounts[i]->email_addr);
    mutt_addr_to_local(entries[i].addr);
  }
  FREE(&accounts);

  mutt_menu_push_current(menu);

  return menu;
}

/**
 * menu_free - Free the Autocrypt account Menu
 * @param menu Menu to free
 */
static void menu_free(struct Menu **menu)
{
  struct AccountEntry *entries = (struct AccountEntry *) (*menu)->mdata;

  for (int i = 0; i < (*menu)->max; i++)
  {
    mutt_autocrypt_db_account_free(&entries[i].account);
    mutt_addr_free(&entries[i].addr);
  }
  FREE(&(*menu)->mdata);

  mutt_menu_pop_current(*menu);
  FREE(&(*menu)->help);
  mutt_menu_free(menu);
}

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

/**
 * mutt_autocrypt_account_menu - Display the Autocrypt account Menu
 */
void mutt_autocrypt_account_menu(void)
{
  if (!C_Autocrypt)
    return;

  if (mutt_autocrypt_init(false))
    return;

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_AUTOCRYPT, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  dialog_push(dlg);

  struct Menu *menu = create_menu();
  if (!menu)
    return;

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  bool done = false;
  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_EXIT:
        done = true;
        break;

      case OP_AUTOCRYPT_CREATE_ACCT:
        if (mutt_autocrypt_account_init(false))
          break;

        menu_free(&menu);
        menu = create_menu();
        menu->pagelen = index->state.rows;
        menu->win_index = index;
        menu->win_ibar = ibar;
        break;

      case OP_AUTOCRYPT_DELETE_ACCT:
      {
        if (!menu->mdata)
          break;

        struct AccountEntry *entry = (struct AccountEntry *) (menu->mdata) + menu->current;
        char msg[128];
        snprintf(msg, sizeof(msg),
                 // L10N: Confirmation message when deleting an autocrypt account
                 _("Really delete account \"%s\"?"), entry->addr->mailbox);
        if (mutt_yesorno(msg, MUTT_NO) != MUTT_YES)
          break;

        if (!mutt_autocrypt_db_account_delete(entry->account))
        {
          menu_free(&menu);
          menu = create_menu();
          menu->pagelen = index->state.rows;
          menu->win_index = index;
          menu->win_ibar = ibar;
        }
        break;
      }

      case OP_AUTOCRYPT_TOGGLE_ACTIVE:
      {
        if (!menu->mdata)
          break;

        struct AccountEntry *entry = (struct AccountEntry *) (menu->mdata) + menu->current;
        toggle_active(entry);
        menu->redraw |= REDRAW_FULL;
        break;
      }

      case OP_AUTOCRYPT_TOGGLE_PREFER:
      {
        if (!menu->mdata)
          break;

        struct AccountEntry *entry = (struct AccountEntry *) (menu->mdata) + menu->current;
        toggle_prefer_encrypt(entry);
        menu->redraw |= REDRAW_FULL;
        break;
      }
    }
  }

  menu_free(&menu);
  dialog_pop();
  mutt_window_free(&dlg);
}
