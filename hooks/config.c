/**
 * @file
 * Config used by libhooks
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
 * @page hooks_config Config used by libhooks
 *
 * Config used by libhooks
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"

/**
 * HooksVars - Config definitions for the Hooks library
 */
struct ConfigDef HooksVars[] = {
  // clang-format off
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL,
    "Pattern to use for hooks that only have a simple regex"
  },
  { "force_name", DT_BOOL, false, 0, NULL,
    "Save outgoing mail in a folder of their name"
  },
  { "save_name", DT_BOOL, false, 0, NULL,
    "Save outgoing message to mailbox of recipient's name if it exists"
  },
  { NULL },
  // clang-format on
};
