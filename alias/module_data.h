/**
 * @file
 * Alias private Module data
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

#ifndef MUTT_ALIAS_MODULE_DATA_H
#define MUTT_ALIAS_MODULE_DATA_H

#include "mutt/lib.h"
#include "alias.h"

/**
 * struct AliasModuleData - Alias private Module data
 */
struct AliasModuleData
{
  struct AliasArray aliases;            ///< User's email aliases
  struct HashTable *reverse_aliases;    ///< Hash Table of aliases (email address -> alias)

  struct RegexList  alternates;         ///< Regexes to match the user's alternate email addresses
  struct RegexList  unalternates;       ///< Regexes to exclude false matches in alternates
  struct Notify    *alternates_notify;  ///< Notifications: #NotifyAlternates
};

#endif /* MUTT_ALIAS_MODULE_DATA_H */
