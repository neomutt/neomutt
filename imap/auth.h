/**
 * @file
 * IMAP authenticator multiplexor
 *
 * @authors
 * Copyright (C) 2000-2001 Brendan Cully <brendan@kublai.com>
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

/* common defs for authenticators. A good place to set up a generic callback
 * system */

#ifndef MUTT_IMAP_AUTH_H
#define MUTT_IMAP_AUTH_H

#include "config.h"

struct ImapAccountData;

/**
 * enum ImapAuthRes - Results of IMAP Authentication
 */
enum ImapAuthRes
{
  IMAP_AUTH_SUCCESS = 0, ///< Authentication successful
  IMAP_AUTH_FAILURE,     ///< Authentication failed
  IMAP_AUTH_UNAVAIL,     ///< Authentication method not permitted
};

bool imap_auth_is_valid(const char *authenticator);

/* external authenticator prototypes */
enum ImapAuthRes imap_auth_plain(struct ImapAccountData *adata, const char *method);
#ifndef USE_SASL
enum ImapAuthRes imap_auth_anon(struct ImapAccountData *adata, const char *method);
enum ImapAuthRes imap_auth_cram_md5(struct ImapAccountData *adata, const char *method);
#endif
enum ImapAuthRes imap_auth_login(struct ImapAccountData *adata, const char *method);
#ifdef USE_GSS
enum ImapAuthRes imap_auth_gss(struct ImapAccountData *adata, const char *method);
#endif
#ifdef USE_SASL
enum ImapAuthRes imap_auth_sasl(struct ImapAccountData *adata, const char *method);
#endif
enum ImapAuthRes imap_auth_oauth(struct ImapAccountData *adata, const char *method);

#endif /* MUTT_IMAP_AUTH_H */
