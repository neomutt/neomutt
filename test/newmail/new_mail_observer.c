/**
 * @file
 * Test code for new_mail_observer()
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

#include "mutt/notify.h"
#include "mutt/observer.h"
#include "mutt/string2.h"
#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "core/lib.h"
#include "newmail/lib.h"

static char *message = NULL;

int dummy_execute_cmd(const char *cmd)
{
  message = mutt_str_dup(cmd);
  return 0;
}

int dummy_new_mail_observer(struct NotifyCallback *nc)
{
  return handle_new_mail_event("New messages", nc, dummy_execute_cmd);
}

void test_new_mail_observer(void)
{
  struct Notify *notify = notify_new();
  notify_observer_add(notify, NT_MAILBOX, dummy_new_mail_observer, NULL);
  struct Mailbox *mailbox = mailbox_new();
  mailbox->name = mutt_str_dup("Mailbox");
  struct EventMailbox event = { mailbox, ARRAY_HEAD_INITIALIZER };

  notify_send(notify, NT_MAILBOX, NT_MAILBOX_NEW_MAIL, NULL);
  TEST_CHECK(message == NULL);

  notify_send(notify, NT_MAILBOX, NT_MAILBOX_NEW_MAIL, &event);
  TEST_CHECK(message != NULL);
  TEST_CHECK(mutt_str_equal(message, "New messages"));
  TEST_MSG("Check failed: \"%s\" != \"New messages\"", message);

  notify_free(&notify);
  mailbox_free(&mailbox);
  FREE(&message);
}
