/**
 * @file
 * Dispatcher of functions
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

/**
 * @page core_dispatcher Dispatcher of functions
 *
 * Dispatcher of functions
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "dispatcher.h"

/// Lookup for function results
const struct Mapping RetvalNames[] = {
  // clang-format off
  { "continue",  FR_CONTINUE },
  { "done",      FR_DONE },
  { "error",     FR_ERROR },
  { "no action", FR_NO_ACTION },
  { "not impl",  FR_NOT_IMPL },
  { "success",   FR_SUCCESS },
  { "unknown",   FR_UNKNOWN },
  { NULL, 0 },
  // clang-format on
};

/**
 * dispacher_get_retval_name - Get the name of a return value
 * @param rv Return value, e.g. #FR_ERROR
 * @retval str Name of the retval
 * @retval ""  Error
 */
const char *dispacher_get_retval_name(int rv)
{
  const char *result = mutt_map_get_name(rv, RetvalNames);
  return NONULL(result);
}
