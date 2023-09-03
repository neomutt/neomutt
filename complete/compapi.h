/**
 * @file
 * API Auto-Completion
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

#ifndef MUTT_COMPLETE_COMPAPI_H
#define MUTT_COMPLETE_COMPAPI_H

#include "core/lib.h"

struct EnterWindowData;

/**
 * @defgroup complete_api Auto-Completion API
 *
 * Auto-Completion API
 */
struct CompleteOps
{
  /**
   * @defgroup compapi_complete complete()
   * @ingroup complete_api
   *
   * complete - Perform Auto-Completion
   * @param wdata  Enter Window data
   * @param op     Operation to perform, e.g. OP_COMPLETE
   * @retval num #FunctionRetval, e.g. #FR_SUCCESS
   */
  enum FunctionRetval (*complete)(struct EnterWindowData *wdata, int op);
};

#endif /* MUTT_COMPLETE_COMPAPI_H */
