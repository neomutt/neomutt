/**
 * @file
 * Connection Credentials External Command
 *
 * @authors
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
 * @page conn_account_ext_cmd Connection Credentials External Command
 *
 * Connection Credentials External Command
 */

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "accountcmd.h"
#include "connaccount.h"
#include "globals.h"
#include "mutt_account.h"

/**
 * make_cmd - Build the command line for the external account command.
 * @param buf Buffer to fill with a command line
 * @param cac ConnAccount to read params from
 * @param cmd Account command from the user config
 */
static void make_cmd(struct Buffer *buf, const struct ConnAccount *cac, const char *cmd)
{
  assert(buf && cac);

  buf_addstr(buf, cmd);
  buf_add_printf(buf, " --hostname %s", cac->host);
  if (cac->flags & MUTT_ACCT_USER)
  {
    buf_add_printf(buf, " --username %s", cac->user);
  }

  static const char *types[] = { "", "imap", "pop", "smtp", "nntp" };
  assert(sizeof(types) / sizeof(*types) == MUTT_ACCT_TYPE_MAX);
  if (cac->type != MUTT_ACCT_TYPE_NONE)
  {
    buf_add_printf(buf, " --type %s%c", types[cac->type],
                   (cac->flags & MUTT_ACCT_SSL) ? 's' : '\0');
  }
}

/**
 * parse_one - Parse a single line of the response
 * @param cac ConnAccount to write to
 * @param line Line from the response
 * @retval num #MuttAccountFlags that matched
 * @retval #MUTT_ACCT_NO_FLAGS Failure
 */
static MuttAccountFlags parse_one(struct ConnAccount *cac, char *line)
{
  assert(cac && line);

  const regmatch_t *match = mutt_prex_capture(PREX_ACCOUNT_CMD, line);
  if (!match)
  {
    mutt_perror(_("Line is malformed: expected <key: val>, got <%s>"), line);
    return MUTT_ACCT_NO_FLAGS;
  }

  const regmatch_t *keymatch = &match[PREX_ACCOUNT_CMD_MATCH_KEY];
  const regmatch_t *valmatch = &match[PREX_ACCOUNT_CMD_MATCH_VALUE];
  line[mutt_regmatch_start(keymatch) + mutt_regmatch_len(keymatch)] = '\0';
  const char *key = line + mutt_regmatch_start(keymatch);
  const char *val = line + mutt_regmatch_start(valmatch);

  switch (key[0])
  {
    case 'l':
      if (mutt_str_equal(key + 1, "ogin"))
      {
        mutt_str_copy(cac->login, val, sizeof(cac->login));
        return MUTT_ACCT_LOGIN;
      }
      break;

    case 'p':
      if (mutt_str_equal(key + 1, "assword"))
      {
        mutt_str_copy(cac->pass, val, sizeof(cac->pass));
        return MUTT_ACCT_PASS;
      }
      break;

    case 'u':
      if (mutt_str_equal(key + 1, "sername"))
      {
        mutt_str_copy(cac->user, val, sizeof(cac->user));
        return MUTT_ACCT_USER;
      }
      break;

    default:
      break;
  }

  mutt_warning(_("Unhandled key in line <%s: %s>"), key, val);
  return MUTT_ACCT_NO_FLAGS;
}

/**
 * call_cmd - Call the account command
 * @param cac ConnAccount to write to
 * @param cmd Command line to run
 * @retval num #MuttAccountFlags that matched
 * @retval #MUTT_ACCT_NO_FLAGS Failure
 */
static MuttAccountFlags call_cmd(struct ConnAccount *cac, const struct Buffer *cmd)
{
  assert(cac && cmd);

  MuttAccountFlags rc = MUTT_ACCT_NO_FLAGS;

  FILE *fp = NULL;
  pid_t pid = filter_create(buf_string(cmd), NULL, &fp, NULL, EnvList);
  if (pid < 0)
  {
    mutt_perror(_("Unable to run account command"));
    return rc;
  }

  size_t len = 0;
  char *line = NULL;
  while ((line = mutt_file_read_line(NULL, &len, fp, NULL, MUTT_RL_NO_FLAGS)))
  {
    rc |= parse_one(cac, line);
    FREE(&line);
  }

  mutt_file_fclose(&fp);
  filter_wait(pid);
  return rc;
}

/**
 * mutt_account_call_external_cmd - Retrieve account credentials via an external command
 * @param cac ConnAccount to fill
 * @retval num A bitmask of MuttAccountFlags that were retrieved on success
 * @retval #MUTT_ACCT_NO_FLAGS Failure
 */
MuttAccountFlags mutt_account_call_external_cmd(struct ConnAccount *cac)
{
  if (!cac || (cac->host[0] == '\0') || (cac->type == MUTT_ACCT_TYPE_NONE))
  {
    return MUTT_ACCT_NO_FLAGS;
  }

  const char *c_account_command = cs_subset_string(NeoMutt->sub, "account_command");
  if (!c_account_command)
  {
    return MUTT_ACCT_NO_FLAGS;
  }

  MuttAccountFlags rc = MUTT_ACCT_NO_FLAGS;
  struct Buffer *cmd_buf = buf_pool_get();

  make_cmd(cmd_buf, cac, c_account_command);
  rc = call_cmd(cac, cmd_buf);
  cac->flags |= rc;

  buf_pool_release(&cmd_buf);
  return rc;
}
