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

/**
 * @page conn_macos_keychain Read/write macOS Keychain credentials
 *
 * Called by mutt_account_getuser() and mutt_user_getpass() as an alternative to
 * storing/retrieving credentials in the configutation file, or entering them in
 * the prompt repeatedly. A dialog will automatically pop up requesting permissions
 * on macOS the very first time.
 *
 * Disabled using the configure option '--disable-macoskeychain'.
 */

#include <Security/Security.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "connaccount.h"
#include "mutt_account.h"

/**
 * struct Credential - A bunch of pointers that act as a lens into ConnAccount,
 *                     filtering out only the information relevant to keychain.
 */
struct Credential
{
  SecProtocolType protocol; ///< Connection type, e.g. HTTP
  UInt16 port;              ///< Port to connect to
  const char *host;         ///< Server to login to
  const char *user;         ///< Username
  const char *pass;         ///< Password
};

#define KEYCHAIN_ITEM(x) mutt_str_len(x), x

/**
 * find_username_in_item - Find the username in a given SecKeychainItem
 * @param item A reference to the security keychain item
 * @param user A pre-allocated buffer into which to write the username
 * @param buflen The length of the pre-allocated user buffer
 * @retval 0 on success, a specialized error-code on failure
 */
static int find_username_in_item(SecKeychainItemRef item, char *user, unsigned buflen)
{
  SecKeychainAttributeList list = { 0 };
  SecKeychainAttribute attr = { 0 };

  list.count = 1;
  list.attr = &attr;
  attr.tag = kSecAccountItemAttr;

  int ret = SecKeychainItemCopyContent(item, NULL, &list, NULL, NULL);
  if (ret != 0)
    return ret;

  assert(attr.length + 1 <= buflen &&
         "Did you modify the keychain item by hand?");
  mutt_str_copy(user, attr.data, attr.length + 1);

  SecKeychainItemFreeContent(&list, NULL);
  return ret;
}

/**
 * find_internet_password - Find a username/password pair in keychain
 * @param user A pre-allocated buffer into which to write the username
 * @param user_len The length of the pre-allocated user buffer
 * @param pass A pre-allocated buffer into which to write the password
 * @param pass_len The length of the pre-allocated pass buffer
 * @param cred A Credential object with partial data to look up in keychain
 * @retval 0 on success, a specialized error-code on failure
 */
static int find_internet_password(char *user, unsigned user_len, char *pass,
                                  unsigned pass_len, struct Credential cred)
{
  void *buf = NULL;
  UInt32 len = 0;
  SecKeychainItemRef item = NULL;
  int ret = 0;

  ret = SecKeychainFindInternetPassword(NULL, /* default keychain */
                                        KEYCHAIN_ITEM(cred.host), 0, NULL, /* account domain */
                                        KEYCHAIN_ITEM(cred.user), 0, NULL, cred.port,
                                        cred.protocol, kSecAuthenticationTypeDefault,
                                        &len, &buf, &item);
  if (ret != 0)
    return ret;

  // guard against buffer overflow
  assert(len + 1 <= pass_len && "Did you modify the keychain item by hand?");

  // Keychain does not send the password with null-terminator
  mutt_str_copy(pass, buf, len + 1);
  if (!cred.user)
    ret = find_username_in_item(item, user, user_len);

  SecKeychainItemFreeContent(NULL, buf);
  return ret;
}

/**
 * add_internet_password - Add a credential to the keychain
 * @param cred A credential object to insert
 * @retval 0 on success, a specialized error-code on failure
 */
static int add_internet_password(struct Credential cred)
{
  int ret = 0;
  /* Only store complete credentials */
  if (!cred.protocol || !cred.host || !cred.user || !cred.pass)
    return -1;
  ret = SecKeychainAddInternetPassword(NULL, /* default keychain */
                                       KEYCHAIN_ITEM(cred.host), 0, NULL, /* account domain */
                                       KEYCHAIN_ITEM(cred.user), 0, NULL, cred.port,
                                       cred.protocol, kSecAuthenticationTypeDefault,
                                       KEYCHAIN_ITEM(cred.pass), NULL);
  if (ret == 0)
    mutt_message(
        _("Credentials stored in Keychain. It will be used in future logins."));
  return ret;
}

/**
 * conn_account_to_cred - Copy relevant fields from a ConnAccount struct to a
 *                       Credential struct
 * @param account A connection account, as defined in connaccount.h
 */
static struct Credential conn_account_to_cred(struct ConnAccount *account)
{
  struct Credential cred = { 0 };
  switch (account->type)
  {
    case MUTT_ACCT_TYPE_NONE:
      assert(false && "Unreachable");
    case MUTT_ACCT_TYPE_IMAP:
      cred.protocol = account->flags & MUTT_ACCT_SSL ? kSecProtocolTypeIMAPS :
                                                       kSecProtocolTypeIMAP;
      break;
    case MUTT_ACCT_TYPE_POP:
      cred.protocol = account->flags & MUTT_ACCT_SSL ? kSecProtocolTypePOP3S :
                                                       kSecProtocolTypePOP3;
      break;
    case MUTT_ACCT_TYPE_SMTP:
      cred.protocol = kSecProtocolTypeSMTP;
      break;
    case MUTT_ACCT_TYPE_NNTP:
      cred.protocol = account->flags & MUTT_ACCT_SSL ? kSecProtocolTypeNNTPS :
                                                       kSecProtocolTypeNNTP;
      break;
  }
  cred.user = account->user;
  cred.pass = account->pass;
  cred.port = account->port;
  cred.host = account->host;
  return cred;
}

/**
 * mutt_account_write_keychain - Write the relevant data from a pre-filled
 *                               connection account to keychain
 * @param account A connection account, as defined in connaccount.h
 * @retval 0 on success, -1 on failure
 */
int mutt_account_write_keychain(struct ConnAccount *account)
{
  if (!account || (account->type == MUTT_ACCT_TYPE_NONE))
    return -1;
  struct Credential cred = conn_account_to_cred(account);

  // Find more error codes in SecBase.h, and write appropriate errors
  int ret = add_internet_password(cred);
  switch (ret)
  {
    case errSecSuccess:
      return 0;
    case errSecDuplicateItem:
      mutt_error(_("Duplicate item in keychain"));
    default:
      mutt_error(_("Error code from keychain add: %d"), ret);
  }
  return -1;
}

/**
 * mutt_account_read_keychain - Read user/pass into a paritially-filled
 *                               connection account from keychain
 * @param account A connection account, as defined in connaccount.h
 * @retval 0 on success, -1 on failure
 */
int mutt_account_read_keychain(struct ConnAccount *account)
{
  if (!account || (account->type == MUTT_ACCT_TYPE_NONE))
    return -1;
  struct Credential cred = conn_account_to_cred(account);

  int ret = find_internet_password(account->user, sizeof(account->user),
                                   account->pass, sizeof(account->pass), cred);
  switch (ret)
  {
    case errSecSuccess:
      return 0;
    default:
      mutt_message(_("Missing credentials for %s in keychain"), account->host);
  }
  return -1;
}
