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
 * | Certificate Verification Dialog | WT_DLG_CERTIFICATE | dlg_certificate() |
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
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
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
 * menu_dialog_dokey - Check if there are any menu key events to process
 * @param menu Current Menu
 * @param id   KeyEvent ID
 * @retval  0 An event occurred for the menu, or a timeout
 * @retval -1 There was an event, but not for menu
 */
static int menu_dialog_dokey(struct Menu *menu, int *id)
{
  struct KeyEvent event = mutt_getch(GETCH_IGNORE_MACRO);

  if ((event.op == OP_TIMEOUT) || (event.op == OP_ABORT))
  {
    *id = event.op;
    return 0;
  }

  struct CertMenuData *mdata = menu->mdata;
  char *p = NULL;
  if ((event.ch != 0) && (p = strchr(mdata->keys, event.ch)))
  {
    *id = OP_MAX + (p - mdata->keys + 1);
    return 0;
  }

  if (event.op == OP_NULL)
    mutt_unget_ch(event.ch);
  else
    mutt_unget_op(event.op);
  return -1;
}

/**
 * menu_dialog_translate_op - Convert menubar movement to scrolling
 * @param op Action requested, e.g. OP_NEXT_ENTRY
 * @retval num Action to perform, e.g. OP_NEXT_LINE
 */
static int menu_dialog_translate_op(int op)
{
  switch (op)
  {
    case OP_NEXT_ENTRY:
      return OP_NEXT_LINE;
    case OP_PREV_ENTRY:
      return OP_PREV_LINE;
    case OP_CURRENT_TOP:
      return OP_TOP_PAGE;
    case OP_CURRENT_BOTTOM:
      return OP_BOTTOM_PAGE;
    case OP_CURRENT_MIDDLE:
      return OP_MIDDLE_PAGE;
  }

  return op;
}

/**
 * cert_make_entry - Create a Certificate for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static void cert_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct CertMenuData *mdata = menu->mdata;

  menu->current = -1; /* hide menubar */

  const char **line_ptr = ARRAY_GET(mdata->carr, line);
  if (!line_ptr)
  {
    buf[0] = '\0';
    return;
  }

  mutt_str_copy(buf, *line_ptr, buflen);
}

/**
 * cert_array_clear - Free all memory of a CertArray
 * @param carr Array of text to clear
 *
 * @note Array is emptied, but not freed
 */
void cert_array_clear(struct CertArray *carr)
{
  const char **line = NULL;
  ARRAY_FOREACH(line, carr)
  {
    FREE(line);
  }
}

/**
 * dlg_certificate - Ask the user to validate the certificate - @ingroup gui_dlg
 * @param title        Menu title
 * @param carr         Certificate text to display
 * @param allow_always If true, allow the user to always accept the certificate
 * @param allow_skip   If true, allow the user to skip the verification
 * @retval 1 Reject certificate (or menu aborted)
 * @retval 2 Accept certificate once
 * @retval 3 Accept certificate always/skip (see notes)
 * @retval 4 Accept certificate skip
 *
 * The Verify Certificate Dialog shows a list of signatures for a domain certificate.
 * They can choose whether to accept or reject it.
 *
 * The possible retvals will depend on the parameters.
 * The options are given in the order: Reject, Once, Always, Skip.
 * The retval represents the chosen option.
 */
int dlg_certificate(const char *title, struct CertArray *carr, bool allow_always, bool allow_skip)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_CERTIFICATE, VerifyHelp);

  struct CertMenuData mdata = { carr };

  struct Menu *menu = dlg->wdata;
  menu->mdata = &mdata;
  menu->mdata_free = NULL; // Menu doesn't own the data
  menu->make_entry = cert_make_entry;
  menu->max = ARRAY_SIZE(carr);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, title);

  if (allow_always)
  {
    if (allow_skip)
    {
      mdata.prompt = _("(r)eject, accept (o)nce, (a)ccept always, (s)kip");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (a)ccept always, (s)kip"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      mdata.keys = _("roas");
    }
    else
    {
      mdata.prompt = _("(r)eject, accept (o)nce, (a)ccept always");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (a)ccept always"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      mdata.keys = _("roa");
    }
  }
  else
  {
    if (allow_skip)
    {
      mdata.prompt = _("(r)eject, accept (o)nce, (s)kip");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce, (s)kip"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      mdata.keys = _("ros");
    }
    else
    {
      mdata.prompt = _("(r)eject, accept (o)nce");
      /* L10N: The letters correspond to the choices in the string:
         "(r)eject, accept (o)nce"
         This is an interactive certificate confirmation prompt for an SSL connection. */
      mdata.keys = _("ro");
    }
  }
  msgwin_set_text(NULL, mdata.prompt, MT_COLOR_PROMPT);

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int choice = 0;
  int op = OP_NULL;
  do
  {
    window_redraw(NULL);
    msgwin_set_text(NULL, mdata.prompt, MT_COLOR_PROMPT);

    // Try to catch dialog keys before ops
    if (menu_dialog_dokey(menu, &op) != 0)
    {
      op = km_dokey(MENU_DIALOG, GETCH_IGNORE_MACRO);
    }

    if (op == OP_TIMEOUT)
      continue;

    // Convert menubar movement to scrolling
    op = menu_dialog_translate_op(op);

    if (op <= OP_MAX)
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    else
      mutt_debug(LL_DEBUG1, "Got choice %d\n", op - OP_MAX);

    switch (op)
    {
      case -1:         // Abort: Ctrl-G
      case OP_QUIT:    // Q)uit
      case OP_MAX + 1: // R)eject
        choice = 1;
        break;
      case OP_MAX + 2: // O)nce
        choice = 2;
        break;
      case OP_MAX + 3: // A)lways / S)kip
        choice = 3;
        break;
      case OP_MAX + 4: // S)kip
        choice = 4;
        break;

      case OP_JUMP:
      case OP_JUMP_1:
      case OP_JUMP_2:
      case OP_JUMP_3:
      case OP_JUMP_4:
      case OP_JUMP_5:
      case OP_JUMP_6:
      case OP_JUMP_7:
      case OP_JUMP_8:
      case OP_JUMP_9:
        mutt_error(_("Jumping is not implemented for dialogs"));
        continue;

      case OP_SEARCH:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
      case OP_SEARCH_REVERSE:
        mutt_error(_("Search is not implemented for this menu"));
        continue;
    }

    (void) menu_function_dispatcher(menu->win, op);
  } while (choice == 0);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);

  return choice;
}
