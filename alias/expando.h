/**
 * @file
 * Alias Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ALIAS_EXPANDO_H
#define MUTT_ALIAS_EXPANDO_H

#include "expando/lib.h" // IWYU pragma: keep

/**
 * ExpandoDataAlias - Expando UIDs for Aliases
 *
 * @sa ED_ALIAS, ExpandoDomain
 */
enum ExpandoDataAlias
{
  ED_ALI_ADDRESS = 1,          ///< Alias.addr
  ED_ALI_ALIAS,                ///< Alias.name
  ED_ALI_COMMENT,              ///< Alias.comment
  ED_ALI_EMAIL,                ///< Alias.addr.mailbox
  ED_ALI_FLAGS,                ///< Alias.flags
  ED_ALI_NAME,                 ///< Alias.addr.personal
  ED_ALI_NUMBER,               ///< AliasView.num
  ED_ALI_TAGGED,               ///< AliasView.tagged
  ED_ALI_TAGS,                 ///< Alias.tags
};

extern const struct ExpandoRenderCallback AliasRenderCallbacks[];
extern const struct ExpandoRenderCallback QueryRenderCallbacks[];

#endif /* MUTT_ALIAS_EXPANDO_H */
