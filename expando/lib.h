/**
 * @file
 * Parse Expando string
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
 * @page lib_expando Parse Expando string
 *
 * Parse Expando string
 *
 * | File                              | Description                     |
 * | :-------------------------------- | :------------------------------ |
 * | expando/config_type.c             | @subpage expando_config_type    |
 * | expando/expando.c                 | @subpage expando_expando        |
 * | expando/filter.c                  | @subpage expando_filter         |
 * | expando/format.c                  | @subpage expando_format         |
 * | expando/helpers.c                 | @subpage expando_helpers        |
 * | expando/module.c                  | @subpage expando_module         |
 * | expando/node.c                    | @subpage expando_node           |
 * | expando/node_condbool.c           | @subpage expando_node_condbool  |
 * | expando/node_conddate.c           | @subpage expando_node_conddate  |
 * | expando/node_condition.c          | @subpage expando_node_condition |
 * | expando/node_container.c          | @subpage expando_node_container |
 * | expando/node_expando.c            | @subpage expando_node_expando   |
 * | expando/node_padding.c            | @subpage expando_node_padding   |
 * | expando/node_text.c               | @subpage expando_node_text      |
 * | expando/parse.c                   | @subpage expando_parse          |
 * | expando/render.c                  | @subpage expando_render         |
 */

#ifndef MUTT_EXPANDO_LIB_H
#define MUTT_EXPANDO_LIB_H

// IWYU pragma: begin_keep
#include "definition.h"
#include "domain.h"
#include "expando.h"
#include "filter.h"
#include "format.h"
#include "helpers.h"
#include "node.h"
#include "node_condbool.h"
#include "node_conddate.h"
#include "node_condition.h"
#include "node_container.h"
#include "node_expando.h"
#include "node_padding.h"
#include "node_text.h"
#include "parse.h"
#include "render.h"
#include "uid.h"
// IWYU pragma: end_keep

struct ConfigSubset;

const struct Expando *cs_subset_expando(const struct ConfigSubset *sub, const char *name);

#endif /* MUTT_EXPANDO_LIB_H */
