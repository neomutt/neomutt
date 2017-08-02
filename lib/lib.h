/**
 * @file
 * Convenience wrapper for the library headers
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
 * @page lib Library of shared functions
 *
 * Each source file in the library provides a group of related functions.
 *
 * The library is self-contained -- some files may depend on others in the
 * library, but none depends on source from outside.
 *
 * -# @subpage ascii
 * -# @subpage base64
 * -# @subpage date
 * -# @subpage exit
 */

#ifndef _LIB_LIB_H
#define _LIB_LIB_H

#include "lib_ascii.h"
#include "lib_base64.h"
#include "lib_date.h"
#include "lib_exit.h"

#endif /* _LIB_LIB_H */
