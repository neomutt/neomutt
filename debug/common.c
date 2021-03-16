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
  if (!win)
    return "UNKNOWN";

  switch (win->type)
  {
    case WT_ALL_DIALOGS:
      return "All Dialogs";
    case WT_CONTAINER:
      return "Container";
    case WT_CUSTOM:
      return "Custom";
    case WT_DLG_ALIAS:
      return "Alias Dialog";
    case WT_DLG_ATTACH:
      return "Attach Dialog";
    case WT_DLG_AUTOCRYPT:
      return "Autocrypt Dialog";
    case WT_DLG_BROWSER:
      return "Browser Dialog";
    case WT_DLG_CERTIFICATE:
      return "Certificate Dialog";
    case WT_DLG_COMPOSE:
      return "Compose Dialog";
    case WT_DLG_CRYPT_GPGME:
      return "Crypt-GPGME Dialog";
    case WT_DLG_DO_PAGER:
      return "Pager Dialog";
    case WT_DLG_HISTORY:
      return "History Dialog";
    case WT_DLG_INDEX:
      return "Index Dialog";
    case WT_DLG_PGP:
      return "Pgp Dialog";
    case WT_DLG_POSTPONE:
      return "Postpone Dialog";
    case WT_DLG_QUERY:
      return "Query Dialog";
    case WT_DLG_REMAILER:
      return "Remailer Dialog";
    case WT_DLG_SMIME:
      return "Smime Dialog";
    case WT_HELP_BAR:
      return "Help Bar";
    case WT_INDEX:
      return "Index";
    case WT_INDEX_BAR:
      return "Index Bar";
    case WT_MESSAGE:
      return "Message";
    case WT_PAGER:
      return "Pager";
    case WT_PAGER_BAR:
      return "Pager Bar";
    case WT_ROOT:
      return "Root Dialog";
    case WT_SIDEBAR:
      return "Sidebar";
    default:
      return "UNKNOWN";
  }
}
