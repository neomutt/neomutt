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

struct ConnAccount;
struct Url;

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

int   mutt_account_fromurl(struct ConnAccount *account, const struct Url *url);
void  mutt_account_tourl  (struct ConnAccount *account, struct Url *url);

#endif /* MUTT_MUTT_ACCOUNT_H */
