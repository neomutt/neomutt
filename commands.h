/**
 * @file
 * Manage where the email is piped to external commands
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

#ifndef MUTT_COMMANDS_H
#define MUTT_COMMANDS_H

#include <stdbool.h>
#include <stdio.h>
#include "mutt_menu.h"

struct Body;
struct Email;
struct EmailList;
struct Envelope;
struct Mailbox;
struct MuttWindow;

/**
 * enum MessageTransformOpt - Message transformation option
 */
enum MessageTransformOpt
{
  TRANSFORM_NONE = 0,   ///< No transformation
  TRANSFORM_DECRYPT,    ///< Decrypt message
  TRANSFORM_DECODE,     ///< Decode message
};

/**
 * enum MessageSaveOpt - Message save option
 */
enum MessageSaveOpt
{
  SAVE_COPY = 0,   ///< Copy message, making a duplicate in another mailbox
  SAVE_MOVE,       ///< Move message to another mailbox, removing the original
};

void ci_bounce_message(struct Mailbox *m, struct EmailList *el);
void mutt_check_stats(struct Mailbox *m);
bool mutt_check_traditional_pgp(struct Mailbox *m, struct EmailList *el);
void mutt_commands_cleanup(void);
void mutt_display_address(struct Envelope *env);
int  mutt_display_message(struct MuttWindow *win_index, struct MuttWindow *win_ibar, struct MuttWindow *win_pager, struct MuttWindow *win_pbar, struct Mailbox *m, struct Email *e);
bool mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp);
void mutt_enter_command(void);
void mutt_pipe_message(struct Mailbox *m, struct EmailList *el);
void mutt_print_message(struct Mailbox *m, struct EmailList *el);
int  mutt_save_message(struct Mailbox *m, struct EmailList *el, enum MessageSaveOpt, enum MessageTransformOpt transform_opt);
int mutt_save_message_ctx(struct Mailbox *m_src, struct Email *e, enum MessageSaveOpt save_opt, enum MessageTransformOpt transform_opt, struct Mailbox *m_dst);
bool mutt_select_sort(bool reverse);
bool mutt_shell_escape(void);

#endif /* MUTT_COMMANDS_H */
