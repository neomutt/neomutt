/**
 * @file
 * Config used by libpop
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
 * @page pop_config Config used by libpop
 *
 * Config used by libpop
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "private.h"
#include "lib.h"

// clang-format off
bool          C_PopAuthTryAll;          ///< Config: (pop) Try all available authentication methods
struct Slist *C_PopAuthenticators;      ///< Config: (pop) List of allowed authentication methods
short         C_PopCheckinterval;       ///< Config: (pop) Interval between checks for new mail
unsigned char C_PopDelete;              ///< Config: (pop) After downloading POP messages, delete them on the server
char *        C_PopHost;                ///< Config: (pop) Url of the POP server
bool          C_PopLast;                ///< Config: (pop) Use the 'LAST' command to fetch new mail
char *        C_PopOauthRefreshCommand; ///< Config: (pop) External command to generate OAUTH refresh token
char *        C_PopPass;                ///< Config: (pop) Password of the POP server
unsigned char C_PopReconnect;           ///< Config: (pop) Reconnect to the server is the connection is lost
char *        C_PopUser;                ///< Config: (pop) Username of the POP server
// clang-format on

/**
 * pop_auth_validator - Validate the "pop_authenticators" config variable - Implements ConfigDef::validator()
 */
static int pop_auth_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                              intptr_t value, struct Buffer *err)
{
  const struct Slist *pop_auth_methods = (const struct Slist *) value;
  if (!pop_auth_methods || (pop_auth_methods->count == 0))
    return CSR_SUCCESS;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &pop_auth_methods->head, entries)
  {
    if (pop_auth_is_valid(np->data))
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

struct ConfigDef PopVars[] = {
  // clang-format off
  { "pop_auth_try_all", DT_BOOL, &C_PopAuthTryAll, true, 0, NULL,
    "(pop) Try all available authentication methods"
  },
  { "pop_authenticators", DT_SLIST|SLIST_SEP_COLON, &C_PopAuthenticators, 0, 0, pop_auth_validator,
    "(pop) List of allowed authentication methods"
  },
  { "pop_checkinterval", DT_NUMBER|DT_NOT_NEGATIVE, &C_PopCheckinterval, 60, 0, NULL,
    "(pop) Interval between checks for new mail"
  },
  { "pop_delete", DT_QUAD, &C_PopDelete, MUTT_ASKNO, 0, NULL,
    "(pop) After downloading POP messages, delete them on the server"
  },
  { "pop_host", DT_STRING, &C_PopHost, 0, 0, NULL,
    "(pop) Url of the POP server"
  },
  { "pop_last", DT_BOOL, &C_PopLast, false, 0, NULL,
    "(pop) Use the 'LAST' command to fetch new mail"
  },
  { "pop_oauth_refresh_command", DT_STRING|DT_COMMAND|DT_SENSITIVE, &C_PopOauthRefreshCommand, 0, 0, NULL,
    "(pop) External command to generate OAUTH refresh token"
  },
  { "pop_pass", DT_STRING|DT_SENSITIVE, &C_PopPass, 0, 0, NULL,
    "(pop) Password of the POP server"
  },
  { "pop_reconnect", DT_QUAD, &C_PopReconnect, MUTT_ASKYES, 0, NULL,
    "(pop) Reconnect to the server is the connection is lost"
  },
  { "pop_user", DT_STRING|DT_SENSITIVE, &C_PopUser, 0, 0, NULL,
    "(pop) Username of the POP server"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_pop - Register pop config variables - Implements ::module_init_config_t
 */
bool config_init_pop(struct ConfigSet *cs)
{
  return cs_register_variables(cs, PopVars, 0);
}
