/**
 * @file
 * PGP/Smime functions
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page crypt_functions PGP/Smime functions
 *
 * PGP/Smime functions
 */

#include "config.h"
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <stddef.h>
#include "gui/lib.h"
#include "key/lib.h"
#endif

// clang-format off
/**
 * OpPgp - Functions for the Pgp Menu
 */
const struct MenuFuncOp OpPgp[] = { /* map: pgp */
  { "exit",                          OP_EXIT },
  { "verify-key",                    OP_VERIFY_KEY },
  { "view-name",                     OP_VIEW_ID },
  { NULL, 0 },
};

/**
 * OpSmime - Functions for the Smime Menu
 */
const struct MenuFuncOp OpSmime[] = { /* map: smime */
  { "exit",                          OP_EXIT },
#ifdef CRYPT_BACKEND_GPGME
  { "verify-key",                    OP_VERIFY_KEY },
  { "view-name",                     OP_VIEW_ID },
#endif
  { NULL, 0 },
};

/**
 * PgpDefaultBindings - Key bindings for the Pgp Menu
 */
const struct MenuOpSeq PgpDefaultBindings[] = { /* map: pgp */
  { OP_EXIT,                               "q" },
  { OP_VERIFY_KEY,                         "c" },
  { OP_VIEW_ID,                            "%" },
  { 0, NULL },
};

/**
 * SmimeDefaultBindings - Key bindings for the Smime Menu
 */
const struct MenuOpSeq SmimeDefaultBindings[] = { /* map: smime */
  { OP_EXIT,                               "q" },
#ifdef CRYPT_BACKEND_GPGME
  { OP_VERIFY_KEY,                         "c" },
  { OP_VIEW_ID,                            "%" },
#endif
  { 0, NULL },
};
// clang-format on
