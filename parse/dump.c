/**
 * @file
 * Functions to parse commands in a config file
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Marco Sirabella <marco@sirabella.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page parse_dump Functions to parse commands in a config file
 *
 * Functions to parse commands in a config file
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "dump.h"
#include "pager/lib.h"

/**
 * set_dump - Dump list of config variables into a file/pager
 * @param flags Which config to dump, e.g. #GEL_CHANGED_CONFIG
 * @param err   Buffer for error message
 * @return num See #CommandResult
 *
 * FIXME: Move me into parse/set.c.  Note: this function currently depends on
 * pager, which is the reason it is not included in the parse library.
 */
enum CommandResult set_dump(enum GetElemListFlags flags, struct Buffer *err)
{
  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    buf_pool_release(&tempfile);
    return MUTT_CMD_ERROR;
  }

  struct ConfigSet *cs = NeoMutt->sub->cs;
  struct HashElemArray hea = get_elem_list(cs, flags);
  dump_config(cs, &hea, CS_DUMP_NO_FLAGS, fp_out);
  ARRAY_FREE(&hea);

  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = "set";
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);

  return MUTT_CMD_SUCCESS;
}
