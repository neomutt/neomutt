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
 * @page mutt Shared functions
 *
 * Each source file in the library provides a group of related functions.
 *
 * | File             | Description        |
 * | :--------------- | :----------------- |
 * | mutt/address.c   | @subpage address   |
 * | mutt/base64.c    | @subpage base64    |
 * | mutt/buffer.c    | @subpage buffer    |
 * | mutt/charset.c   | @subpage charset   |
 * | mutt/date.c      | @subpage date      |
 * | mutt/exit.c      | @subpage exit      |
 * | mutt/file.c      | @subpage file      |
 * | mutt/hash.c      | @subpage hash      |
 * | mutt/idna.c      | @subpage idna      |
 * | mutt/list.c      | @subpage list      |
 * | mutt/logging.c   | @subpage logging   |
 * | mutt/mapping.c   | @subpage mapping   |
 * | mutt/mbyte.c     | @subpage mbyte     |
 * | mutt/md5.c       | @subpage md5       |
 * | mutt/memory.c    | @subpage memory    |
 * | mutt/mime.c      | @subpage mime      |
 * | mutt/parameter.c | @subpage parameter |
 * | mutt/regex.c     | @subpage regex     |
 * | mutt/rfc2047.c   | @subpage rfc2047   |
 * | mutt/sha1.c      | @subpage sha1      |
 * | mutt/signal.c    | @subpage signal    |
 * | mutt/string.c    | @subpage string    |
 *
 * @note The library is self-contained -- some files may depend on others in
 *       the library, but none depends on source from outside.
 */

#ifndef _MUTT_MUTT_H
#define _MUTT_MUTT_H

#include "address.h"
#include "base64.h"
#include "buffer.h"
#include "charset.h"
#include "date.h"
#include "exit.h"
#include "file.h"
#include "hash.h"
#include "idna2.h"
#include "list.h"
#include "logging.h"
#include "mapping.h"
#include "mbyte.h"
#include "md5.h"
#include "memory.h"
#include "message.h"
#include "mime.h"
#include "parameter.h"
#include "regex3.h"
#include "rfc2047.h"
#include "sha1.h"
#include "signal2.h"
#include "string2.h"

#endif /* _MUTT_MUTT_H */
