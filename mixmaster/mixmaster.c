/**
 * @file
 * Mixmaster Remailer
 *
 * @authors
 * Copyright (C) 1999-2001 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page mixmaster_mixmaster Mixmaster Remailer
 *
 * Mixmaster Remailer
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "send/lib.h"
#include "globals.h"
#include "protos.h"

/**
 * mix_check_message - Safety-check the message before passing it to mixmaster
 * @param e Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mix_check_message(struct Email *e)
{
  bool need_hostname = false;

  if (!TAILQ_EMPTY(&e->env->cc) || !TAILQ_EMPTY(&e->env->bcc))
  {
    mutt_error(_("Mixmaster doesn't accept Cc or Bcc headers"));
    return -1;
  }

  /* When using mixmaster, we MUST qualify any addresses since
   * the message will be delivered through remote systems.
   *
   * use_domain won't be respected at this point, hidden_host will.
   */

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &e->env->to, entries)
  {
    if (!a->group && !buf_find_char(a->mailbox, '@'))
    {
      need_hostname = true;
      break;
    }
  }

  if (need_hostname)
  {
    const char *fqdn = mutt_fqdn(true, NeoMutt->sub);
    if (!fqdn)
    {
      mutt_error(_("Please set the hostname variable to a proper value when using mixmaster"));
      return -1;
    }

    /* Cc and Bcc are empty at this point. */
    mutt_addrlist_qualify(&e->env->to, fqdn);
    mutt_addrlist_qualify(&e->env->reply_to, fqdn);
    mutt_addrlist_qualify(&e->env->mail_followup_to, fqdn);
  }

  return 0;
}

/**
 * mix_send_message - Send an email via Mixmaster
 * @param chain    String list of hosts
 * @param tempfile Temporary file containing email
 * @retval -1  Error
 * @retval >=0 Success (Mixmaster's return code)
 */
int mix_send_message(struct ListHead *chain, const char *tempfile)
{
  int i = 0;
  struct Buffer *cmd = buf_pool_get();
  struct Buffer *cd_quoted = buf_pool_get();

  const char *const c_mixmaster = cs_subset_string(NeoMutt->sub, "mixmaster");
  buf_printf(cmd, "cat %s | %s -m ", tempfile, c_mixmaster);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, chain, entries)
  {
    buf_addstr(cmd, (i != 0) ? "," : " -l ");
    buf_quote_filename(cd_quoted, (char *) np->data, true);
    buf_addstr(cmd, buf_string(cd_quoted));
    i = 1;
  }

  mutt_endwin();

  i = mutt_system(cmd->data);
  if (i != 0)
  {
    fprintf(stderr, _("Error sending message, child exited %d\n"), i);
    if (!OptNoCurses)
    {
      mutt_any_key_to_continue(NULL);
      mutt_error(_("Error sending message"));
    }
  }

  buf_pool_release(&cmd);
  buf_pool_release(&cd_quoted);
  unlink(tempfile);
  return i;
}
