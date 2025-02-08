/**
 * @file
 * Config used by the Email library
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page email_config Config used by the Email library
 *
 * Config used by the Email library
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"

/**
 * EmailVars - Config definitions for the Email library
 */
struct ConfigDef EmailVars[] = {
  // clang-format off
  { "auto_subscribe", DT_BOOL, false, 0, NULL,
    "Automatically check if the user is subscribed to a mailing list"
  },
  { "hidden_tags", DT_SLIST|D_SLIST_SEP_COMMA, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted", 0, NULL,
    "List of tags that shouldn't be displayed on screen (comma-separated)"
  },
  // L10N: $reply_regex default format
  //
  // This is a regular expression that matches reply subject lines.
  // By default, it only matches an initial "Re: ", which is the
  // standardized Latin prefix.
  //
  // However, many locales have other prefixes that are commonly used
  // too, such as Aw in Germany.  To add other prefixes, modify the first
  // parenthesized expression, such as:
  //    "^(re|aw)
  // you can add multiple values, for example:
  //    "^(re|aw|sv)
  //
  // Important:
  // - Use all lower case letters.
  // - Don't remove the 're' prefix from the list of choices.
  // - Please test the value you use inside Mutt.  A mistake here will break
  //   NeoMutt's threading behavior.  Note: the header cache can interfere with
  //   testing, so be sure to test with $header_cache unset.
  { "reply_regex", DT_REGEX|D_L10N_STRING, IP N_("^((re)(\\[[0-9]+\\])*:[ \t]*)*"), 0, NULL,
    "Regex to match message reply subjects like 're: '"
  },
  { "reverse_alias", DT_BOOL, false, 0, NULL,
    "Display the alias in the index, rather than the message's sender"
  },
  { "rfc2047_parameters", DT_BOOL, true, 0, NULL,
    "Decode RFC2047-encoded MIME parameters"
  },
  { "spam_separator", DT_STRING, IP ",", 0, NULL,
    "Separator for multiple spam headers"
  },

  { "reply_regexp", DT_SYNONYM, IP "reply_regex", IP "2021-03-21" },
  { NULL },
  // clang-format on
};
