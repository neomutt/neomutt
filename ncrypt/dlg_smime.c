/**
 * @file
 * SMIME Key Selection Dialog
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page crypt_dlg_smime SMIME Key Selection Dialog
 *
 * The SMIME Key Selection Dialog lets the user select a SMIME key.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                       | Type         | See Also    |
 * | :------------------------- | :----------- | :---------- |
 * | SMIME Key Selection Dialog | WT_DLG_SMIME | dlg_smime() |
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
 * - #SmimeKey
 *
 * The @ref gui_simple holds a Menu.  The SMIME Key Selection Dialog stores its
 * data (#SmimeKey) in Menu::mdata.
 *
 * ## Events
 *
 * None.  The dialog is not affected by any config or colours and doesn't
 * support sorting.  Once constructed, the events are handled by the Menu (part
 * of the @ref gui_simple).
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "mutt_logging.h"
#include "smime.h"
#include "smime_functions.h"

/// Help Bar for the Smime key selection dialog
static const struct Mapping SmimeHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Help"),   OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * smime_key_flags - Turn SMIME key flags into a string
 * @param flags Flags, see #KeyFlags
 * @retval ptr Flag string
 *
 * @note The string is statically allocated
 */
static char *smime_key_flags(KeyFlags flags)
{
  static char buf[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buf[0] = '-';
  else
    buf[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buf[1] = '-';
  else
    buf[1] = 's';

  buf[2] = '\0';

  return buf;
}

/**
 * smime_make_entry - Format an S/MIME Key for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static void smime_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct SmimeKey **table = menu->mdata;
  struct SmimeKey *key = table[line];
  char *truststate = NULL;
  switch (key->trust)
  {
    case 'e':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Expired   ");
      break;
    case 'i':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Invalid   ");
      break;
    case 'r':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Revoked   ");
      break;
    case 't':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Trusted   ");
      break;
    case 'u':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Unverified");
      break;
    case 'v':
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Verified  ");
      break;
    default:
      /* L10N: Describes the trust state of a S/MIME key.
         This translation must be padded with spaces to the right such that it
         has the same length as the other translations.
         The translation strings which need to be padded are:
         Expired, Invalid, Revoked, Trusted, Unverified, Verified, and Unknown.  */
      truststate = _("Unknown   ");
  }
  snprintf(buf, buflen, " 0x%s %s %s %-35.35s %s", key->hash,
           smime_key_flags(key->flags), truststate, key->email, key->label);
}

/**
 * smime_key_table_free - Free the key table - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 *
 * @note The keys are owned by the caller of the dialog
 */
static void smime_key_table_free(struct Menu *menu, void **ptr)
{
  FREE(ptr);
}

/**
 * dlg_smime - Get the user to select a key - @ingroup gui_dlg
 * @param keys  List of keys to select from
 * @param query String to match
 * @retval ptr Key selected by user
 *
 * The Select SMIME Key Dialog lets the user select an SMIME Key to use.
 */
struct SmimeKey *dlg_smime(struct SmimeKey *keys, const char *query)
{
  struct SmimeKey **table = NULL;
  int table_size = 0;
  int table_index = 0;
  struct SmimeKey *key = NULL;

  for (table_index = 0, key = keys; key; key = key->next)
  {
    if (table_index == table_size)
    {
      table_size += 5;
      mutt_mem_realloc(&table, sizeof(struct SmimeKey *) * table_size);
    }

    table[table_index++] = key;
  }

  struct MuttWindow *dlg = simple_dialog_new(MENU_SMIME, WT_DLG_SMIME, SmimeHelp);

  struct Menu *menu = dlg->wdata;
  menu->max = table_index;
  menu->make_entry = smime_make_entry;
  menu->mdata = table;
  menu->mdata_free = smime_key_table_free;
  /* sorting keys might be done later - TODO */

  struct SmimeData sd = { false, menu, table, NULL };
  dlg->wdata = &sd;

  char title[256] = { 0 };
  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  snprintf(title, sizeof(title), _("S/MIME certificates matching \"%s\""), query);
  sbar_set_title(sbar, title);

  mutt_clear_error();

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_SMIME, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_SMIME);
      continue;
    }
    mutt_clear_error();

    int rc = smime_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!sd.done);

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
  return sd.key;
}
