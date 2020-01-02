/**
 * @file
 * Dump all Accounts
 *
 * @authors
 * Copyright (C) 2019-2020 Richard Russon <rich@flatcap.org>
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
 * @page debug_account Dump all Accounts
 *
 * Dump all Accounts
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "globals.h"
#include "init.h"

void dump_one(struct Buffer *tmp, struct Buffer *value, const char *name)
{
  mutt_buffer_reset(value);
  mutt_buffer_reset(tmp);
  struct HashElem *he = NULL;
  he = cs_get_elem(NeoMutt->sub->cs, name);
  if (!he)
  {
    printf("\n");
    return;
  }
  int type = DTYPE(he->type);
  cs_he_string_get(NeoMutt->sub->cs, he, value);
  if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) && (type != DT_QUAD))
  {
    mutt_buffer_reset(tmp);
    pretty_var(value->data, tmp);
    mutt_buffer_strcpy(value, tmp->data);
  }
  dump_config_neo(NeoMutt->sub->cs, he, value, NULL, CS_DUMP_NO_FLAGS, stdout);
}

void dump_vars(const char *account)
{
  const char *vars[] = { "folder", "index_format", "sort", "sort_aux" };
  struct Buffer tmp = mutt_buffer_make(1024);
  struct Buffer value = mutt_buffer_make(1024);
  char name[128];

  printf("%s:\n", account ? account : "base values");
  for (size_t i = 0; i < mutt_array_size(vars); i++)
  {
    printf("    ");
    if (account)
      snprintf(name, sizeof(name), "%s:%s", account, vars[i]);
    else
      snprintf(name, sizeof(name), "%s", vars[i]);
    dump_one(&tmp, &value, name);
  }

  mutt_buffer_dealloc(&tmp);
  mutt_buffer_dealloc(&value);
}

void dump_accounts2(void)
{
  printf("\n");
  dump_vars(NULL);
  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    dump_vars(np->name);
  }
}

void dump_inherited(struct ConfigSet *cs)
{
  printf("\n");

  struct Buffer tmp = mutt_buffer_make(1024);
  struct Buffer value = mutt_buffer_make(1024);

  struct HashElem **list = get_elem_list(cs);
  for (size_t i = 0; list[i]; i++)
  {
    const char *item = list[i]->key.strkey;
    if (!strchr(item, ':'))
      continue;
    cs_he_string_get(cs, list[i], &value);
    dump_one(&tmp, &value, item);
  }

  mutt_buffer_dealloc(&tmp);
  mutt_buffer_dealloc(&value);
}

void kill_accounts(void)
{
  char buf[128];
  struct Buffer err = mutt_buffer_make(1024);

  struct Account *np = NULL;
  struct Account *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, &NeoMutt->accounts, entries, tmp)
  {
    snprintf(buf, sizeof(buf), "unaccount %s", np->name);
    mutt_parse_rc_line(buf, &err);
  }

  mutt_buffer_dealloc(&err);
}

struct HashElem *get_he(struct ConfigSet *cs, const char *name)
{
  struct Slist *sl = slist_parse(name, SLIST_SEP_COLON);
  if (!sl || (sl->count < 1) || (sl->count > 3))
    return NULL;

  struct ConfigSubset *sub = NULL;
  if (sl->count == 1)
  {
    sub = NeoMutt->sub;
    goto have_sub;
  }

  struct ListNode *np = STAILQ_FIRST(&sl->head);
  struct Account *a = account_find(np->data);
  if (!a)
    return NULL;

  if (sl->count == 2)
  {
    sub = a->sub;
    goto have_sub;
  }

  np = STAILQ_NEXT(STAILQ_FIRST(&sl->head), entries);
  struct Mailbox *m = mailbox_find(np->data);
  if (!m)
    return NULL;

  sub = m->sub;
  if (sub)
    ;

have_sub:
  return NULL;
}

#if 0
void test_parse_set2(int argc, char *argv[])
{
    struct Buffer *tmp = mutt_buffer_alloc(256);
    struct Buffer *var = mutt_buffer_alloc(256);
    struct Buffer *err = mutt_buffer_alloc(256);
    for (size_t j = 1; j < argc; j++)
    {
      mutt_buffer_reset(tmp);
      mutt_buffer_reset(var);
      mutt_buffer_reset(err);

      printf("arg = %s\n", argv[j]);
      mutt_buffer_printf(var, "set %s", argv[j]);
      var->dptr = var->data + 4;
      printf("%s\n", var->data);
      int rc = parse_set(tmp, var, 0, err);
      mutt_buffer_reset(var);
      char *eq = strchr(argv[j], '=');
      if (eq)
        *eq = '\0';
      struct HashElem *he = get_he(NeoMutt->sub->cs, argv[j]);
      cs_subset_he_string_get(sub, he, var);
      if (rc < 0)
        printf("%2d %s\n", rc, var->data);
      else
        printf("%2d %s = %s\n", rc, argv[j], var->data);
    }
    mutt_buffer_free(&err);
    mutt_buffer_free(&var);
    mutt_buffer_free(&tmp);
}
#endif

void dump_config_notify(const char *level, struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return;

  struct EventConfig *ec = nc->event_data;
  const struct ConfigSubset *sub = ec->sub;
  const char *scope = "???";
  switch (sub->scope)
  {
    case SET_SCOPE_NEOMUTT:
      scope = "neomutt";
      break;
    case SET_SCOPE_ACCOUNT:
      scope = "account";
      break;
    case SET_SCOPE_MAILBOX:
      scope = "mailbox";
      break;
  }

  printf("Event %s, Observer %s: %s (%s)\n", scope, level, ec->name, NONULL(sub->name));
}

int neomutt_observer(struct NotifyCallback *nc)
{
  dump_config_notify("neomutt", nc);
  return 0;
}

int account_observer(struct NotifyCallback *nc)
{
  dump_config_notify("account", nc);
  return 0;
}

int mailbox_observer(struct NotifyCallback *nc)
{
  dump_config_notify("mailbox", nc);
  return 0;
}

void test1(struct NeoMutt *n)
{
  const char *name = "time_inc";
  // struct HashElem *he_n = NULL;
  // struct HashElem *he_a = NULL;
  // struct HashElem *he_m = NULL;
  intptr_t val_n;
  intptr_t val_a;
  intptr_t val_m;

  struct Account *a = account_new("fruit", n->sub);
  a->type = MUTT_MAILDIR;

  struct Mailbox *m = mailbox_new(NULL); // "apple"
  mailbox_set_subset(m, a->sub);
  account_mailbox_add(a, m);

  subset_dump(m->sub);
  // subset_dump_var(m->sub, name);

  notify_observer_add(n->notify, neomutt_observer, NULL);
  notify_observer_add(a->notify, account_observer, NULL);
  notify_observer_add(m->notify, mailbox_observer, NULL);

  // cs_subset_str_native_set(m->sub, name, 42, NULL);

  // he_n = cs_subset_lookup(n->sub, name);
  // he_a = cs_subset_lookup(a->sub, name);
  // he_m = cs_subset_lookup(m->sub, name);

  // val_n = cs_subset_str_native_get(n->sub, name, NULL);
  // val_a = cs_subset_str_native_get(a->sub, name, NULL);
  // val_m = cs_subset_str_native_get(m->sub, name, NULL);

  // printf("neomutt: %ld\n", val_n);
  // printf("account: %ld\n", val_a);
  // printf("mailbox: %ld\n", val_m);

  // cs_subset_he_native_set(n->sub, he_n, 10, NULL);
  cs_subset_str_native_set(n->sub, name, 10, NULL);
  subset_dump_var(m->sub, name);

  // val_n = cs_subset_str_native_get(n->sub, name, NULL);
  // val_a = cs_subset_str_native_get(a->sub, name, NULL);
  // val_m = cs_subset_str_native_get(m->sub, name, NULL);

  // printf("neomutt: %ld\n", val_n);
  // printf("account: %ld\n", val_a);
  // printf("mailbox: %ld\n", val_m);

  // cs_subset_he_native_set(a->sub, he_a, 20, NULL);
  cs_subset_str_native_set(a->sub, name, 20, NULL);
  subset_dump_var(m->sub, name);

  // val_n = cs_subset_str_native_get(n->sub, name, NULL);
  // val_a = cs_subset_str_native_get(a->sub, name, NULL);
  // val_m = cs_subset_str_native_get(m->sub, name, NULL);

  // printf("neomutt: %ld\n", val_n);
  // printf("account: %ld\n", val_a);
  // printf("mailbox: %ld\n", val_m);

  // cs_subset_he_native_set(m->sub, he_m, 30, NULL);
  cs_subset_str_native_set(m->sub, name, 30, NULL);
  subset_dump_var(m->sub, name);

  val_n = cs_subset_str_native_get(n->sub, name, NULL);
  val_a = cs_subset_str_native_get(a->sub, name, NULL);
  val_m = cs_subset_str_native_get(m->sub, name, NULL);

  printf("neomutt: %ld\n", val_n);
  printf("account: %ld\n", val_a);
  printf("mailbox: %ld\n", val_m);

  account_free(&a);
}

void test2(struct NeoMutt *n)
{
  const char *name = "copy";

  struct Account *a = account_new("fruit", n->sub);
  a->type = MUTT_MAILDIR;

  struct Mailbox *m = mailbox_new(NULL); // "apple"
  mailbox_set_subset(m, a->sub);
  account_mailbox_add(a, m);

  cs_subset_str_native_set(n->sub, name, MUTT_ASKNO, NULL);
  subset_dump_var(m->sub, name);

  quad_str_toggle(m->sub, name, NULL);
  subset_dump_var(m->sub, name);

  account_free(&a);
}

void test_config_notify(struct NeoMutt *n)
{
  // test1(n);
  test2(n);
}
