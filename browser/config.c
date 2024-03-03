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
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "lib.h"
#include "expando/lib.h"
#include "menu/lib.h"

/**
 * SortBrowserMethods - Sort methods for the folder/dir browser
 */
static const struct Mapping SortBrowserMethods[] = {
  // clang-format off
  { "alpha",    SORT_SUBJECT },
  { "count",    SORT_COUNT },
  { "date",     SORT_DATE },
  { "desc",     SORT_DESC },
  { "new",      SORT_UNREAD },
  { "unread",   SORT_UNREAD },
  { "size",     SORT_SIZE },
  { "unsorted", SORT_ORDER },
  { NULL, 0 },
  // clang-format on
};

/**
 * parse_folder_date - Parse a Date Expando - Implements ExpandoDefinition::parse - @ingroup expando_parse_api
 *
 * Parse a custom Expando of the form, "%[string]".
 * The "string" will be passed to strftime().
 */
struct ExpandoNode *parse_folder_date(const char *str, const char **parsed_until,
                                      int did, int uid, ExpandoParserFlags flags,
                                      struct ExpandoParseError *error)
{
  if (flags & EP_CONDITIONAL)
  {
    return node_conddate_parse(str + 1, parsed_until, did, uid, error);
  }

  return node_expando_parse_enclosure(str, parsed_until, did, uid, ']', error);
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
  { "^", "arrow",         ED_GLOBAL, ED_MEN_ARROW,         E_TYPE_STRING, NULL },
  { "*", "padding-soft",  ED_GLOBAL, ED_GLO_PADDING_SOFT,  E_TYPE_STRING, node_padding_parse },
  { ">", "padding-hard",  ED_GLOBAL, ED_GLO_PADDING_HARD,  E_TYPE_STRING, node_padding_parse },
  { "|", "padding-eol",   ED_GLOBAL, ED_GLO_PADDING_EOL,   E_TYPE_STRING, node_padding_parse },
  { " ", "padding-space", ED_GLOBAL, ED_GLO_PADDING_SPACE, E_TYPE_STRING, NULL },
  { "a", "notify",        ED_FOLDER, ED_FOL_NOTIFY,        E_TYPE_NUMBER, NULL },
  { "C", "number",        ED_FOLDER, ED_FOL_NUMBER,        E_TYPE_NUMBER, NULL },
  { "d", "date",          ED_FOLDER, ED_FOL_DATE,          E_TYPE_STRING, NULL },
  { "D", "date-format",   ED_FOLDER, ED_FOL_DATE_FORMAT,   E_TYPE_STRING, NULL },
  { "f", "filename",      ED_FOLDER, ED_FOL_FILENAME,      E_TYPE_STRING, NULL },
  { "F", "file-mode",     ED_FOLDER, ED_FOL_FILE_MODE,     E_TYPE_STRING, NULL },
  { "g", "file-group",    ED_FOLDER, ED_FOL_FILE_GROUP,    E_TYPE_STRING, NULL },
  { "i", "description",   ED_FOLDER, ED_FOL_DESCRIPTION,   E_TYPE_STRING, NULL },
  { "l", "hard-links",    ED_FOLDER, ED_FOL_HARD_LINKS,    E_TYPE_STRING, NULL },
  { "m", "message-count", ED_FOLDER, ED_FOL_MESSAGE_COUNT, E_TYPE_NUMBER, NULL },
  { "n", "unread-count",  ED_FOLDER, ED_FOL_UNREAD_COUNT,  E_TYPE_NUMBER, NULL },
  { "N", "new-mail",      ED_FOLDER, ED_FOL_NEW_MAIL,      E_TYPE_STRING, NULL },
  { "p", "poll",          ED_FOLDER, ED_FOL_POLL,          E_TYPE_NUMBER, NULL },
  { "s", "file-size",     ED_FOLDER, ED_FOL_FILE_SIZE,     E_TYPE_NUMBER, NULL },
  { "t", "tagged",        ED_FOLDER, ED_FOL_TAGGED,        E_TYPE_STRING, NULL },
  { "u", "file-owner",    ED_FOLDER, ED_FOL_FILE_OWNER,    E_TYPE_STRING, NULL },
  { "[", NULL,            ED_FOLDER, ED_FOL_STRF,          E_TYPE_STRING, parse_folder_date },
  { NULL, NULL, 0, -1, -1, NULL }
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
  { "^", "arrow",        ED_GLOBAL, ED_MEN_ARROW,        E_TYPE_STRING, NULL },
  { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
  { "a", "notify",       ED_FOLDER, ED_FOL_NOTIFY,       E_TYPE_NUMBER, NULL },
  { "C", "number",       ED_FOLDER, ED_FOL_NUMBER,       E_TYPE_NUMBER, NULL },
  { "d", "description",  ED_FOLDER, ED_FOL_DESCRIPTION,  E_TYPE_STRING, NULL },
  { "f", "newsgroup",    ED_FOLDER, ED_FOL_NEWSGROUP,    E_TYPE_STRING, NULL },
  { "M", "flags",        ED_FOLDER, ED_FOL_FLAGS,        E_TYPE_STRING, NULL },
  { "n", "new-count",    ED_FOLDER, ED_FOL_NEW_COUNT,    E_TYPE_NUMBER, NULL },
  { "N", "flags2",       ED_FOLDER, ED_FOL_FLAGS2,       E_TYPE_STRING, NULL },
  { "p", "poll",         ED_FOLDER, ED_FOL_POLL,         E_TYPE_NUMBER, NULL },
  { "s", "unread-count", ED_FOLDER, ED_FOL_UNREAD_COUNT, E_TYPE_NUMBER, NULL },
  { NULL, NULL, 0, -1, -1, NULL }
  // clang-format on
};

/**
 * BrowserVars - Config definitions for the browser
 */
static struct ConfigDef BrowserVars[] = {
  // clang-format off
  { "browser_abbreviate_mailboxes", DT_BOOL, true, 0, NULL,
    "Abbreviate mailboxes using '~' and '=' in the browser"
  },
  { "folder_format", DT_EXPANDO|D_NOT_EMPTY, IP "%^%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %i", IP &FolderFormatDef, NULL,
    "printf-like format string for the browser's display of folders"
  },
  { "group_index_format", DT_EXPANDO|D_NOT_EMPTY, IP "%^%4C %M%N %5s  %-45.45f %d", IP &GroupIndexFormatDef, NULL,
    "(nntp) printf-like format string for the browser's display of newsgroups"
  },
  { "mailbox_folder_format", DT_EXPANDO|D_NOT_EMPTY, IP "%^%2C %<n?%6n&      > %6m %i", IP &FolderFormatDef, NULL,
    "printf-like format string for the browser's display of mailbox folders"
  },
  { "mask", DT_REGEX|D_REGEX_MATCH_CASE|D_REGEX_ALLOW_NOT|D_REGEX_NOSUB, IP "!^\\.[^.]", 0, NULL,
    "Only display files/dirs matching this regex in the browser"
  },
  { "show_only_unread", DT_BOOL, false, 0, NULL,
    "(nntp) Only show subscribed newsgroups with unread articles"
  },
  { "sort_browser", DT_SORT|D_SORT_REVERSE, SORT_ALPHA, IP SortBrowserMethods, NULL,
    "Sort method for the browser"
  },
  { "browser_sort_dirs_first", DT_BOOL, false, 0, NULL,
    "Group directories before files in the browser"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_browser - Register browser config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_browser(struct ConfigSet *cs)
{
  return cs_register_variables(cs, BrowserVars);
}
