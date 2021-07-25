/**
 * @file
 * Validator for the "charset" config variables
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
 * @page config_charset Validate charset
 *
 * Validate the "charset" config variables.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "mutt/lib.h"
#include "charset.h"
#include "lib.h"

/**
 * charset_validator - Validate the "charset" config variable - Implements ConfigDef::validator() - @ingroup cfg_def_validator
 */
int charset_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                      intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if ((cdef->type & DT_CHARSET_SINGLE) && strchr(str, ':'))
  {
    mutt_buffer_printf(
        err, _("'charset' must contain exactly one character set name"));
    return CSR_ERR_INVALID;
  }

  int rc = CSR_SUCCESS;
  bool strict = (cdef->type & DT_CHARSET_STRICT);
  char *q = NULL;
  char *s = mutt_str_dup(str);

  for (char *p = strtok_r(s, ":", &q); p; p = strtok_r(NULL, ":", &q))
  {
    if (*p == '\0')
      continue;
    if (!mutt_ch_check_charset(p, strict))
    {
      rc = CSR_ERR_INVALID;
      mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, p);
      break;
    }
  }

  FREE(&s);
  return rc;
}
