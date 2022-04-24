/**
 * @file
 * Mixmaster Chain Data
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

#ifndef MUTT_MIXMASTER_CHAIN_DATA_H
#define MUTT_MIXMASTER_CHAIN_DATA_H

struct MuttWindow;

/// Mixmaster's maximum chain length.  Don't change this.
#define MAX_MIXES 19

/**
 * struct Coord - Screen coordinates
 */
struct Coord
{
  short row;   ///< Row
  short col;   ///< Column
};

/**
 * struct ChainData - An ordered set of Remailer hosts
 *
 * The `chain` stores a set of indices that reference Remailers in `ra`.
 */
struct ChainData
{
  int sel;                        ///< Current selection
  int chain_len;                  ///< Length of chain
  int chain[MAX_MIXES];           ///< Indexes of chain hosts
  struct Coord coords[MAX_MIXES]; ///< Screen coordinates of each entry
  struct RemailerArray *ra;       ///< Array of all Remailer hosts
  struct MuttWindow *win_cbar;    ///< Chain Bar (status window)
};

struct ChainData *chain_data_new(void);
void              chain_data_free(struct MuttWindow *win, void **ptr);

#endif /* MUTT_MIXMASTER_CHAIN_DATA_H */
