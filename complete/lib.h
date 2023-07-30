/**
 * @file
 * Auto-completion
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page lib_complete Auto-completion
 *
 * Auto-complete a string
 *
 * | File                   | Description                  |
 * | :--------------------- | :--------------------------- |
 * | complete/complete.c    | @subpage complete_complete   |
 * | complete/data.c        | @subpage complete_data       |
 * | complete/helpers.c     | @subpage complete_helpers    |
 */

#ifndef MUTT_COMPLETE_LIB_H
#define MUTT_COMPLETE_LIB_H

#include <stdbool.h>
// IWYU pragma: begin_exports
#include "data.h"
// IWYU pragma: end_exports

struct Buffer;

int  mutt_command_complete  (struct CompletionData *cd, struct Buffer *buf, int pos, int numtabs);
int  mutt_complete          (struct CompletionData *cd, struct Buffer *buf);
int  mutt_label_complete    (struct CompletionData *cd, struct Buffer *buf, int numtabs);
bool mutt_nm_query_complete (struct CompletionData *cd, struct Buffer *buf, int pos, int numtabs);
bool mutt_nm_tag_complete   (struct CompletionData *cd, struct Buffer *buf, int numtabs);
int  mutt_var_value_complete(struct CompletionData *cd, struct Buffer *buf, int pos);

#endif /* MUTT_COMPLETE_LIB_H */
