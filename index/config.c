/**
 * @file
 * Config used by libindex
 *
 * @authors
 * Copyright (C) 2021-2026 Richard Russon <rich@flatcap.org>
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
 * @page index_config Config used by the Index
 *
 * Config used by libindex
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "menu/lib.h"
#include "shared_data.h"

extern const struct ExpandoDefinition IndexFormatDef[];

/**
 * StatusFormatDef - Expando definitions
 *
 * Config:
 * - $new_mail_command
 * - $status_format
 * - $ts_icon_format
 * - $ts_status_format
 */
const struct ExpandoDefinition StatusFormatDef[] = {
  // clang-format off
  { "*", "padding-soft",     ED_GLOBAL, ED_GLO_PADDING_SOFT,       node_padding_parse },
  { ">", "padding-hard",     ED_GLOBAL, ED_GLO_PADDING_HARD,       node_padding_parse },
  { "|", "padding-eol",      ED_GLOBAL, ED_GLO_PADDING_EOL,        node_padding_parse },
  { "b", "unread-mailboxes", ED_INDEX,  ED_IND_UNREAD_MAILBOXES,   NULL },
  { "d", "deleted-count",    ED_INDEX,  ED_IND_DELETED_COUNT,      NULL },
  { "D", "description",      ED_INDEX,  ED_IND_DESCRIPTION,        NULL },
  { "f", "mailbox-path",     ED_INDEX,  ED_IND_MAILBOX_PATH,       NULL },
  { "F", "flagged-count",    ED_INDEX,  ED_IND_FLAGGED_COUNT,      NULL },
  { "h", "hostname",         ED_GLOBAL, ED_GLO_HOSTNAME,           NULL },
  { "l", "mailbox-size",     ED_INDEX,  ED_IND_MAILBOX_SIZE,       NULL },
  { "L", "limit-size",       ED_INDEX,  ED_IND_LIMIT_SIZE,         NULL },
  { "m", "message-count",    ED_INDEX,  ED_IND_MESSAGE_COUNT,      NULL },
  { "M", "limit-count",      ED_INDEX,  ED_IND_LIMIT_COUNT,        NULL },
  { "n", "new-count",        ED_INDEX,  ED_IND_NEW_COUNT,          NULL },
  { "o", "old-count",        ED_INDEX,  ED_IND_OLD_COUNT,          NULL },
  { "p", "postponed-count",  ED_INDEX,  ED_IND_POSTPONED_COUNT,    NULL },
  { "P", "percentage",       ED_MENU,   ED_MEN_PERCENTAGE,         NULL },
  { "r", "readonly",         ED_INDEX,  ED_IND_READONLY,           NULL },
  { "R", "read-count",       ED_INDEX,  ED_IND_READ_COUNT,         NULL },
  { "s", "sort",             ED_GLOBAL, ED_GLO_CONFIG_SORT,        NULL },
  { "S", "sort-aux",         ED_GLOBAL, ED_GLO_CONFIG_SORT_AUX,    NULL },
  { "t", "tagged-count",     ED_INDEX,  ED_IND_TAGGED_COUNT,       NULL },
  { "T", "use-threads",      ED_GLOBAL, ED_GLO_CONFIG_USE_THREADS, NULL },
  { "u", "unread-count",     ED_INDEX,  ED_IND_UNREAD_COUNT,       NULL },
  { "v", "version",          ED_GLOBAL, ED_GLO_VERSION,            NULL },
  { "V", "limit-pattern",    ED_INDEX,  ED_IND_LIMIT_PATTERN,      NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/// StatusFormatDefNoPadding - Status format definitions, without padding
static const struct ExpandoDefinition *const StatusFormatDefNoPadding = &(StatusFormatDef[3]);

/**
 * IndexVars - Config definitions for the Index
 */
struct ConfigDef IndexVars[] = {
  // clang-format off
  { "beep_new", DT_BOOL, false, 0, NULL,
    "Make a noise when new mail arrives"
  },
  { "change_folder_next", DT_BOOL, false, 0, NULL,
    "Suggest the next folder, rather than the first when using '<change-folder>'"
  },
  { "collapse_all", DT_BOOL, false, 0, NULL,
    "Collapse all threads when entering a folder"
  },
  { "crypt_chars", DT_MBTABLE, IP "SPsK ", 0, NULL,
    "User-configurable crypto flags: signed, encrypted etc."
  },
  { "flag_chars", DT_MBTABLE, IP "*!DdrONon- ", 0, NULL,
    "User-configurable index flags: tagged, new, etc"
  },
  { "from_chars", DT_MBTABLE, 0, 0, NULL,
    "User-configurable index flags: to address, cc address, etc"
  },
  { "index_format", DT_EXPANDO|D_NOT_EMPTY, IP "%4C %Z %{%b %d} %-15.15L (%<l?%4l&%4c>) %s", IP &IndexFormatDef, NULL,
    "printf-like format string for the index menu (emails)"
  },
  { "mark_macro_prefix", DT_STRING, IP "'", 0, NULL,
    "Prefix for macros using '<mark-message>'"
  },
  { "new_mail_command", DT_EXPANDO|D_STRING_COMMAND, 0, IP StatusFormatDefNoPadding, NULL,
    "External command to run when new mail arrives"
  },
  { "quit", DT_QUAD, MUTT_YES, 0, NULL,
    "Prompt before exiting NeoMutt"
  },
  { "read_only", DT_BOOL, false, 0, NULL,
    "Open folders in read-only mode"
  },
  // L10N: $status_format default format
  { "status_format", DT_EXPANDO|D_L10N_STRING, IP N_("-%r-NeoMutt: %D [Msgs:%<M?%M/>%m%<n? New:%n>%<o? Old:%o>%<d? Del:%d>%<F? Flag:%F>%<t? Tag:%t>%<p? Post:%p>%<b? Inc:%b>%<l? %l>]---(%<T?%T/>%s/%S)-%>-(%P)---"), IP &StatusFormatDef, NULL,
    "printf-like format string for the index's status line"
  },
  { "status_chars", DT_MBTABLE, IP "-*%A", 0, NULL,
    "Indicator characters for the status bar"
  },
  { "to_chars", DT_MBTABLE, IP " +TCFLR", 0, NULL,
    "Indicator characters for the 'To' field in the index"
  },
  { "ts_enabled", DT_BOOL, false, 0, NULL,
    "Allow NeoMutt to set the terminal status line and icon"
  },
  // L10N: $ts_icon_format default format
  { "ts_icon_format", DT_EXPANDO|D_L10N_STRING, IP N_("M%<n?AIL&ail>"), IP StatusFormatDefNoPadding, NULL,
    "printf-like format string for the terminal's icon title"
  },
  // L10N: $ts_status_format default format
  { "ts_status_format", DT_EXPANDO|D_L10N_STRING, IP N_("NeoMutt with %<m?%m messages&no messages>%<n? [%n NEW]>"), IP StatusFormatDefNoPadding, NULL,
    "printf-like format string for the terminal's status (window title)"
  },
  { "uncollapse_jump", DT_BOOL, false, 0, NULL,
    "When opening a thread, jump to the next unread message"
  },
  { "uncollapse_new", DT_BOOL, true, 0, NULL,
    "Open collapsed threads when new mail arrives"
  },

  { "hdr_format",       DT_SYNONYM, IP "index_format",   IP "2021-03-21" },
  { "xterm_icon",       DT_SYNONYM, IP "ts_icon_format", IP "2021-03-21" },
  { "xterm_set_titles", DT_SYNONYM, IP "ts_enabled",     IP "2021-03-21" },
  { "xterm_title",      DT_SYNONYM, IP "ts_status_format", IP "2021-03-21" },

  { NULL },
  // clang-format on
};
