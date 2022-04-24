/**
 * @file
 * Mixmaster Remailer
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

#ifndef MUTT_MIXMASTER_REMAILER_H
#define MUTT_MIXMASTER_REMAILER_H

#include <stdint.h>
#include "mutt/lib.h"

typedef uint8_t MixCapFlags;       ///< Flags, e.g. #MIX_CAP_NO_FLAGS
#define MIX_CAP_NO_FLAGS        0  ///< No flags are set
#define MIX_CAP_COMPRESS  (1 << 0) ///< Accepts compressed messages
#define MIX_CAP_MIDDLEMAN (1 << 1) ///< Must be a middle-man (not at the end of a chain)
#define MIX_CAP_NEWSPOST  (1 << 2) ///< Supports direct posting to Usenet
#define MIX_CAP_NEWSMAIL  (1 << 3) ///< Supports posting to Usenet through a mail-to-news gateway

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
ARRAY_HEAD(RemailerArray, struct Remailer *);

void             remailer_free(struct Remailer **ptr);
struct Remailer *remailer_new (void);

void                 remailer_clear_hosts(struct RemailerArray *ra);
struct RemailerArray remailer_get_hosts(void);

#endif /* MUTT_MIXMASTER_REMAILER_H */
