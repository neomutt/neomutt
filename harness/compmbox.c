/**
 * @file
 * Compressed Mailbox test harness
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page harness_compmbox Compressed Mailbox test harness
 *
 * Compressed Mailbox test harness
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "common.h"

/**
 * compmbox_register_hooks - Register default compressed mailbox hooks
 *
 * Registers open-hook, close-hook, and append-hook for .gz files,
 * equivalent to the config file commands:
 *   open-hook   '\.gz$' "gzip -cd '%f' >  '%t'"
 *   close-hook  '\.gz$' "gzip -c  '%t' >  '%f'"
 *   append-hook '\.gz$' "gzip -c  '%t' >> '%f'"
 */
static void compmbox_register_hooks(void)
{
  static const char *HookCommands[] = {
    "open-hook   '\\.gz$' \"gzip -cd '%f' >  '%t'\"",
    "close-hook  '\\.gz$' \"gzip -c  '%t' >  '%f'\"",
    "append-hook '\\.gz$' \"gzip -c  '%t' >> '%f'\"",
    NULL,
  };

  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  struct Buffer *line = buf_pool_get();

  for (const char **cmd = HookCommands; *cmd; cmd++)
  {
    buf_strcpy(line, *cmd);
    parse_rc_line(line, pc, pe);
  }

  buf_pool_release(&line);
  parse_error_free(&pe);
  parse_context_free(&pc);
}

int main(int argc, char *argv[])
{
  struct HarnessOpts opts = { 0 };
  if (harness_parse_args(&opts, argc, argv) != 0)
    return 1;

  opts.type = MUTT_COMPRESSED;

  if (!harness_init(Modules, opts.quiet))
    return 1;

  compmbox_register_hooks();

  int rc = harness_run(&opts);
  harness_cleanup();
  return rc;
}
