/**
 * @file
 * Config used by libimap
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
 * @page imap_config Config used by libimap
 *
 * Config used by libimap
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "conn/lib.h"
#include "lib.h"
#include "auth.h"
#include "init.h"

// clang-format off
struct Slist *C_ImapAuthenticators;      ///< Config: (imap) List of allowed IMAP authentication methods
bool          C_ImapCheckSubscribed;     ///< Config: (imap) When opening a mailbox, ask the server for a list of subscribed folders
bool          C_ImapCondstore;           ///< Config: (imap) Enable the CONDSTORE extension
#ifdef USE_ZLIB
bool          C_ImapDeflate;             ///< Config: (imap) Compress network traffic
#endif
char *        C_ImapDelimChars;          ///< Config: (imap) Characters that denote separators in IMAP folders
long          C_ImapFetchChunkSize;      ///< Config: (imap) Download headers in blocks of this size
char *        C_ImapHeaders;             ///< Config: (imap) Additional email headers to download when getting index
bool          C_ImapIdle;                ///< Config: (imap) Use the IMAP IDLE extension to check for new mail
short         C_ImapKeepalive;           ///< Config: (imap) Time to wait before polling an open IMAP connection
bool          C_ImapListSubscribed;      ///< Config: (imap) When browsing a mailbox, only display subscribed folders
char *        C_ImapLogin;               ///< Config: (imap) Login name for the IMAP server (defaults to #C_ImapUser)
char *        C_ImapOauthRefreshCommand; ///< Config: (imap) External command to generate OAUTH refresh token
char *        C_ImapPass;                ///< Config: (imap) Password for the IMAP server
bool          C_ImapPassive;             ///< Config: (imap) Reuse an existing IMAP connection to check for new mail
bool          C_ImapPeek;                ///< Config: (imap) Don't mark messages as read when fetching them from the server
short         C_ImapPipelineDepth;       ///< Config: (imap) Number of IMAP commands that may be queued up
short         C_ImapPollTimeout;         ///< Config: (imap) Maximum time to wait for a server response
bool          C_ImapQresync;             ///< Config: (imap) Enable the QRESYNC extension
bool          C_ImapRfc5161;             ///< Config: (imap) Use the IMAP ENABLE extension to select capabilities
bool          C_ImapServernoise;         ///< Config: (imap) Display server warnings as error messages
char *        C_ImapUser;                ///< Config: (imap) Username for the IMAP server
// clang-format on

/**
 * imap_auth_validator - Validate the "imap_authenticators" config variable - Implements ConfigDef::validator()
 */
static int imap_auth_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                               intptr_t value, struct Buffer *err)
{
  const struct Slist *imap_auth_methods = (const struct Slist *) value;
  if (!imap_auth_methods || (imap_auth_methods->count == 0))
    return CSR_SUCCESS;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &imap_auth_methods->head, entries)
  {
    if (imap_auth_is_valid(np->data))
      continue;
#ifdef USE_SASL
    if (sasl_auth_validator(np->data))
      continue;
#endif
    mutt_buffer_printf(err, _("Option %s: %s is not a valid authenticator"),
                       cdef->name, np->data);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}

struct ConfigDef ImapVars[] = {
  // clang-format off
  { "imap_check_subscribed", DT_BOOL, &C_ImapCheckSubscribed, false, 0, NULL,
    "(imap) When opening a mailbox, ask the server for a list of subscribed folders"
  },
  { "imap_condstore", DT_BOOL, &C_ImapCondstore, false, 0, NULL,
    "(imap) Enable the CONDSTORE extension"
  },
#ifdef USE_ZLIB
  { "imap_deflate", DT_BOOL, &C_ImapDeflate, true, 0, NULL,
    "(imap) Compress network traffic"
  },
#endif
  { "imap_authenticators", DT_SLIST|SLIST_SEP_COLON, &C_ImapAuthenticators, 0, 0, imap_auth_validator,
    "(imap) List of allowed IMAP authentication methods"
  },
  { "imap_delim_chars", DT_STRING, &C_ImapDelimChars, IP "/.", 0, NULL,
    "(imap) Characters that denote separators in IMAP folders"
  },
  { "imap_fetch_chunk_size", DT_LONG|DT_NOT_NEGATIVE, &C_ImapFetchChunkSize, 0, 0, NULL,
    "(imap) Download headers in blocks of this size"
  },
  { "imap_headers", DT_STRING|R_INDEX, &C_ImapHeaders, 0, 0, NULL,
    "(imap) Additional email headers to download when getting index"
  },
  { "imap_idle", DT_BOOL, &C_ImapIdle, false, 0, NULL,
    "(imap) Use the IMAP IDLE extension to check for new mail"
  },
  { "imap_login", DT_STRING|DT_SENSITIVE, &C_ImapLogin, 0, 0, NULL,
    "(imap) Login name for the IMAP server (defaults to #C_ImapUser)"
  },
  { "imap_oauth_refresh_command", DT_STRING|DT_COMMAND|DT_SENSITIVE, &C_ImapOauthRefreshCommand, 0, 0, NULL,
    "(imap) External command to generate OAUTH refresh token"
  },
  { "imap_pass", DT_STRING|DT_SENSITIVE, &C_ImapPass, 0, 0, NULL,
    "(imap) Password for the IMAP server"
  },
  { "imap_pipeline_depth", DT_NUMBER|DT_NOT_NEGATIVE, &C_ImapPipelineDepth, 15, 0, NULL,
    "(imap) Number of IMAP commands that may be queued up"
  },
  { "imap_rfc5161", DT_BOOL, &C_ImapRfc5161, true, 0, NULL,
    "(imap) Use the IMAP ENABLE extension to select capabilities"
  },
  { "imap_servernoise", DT_BOOL, &C_ImapServernoise, true, 0, NULL,
    "(imap) Display server warnings as error messages"
  },
  { "imap_keepalive", DT_NUMBER|DT_NOT_NEGATIVE, &C_ImapKeepalive, 300, 0, NULL,
    "(imap) Time to wait before polling an open IMAP connection"
  },
  { "imap_list_subscribed", DT_BOOL, &C_ImapListSubscribed, false, 0, NULL,
    "(imap) When browsing a mailbox, only display subscribed folders"
  },
  { "imap_passive", DT_BOOL, &C_ImapPassive, true, 0, NULL,
    "(imap) Reuse an existing IMAP connection to check for new mail"
  },
  { "imap_peek", DT_BOOL, &C_ImapPeek, true, 0, NULL,
    "(imap) Don't mark messages as read when fetching them from the server"
  },
  { "imap_poll_timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_ImapPollTimeout, 15, 0, NULL,
    "(imap) Maximum time to wait for a server response"
  },
  { "imap_qresync", DT_BOOL, &C_ImapQresync, false, 0, NULL,
    "(imap) Enable the QRESYNC extension"
  },
  { "imap_user", DT_STRING|DT_SENSITIVE, &C_ImapUser, 0, 0, NULL,
    "(imap) Username for the IMAP server"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_imap - Register imap config variables - Implements ::module_init_config_t
 */
bool config_init_imap(struct ConfigSet *cs)
{
  return cs_register_variables(cs, ImapVars, 0);
}
