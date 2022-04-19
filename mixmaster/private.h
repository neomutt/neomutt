/**
 * @file
 * Mixmaster private header
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

#ifndef MUTT_MIXMASTER_PRIVATE_H
#define MUTT_MIXMASTER_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

struct Coord;
struct MixChain;
struct MuttWindow;
struct Remailer;

/* Mixmaster's maximum chain length.  Don't change this. */
#define MAX_MIXES 19

typedef uint8_t MixCapFlags;       ///< Flags, e.g. #MIX_CAP_NO_FLAGS
#define MIX_CAP_NO_FLAGS        0  ///< No flags are set
#define MIX_CAP_COMPRESS  (1 << 0)
#define MIX_CAP_MIDDLEMAN (1 << 1)
#define MIX_CAP_NEWSPOST  (1 << 2)
#define MIX_CAP_NEWSMAIL  (1 << 3)

/**
 * struct Remailer - A Mixmaster remailer
 */
struct Remailer
{
  int num;          ///< Index number
  char *shortname;  ///< Short name of remailer host
  char *addr;       ///< Address of host
  char *ver;        ///< Version of host
  MixCapFlags caps; ///< Capabilities of host
};

/**
 * struct MixChain - A Mixmaster chain
 */
struct MixChain
{
  size_t cl;         ///< Length of chain
  int ch[MAX_MIXES]; ///< Indexes of chain hosts
};

void mix_redraw_chain      (struct MuttWindow *win, struct Remailer **type2_list, struct Coord *coords, struct MixChain *chain, int cur);
void mix_redraw_head       (struct MuttWindow *win, struct MixChain *chain);
void mix_screen_coordinates(struct MuttWindow *win, struct Remailer **type2_list, struct Coord **coordsp, struct MixChain *chain, int i);

#endif /* MUTT_MIXMASTER_PRIVATE_H */
