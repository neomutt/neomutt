/**
 * @file
 * Credential management via macOS Keychain
 *
 * @authors
 * Copyright (C) 2022 Ramkumar Ramachandra <r@artagnon.com>
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

#ifndef MUTT_CONN_MACOS_KEYCHAIN_H
#define MUTT_CONN_MACOS_KEYCHAIN_H

struct ConnAccount;

int mutt_account_read_keychain (struct ConnAccount *account);
int mutt_account_write_keychain(struct ConnAccount *account);

#endif /* MUTT_CONN_MACOS_KEYCHAIN_H */
