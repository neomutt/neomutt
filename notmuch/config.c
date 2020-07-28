/**
 * @file
 * Config used by libnotmuch
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page nm_config Config used by libnotmuch
 *
 * Config used by libnotmuch
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "private.h"

// clang-format off
int   C_NmDbLimit;                    ///< Config: (notmuch) Default limit for Notmuch queries
char *C_NmDefaultUrl;                 ///< Config: (notmuch) Path to the Notmuch database
char *C_NmExcludeTags;                ///< Config: (notmuch) Exclude messages with these tags
char *C_NmFlaggedTag;                 ///< Config: (notmuch) Tag to use for flagged messages
int   C_NmOpenTimeout;                ///< Config: (notmuch) Database timeout
char *C_NmQueryType;                  ///< Config: (notmuch) Default query type: 'threads' or 'messages'
int   C_NmQueryWindowCurrentPosition; ///< Config: (notmuch) Position of current search window
char *C_NmQueryWindowCurrentSearch;   ///< Config: (notmuch) Current search parameters
int   C_NmQueryWindowDuration;        ///< Config: (notmuch) Time duration of the current search window
char *C_NmQueryWindowTimebase;        ///< Config: (notmuch) Units for the time duration
char *C_NmRecordTags;                 ///< Config: (notmuch) Tags to apply to the 'record' mailbox (sent mail)
char *C_NmRepliedTag;                 ///< Config: (notmuch) Tag to use for replied messages
char *C_NmUnreadTag;                  ///< Config: (notmuch) Tag to use for unread messages
char *C_VfolderFormat;                ///< Config: (notmuch) printf-like format string for the browser's display of virtual folders
bool  C_VirtualSpoolfile;             ///< Config: (notmuch) Use the first virtual mailbox as a spool file
// clang-format on

struct ConfigDef NotmuchVars[] = {
  // clang-format off
  { "nm_db_limit", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmDbLimit, 0, 0, NULL,
    "(notmuch) Default limit for Notmuch queries"
  },
  { "nm_default_url", DT_STRING, &C_NmDefaultUrl, 0, 0, NULL,
    "(notmuch) Path to the Notmuch database"
  },
  { "nm_exclude_tags", DT_STRING, &C_NmExcludeTags, 0, 0, NULL,
    "(notmuch) Exclude messages with these tags"
  },
  { "nm_flagged_tag", DT_STRING, &C_NmFlaggedTag, IP "flagged", 0, NULL,
    "(notmuch) Tag to use for flagged messages"
  },
  { "nm_open_timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmOpenTimeout, 5, 0, NULL,
    "(notmuch) Database timeout"
  },
  { "nm_query_type", DT_STRING, &C_NmQueryType, IP "messages", 0, NULL,
    "(notmuch) Default query type: 'threads' or 'messages'"
  },
  { "nm_query_window_current_position", DT_NUMBER, &C_NmQueryWindowCurrentPosition, 0, 0, NULL,
    "(notmuch) Position of current search window"
  },
  { "nm_query_window_current_search", DT_STRING, &C_NmQueryWindowCurrentSearch, 0, 0, NULL,
    "(notmuch) Current search parameters"
  },
  { "nm_query_window_duration", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmQueryWindowDuration, 0, 0, NULL,
    "(notmuch) Time duration of the current search window"
  },
  { "nm_query_window_timebase", DT_STRING, &C_NmQueryWindowTimebase, IP "week", 0, NULL,
    "(notmuch) Units for the time duration"
  },
  { "nm_record_tags", DT_STRING, &C_NmRecordTags, 0, 0, NULL,
    "(notmuch) Tags to apply to the 'record' mailbox (sent mail)"
  },
  { "nm_replied_tag", DT_STRING, &C_NmRepliedTag, IP "replied", 0, NULL,
    "(notmuch) Tag to use for replied messages"
  },
  { "nm_unread_tag", DT_STRING, &C_NmUnreadTag, IP "unread", 0, NULL,
    "(notmuch) Tag to use for unread messages"
  },
  { "vfolder_format", DT_STRING|DT_NOT_EMPTY|R_INDEX, &C_VfolderFormat, IP "%2C %?n?%4n/&     ?%4m %f", 0, NULL,
    "(notmuch) printf-like format string for the browser's display of virtual folders"
  },
  { "virtual_spoolfile", DT_BOOL, &C_VirtualSpoolfile, false, 0, NULL,
    "(notmuch) Use the first virtual mailbox as a spool file"
  },

  { "nm_default_uri", DT_SYNONYM, NULL, IP "nm_default_url" },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_notmuch - Register notmuch config variables
 */
bool config_init_notmuch(struct ConfigSet *cs)
{
  return cs_register_variables(cs, NotmuchVars, 0);
}
