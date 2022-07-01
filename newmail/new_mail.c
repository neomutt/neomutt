/**
 * @file
 * New mail notification observer implementation.
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

/**
 * @page new_mail Notify about new incoming mail
 *
 * Use an external command to send a system notification when new mail arrives.
 */

#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "newmail/lib.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "protos.h"

/**
 * new_mail_format_str - Format a string for the new mail notification.
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Private data
 * @param[in]  flags    Flags, see #MuttFormatFlags
 * @retval ptr src (unchanged)
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%c     | New messages
 * | \%n     | Folder name
 * | \%f     | Folder path
 * | \%u     | Unread messages
 */
const char *new_mail_format_str(char *buf, size_t buflen, size_t col, int cols,
                                char op, const char *src, const char *prec,
                                const char *if_str, const char *else_str,
                                intptr_t data, MuttFormatFlags flags)
{
  struct EventMailbox *ev_m = (struct EventMailbox *) data;
  struct Mailbox *mailbox = ev_m->mailbox;
  struct EmailArray *emails = &ev_m->emails;

  switch (op)
  {
    case 'c':
      snprintf(buf, buflen, "%ld", ARRAY_SIZE(emails));
      break;
    case 'f':
      snprintf(buf, buflen, "%s", NONULL(mailbox_path(mailbox)));
      break;
    case 'n':
      snprintf(buf, buflen, "%s", NONULL(mailbox->name));
      break;
    case 'u':
      snprintf(buf, buflen, "%d", mailbox->msg_unread);
      break;
  }
  return src;
}

int handle_new_mail_event(const char *cmd, struct NotifyCallback *nc, Execute *execute)
{
  if (nc->event_subtype != NT_MAILBOX_NEW_MAIL)
    return 0;

  struct EventMailbox *ev_m = nc->event_data;
  if (!ev_m || !ev_m->mailbox)
    return 0;

  char expanded_cmd[1024];
  mutt_expando_format(expanded_cmd, 1024, 0, 1024, cmd, new_mail_format_str,
                      (intptr_t) ev_m, MUTT_FORMAT_NO_FLAGS);
  execute(expanded_cmd);
  ARRAY_FREE(&ev_m->emails);
  return 0;
}

int execute_cmd(const char *cmd)
{
  if (mutt_system(cmd) != 0)
    mutt_error(_("Error running \"%s\""), cmd);
  return 0;
}

int new_mail_observer(struct NotifyCallback *nc)
{
  const char *c_devel_new_mail_command = cs_subset_string(NeoMutt->sub, "devel_new_mail_command");
  if (!c_devel_new_mail_command)
    return 0;
  return handle_new_mail_event(c_devel_new_mail_command, nc, execute_cmd);
}
