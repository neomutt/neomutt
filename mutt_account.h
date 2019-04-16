/**
 * @file
 * ConnAccount object used by POP and IMAP
 *
 * @authors
 * Copyright (C) 2000-2007,2012 Brendan Cully <brendan@kublai.com>
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

/* remote host account manipulation (POP/IMAP) */

#ifndef MUTT_MUTT_ACCOUNT_H
#define MUTT_MUTT_ACCOUNT_H

#include <stdbool.h>
#include <stdint.h>

struct ConnAccount;
struct Url;

/* These Config Variables are only used in mutt_account.c */
extern char *C_ImapLogin;
extern char *C_ImapOauthRefreshCommand;
extern char *C_ImapPass;
extern char *C_NntpPass;
extern char *C_NntpUser;
extern char *C_PopOauthRefreshCommand;
extern char *C_PopPass;
extern char *C_PopUser;
extern char *C_SmtpOauthRefreshCommand;
extern char *C_SmtpPass;

/**
 * enum AccountType - account types
 */
enum AccountType
{
  MUTT_ACCT_TYPE_NONE = 0, ///< Account type is unknown
  MUTT_ACCT_TYPE_IMAP,     ///< Imap Account
  MUTT_ACCT_TYPE_POP,      ///< Pop Account
  MUTT_ACCT_TYPE_SMTP,     ///< Smtp Account
  MUTT_ACCT_TYPE_NNTP,     ///< Nntp (Usenet) Account
};

typedef uint8_t MuttAccountFlags;     ///< Flags, Which ConnAccount fields are initialised, e.g. #MUTT_ACCT_PORT
#define MUTT_ACCT_NO_FLAGS        0   ///< No flags are set
#define MUTT_ACCT_PORT      (1 << 0)  ///< Port field has been set
#define MUTT_ACCT_USER      (1 << 1)  ///< User field has been set
#define MUTT_ACCT_LOGIN     (1 << 2)  ///< Login field has been set
#define MUTT_ACCT_PASS      (1 << 3)  ///< Password field has been set
#define MUTT_ACCT_SSL       (1 << 4)  ///< Account uses SSL/TLS

bool mutt_account_match(const struct ConnAccount *a1, const struct ConnAccount *a2);
int mutt_account_fromurl(struct ConnAccount *account, const struct Url *url);
void mutt_account_tourl(struct ConnAccount *account, struct Url *url);
int mutt_account_getuser(struct ConnAccount *account);
int mutt_account_getlogin(struct ConnAccount *account);
int mutt_account_getpass(struct ConnAccount *account);
void mutt_account_unsetpass(struct ConnAccount *account);
char *mutt_account_getoauthbearer(struct ConnAccount *account);

#endif /* MUTT_MUTT_ACCOUNT_H */
