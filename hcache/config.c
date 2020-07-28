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
#include <config/lib.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "init.h"

// clang-format off
char *C_HeaderCache;               ///< Config: (hcache) Directory/file for the header cache database
char *C_HeaderCacheBackend;        ///< Config: (hcache) Header cache backend to use
#ifdef USE_HCACHE_COMPRESSION
short C_HeaderCacheCompressLevel;  ///< Config: (hcache) Level of compression for method
char *C_HeaderCacheCompressMethod; ///< Config: (hcache) Enable generic hcache database compression
#endif
bool  C_MaildirHeaderCacheVerify;  ///< Config: (hcache) Check for maildir changes when opening mailbox
// clang-format on

bool C_HeaderCacheCompress = false;
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
long C_HeaderCachePagesize = 0;
#endif

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
  { "maildir_header_cache_verify", DT_BOOL, &C_MaildirHeaderCacheVerify, true, 0, NULL,
    "(hcache) Check for maildir changes when opening mailbox"
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
 * config_init_hcache - Register hcache config variables
 */
bool config_init_hcache(struct ConfigSet *cs)
{
  return cs_register_variables(cs, HcacheVars, 0);
}
