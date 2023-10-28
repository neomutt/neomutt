/**
 * @file
 * Test code for notify_send()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"

static int email_observer(struct NotifyCallback *nc)
{
  return -1;
}

void test_notify_send(void)
{
  // bool notify_send(struct Notify *notify, enum NotifyType event_type, int event_subtype, void *event_data);

  TEST_CHECK(!notify_send(NULL, NT_ACCOUNT, NT_ACCOUNT_DELETE, NULL));

  struct Email *e = email_new();

  notify_observer_add(e->notify, NT_EMAIL, email_observer, NULL);

  struct EventEmail ev_e = { 0, NULL };
  notify_send(e->notify, NT_EMAIL, NT_EMAIL_CHANGE, &ev_e);

  notify_observer_remove(e->notify, email_observer, NULL);

  email_free(&e);
}
