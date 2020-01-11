/**
 * @file
 * ConnAccount object used by POP and IMAP
 *
 * @authors
 * Copyright (C) 2000-2007 Brendan Cully <brendan@kublai.com>
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
 * @page mutt_account ConnAccount object used by POP and IMAP
 *
 * ConnAccount object used by POP and IMAP
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "mutt_account.h"
#include "globals.h"
#include "options.h"

/* These Config Variables are only used in mutt_account.c */
char *C_ImapLogin; ///< Config: (imap) Login name for the IMAP server (defaults to #C_ImapUser)
char *C_ImapOauthRefreshCommand; ///< Config: (imap) External command to generate OAUTH refresh token
char *C_ImapPass; ///< Config: (imap) Password for the IMAP server
char *C_NntpPass; ///< Config: (nntp) Password for the news server
char *C_NntpUser; ///< Config: (nntp) Username for the news server
char *C_PopOauthRefreshCommand; ///< Config: (pop) External command to generate OAUTH refresh token
char *C_PopPass; ///< Config: (pop) Password of the POP server
char *C_PopUser; ///< Config: (pop) Username of the POP server
char *C_SmtpOauthRefreshCommand; ///< Config: (smtp) External command to generate OAUTH refresh token
char *C_SmtpPass; ///< Config: (smtp) Password for the SMTP server
char *C_SmtpUser; ///< Config: (smtp) Username for the SMTP server

/**
 * mutt_account_fromurl - Fill ConnAccount with information from url
 * @param cac ConnAccount to fill
 * @param url Url to parse
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_account_fromurl(struct ConnAccount *cac, const struct Url *url)
{
  /* must be present */
  if (url->host)
    mutt_str_strfcpy(cac->host, url->host, sizeof(cac->host));
  else
    return -1;

  if (url->user)
  {
    mutt_str_strfcpy(cac->user, url->user, sizeof(cac->user));
    cac->flags |= MUTT_ACCT_USER;
  }
  if (url->pass)
  {
    mutt_str_strfcpy(cac->pass, url->pass, sizeof(cac->pass));
    cac->flags |= MUTT_ACCT_PASS;
  }
  if (url->port)
  {
    cac->port = url->port;
    cac->flags |= MUTT_ACCT_PORT;
  }

  return 0;
}

/**
 * mutt_account_tourl - Fill URL with info from account
 * @param cac Source ConnAccount
 * @param url Url to fill
 *
 * The URL information is a set of pointers into cac - don't free or edit cac
 * until you've finished with url (make a copy of cac if you need it for a
 * while).
 */
void mutt_account_tourl(struct ConnAccount *cac, struct Url *url)
{
  url->scheme = U_UNKNOWN;
  url->user = NULL;
  url->pass = NULL;
  url->port = 0;
  url->path = NULL;

#ifdef USE_IMAP
  if (cac->type == MUTT_ACCT_TYPE_IMAP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_IMAPS;
    else
      url->scheme = U_IMAP;
  }
#endif

#ifdef USE_POP
  if (cac->type == MUTT_ACCT_TYPE_POP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_POPS;
    else
      url->scheme = U_POP;
  }
#endif

#ifdef USE_SMTP
  if (cac->type == MUTT_ACCT_TYPE_SMTP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_SMTPS;
    else
      url->scheme = U_SMTP;
  }
#endif

#ifdef USE_NNTP
  if (cac->type == MUTT_ACCT_TYPE_NNTP)
  {
    if (cac->flags & MUTT_ACCT_SSL)
      url->scheme = U_NNTPS;
    else
      url->scheme = U_NNTP;
  }
#endif

  url->host = cac->host;
  if (cac->flags & MUTT_ACCT_PORT)
    url->port = cac->port;
  if (cac->flags & MUTT_ACCT_USER)
    url->user = cac->user;
  if (cac->flags & MUTT_ACCT_PASS)
    url->pass = cac->pass;
}

/**
 * mutt_account_getuser - Retrieve username into ConnAccount, if necessary
 * @param cac ConnAccount to fill
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_account_getuser(struct ConnAccount *cac)
{
  if (cac->flags & MUTT_ACCT_USER)
    return 0;
  if (!cac->get_field)
    return -1;

  const char *user = cac->get_field(MUTT_CA_USER);
  if (user)
    mutt_str_strfcpy(cac->user, user, sizeof(cac->user));
  else if (OptNoCurses)
    return -1;
  else
  {
    /* prompt (defaults to unix username), copy into cac->user */
    char prompt[256];
    /* L10N: Example: Username at myhost.com */
    snprintf(prompt, sizeof(prompt), _("Username at %s: "), cac->host);
    mutt_str_strfcpy(cac->user, Username, sizeof(cac->user));
    if (mutt_get_field_unbuffered(prompt, cac->user, sizeof(cac->user), MUTT_COMP_NO_FLAGS))
      return -1;
  }

  cac->flags |= MUTT_ACCT_USER;
  return 0;
}

/**
 * mutt_account_getlogin - Retrieve login info into ConnAccount, if necessary
 * @param cac ConnAccount to fill
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_account_getlogin(struct ConnAccount *cac)
{
  if (cac->flags & MUTT_ACCT_LOGIN)
    return 0;
  if (!cac->get_field)
    return -1;

  const char *login = cac->get_field(MUTT_CA_LOGIN);
  if (!login)
  {
    mutt_debug(LL_DEBUG1, "Couldn't get user info\n");
    return -1;
  }

  mutt_str_strfcpy(cac->login, login, sizeof(cac->login));
  cac->flags |= MUTT_ACCT_LOGIN;
  return 0;
}

/**
 * mutt_account_getpass - Fetch password into ConnAccount, if necessary
 * @param cac ConnAccount to fill
 * @retval  0 Success
 * @retval -1 Failure
 */
int mutt_account_getpass(struct ConnAccount *cac)
{
  if (cac->flags & MUTT_ACCT_PASS)
    return 0;
  if (!cac->get_field)
    return -1;

  const char *pass = cac->get_field(MUTT_CA_PASS);
  if (pass)
    mutt_str_strfcpy(cac->pass, pass, sizeof(cac->pass));
  else if (OptNoCurses)
    return -1;
  else
  {
    char prompt[256];
    snprintf(prompt, sizeof(prompt), _("Password for %s@%s: "),
             (cac->flags & MUTT_ACCT_LOGIN) ? cac->login : cac->user, cac->host);
    cac->pass[0] = '\0';
    if (mutt_get_password(prompt, cac->pass, sizeof(cac->pass)))
      return -1;
  }

  cac->flags |= MUTT_ACCT_PASS;
  return 0;
}

/**
 * mutt_account_unsetpass - Unset ConnAccount's password
 * @param cac ConnAccount to modify
 */
void mutt_account_unsetpass(struct ConnAccount *cac)
{
  cac->flags &= ~MUTT_ACCT_PASS;
}

/**
 * mutt_account_getoauthbearer - Get an OAUTHBEARER token
 * @param cac Account to use
 * @retval ptr  OAuth token
 * @retval NULL Error
 *
 * Run an external command to generate the oauth refresh token for an account,
 * then create and encode the OAUTHBEARER token based on RFC7628.
 *
 * @note Caller should free the token
 */
char *mutt_account_getoauthbearer(struct ConnAccount *cac)
{
  if (!cac || !cac->get_field)
    return NULL;

  /* The oauthbearer token includes the login */
  if (mutt_account_getlogin(cac))
    return NULL;

  const char *cmd = cac->get_field(MUTT_CA_OAUTH_CMD);
  if (!cmd)
  {
    /* L10N: You will see this error message if (1) you have "oauthbearer" in
       one of your $*_authenticators and (2) you do not have the corresponding
       $*_oauth_refresh_command defined. So the message does not mean "None of
       your $*_oauth_refresh_command's are defined." */
    mutt_error(_("No OAUTH refresh command defined"));
    return NULL;
  }

  FILE *fp = NULL;
  pid_t pid = filter_create(cmd, NULL, &fp, NULL);
  if (pid < 0)
  {
    mutt_perror(_("Unable to run refresh command"));
    return NULL;
  }

  size_t token_size = 0;
  char *token = mutt_file_read_line(NULL, &token_size, fp, NULL, 0);
  mutt_file_fclose(&fp);
  filter_wait(pid);

  if (!token || (*token == '\0'))
  {
    mutt_error(_("Command returned empty string"));
    FREE(&token);
    return NULL;
  }

  /* Determine the length of the keyed message digest, add 50 for overhead. */
  size_t oalen = strlen(cac->login) + strlen(cac->host) + strlen(token) + 50;
  char *oauthbearer = mutt_mem_malloc(oalen);

  snprintf(oauthbearer, oalen, "n,a=%s,\001host=%s\001port=%d\001auth=Bearer %s\001\001",
           cac->login, cac->host, cac->port, token);

  FREE(&token);

  size_t encoded_len = strlen(oauthbearer) * 4 / 3 + 10;
  char *encoded_token = mutt_mem_malloc(encoded_len);
  mutt_b64_encode(oauthbearer, strlen(oauthbearer), encoded_token, encoded_len);
  FREE(&oauthbearer);
  return encoded_token;
}
