/**
 * @file
 * Shared debug code
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
 * @page debug_common Shared debug code
 *
 * Shared debug code
 */

#include "config.h"
#include "gui/lib.h"
#include "lib.h"

const char *win_name(const struct MuttWindow *win)
{
  const bool symbol = true;
  if (!win)
    return "UNKNOWN";

  switch (win->type)
  {
    case WT_ALL_DIALOGS:
      if (symbol)
        return "WT_ALL_DIALOGS";
      else
        return "All Dialogs";
    case WT_CONTAINER:
      if (symbol)
        return "WT_CONTAINER";
      else
        return "Container";
    case WT_CUSTOM:
      if (symbol)
        return "WT_CUSTOM";
      else
        return "Custom";
    case WT_DLG_ALIAS:
      if (symbol)
        return "WT_DLG_ALIAS";
      else
        return "Alias Dialog";
    case WT_DLG_ATTACH:
      if (symbol)
        return "WT_DLG_ATTACH";
      else
        return "Attach Dialog";
    case WT_DLG_AUTOCRYPT:
      if (symbol)
        return "WT_DLG_AUTOCRYPT";
      else
        return "Autocrypt Dialog";
    case WT_DLG_BROWSER:
      if (symbol)
        return "WT_DLG_BROWSER";
      else
        return "Browser Dialog";
    case WT_DLG_CERTIFICATE:
      if (symbol)
        return "WT_DLG_CERTIFICATE";
      else
        return "Certificate Dialog";
    case WT_DLG_COMPOSE:
      if (symbol)
        return "WT_DLG_COMPOSE";
      else
        return "Compose Dialog";
    case WT_DLG_CRYPT_GPGME:
      if (symbol)
        return "WT_DLG_CRYPT_GPGME";
      else
        return "Crypt-GPGME Dialog";
    case WT_DLG_DO_PAGER:
      if (symbol)
        return "WT_DLG_DO_PAGER";
      else
        return "Pager Dialog";
    case WT_DLG_HISTORY:
      if (symbol)
        return "WT_DLG_HISTORY";
      else
        return "History Dialog";
    case WT_DLG_INDEX:
      if (symbol)
        return "WT_DLG_INDEX";
      else
        return "Index Dialog";
    case WT_DLG_PGP:
      if (symbol)
        return "WT_DLG_PGP";
      else
        return "Pgp Dialog";
    case WT_DLG_POSTPONE:
      if (symbol)
        return "WT_DLG_POSTPONE";
      else
        return "Postpone Dialog";
    case WT_DLG_QUERY:
      if (symbol)
        return "WT_DLG_QUERY";
      else
        return "Query Dialog";
    case WT_DLG_REMAILER:
      if (symbol)
        return "WT_DLG_REMAILER";
      else
        return "Remailer Dialog";
    case WT_DLG_SMIME:
      if (symbol)
        return "WT_DLG_SMIME";
      else
        return "Smime Dialog";
    case WT_HELP_BAR:
      if (symbol)
        return "WT_HELP_BAR";
      else
        return "Help Bar";
    case WT_INDEX:
      if (symbol)
        return "WT_INDEX";
      else
        return "Index";
    case WT_INDEX_BAR:
      if (symbol)
        return "WT_INDEX_BAR";
      else
        return "Index Bar";
    case WT_MENU:
      if (symbol)
        return "WT_MENU";
      else
        return "Menu";
    case WT_MESSAGE:
      if (symbol)
        return "WT_MESSAGE";
      else
        return "Message";
    case WT_PAGER:
      if (symbol)
        return "WT_PAGER";
      else
        return "Pager";
    case WT_PAGER_BAR:
      if (symbol)
        return "WT_PAGER_BAR";
      else
        return "Pager Bar";
    case WT_ROOT:
      if (symbol)
        return "WT_ROOT";
      else
        return "Root Dialog";
    case WT_SIDEBAR:
      if (symbol)
        return "WT_SIDEBAR";
      else
        return "Sidebar";
    default:
      return "UNKNOWN";
  }
}
