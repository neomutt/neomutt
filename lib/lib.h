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
 * @note The library is self-contained -- some files may depend on others in
 *       the library, but none depends on source from outside.
 *
 * -# @subpage base64
 * -# @subpage buffer
 * -# @subpage date
 * -# @subpage debug
 * -# @subpage exit
 * -# @subpage file
 * -# @subpage hash
 * -# @subpage list
 * -# @subpage mapping
 * -# @subpage mbyte
 * -# @subpage md5
 * -# @subpage memory
 * -# @subpage message
 * -# @subpage sha1
 * -# @subpage string
 */

#ifndef _LIB_LIB_H
#define _LIB_LIB_H

#include "base64.h"
#include "buffer.h"
#include "date.h"
#include "debug.h"
#include "exit.h"
#include "file.h"
#include "hash.h"
#include "list.h"
#include "mapping.h"
#include "mbyte.h"
#include "md5.h"
#include "memory.h"
#include "message.h"
#include "sha1.h"
#include "string2.h"

#endif /* _LIB_LIB_H */
