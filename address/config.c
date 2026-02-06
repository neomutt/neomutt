/**
 * @file
 * Config used by libaddress
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 */

/**
 * @page addr_config Config used by libaddress
 *
 * Config used by libaddress
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"

#if defined(HAVE_LIBIDN)
/**
 * AddressVarsIdn - IDN Config definitions
 */
struct ConfigDef AddressVarsIdn[] = {
  // clang-format off
  { "idn_decode", DT_BOOL, true, 0, NULL,
    "(idn) Decode international domain names"
  },
  { "idn_encode", DT_BOOL, true, 0, NULL,
    "(idn) Encode international domain names"
  },
  { NULL },
  // clang-format on
};
#endif
