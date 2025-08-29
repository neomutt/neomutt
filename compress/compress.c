/**
 * @file
 * Shared compression code
 *
 * @authors
 * Copyright (C) 2019 Tino Reichardt <milky-neomutt@mcmilk.de>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"
#include "lib.h"

/**
 * CompressOps - Backend implementations
 */
static const struct ComprOps *CompressOps[] = {
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
  const struct ComprOps **compr_ops = CompressOps;
  size_t len = 0;

  for (; *compr_ops; compr_ops++)
  {
    if (len != 0)
    {
      len += snprintf(tmp + len, sizeof(tmp) - len, ", ");
    }
    len += snprintf(tmp + len, sizeof(tmp) - len, "%s", (*compr_ops)->name);
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
  const struct ComprOps **compr_ops = CompressOps;

  if (!compr || !*compr)
    return *compr_ops;

  for (; *compr_ops; compr_ops++)
  {
    if (mutt_str_equal(compr, (*compr_ops)->name))
      break;
  }

  return *compr_ops;
}
