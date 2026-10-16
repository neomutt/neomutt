/**
 * @file
 * Test code for mailbox_clear_emails()
 *
 * @authors
 * Copyright (C) 2026 Alejandro Pacheco <alepacheco99@gmail.com>
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
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"

static int EdataFreed = 0;

static void count_edata_free(void **ptr)
{
  EdataFreed++;
  FREE(ptr);
}

void test_mailbox_clear_emails(void)
{
  // void mailbox_clear_emails(struct Mailbox *m);

  {
    mailbox_clear_emails(NULL);
    TEST_CHECK_(1, "mailbox_clear_emails(NULL)");
  }

  {
    struct Mailbox *m = mailbox_new();
    mailbox_clear_emails(m);
    TEST_CHECK(m->msg_count == 0);
    mailbox_free(&m);
  }

  {
    struct Mailbox *m = mailbox_new();
    const int email_max = m->email_max;

    // Two counted emails, and one left beyond msg_count
    for (int i = 0; i < 3; i++)
    {
      struct Email *e = email_new();
      e->env = mutt_env_new();
      e->env->message_id = mutt_str_dup("<id@example.com>");
      mutt_str_replace((char **) &e->env->subject, "subject");
      m->emails[i] = e;
    }
    m->msg_count = 2;
    m->msg_unread = 2;
    m->msg_flagged = 1;
    m->msg_new = 1;
    m->msg_deleted = 1;
    m->msg_tagged = 1;
    m->vcount = 2;
    m->size = 1234;

    m->id_hash = mutt_hash_new(8, MUTT_HASH_NONE);
    m->subj_hash = mutt_hash_new(8, MUTT_HASH_ALLOW_DUPS);
    for (int i = 0; i < m->msg_count; i++)
    {
      mutt_hash_insert(m->id_hash, m->emails[i]->env->message_id, m->emails[i]);
      mutt_hash_insert(m->subj_hash, m->emails[i]->env->subject, m->emails[i]);
    }

    mailbox_clear_emails(m);

    TEST_CHECK(m->id_hash == NULL);
    TEST_CHECK(m->subj_hash == NULL);
    for (int i = 0; i < m->email_max; i++)
      TEST_CHECK(m->emails[i] == NULL);
    TEST_CHECK(m->emails != NULL);
    TEST_CHECK(m->email_max == email_max);
    TEST_CHECK(m->msg_count == 0);
    TEST_CHECK(m->msg_unread == 0);
    TEST_CHECK(m->msg_flagged == 0);
    TEST_CHECK(m->msg_new == 0);
    TEST_CHECK(m->msg_deleted == 0);
    TEST_CHECK(m->msg_tagged == 0);
    TEST_CHECK(m->vcount == 0);
    TEST_CHECK(m->size == 0);

    mailbox_free(&m);
  }

  {
    struct Mailbox *m = mailbox_new();

    // Every email's private data is freed once, including beyond msg_count
    for (int i = 0; i < 3; i++)
    {
      struct Email *e = email_new();
      e->edata = mutt_mem_calloc(1, 8);
      e->edata_free = count_edata_free;
      m->emails[i] = e;
    }
    m->msg_count = 2;

    EdataFreed = 0;
    mailbox_clear_emails(m);
    TEST_CHECK(EdataFreed == 3);

    mailbox_clear_emails(m);
    TEST_CHECK(EdataFreed == 3);

    mailbox_free(&m);
    TEST_CHECK(EdataFreed == 3);
  }
}
