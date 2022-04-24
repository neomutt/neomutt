/**
 * @file
 * Private state data for the Mixmaster
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

#ifndef MUTT_MIXMASTER_PRIVATE_DATA_H
#define MUTT_MIXMASTER_PRIVATE_DATA_H

#include "config.h"
#include <stdbool.h>

/**
 * struct MixmasterPrivateData - Private state data for the Mixmaster
 */
struct MixmasterPrivateData
{
  struct MuttWindow *win_hosts;    ///< Hosts Window
  struct MuttWindow *win_chain;    ///< Chain Window
};

#endif /* MUTT_MIXMASTER_PRIVATE_DATA_H */
