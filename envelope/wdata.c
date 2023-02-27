/**
 * @file
 * Envelope Window Data
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
 * @page envelope_wdata Envelope Window Data
 *
 * Envelope Window Data
 */

#include "config.h"
#include "mutt/lib.h"
#include "wdata.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

/**
 * env_wdata_free - Free the Envelope Data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
void env_wdata_free(struct MuttWindow *win, void **ptr)
{
  FREE(ptr);
}

/**
 * env_wdata_new - Create new Envelope Data
 * @retval ptr New Envelope Data
 */
struct EnvelopeWindowData *env_wdata_new(void)
{
  struct EnvelopeWindowData *wdata = mutt_mem_calloc(1, sizeof(struct EnvelopeWindowData));

#ifdef USE_AUTOCRYPT
  wdata->autocrypt_rec = AUTOCRYPT_REC_OFF;
#endif

  return wdata;
}
