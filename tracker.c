/**
 * @file
 * Keep track of the current Account and Mailbox
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
 * @page tracker Keep track of the current Account and Mailbox
 *
 * When reading a config file, keep track of the current Account and Mailbox.
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "tracker.h"

/**
 * struct ScopePair - A list of Account,Mailbox pairs
 *
 * This is used to keep track of the current 'account' or 'mailbox' command in
 * the config file.
 */
struct ScopePair
{
  struct Account *account;
  struct Mailbox *mailbox;
  STAILQ_ENTRY(ScopePair) entries;
};
STAILQ_HEAD(ScopePairHead, ScopePair);

struct ScopePairHead ConfigStack = STAILQ_HEAD_INITIALIZER(ConfigStack);

/**
 * ct_get_account - Get the current Account
 * @retval ptr  An Account
 * @retval NULL If no 'account' command in use
 */
struct Account *ct_get_account(void)
{
  struct ScopePair *sp = NULL;
  STAILQ_FOREACH(sp, &ConfigStack, entries)
  {
    if (sp->account)
      return sp->account;
  }

  return NULL;
}

/**
 * ct_set_account - Set the current Account
 * @param a Account
 *
 * The 'account' command will scope the following config commands.
 */
void ct_set_account(struct Account *a)
{
  // printf("\033[1;31mset account: %s\033[0m\n", a ? a->name : "[NONE]");
  struct ScopePair *sp = STAILQ_FIRST(&ConfigStack);
  if (!sp)
  {
    mutt_error("FAIL - stack empty1");
    return;
  }

  if (!sp->account && !a)
    mutt_error("WARN - no active account1");
  else
  {
    sp->account = a;
    sp->mailbox = NULL;
  }
  // ct_dump();
}

/**
 * ct_get_mailbox - Get the current Mailbox
 * @retval ptr  A Mailbox
 * @retval NULL If no 'mailbox' command in use
 */
struct Mailbox *ct_get_mailbox(void)
{
  struct ScopePair *sp = NULL;
  STAILQ_FOREACH(sp, &ConfigStack, entries)
  {
    if (sp->account)
      return sp->mailbox;
  }

  return NULL;
}

/**
 * ct_set_mailbox - Set the current Mailbox
 * @param m Mailbox
 *
 * The 'mailbox' command will scope the following config commands.
 */
void ct_set_mailbox(struct Mailbox *m)
{
  // printf("\033[1;34mset mailbox: %s\033[0m\n", m ? m->path->desc : "[NONE]");
  struct ScopePair *sp = STAILQ_FIRST(&ConfigStack);
  if (!sp)
    return;

  if (sp->account)
    sp->mailbox = m;
  // ct_dump();
}

/**
 * ct_push_top - Duplicate the top of the Account/Mailbox stack
 *
 * When a new config file is read, the 'account' or 'mailbox' commands are
 * inherited.
 */
void ct_push_top(void)
{
  // printf("\033[1;31mpush top\033[0m\n");
  struct ScopePair *sp_dup = mutt_mem_calloc(1, sizeof(*sp_dup));

  struct ScopePair *sp = STAILQ_FIRST(&ConfigStack);
  if (sp)
  {
    sp_dup->account = sp->account;
    sp_dup->mailbox = sp->mailbox;
  }

  STAILQ_INSERT_HEAD(&ConfigStack, sp_dup, entries);
  // ct_dump();
}

/**
 * ct_pop - Pop the current Account/Mailbox from the stack
 *
 * When the end of a config file is reached, the current 'account' or 'mailbox'
 * scope ends.
 */
void ct_pop(void)
{
  // printf("\033[1;31mpop\033[0m\n");
  struct ScopePair *sp = STAILQ_FIRST(&ConfigStack);
  if (!sp)
  {
    mutt_error("FAIL - stack empty2");
    return;
  }

  STAILQ_REMOVE_HEAD(&ConfigStack, entries);
  FREE(&sp);
  // ct_dump();
}

/**
 * ct_get_sub - Get the active Config Subset
 * @retval ptr  Config Subset
 *
 * This will depend on any 'account' or 'mailbox' config commands.
 * If none is active, then the global Subset (from NeoMutt) will be returned.
 */
struct ConfigSubset *ct_get_sub(void)
{
  struct ScopePair *sp = NULL;
  STAILQ_FOREACH(sp, &ConfigStack, entries)
  {
    if (sp->mailbox)
      return sp->mailbox->sub;
    else if (sp->account)
      return sp->account->sub;
  }

  return NeoMutt->sub;
}

/**
 * ct_dump - Dump the tracker stack
 */
void ct_dump(void)
{
  size_t i = 0;
  struct ScopePair *sp = NULL;
  printf("tracker stack:");
  STAILQ_FOREACH(sp, &ConfigStack, entries)
  {
    i++;
    struct Account *a = sp->account;
    const char *a_name = a ? a->name : "-";
    struct Mailbox *m = sp->mailbox;
    const char *m_name = m ? m->path->desc : "-";
    printf(" (%s,%s)", a_name, m_name);
  }
  printf("\n");
}
