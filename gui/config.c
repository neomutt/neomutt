/**
 * @file
 * Config used by libgui
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
 * @page gui_config Config used by libgui
 *
 * Config used by libgui
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"

/**
 * GuiVars - Config definitions for the Gui library
 */
struct ConfigDef GuiVars[] = {
  // clang-format off
  { "beep", DT_BOOL, true, 0, NULL,
    "Make a noise when an error occurs"
  },
  { "collapse_flagged", DT_BOOL, true, 0, NULL,
    "Prevent the collapse of threads with flagged emails"
  },
  { "collapse_unread", DT_BOOL, true, 0, NULL,
    "Prevent the collapse of threads with unread emails"
  },
  { "duplicate_threads", DT_BOOL, true, 0, NULL,
    "Highlight messages with duplicated message IDs"
  },
  { "hide_limited", DT_BOOL, false, 0, NULL,
    "Don't indicate hidden messages, in the thread tree"
  },
  { "hide_missing", DT_BOOL, true, 0, NULL,
    "Don't indicate missing messages, in the thread tree"
  },
  { "hide_thread_subject", DT_BOOL, true, 0, NULL,
    "Hide subjects that are similar to that of the parent message"
  },
  { "hide_top_limited", DT_BOOL, false, 0, NULL,
    "Don't indicate hidden top message, in the thread tree"
  },
  { "hide_top_missing", DT_BOOL, true, 0, NULL,
    "Don't indicate missing top message, in the thread tree"
  },
  { "narrow_tree", DT_BOOL, false, 0, NULL,
    "Draw a narrower thread tree in the index"
  },
  { "sort_re", DT_BOOL, true, 0, NULL,
    "Whether $reply_regex must be matched when not $strict_threads"
  },
  { "strict_threads", DT_BOOL, false, 0, NULL,
    "Thread messages using 'In-Reply-To' and 'References' headers"
  },
  { "thread_received", DT_BOOL, false, 0, NULL,
    "Sort threaded messages by their received date"
  },

  { NULL },
  // clang-format on
};
