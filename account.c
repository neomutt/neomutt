/**
 * @file
 * Representation of an account
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page account Representation of an account
 *
 * Representation of an account
 */

#include "config.h"
#include "mutt/mutt.h"
#include "account.h"

struct AccountList AllAccounts = TAILQ_HEAD_INITIALIZER(AllAccounts);

/**
 * account_new - Create a new Account
 * @retval ptr New Account
 */
struct Account *account_new(void)
{
  struct Account *a = mutt_mem_calloc(1, sizeof(struct Account));
  STAILQ_INIT(&a->mailboxes);

  return a;
}

/**
 * account_add_config - Add some inherited Config items
 * @param a         Account to add to
 * @param cs        Parent Config set
 * @param name      Account name
 * @param var_names Names of Config items
 * @retval bool True, if Config was successfully added
 */
bool account_add_config(struct Account *a, const struct ConfigSet *cs,
                        const char *name, const char *var_names[])
{
  if (!a || !cs || !name || !var_names)
    return false;

  size_t count = 0;
  for (; var_names[count]; count++)
    ;

  a->name = mutt_str_strdup(name);
  a->cs = cs;
  a->var_names = var_names;
  a->vars = mutt_mem_calloc(count, sizeof(struct HashElem *));
  a->num_vars = count;

  char a_name[64];

  for (size_t i = 0; i < a->num_vars; i++)
  {
    struct HashElem *parent = cs_get_elem(cs, a->var_names[i]);
    if (!parent)
    {
      mutt_debug(LL_DEBUG1, "%s doesn't exist\n", a->var_names[i]);
      return false;
    }

    snprintf(a_name, sizeof(a_name), "%s:%s", name, a->var_names[i]);
    a->vars[i] = cs_inherit_variable(cs, parent, a_name);
    if (!a->vars[i])
    {
      mutt_debug(LL_DEBUG1, "failed to create %s\n", a_name);
      return false;
    }
  }

  return true;
}

/**
 * account_free_config - Remove an Account's Config items
 * @param a Account
 */
void account_free_config(struct Account *a)
{
  if (!a)
    return;

  char child[128];
  struct Buffer *err = mutt_buffer_alloc(128);

  for (size_t i = 0; i < a->num_vars; i++)
  {
    snprintf(child, sizeof(child), "%s:%s", a->name, a->var_names[i]);
    mutt_buffer_reset(err);
    int result = cs_str_reset(a->cs, child, err);
    if (CSR_RESULT(result) != CSR_SUCCESS)
      mutt_debug(LL_DEBUG1, "reset failed for %s: %s\n", child, mutt_b2s(err));
    mutt_hash_delete(a->cs->hash, child, NULL);
  }

  mutt_buffer_free(&err);
  FREE(&a->name);
  FREE(&a->vars);
}

/**
 * account_mailbox_remove - Remove a Mailbox from an Account
 * @param a Account
 * @param m Mailbox to remove
 */
bool account_mailbox_remove(struct Account *a, struct Mailbox *m)
{
  if (!a)
    return false;

  bool result = false;
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (np->mailbox == m)
    {
      STAILQ_REMOVE(&a->mailboxes, np, MailboxNode, entries);
      result = true;
      break;
    }
  }

  if (STAILQ_EMPTY(&a->mailboxes))
  {
    account_free(&a);
  }
  return result;
}

/**
 * account_free - Free an Account
 * @param[out] ptr Account to free
 */
void account_free(struct Account **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Account *a = *ptr;
  if (!TAILQ_EMPTY(&AllAccounts))
    TAILQ_REMOVE(&AllAccounts, a, entries);
  if (a->free_adata)
    a->free_adata(&a->adata);

  account_free_config(a);

  FREE(ptr);
}

/**
 * account_set_value - Set an Account-specific config item
 * @param a     Account
 * @param vid   Value ID (index into Account's HashElem's)
 * @param value Native pointer/value to set
 * @param err   Buffer for error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int account_set_value(const struct Account *a, size_t vid, intptr_t value, struct Buffer *err)
{
  if (!a)
    return CSR_ERR_CODE;
  if (vid >= a->num_vars)
    return CSR_ERR_UNKNOWN;

  struct HashElem *he = a->vars[vid];
  return cs_he_native_set(a->cs, he, value, err);
}

/**
 * account_get_value - Get an Account-specific config item
 * @param a      Account
 * @param vid    Value ID (index into Account's HashElem's)
 * @param result Buffer for results or error messages
 * @retval num Result, e.g. #CSR_SUCCESS
 */
int account_get_value(const struct Account *a, size_t vid, struct Buffer *result)
{
  if (!a)
    return CSR_ERR_CODE;
  if (vid >= a->num_vars)
    return CSR_ERR_UNKNOWN;

  struct HashElem *he = a->vars[vid];

  if ((he->type & DT_INHERITED) && (DTYPE(he->type) == 0))
  {
    struct Inheritance *i = he->data;
    he = i->parent;
  }

  return cs_he_string_get(a->cs, he, result);
}
