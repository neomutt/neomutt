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

#ifndef MUTT_ENVELOPE_WDATA_H
#define MUTT_ENVELOPE_WDATA_H

#include <stdbool.h>
#include "config.h"
#include "mutt/lib.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

struct MuttWindow;

/**
 * struct EnvelopeWindowData - Data to fill the Envelope Window
 */
struct EnvelopeWindowData
{
  struct ConfigSubset *sub;        ///< Inherited config items
  struct Email *email;             ///< Email being composed
  struct Buffer *fcc;              ///< Where the outgoing Email will be saved

  short to_rows;                   ///< Number of rows used by the 'To:' field
  short cc_rows;                   ///< Number of rows used by the 'Cc:' field
  short bcc_rows;                  ///< Number of rows used by the 'Bcc:' field
  short sec_rows;                  ///< Number of rows used by the security fields

#ifdef USE_NNTP
  bool is_news;                    ///< Email is a news article
#endif
#ifdef USE_AUTOCRYPT
  enum AutocryptRec autocrypt_rec; ///< Autocrypt recommendation
#endif
};

void                       env_wdata_free(struct MuttWindow *win, void **ptr);
struct EnvelopeWindowData *env_wdata_new (void);

#endif /* MUTT_ENVELOPE_WDATA_H */
