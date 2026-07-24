/**
 * @file
 * Mlist Expando definitions
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MLIST_EXPANDO_H
#define MUTT_MLIST_EXPANDO_H

#include "expando/lib.h" // IWYU pragma: keep

/**
 * struct MlistExpandoData - Data for the Mailing list expando
 */
struct MlistExpandoData
{
  const char *url;    ///< Mailing list URL
};

/**
 * ExpandoDataMlist - Expando UIDs for Mailing Lists
 *
 * @sa ED_MLIST, ExpandoDomain
 */
enum ExpandoDataMlist
{
  ED_MLS_URL = 1,    ///< Mailing List URL
};

extern const struct ExpandoRenderCallback MlistRenderCallbacks[];

#endif /* MUTT_MLIST_EXPANDO_H */
