/**
 * @file
 * Config type representing an email address
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ADDRESS_CONFIG_TYPE_H
#define MUTT_ADDRESS_CONFIG_TYPE_H

struct ConfigSubset;

struct Address       *address_new(const char *addr);
const struct Address *cs_subset_address(const struct ConfigSubset *sub, const char *name);

#endif /* MUTT_ADDRESS_CONFIG_TYPE_H */
