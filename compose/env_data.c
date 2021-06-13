/**
 * @file
 * Compose Envelope Data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page compose_env_data Compose Envelope Data
 *
 * Compose Envelope Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "env_data.h"

/**
 * env_data_free - Free the Compose Envelope Data - Implements MuttWindow::wdata_free()
 */
void env_data_free(struct MuttWindow *win, void **ptr)
{
  struct ComposeEnvelopeData *env_data = *ptr;

  mutt_list_free(&env_data->to_list);
  mutt_list_free(&env_data->cc_list);
  mutt_list_free(&env_data->bcc_list);

  FREE(ptr);
}

/**
 * env_data_new - Create new Compose Envelope Data
 * @retval ptr New Compose Envelope Data
 */
struct ComposeEnvelopeData *env_data_new(void)
{
  struct ComposeEnvelopeData *env_data =
      mutt_mem_calloc(1, sizeof(struct ComposeEnvelopeData));

  STAILQ_INIT(&env_data->to_list);
  STAILQ_INIT(&env_data->cc_list);
  STAILQ_INIT(&env_data->bcc_list);

  return env_data;
}
