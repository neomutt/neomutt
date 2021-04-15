/**
 * @file
 * Shared Compose Data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMPOSE_REDRAW_H
#define MUTT_COMPOSE_REDRAW_H

#include "config.h"
#include "mutt/lib.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/**
 * struct ComposeRedrawData - Keep track when the compose screen needs redrawing
 */
struct ComposeRedrawData
{
  struct Email *email;
  struct Buffer *fcc;

  struct ListHead to_list;
  struct ListHead cc_list;
  struct ListHead bcc_list;

  short to_rows;
  short cc_rows;
  short bcc_rows;
  short sec_rows;

#ifdef USE_AUTOCRYPT
  enum AutocryptRec autocrypt_rec;
#endif
  struct MuttWindow *win_env;  ///< Envelope: From, To, etc
  struct MuttWindow *win_cbar; ///< Compose bar

  struct Menu *menu;
  struct AttachCtx *actx;   ///< Attachments
  struct ConfigSubset *sub; ///< Inherited config items
};

#endif /* MUTT_COMPOSE_REDRAW_H */
