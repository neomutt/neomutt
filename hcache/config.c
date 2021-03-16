/**
 * @file
 * Config used by libhcache
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page hc_config Config used by libhcache
 *
 * Config used by libhcache
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "compress/lib.h"
#include "store/lib.h"

// clang-format off
char *C_HeaderCache;               ///< Config: (hcache) Directory/file for the header cache database
char *C_HeaderCacheBackend;        ///< Config: (hcache) Header cache backend to use
#ifdef USE_HCACHE_COMPRESSION
short C_HeaderCacheCompressLevel;  ///< Config: (hcache) Level of compression for method
char *C_HeaderCacheCompressMethod; ///< Config: (hcache) Enable generic hcache database compression
#endif
// clang-format on

bool C_HeaderCacheCompress = false;
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
long C_HeaderCachePagesize = 0;
#endif

/**
 * hcache_validator - Validate the "header_cache_backend" config variable - Implements ConfigDef::validator()
 */
int hcache_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                     intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (store_is_valid_backend(str))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

#ifdef USE_HCACHE_COMPRESSION
/**
 * compress_method_validator - Validate the "header_cache_compress_method" config variable - Implements ConfigDef::validator()
 */
int compress_method_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                              intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (compress_get_ops(str))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

/**
 * compress_level_validator - Validate the "header_cache_compress_level" config variable - Implements ConfigDef::validator()
 */
int compress_level_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                             intptr_t value, struct Buffer *err)
{
  if (!C_HeaderCacheCompressMethod)
  {
    mutt_buffer_printf(err, _("Set option %s before setting %s"),
                       "header_cache_compress_method", cdef->name);
    return CSR_ERR_INVALID;
  }

  const struct ComprOps *cops = compress_get_ops(C_HeaderCacheCompressMethod);
  if (!cops)
  {
    mutt_buffer_printf(err, _("Invalid value for option %s: %s"),
                       "header_cache_compress_method", C_HeaderCacheCompressMethod);
    return CSR_ERR_INVALID;
  }

  if ((value < cops->min_level) || (value > cops->max_level))
  {
    // L10N: This applies to the "$header_cache_compress_level" config variable.
    //       It shows the minimum and maximum values, e.g. 'between 1 and 22'
    mutt_buffer_printf(err, _("Option %s must be between %d and %d inclusive"),
                       cdef->name, cops->min_level, cops->max_level);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}
#endif /* USE_HCACHE_COMPRESSION */

struct ConfigDef HcacheVars[] = {
  // clang-format off
  { "header_cache", DT_PATH, &C_HeaderCache, 0, 0, NULL,
    "(hcache) Directory/file for the header cache database"
  },
  { "header_cache_backend", DT_STRING, &C_HeaderCacheBackend, 0, 0, hcache_validator,
    "(hcache) Header cache backend to use"
  },
#if defined(USE_HCACHE_COMPRESSION)
  { "header_cache_compress_level", DT_NUMBER|DT_NOT_NEGATIVE, &C_HeaderCacheCompressLevel, 1, 0, compress_level_validator,
    "(hcache) Level of compression for method"
  },
  { "header_cache_compress_method", DT_STRING, &C_HeaderCacheCompressMethod, 0, 0, compress_method_validator,
    "(hcache) Enable generic hcache database compression"
  },
#endif
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
  { "header_cache_compress", DT_DEPRECATED|DT_BOOL, &C_HeaderCacheCompress, false, 0, NULL, NULL },
#endif
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
  { "header_cache_pagesize", DT_DEPRECATED|DT_LONG, &C_HeaderCachePagesize, 0, 0, NULL, NULL },
#endif
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_hcache - Register hcache config variables - Implements ::module_init_config_t
 */
bool config_init_hcache(struct ConfigSet *cs)
{
  return cs_register_variables(cs, HcacheVars, 0);
}
