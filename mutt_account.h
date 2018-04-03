/**
 * @file
 * Account object used by POP and IMAP
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

#ifndef _MUTT_ACCOUNT_H
#define _MUTT_ACCOUNT_H

struct Account;
struct Url;

/**
 * enum AccountType - account types
 */
enum AccountType
{
  MUTT_ACCT_TYPE_NONE = 0,
  MUTT_ACCT_TYPE_IMAP,
  MUTT_ACCT_TYPE_POP,
  MUTT_ACCT_TYPE_SMTP,
  MUTT_ACCT_TYPE_NNTP
};

/* account flags */
#define MUTT_ACCT_PORT  (1 << 0)
#define MUTT_ACCT_USER  (1 << 1)
#define MUTT_ACCT_LOGIN (1 << 2)
#define MUTT_ACCT_PASS  (1 << 3)
#define MUTT_ACCT_SSL   (1 << 4)

int mutt_account_match(const struct Account *a1, const struct Account *a2);
int mutt_account_fromurl(struct Account *account, struct Url *url);
void mutt_account_tourl(struct Account *account, struct Url *url);
int mutt_account_getuser(struct Account *account);
int mutt_account_getlogin(struct Account *account);
int mutt_account_getpass(struct Account *account);
void mutt_account_unsetpass(struct Account *account);

#endif /* _MUTT_ACCOUNT_H */
