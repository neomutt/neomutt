/**
 * @file
 * Expando Data UIDs
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EXPANDO_UID_H
#define MUTT_EXPANDO_UID_H

/**
 * ExpandoDataGlobal - Expando UIDs for Globals
 *
 * @sa ED_GLOBAL, ExpandoDomain
 */
enum ExpandoDataGlobal
{
  ED_GLO_CERTIFICATE_PATH = 1, ///< Path of Smime certificates
  ED_GLO_HOSTNAME,             ///< Local hostname
  ED_GLO_PADDING_EOL,          ///< Padding to end-of-line
  ED_GLO_PADDING_HARD,         ///< Hard Padding
  ED_GLO_PADDING_SOFT,         ///< Soft Padding
  ED_GLO_PADDING_SPACE,        ///< Space Padding
  ED_GLO_SORT,                 ///< Value of $sort
  ED_GLO_SORT_AUX,             ///< Value of $sort_aux
  ED_GLO_USE_THREADS,          ///< Value of $use_threads
  ED_GLO_VERSION,              ///< NeoMutt version
};

#endif /* MUTT_EXPANDO_UID_H */
