/**
 * @file
 * An inherited config item
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

#ifndef _CONFIG_INHERITANCE_H
#define _CONFIG_INHERITANCE_H

#include <stdint.h>

/**
 * struct Inheritance - An inherited config item
 */
struct Inheritance
{
  struct HashElem *parent; /**< HashElem of parent config item */
  const char *name;        /**< Name of this config item */
  struct Account *ac;      /**< Account holding this config item */
  intptr_t var;            /**< (Pointer to) value, of config item */
};

#endif /* _CONFIG_INHERITANCE_H */
