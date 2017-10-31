/**
 * @file
 * Account object
 *
 * @authors
 * Copyright (C) 2000-2005,2008 Brendan Cully <brendan@kublai.com>
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

#ifndef _CONN_ACCOUNT_H
#define _CONN_ACCOUNT_H

/**
 * struct Account - Login details for a remote server
 */
struct Account
{
  char user[64];
  char login[64];
  char pass[256];
  char host[128];
  unsigned short port;
  unsigned char type;
  unsigned char flags;
};

#endif /* _CONN_ACCOUNT_H */
