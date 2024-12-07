/**
 * @file
 * Colour used by libcompose
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMPOSE_COLOR_H
#define MUTT_COMPOSE_COLOR_H

/**
 * enum ColorCompose - Compose Dialog Colours
 *
 * @sa CD_COMPOSE, ColorDomain
 */
enum ColorCompose
{
  CD_COM_HEADER = 1,               ///< Header labels, e.g. From:
  CD_COM_SECURITY_BOTH,            ///< Mail will be encrypted and signed
  CD_COM_SECURITY_ENCRYPT,         ///< Mail will be encrypted
  CD_COM_SECURITY_NONE,            ///< Mail will not be encrypted or signed
  CD_COM_SECURITY_SIGN,            ///< Mail will be signed
};

void compose_colors_init(void);

#endif /* MUTT_COMPOSE_COLOR_H */
