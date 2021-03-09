/**
 * @file
 * Helper functions to get config values
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

#ifndef MUTT_CONFIG_HELPERS_H
#define MUTT_CONFIG_HELPERS_H

#include "quad.h"

struct ConfigSubset;

const struct Address *cs_subset_address(const struct ConfigSubset *sub, const char *name);
bool                  cs_subset_bool   (const struct ConfigSubset *sub, const char *name);
unsigned char         cs_subset_enum   (const struct ConfigSubset *sub, const char *name);
long                  cs_subset_long   (const struct ConfigSubset *sub, const char *name);
struct MbTable       *cs_subset_mbtable(const struct ConfigSubset *sub, const char *name);
short                 cs_subset_number (const struct ConfigSubset *sub, const char *name);
const char *          cs_subset_path   (const struct ConfigSubset *sub, const char *name);
enum QuadOption       cs_subset_quad   (const struct ConfigSubset *sub, const char *name);
const struct Regex *  cs_subset_regex  (const struct ConfigSubset *sub, const char *name);
const struct Slist *  cs_subset_slist  (const struct ConfigSubset *sub, const char *name);
short                 cs_subset_sort   (const struct ConfigSubset *sub, const char *name);
const char *          cs_subset_string (const struct ConfigSubset *sub, const char *name);

#endif /* MUTT_CONFIG_HELPERS_H */

