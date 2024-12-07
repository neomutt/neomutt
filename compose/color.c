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

/**
 * @page compose_color Colour used by libcompose
 *
 * Colour used by libcompose
 */

#include "config.h"
#include "color.h"
#include "color/lib.h"

/**
 * ComposeColorDefs - Mapping of colour names to their IDs
 */
static const struct ColorDefinition ComposeColorDefs[] = {
  // clang-format off
  { "compose_header",           CD_COM_HEADER,           CDT_SIMPLE },
  { "compose_security_both",    CD_COM_SECURITY_BOTH,    CDT_SIMPLE },
  { "compose_security_encrypt", CD_COM_SECURITY_ENCRYPT, CDT_SIMPLE },
  { "compose_security_none",    CD_COM_SECURITY_NONE,    CDT_SIMPLE },
  { "compose_security_sign",    CD_COM_SECURITY_SIGN,    CDT_SIMPLE },
  { NULL, 0 },
  // clang-format on
};

/**
 * compose_colors_init - Initialise the Compose Colours
 */
void compose_colors_init(void)
{
  color_register_domain("compose", CD_COMPOSE);
  color_register_colors(CD_COMPOSE, ComposeColorDefs);
}
