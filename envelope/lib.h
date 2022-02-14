/**
 * @file
 * Envelope-editing Window
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
 * @page lib_envelope Envelope-editing Window
 *
 * Envelope-editing Window
 *
 * | File                 | Description                 |
 * | :------------------  | :-------------------------- |
 * | envelope/functions.c | @subpage envelope_functions |
 * | envelope/wdata.c     | @subpage envelope_wdata     |
 * | envelope/window.c    | @subpage envelope_window    |
 */

#ifndef MUTT_ENVELOPE_LIB_H
#define MUTT_ENVELOPE_LIB_H

#include "private.h"
#include "wdata.h"

struct MuttWindow;

extern int HeaderPadding[];
extern int MaxHeaderWidth;

int env_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_ENVELOPE_LIB_H */
