/**
 * @file
 * Config used by libnntp
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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
 * @page nntp_config Config used by libnntp
 *
 * Config used by libnntp
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "conn/lib.h"
#include "expando/lib.h"

/**
 * NntpFormatDef - Expando definitions
 *
 * Config:
 * - $inews
 * - $newsrc
 */
const struct ExpandoDefinition NntpFormatDef[] = {
  // clang-format off
  { "a", "account",  ED_NNTP, ED_NTP_ACCOUNT,  NULL },
  { "p", "port",     ED_NNTP, ED_NTP_PORT,     NULL },
  { "P", "port-if",  ED_NNTP, ED_NTP_PORT_IF,  NULL },
  { "s", "server",   ED_NNTP, ED_NTP_SERVER,   NULL },
  { "S", "schema",   ED_NNTP, ED_NTP_SCHEMA,   NULL },
  { "u", "username", ED_NNTP, ED_NTP_USERNAME, NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * NntpVars - Config definitions for the NNTP library
 */
struct ConfigDef NntpVars[] = {
  // clang-format off
  { "catchup_newsgroup", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "(nntp) Mark all articles as read when leaving a newsgroup"
  },
  { "followup_to_poster", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "(nntp) Reply to the poster if 'poster' is in the 'Followup-To' header"
  },
  { "newsgroups_charset", DT_STRING, IP "utf-8", 0, charset_validator,
    "(nntp) Character set of newsgroups' descriptions"
  },
  { "newsrc", DT_EXPANDO|D_PATH_FILE, IP "~/.newsrc", IP &NntpFormatDef, NULL,
    "(nntp) File containing list of subscribed newsgroups"
  },
  { "news_cache_dir", DT_PATH|D_PATH_DIR, IP "~/.neomutt", 0, NULL,
    "(nntp) Directory for cached news articles"
  },
  { "news_server", DT_STRING, 0, 0, NULL,
    "(nntp) Url of the news server"
  },
  { "nntp_authenticators", DT_STRING, 0, 0, NULL,
    "(nntp) Allowed authentication methods"
  },
  { "nntp_context", DT_LONG|D_INTEGER_NOT_NEGATIVE, 1000, 0, NULL,
    "(nntp) Maximum number of articles to list (0 for all articles)"
  },
  { "nntp_listgroup", DT_BOOL, true, 0, NULL,
    "(nntp) Check all articles when opening a newsgroup"
  },
  { "nntp_load_description", DT_BOOL, true, 0, NULL,
    "(nntp) Load descriptions for newsgroups when adding to the list"
  },
  { "nntp_pass", DT_STRING|D_SENSITIVE, 0, 0, NULL,
    "(nntp) Password for the news server"
  },
  { "nntp_poll", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 60, 0, NULL,
    "(nntp) Interval between checks for new posts"
  },
  { "nntp_user", DT_STRING|D_SENSITIVE, 0, 0, NULL,
    "(nntp) Username for the news server"
  },
  { "post_moderated", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "(nntp) Allow posting to moderated newsgroups"
  },
  { "save_unsubscribed", DT_BOOL, false, 0, NULL,
    "(nntp) Save a list of unsubscribed newsgroups to the 'newsrc'"
  },
  { "show_new_news", DT_BOOL, true, 0, NULL,
    "(nntp) Check for new newsgroups when entering the browser"
  },
  { "x_comment_to", DT_BOOL, false, 0, NULL,
    "(nntp) Add 'X-Comment-To' header that contains article author"
  },
  { NULL },
  // clang-format on
};
