/**
 * @file
 * Config used by libbrowser
 *
 * @authors
 * Copyright (C) 2022 Carlos Henrique Lima Melara <charlesmelara@outlook.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page browser_config Config used by libbrowser
 *
 * Config used by libbrowser
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "lib.h"
#include "expando/lib.h"
#include "sort.h"

/**
 * BrowserSortMethods - Sort methods for the folder/dir browser
 */
static const struct Mapping BrowserSortMethods[] = {
  // clang-format off
  { "alpha",    BROWSER_SORT_ALPHA },
  { "count",    BROWSER_SORT_COUNT },
  { "date",     BROWSER_SORT_DATE },
  { "desc",     BROWSER_SORT_DESC },
  { "size",     BROWSER_SORT_SIZE },
  { "new",      BROWSER_SORT_NEW },
  { "unsorted", BROWSER_SORT_UNSORTED },
  // Compatibility
  { "unread",   BROWSER_SORT_NEW },
  { NULL, 0 },
  // clang-format on
};

/**
 * parse_folder_date - Parse a Date Expando - Implements ExpandoDefinition::parse() - @ingroup expando_parse_api
 *
 * Parse a custom Expando of the form, "%[string]".
 * The "string" will be passed to strftime().
 */
struct ExpandoNode *parse_folder_date(const char *str, struct ExpandoFormat *fmt,
                                      int did, int uid, ExpandoParserFlags flags,
                                      const char **parsed_until,
                                      struct ExpandoParseError *err)
{
  if (flags & EP_CONDITIONAL)
  {
    return node_conddate_parse(str, did, uid, parsed_until, err);
  }

  return node_expando_parse_enclosure(str, did, uid, ']', fmt, parsed_until, err);
}

/**
 * FolderFormatDef - Expando definitions
 *
 * Config:
 * - $folder_format
 * - $mailbox_folder_format
 */
static const struct ExpandoDefinition FolderFormatDef[] = {
  // clang-format off
  { "*", "padding-soft",  ED_GLOBAL, ED_GLO_PADDING_SOFT,  node_padding_parse },
  { ">", "padding-hard",  ED_GLOBAL, ED_GLO_PADDING_HARD,  node_padding_parse },
  { "|", "padding-eol",   ED_GLOBAL, ED_GLO_PADDING_EOL,   node_padding_parse },
  { " ", "padding-space", ED_GLOBAL, ED_GLO_PADDING_SPACE, NULL },
  { "a", "notify",        ED_FOLDER, ED_FOL_NOTIFY,        NULL },
  { "C", "number",        ED_FOLDER, ED_FOL_NUMBER,        NULL },
  { "d", "date",          ED_FOLDER, ED_FOL_DATE,          NULL },
  { "D", "date-format",   ED_FOLDER, ED_FOL_DATE_FORMAT,   NULL },
  { "f", "filename",      ED_FOLDER, ED_FOL_FILENAME,      NULL },
  { "F", "file-mode",     ED_FOLDER, ED_FOL_FILE_MODE,     NULL },
  { "g", "file-group",    ED_FOLDER, ED_FOL_FILE_GROUP,    NULL },
  { "i", "description",   ED_FOLDER, ED_FOL_DESCRIPTION,   NULL },
  { "l", "hard-links",    ED_FOLDER, ED_FOL_HARD_LINKS,    NULL },
  { "m", "message-count", ED_FOLDER, ED_FOL_MESSAGE_COUNT, NULL },
  { "n", "unread-count",  ED_FOLDER, ED_FOL_UNREAD_COUNT,  NULL },
  { "N", "new-mail",      ED_FOLDER, ED_FOL_NEW_MAIL,      NULL },
  { "p", "poll",          ED_FOLDER, ED_FOL_POLL,          NULL },
  { "s", "file-size",     ED_FOLDER, ED_FOL_FILE_SIZE,     NULL },
  { "t", "tagged",        ED_FOLDER, ED_FOL_TAGGED,        NULL },
  { "u", "file-owner",    ED_FOLDER, ED_FOL_FILE_OWNER,    NULL },
  { "[", NULL,            ED_FOLDER, ED_FOL_DATE_STRF,     parse_folder_date },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * GroupIndexFormatDef - Expando definitions
 *
 * Config:
 * - $group_index_format
 */
static const struct ExpandoDefinition GroupIndexFormatDef[] = {
  // clang-format off
  { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  node_padding_parse },
  { "a", "notify",       ED_FOLDER, ED_FOL_NOTIFY,       NULL },
  { "C", "number",       ED_FOLDER, ED_FOL_NUMBER,       NULL },
  { "d", "description",  ED_FOLDER, ED_FOL_DESCRIPTION,  NULL },
  { "f", "newsgroup",    ED_FOLDER, ED_FOL_NEWSGROUP,    NULL },
  { "M", "flags",        ED_FOLDER, ED_FOL_FLAGS,        NULL },
  { "n", "new-count",    ED_FOLDER, ED_FOL_NEW_COUNT,    NULL },
  { "N", "flags2",       ED_FOLDER, ED_FOL_FLAGS2,       NULL },
  { "p", "poll",         ED_FOLDER, ED_FOL_POLL,         NULL },
  { "s", "unread-count", ED_FOLDER, ED_FOL_UNREAD_COUNT, NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * BrowserVars - Config definitions for the browser
 */
struct ConfigDef BrowserVars[] = {
  // clang-format off
  { "browser_abbreviate_mailboxes", DT_BOOL, true, 0, NULL,
    "Abbreviate mailboxes using '~' and '=' in the browser"
  },
  { "browser_sort", DT_SORT|D_SORT_REVERSE, BROWSER_SORT_ALPHA, IP BrowserSortMethods, NULL,
    "Sort method for the browser"
  },
  { "folder_format", DT_EXPANDO|D_NOT_EMPTY, IP "%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %i", IP &FolderFormatDef, NULL,
    "printf-like format string for the browser's display of folders"
  },
  { "group_index_format", DT_EXPANDO|D_NOT_EMPTY, IP "%4C %M%N %5s  %-45.45f %d", IP &GroupIndexFormatDef, NULL,
    "(nntp) printf-like format string for the browser's display of newsgroups"
  },
  { "mailbox_folder_format", DT_EXPANDO|D_NOT_EMPTY, IP "%2C %<n?%6n&      > %6m %i", IP &FolderFormatDef, NULL,
    "printf-like format string for the browser's display of mailbox folders"
  },
  { "mask", DT_REGEX|D_REGEX_MATCH_CASE|D_REGEX_ALLOW_NOT|D_REGEX_NOSUB, IP "!^\\.[^.]", 0, NULL,
    "Only display files/dirs matching this regex in the browser"
  },
  { "show_only_unread", DT_BOOL, false, 0, NULL,
    "(nntp) Only show subscribed newsgroups with unread articles"
  },
  { "browser_sort_dirs_first", DT_BOOL, false, 0, NULL,
    "Group directories before files in the browser"
  },

  { "sort_browser", DT_SYNONYM, IP "browser_sort", IP "2024-11-20" },

  { NULL },
  // clang-format on
};
