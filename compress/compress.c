/**
 * @file
 * Shared compression code
 *
 * @authors
 * Copyright (C) 2019 Tino Reichardt <milky-neomutt@mcmilk.de>
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
 * @page compress_compress Shared compression code
 *
 * Shared compression code
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "lib.h"

/**
 * compr_ops - Backend implementations
 */
const struct ComprOps *compr_ops[] = {
#ifdef HAVE_LZ4
  &compr_lz4_ops,
#endif
#ifdef HAVE_ZLIB
  &compr_zlib_ops,
#endif
#ifdef HAVE_ZSTD
  &compr_zstd_ops,
#endif
  NULL,
};

/**
 * compress_list - Get a list of compression backend names
 * @retval ptr Comma-space-separated list of names
 *
 * @note The caller should free the string
 */
const char *compress_list(void)
{
  char tmp[256] = { 0 };
  const struct ComprOps **ops = compr_ops;
  size_t len = 0;

  for (; *ops; ops++)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, sizeof(tmp) - len, ", ");
    }
    len += snprintf(tmp + len, sizeof(tmp) - len, "%s", (*ops)->name);
  }

  return mutt_str_dup(tmp);
}

/**
 * compress_get_ops - Get the API functions for a compress backend
 * @param compr Name of the backend
 * @retval ptr Set of function pointers
 */
const struct ComprOps *compress_get_ops(const char *compr)
{
  const struct ComprOps **ops = compr_ops;

  if (!compr || !*compr)
    return *ops;

  for (; *ops; ops++)
  {
    if (strcmp(compr, (*ops)->name) == 0)
      break;
  }

  return *ops;
}
