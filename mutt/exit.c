/**
 * @file
 * Leave the program NOW
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page exit Leave the program NOW
 *
 * The default behaviour on a critical error is to notify the user, then stop.
 */

#include "config.h"
#include <stdlib.h>

/**
 * mutt_exit - Leave NeoMutt NOW
 * @param code Value to return to the calling environment
 *
 * Some library routines want to exit immediately on error.
 * By having this in the library, mutt_exit() can be overridden.
 */
void mutt_exit(int code)
{
  exit(code);
}
