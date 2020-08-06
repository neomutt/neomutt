/**
 * @file
 * GUI parts of Connection Library
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
 * @page conn_gui GUI parts of Connection Library
 *
 * GUI parts of Connection Library
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_menu.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"

#ifdef USE_SSL
/// Help Bar for the Certificate Verification dialog
static const struct Mapping VerifyHelp[] = {
  // clang-format off
  { N_("Exit"), OP_EXIT },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
  // clang-format on
};
#endif

#ifdef USE_SSL
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
  struct Menu *menu = mutt_menu_new(MENU_GENERIC);
  struct MuttWindow *dlg = dialog_create_simple_index(menu, WT_DLG_CERTIFICATE);
  dlg->help_data = VerifyHelp;
  dlg->help_menu = MENU_GENERIC;

  mutt_menu_push_current(menu);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, list, entries)
  {
    mutt_menu_add_dialog_row(menu, NONULL(np->data));
  }

  menu->title = title;

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
    switch (mutt_menu_loop(menu))
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

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_destroy_simple_index(&dlg);

  return rc;
}
#endif
