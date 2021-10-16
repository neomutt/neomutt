/**
 * @file
 * Quoted-Email colours
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page color_quote Quoted-Email colours
 *
 * Manage the colours of quoted emails.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "options.h"

int QuotedColors[COLOR_QUOTES_MAX]; ///< Array of colours for quoted email text
int NumQuotedColors;                ///< Number of colours for quoted email text

/**
 * quoted_colors_clear - Reset the quoted-email colours
 */
void quoted_colors_clear(void)
{
  memset(QuotedColors, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  NumQuotedColors = 0;
}

/**
 * quoted_colors_init - Initialise the quoted-email colours
 */
void quoted_colors_init(void)
{
  memset(QuotedColors, A_NORMAL, COLOR_QUOTES_MAX * sizeof(int));
  NumQuotedColors = 0;
}

/**
 * quoted_colors_get - Return the color of a quote, cycling through the used quotes
 * @param q Quote number
 * @retval num Color ID, e.g. MT_COLOR_QUOTED
 */
int quoted_colors_get(int q)
{
  const int used = NumQuotedColors;
  if (used == 0)
    return 0;
  return QuotedColors[q % used];
}

/**
 * quoted_colors_num_used - Return the number of used quotes
 * @retval num Number of used quotes
 */
int quoted_colors_num_used(void)
{
  return NumQuotedColors;
}
