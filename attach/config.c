/**
 * @file
 * Config used by libattach
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page attach_config Config used by libattach
 *
 * Config used by libattach
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"

extern const struct ExpandoDefinition IndexFormatDef[];

/**
 * AttachVars - Config definitions for the Attach library
 */
struct ConfigDef AttachVars[] = {
  // clang-format off
  { "attach_save_dir", DT_PATH|D_PATH_DIR, IP "./", 0, NULL,
    "Default directory where attachments are saved"
  },
  { "attach_save_without_prompting", DT_BOOL, false, 0, NULL,
    "If true, then don't prompt to save"
  },
  { "attach_sep", DT_STRING, IP "\n", 0, NULL,
    "Separator to add between saved/printed/piped attachments"
  },
  { "attach_split", DT_BOOL, true, 0, NULL,
    "Save/print/pipe tagged messages individually"
  },
  { "bounce", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Confirm before bouncing a message"
  },
  { "count_alternatives", DT_BOOL, false, 0, NULL,
    "Recurse inside multipart/alternatives while counting attachments"
  },
  { "digest_collapse", DT_BOOL, true, 0, NULL,
    "Hide the subparts of a multipart/digest"
  },
  { "message_format", DT_EXPANDO|D_NOT_EMPTY, IP "%s", IP &IndexFormatDef, NULL,
    "printf-like format string for listing attached messages"
  },
  { "mime_forward", DT_QUAD, MUTT_NO, 0, NULL,
    "Forward a message as a 'message/RFC822' MIME part"
  },
  { "mime_forward_rest", DT_QUAD, MUTT_YES, 0, NULL,
    "Forward all attachments, even if they can't be decoded"
  },

  { "mime_fwd",   DT_SYNONYM, IP "mime_forward",   IP "2021-03-21" },
  { "msg_format", DT_SYNONYM, IP "message_format", IP "2021-03-21" },

  { NULL },
  // clang-format on
};
