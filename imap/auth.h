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

#ifndef _MUTT_IMAP_AUTH_H
#define _MUTT_IMAP_AUTH_H

struct ImapData;

/**
 * enum ImapAuthRes - Results of IMAP Authentication
 */
enum ImapAuthRes
{
  IMAP_AUTH_SUCCESS = 0,
  IMAP_AUTH_FAILURE,
  IMAP_AUTH_UNAVAIL
};

/**
 * struct ImapAuth - IMAP authentication multiplexor
 */
struct ImapAuth
{
  /* do authentication, using named method or any available if method is NULL */
  enum ImapAuthRes (*authenticate)(struct ImapData *idata, const char *method);
  /* name of authentication method supported, NULL means variable. If this
   * is not null, authenticate may ignore the second parameter. */
  const char *method;
};

/* external authenticator prototypes */
enum ImapAuthRes imap_auth_plain(struct ImapData *idata, const char *method);
#ifndef USE_SASL
enum ImapAuthRes imap_auth_anon(struct ImapData *idata, const char *method);
enum ImapAuthRes imap_auth_cram_md5(struct ImapData *idata, const char *method);
#endif
enum ImapAuthRes imap_auth_login(struct ImapData *idata, const char *method);
#ifdef USE_GSS
enum ImapAuthRes imap_auth_gss(struct ImapData *idata, const char *method);
#endif
#ifdef USE_SASL
enum ImapAuthRes imap_auth_sasl(struct ImapData *idata, const char *method);
#endif

#endif /* _MUTT_IMAP_AUTH_H */
