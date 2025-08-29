/**
 * @file
 * Convenience wrapper for the config headers
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
 * @page lib_config Config
 *
 * User configurable variables.
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | config/bool.c       | @subpage config_bool       |
 * | config/charset.c    | @subpage config_charset    |
 * | config/dump.c       | @subpage config_dump       |
 * | config/enum.c       | @subpage config_enum       |
 * | config/helpers.c    | @subpage config_helpers    |
 * | config/long.c       | @subpage config_long       |
 * | config/mbtable.c    | @subpage config_mbtable    |
 * | config/myvar.c      | @subpage config_myvar      |
 * | config/number.c     | @subpage config_number     |
 * | config/path.c       | @subpage config_path       |
 * | config/quad.c       | @subpage config_quad       |
 * | config/regex.c      | @subpage config_regex      |
 * | config/set.c        | @subpage config_set        |
 * | config/slist.c      | @subpage config_slist      |
 * | config/sort.c       | @subpage config_sort       |
 * | config/string.c     | @subpage config_string     |
 * | config/subset.c     | @subpage config_subset     |
 */

#ifndef MUTT_CONFIG_LIB_H
#define MUTT_CONFIG_LIB_H

#include <stdbool.h>
// IWYU pragma: begin_keep
#include "bool.h"
#include "charset.h"
#include "dump.h"
#include "enum.h"
#include "helpers.h"
#include "inheritance.h"
#include "mbtable.h"
#include "number.h"
#include "quad.h"
#include "regex2.h"
#include "set.h"
#include "sort2.h"
#include "subset.h"
#include "types.h"
// IWYU pragma: end_keep

/**
 * @defgroup cfg_module_api Config Module API
 *
 * Prototype for a Config Definition Function
 *
 * @param cs Config items
 * @retval true All the config variables were registered
 */
typedef bool (*module_init_config_t)(struct ConfigSet *cs);

#endif /* MUTT_CONFIG_LIB_H */
