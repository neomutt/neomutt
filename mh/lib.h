/**
 * @file
 * Mh local mailbox type
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_mh Mh Mailbox
 *
 * Mh local mailbox type
 *
 * | File               | Description               |
 * | :----------------- | :------------------------ |
 * | mh/config.c        | @subpage mh_config        |
 * | mh/mdata.c         | @subpage mh_mdata         |
 * | mh/mh.c            | @subpage mh_mh            |
 * | mh/mhemail.c       | @subpage mh_mdemail       |
 * | mh/sequence.c      | @subpage mh_sequence      |
 * | mh/shared.c        | @subpage mh_shared        |
 */

#ifndef MUTT_MH_LIB_H
#define MUTT_MH_LIB_H

#include "core/lib.h"

extern const struct MxOps MxMhOps;

#endif /* MUTT_MH_LIB_H */
