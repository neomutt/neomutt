/**
 * @file
 * Config used by the Email library
 *
 * @authors
 * Copyright (C) 2024-2026 Richard Russon <rich@flatcap.org>
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
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"

/**
 * multipart_validator - Validate the "show_multipart_alternative" config variable - Implements ConfigDef::validator() - @ingroup cfg_def_validator
 */
static int multipart_validator(const struct ConfigDef *cdef, intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (mutt_str_equal(str, "inline") || mutt_str_equal(str, "info"))
    return CSR_SUCCESS;

  buf_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

/**
 * EmailVars - Config definitions for the Email library
 */
struct ConfigDef EmailVars[] = {
  // clang-format off
  { "auto_subscribe", DT_BOOL, false, 0, NULL,
    "Automatically check if the user is subscribed to a mailing list"
  },
  { "honor_disposition", DT_BOOL, false, 0, NULL,
    "Don't display MIME parts inline if they have a disposition of 'attachment'"
  },
  { "hidden_tags", DT_SLIST|D_SLIST_SEP_COMMA, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted", 0, NULL,
    "List of tags that shouldn't be displayed on screen (comma-separated)"
  },
  { "implicit_auto_view", DT_BOOL, false, 0, NULL,
    "Display MIME attachments inline if a 'copiousoutput' mailcap entry exists"
  },
  { "include_encrypted", DT_BOOL, false, 0, NULL,
    "Whether to include encrypted content when replying"
  },
  { "include_only_first", DT_BOOL, false, 0, NULL,
    "Only include the first attachment when replying"
  },
  { "mailcap_path", DT_SLIST|D_SLIST_SEP_COLON, IP "~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap", 0, NULL,
    "List of mailcap files (colon-separated)"
  },
  { "mailcap_sanitize", DT_BOOL, true, 0, NULL,
    "Restrict the possible characters in mailcap expandos"
  },
  { "preferred_languages", DT_SLIST|D_SLIST_SEP_COMMA, 0, 0, NULL,
    "List of Preferred Languages for multilingual MIME (comma-separated)"
  },
  { "reflow_space_quotes", DT_BOOL, true, 0, NULL,
    "Insert spaces into reply quotes for 'format=flowed' messages"
  },
  { "reflow_text", DT_BOOL, true, 0, NULL,
    "Reformat paragraphs of 'format=flowed' text"
  },
  { "reflow_wrap", DT_NUMBER, 78, 0, NULL,
    "Maximum paragraph width for reformatting 'format=flowed' text"
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
  { "score", DT_BOOL, true, 0, NULL,
    "Use message scoring"
  },
  { "score_threshold_delete", DT_NUMBER, -1, 0, NULL,
    "Messages with a lower score will be automatically deleted"
  },
  { "score_threshold_flag", DT_NUMBER, 9999, 0, NULL,
    "Messages with a greater score will be automatically flagged"
  },
  { "score_threshold_read", DT_NUMBER, -1, 0, NULL,
    "Messages with a lower score will be automatically marked read"
  },
  { "show_multipart_alternative", DT_STRING, 0, 0, multipart_validator,
    "How to display 'multipart/alternative' MIME parts"
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

  { "implicit_autoview", DT_SYNONYM, IP "implicit_auto_view", IP "2023-01-25" },
  { "include_onlyfirst", DT_SYNONYM, IP "include_only_first", IP "2021-03-21" },
  { "reply_regexp", DT_SYNONYM, IP "reply_regex", IP "2021-03-21" },
  { NULL },
  // clang-format on
};
