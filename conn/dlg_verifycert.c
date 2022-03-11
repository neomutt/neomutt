/**
 * @file
 * Certificate Verification Dialog
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page conn_dlg_verifycert Certificate Verification Dialog
 *
 * The Certificate Verification Dialog lets the user check the details of a
 * certificate.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                            | Type               | See Also                 |
 * | :------------------------------ | :----------------- | :----------------------- |
 * | Certificate Verification Dialog | WT_DLG_CERTIFICATE | dlg_verify_certificate() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 *
 * None.
 *
 * ## Events
 *
 * None.  Once constructed, the events are handled by the Menu
 * (part of the @ref gui_simple).
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "opcodes.h"
#include "options.h"
#include "ssl.h"

/// Help Bar for the Certificate Verification dialog
static const struct Mapping VerifyHelp[] = {
  // clang-format off
  { N_("Exit"), OP_EXIT },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * dlg_verify_certificate - Ask the user to validate the certificate
 * @param title        Menu title
 * @param list         Certificate text to display
 * @param allow_always If true, allow the user to always accept the certificate
 * @param allow_skip   If true, allow the user to skip the verification
 * @retval 1 Reject certificate (or menu aborted)
 * @retval 2 Accept certificate once
 * @retval 3 Accept certificate always/skip (see notes)
 * @retval 4 Accept certificate skip
 *
 * The possible retvals will depend on the parameters.
 * The options are given in the order: Reject, Once, Always, Skip.
 * The retval represents the chosen option.
 */
int dlg_verify_certificate(const char *title, struct ListHead *list,
                           bool allow_always, bool allow_skip)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_CERTIFICATE, VerifyHelp);

  struct Menu *menu = dlg->wdata;

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, title);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, list, entries)
  {
    menu_add_dialog_row(menu, NONULL(np->data));
  }

  if (allow_always)
  {
    if (allow_skip)
    {
      menu->prompt = _("(r)eject, accept (o)nce, (a)ccept always, (s)kip");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (a)ccept always, (s)kip"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      menu->keys = _("roas");
    }
    else
    {
      menu->prompt = _("(r)eject, accept (o)nce, (a)ccept always");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (a)ccept always"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      menu->keys = _("roa");
    }
  }
  else
  {
    if (allow_skip)
    {
      menu->prompt = _("(r)eject, accept (o)nce, (s)kip");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (s)kip"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      menu->keys = _("ros");
    }
    else
    {
      menu->prompt = _("(r)eject, accept (o)nce");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      menu->keys = _("ro");
    }
  }

  bool old_ime = OptIgnoreMacroEvents;
  OptIgnoreMacroEvents = true;

  int rc = 0;
  while (rc == 0)
  {
    switch (menu_loop(menu))
    {
      case -1:         // Abort: Ctrl-G
      case OP_EXIT:    // Q)uit
      case OP_MAX + 1: // R)eject
        rc = 1;
        break;
      case OP_MAX + 2: // O)nce
        rc = 2;
        break;
      case OP_MAX + 3: // A)lways / S)kip
        rc = 3;
        break;
      case OP_MAX + 4: // S)kip
        rc = 4;
        break;
    }
  }
  OptIgnoreMacroEvents = old_ime;

  simple_dialog_free(&dlg);

  return rc;
}
