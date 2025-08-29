/**
 * @file
 * Connection Credentials
 *
 * @authors
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Ryan Kavanagh <rak@rak.ac>
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
 * @page conn_account Connection Credentials
 *
 * Connection Credentials
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "connaccount.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "accountcmd.h"
#include "globals.h"

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

  const char *user = cac->get_field(MUTT_CA_USER, cac->gf_data);
  if (user)
  {
    mutt_str_copy(cac->user, user, sizeof(cac->user));
  }
  else if (mutt_account_call_external_cmd(cac) != MUTT_ACCT_NO_FLAGS)
  {
    /* The external command might interact with the screen */
    if (!OptNoCurses)
      mutt_need_hard_redraw();
    return 0;
  }
  else if (OptNoCurses)
  {
    return -1;
  }
  else
  {
    /* prompt (defaults to unix username), copy into cac->user */
    char prompt[256] = { 0 };
    /* L10N: Example: Username at myhost.com */
    snprintf(prompt, sizeof(prompt), _("Username at %s: "), cac->host);
    mutt_str_copy(cac->user, Username, sizeof(cac->user));

    struct Buffer *buf = buf_pool_get();
    const int rc = mw_get_field(prompt, buf, MUTT_COMP_UNBUFFERED, HC_OTHER, NULL, NULL);
    mutt_str_copy(cac->user, buf_string(buf), sizeof(cac->user));
    buf_pool_release(&buf);
    if (rc != 0)
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

  const char *login = cac->get_field(MUTT_CA_LOGIN, cac->gf_data);
  if (!login && (mutt_account_getuser(cac) == 0))
  {
    login = cac->user;
  }

  if (!login)
  {
    mutt_debug(LL_DEBUG1, "Couldn't get user info\n");
    return -1;
  }

  mutt_str_copy(cac->login, login, sizeof(cac->login));
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

  const char *pass = cac->get_field(MUTT_CA_PASS, cac->gf_data);
  if (pass)
  {
    mutt_str_copy(cac->pass, pass, sizeof(cac->pass));
  }
  else if (mutt_account_call_external_cmd(cac) != MUTT_ACCT_NO_FLAGS)
  {
    /* The external command might interact with the screen */
    if (!OptNoCurses)
      mutt_need_hard_redraw();
    return 0;
  }
  else if (OptNoCurses)
  {
    return -1;
  }
  else
  {
    char prompt[512] = { 0 };
    snprintf(prompt, sizeof(prompt), _("Password for %s@%s: "),
             (cac->flags & MUTT_ACCT_LOGIN) ? cac->login : cac->user, cac->host);
    cac->pass[0] = '\0';

    struct Buffer *buf = buf_pool_get();
    const int rc = mw_get_field(prompt, buf, MUTT_COMP_PASS | MUTT_COMP_UNBUFFERED,
                                HC_OTHER, NULL, NULL);
    mutt_str_copy(cac->pass, buf_string(buf), sizeof(cac->pass));
    buf_pool_release(&buf);
    if (rc != 0)
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
  memset(cac->pass, 0, sizeof(cac->pass));
}

/**
 * mutt_account_getoauthbearer - Get an OAUTHBEARER/XOAUTH2 token
 * @param cac Account to use
 * @param xoauth2 Generate a deprecated XOAUTH2 token
 * @retval ptr  OAuth token
 * @retval NULL Error
 *
 * Run an external command to generate the oauth refresh token for an account,
 * then create and encode the OAUTHBEARER token based on RFC7628.
 *
 * @note Caller should free the token
 */
char *mutt_account_getoauthbearer(struct ConnAccount *cac, bool xoauth2)
{
  if (!cac || !cac->get_field)
    return NULL;

  /* The oauthbearer token includes the login */
  if (mutt_account_getlogin(cac))
    return NULL;

  const char *cmd = cac->get_field(MUTT_CA_OAUTH_CMD, cac->gf_data);
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
  pid_t pid = filter_create(cmd, NULL, &fp, NULL, EnvList);
  if (pid < 0)
  {
    mutt_perror(_("Unable to run refresh command"));
    return NULL;
  }

  size_t token_size = 0;
  char *token = mutt_file_read_line(NULL, &token_size, fp, NULL, MUTT_RL_NO_FLAGS);
  mutt_file_fclose(&fp);
  filter_wait(pid);

  /* The refresh cmd in some cases will invoke gpg to decrypt a token */
  if (!OptNoCurses)
    mutt_need_hard_redraw();

  if (!token || (*token == '\0'))
  {
    mutt_error(_("Command returned empty string"));
    FREE(&token);
    return NULL;
  }

  if ((!xoauth2 && (token_size > 512)) || (xoauth2 && (token_size > 4096)))
  {
    mutt_error(_("OAUTH token is too big: %ld"), (long) token_size);
    FREE(&token);
    return NULL;
  }

  /* 4500 is chosen to allow for both a token that is 4096-long plus a
   * username that can be up to 320-long. */
  char oauthbearer[4500] = { 0 };
  int oalen = 0;
  if (xoauth2)
  {
    oalen = snprintf(oauthbearer, sizeof(oauthbearer),
                     "user=%s\001auth=Bearer %s\001\001", cac->login, token);
  }
  else
  {
    oalen = snprintf(oauthbearer, sizeof(oauthbearer),
                     "n,a=%s,\001host=%s\001port=%d\001auth=Bearer %s\001\001",
                     cac->login, cac->host, cac->port, token);
  }
  FREE(&token);

  size_t encoded_len = oalen * 4 / 3 + 10;
  ASSERT(encoded_len < 6010); // Assure LGTM that we won't overflow

  char *encoded_token = MUTT_MEM_MALLOC(encoded_len, char);
  mutt_b64_encode(oauthbearer, oalen, encoded_token, encoded_len);

  return encoded_token;
}
