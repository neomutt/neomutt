/**
 * @file
 * Envelope functions
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
 * @page envelope_functions Envelope functions
 *
 * Envelope functions
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "attach/lib.h"
#include "browser/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "hook.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "rfc3676.h"
#include "wdata.h"
#ifdef MIXMASTER
#include "remailer.h"
#endif

/**
 * EnvelopeFunctions - All the NeoMutt functions that the Envelope supports
 */
struct EnvelopeFunction EnvelopeFunctions[] = {
  // clang-format off
  { 0, NULL },
  // clang-format on
};

/**
 * env_function_dispatcher - Perform an Envelope function
 * @param win Envelope Window
 * @param op  Operation to perform, e.g. OP_ENVELOPE_EDIT_TO
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int env_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return IR_UNKNOWN;

  int rc = IR_UNKNOWN;
  for (size_t i = 0; EnvelopeFunctions[i].op != OP_NULL; i++)
  {
    const struct EnvelopeFunction *fn = &EnvelopeFunctions[i];
    if (fn->op == op)
    {
      struct EnvelopeWindowData *wdata = win->wdata;
      rc = fn->function(wdata, op);
      break;
    }
  }

  if (rc == IR_UNKNOWN) // Not our function
    return rc;

  const char *result = mutt_map_get_name(rc, RetvalNames);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", OpStrings[op][0], op, NONULL(result));

  return rc;
}
