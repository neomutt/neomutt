/**
 * @file
 * Config used by libnntp
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
 * @page nntp_config Config used by libnntp
 *
 * Config used by libnntp
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "private.h"
#include "init.h"

// clang-format off
unsigned char C_CatchupNewsgroup;    ///< Config: (nntp) Mark all articles as read when leaving a newsgroup
unsigned char C_FollowupToPoster;    ///< Config: (nntp) Reply to the poster if 'poster' is in the 'Followup-To' header
char *        C_GroupIndexFormat;    ///< Config: (nntp) printf-like format string for the browser's display of newsgroups
char *        C_NewsCacheDir;        ///< Config: (nntp) Directory for cached news articles
char *        C_NewsServer;          ///< Config: (nntp) Url of the news server
char *        C_NewsgroupsCharset;   ///< Config: (nntp) Character set of newsgroups' descriptions
char *        C_Newsrc;              ///< Config: (nntp) File containing list of subscribed newsgroups
char *        C_NntpAuthenticators;  ///< Config: (nntp) Allowed authentication methods
short         C_NntpContext;         ///< Config: (nntp) Maximum number of articles to list (0 for all articles)
bool          C_NntpListgroup;       ///< Config: (nntp) Check all articles when opening a newsgroup
bool          C_NntpLoadDescription; ///< Config: (nntp) Load descriptions for newsgroups when adding to the list
char *        C_NntpPass;            ///< Config: (nntp) Password for the news server
short         C_NntpPoll;            ///< Config: (nntp) Interval between checks for new posts
char *        C_NntpUser;            ///< Config: (nntp) Username for the news server
unsigned char C_PostModerated;       ///< Config: (nntp) Allow posting to moderated newsgroups
bool          C_SaveUnsubscribed;    ///< Config: (nntp) Save a list of unsubscribed newsgroups to the 'newsrc'
bool          C_ShowNewNews;         ///< Config: (nntp) Check for new newsgroups when entering the browser
bool          C_ShowOnlyUnread;      ///< Config: (nntp) Only show subscribed newsgroups with unread articles
bool          C_XCommentTo;          ///< Config: (nntp) Add 'X-Comment-To' header that contains article author
// clang-format on

struct ConfigDef NntpVars[] = {
  // clang-format off
  { "catchup_newsgroup", DT_QUAD, &C_CatchupNewsgroup, MUTT_ASKYES, 0, NULL,
    "(nntp) Mark all articles as read when leaving a newsgroup"
  },
  { "followup_to_poster", DT_QUAD, &C_FollowupToPoster, MUTT_ASKYES, 0, NULL,
    "(nntp) Reply to the poster if 'poster' is in the 'Followup-To' header"
  },
  { "group_index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, &C_GroupIndexFormat, IP "%4C %M%N %5s  %-45.45f %d", 0, NULL,
    "(nntp) printf-like format string for the browser's display of newsgroups"
  },
  { "newsgroups_charset", DT_STRING, &C_NewsgroupsCharset, IP "utf-8", 0, charset_validator,
    "(nntp) Character set of newsgroups' descriptions"
  },
  { "newsrc", DT_PATH|DT_PATH_FILE, &C_Newsrc, IP "~/.newsrc", 0, NULL,
    "(nntp) File containing list of subscribed newsgroups"
  },
  { "news_cache_dir", DT_PATH|DT_PATH_DIR, &C_NewsCacheDir, IP "~/.neomutt", 0, NULL,
    "(nntp) Directory for cached news articles"
  },
  { "news_server", DT_STRING, &C_NewsServer, 0, 0, NULL,
    "(nntp) Url of the news server"
  },
  { "nntp_authenticators", DT_STRING, &C_NntpAuthenticators, 0, 0, NULL,
    "(nntp) Allowed authentication methods"
  },
  { "nntp_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_NntpContext, 1000, 0, NULL,
    "(nntp) Maximum number of articles to list (0 for all articles)"
  },
  { "nntp_listgroup", DT_BOOL, &C_NntpListgroup, true, 0, NULL,
    "(nntp) Check all articles when opening a newsgroup"
  },
  { "nntp_load_description", DT_BOOL, &C_NntpLoadDescription, true, 0, NULL,
    "(nntp) Load descriptions for newsgroups when adding to the list"
  },
  { "nntp_pass", DT_STRING|DT_SENSITIVE, &C_NntpPass, 0, 0, NULL,
    "(nntp) Password for the news server"
  },
  { "nntp_poll", DT_NUMBER|DT_NOT_NEGATIVE, &C_NntpPoll, 60, 0, NULL,
    "(nntp) Interval between checks for new posts"
  },
  { "nntp_user", DT_STRING|DT_SENSITIVE, &C_NntpUser, 0, 0, NULL,
    "(nntp) Username for the news server"
  },
  { "post_moderated", DT_QUAD, &C_PostModerated, MUTT_ASKYES, 0, NULL,
    "(nntp) Allow posting to moderated newsgroups"
  },
  { "save_unsubscribed", DT_BOOL, &C_SaveUnsubscribed, false, 0, NULL,
    "(nntp) Save a list of unsubscribed newsgroups to the 'newsrc'"
  },
  { "show_new_news", DT_BOOL, &C_ShowNewNews, true, 0, NULL,
    "(nntp) Check for new newsgroups when entering the browser"
  },
  { "show_only_unread", DT_BOOL, &C_ShowOnlyUnread, false, 0, NULL,
    "(nntp) Only show subscribed newsgroups with unread articles"
  },
  { "x_comment_to", DT_BOOL, &C_XCommentTo, false, 0, NULL,
    "(nntp) Add 'X-Comment-To' header that contains article author"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_nntp - Register nntp config variables
 */
bool config_init_nntp(struct ConfigSet *cs)
{
  return cs_register_variables(cs, NntpVars, 0);
}
