/**
 * @file
 * Convenience wrapper for the core headers
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page lib_core Core NeoMutt objects
 *
 * Backbone objects of NeoMutt
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | core/account.c      | @subpage core_account      |
 * | core/dispatcher.c   | @subpage core_dispatcher   |
 * | core/mailbox.c      | @subpage core_mailbox      |
 * | core/message.c      | @subpage core_message      |
 * | core/neomutt.c      | @subpage core_neomutt      |
 * | core/tmp.c          | @subpage core_tmp          |
 */

#ifndef MUTT_CORE_LIB_H
#define MUTT_CORE_LIB_H

// IWYU pragma: begin_exports
#include "account.h"
#include "command.h"
#include "dispatcher.h"
#include "mailbox.h"
#include "message.h"
#include "mxapi.h"
#include "neomutt.h"
#include "tmp.h"
// IWYU pragma: end_exports

#endif /* MUTT_CORE_LIB_H */
