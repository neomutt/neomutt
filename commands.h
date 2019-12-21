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

/* These Config Variables are only used in commands.c */
extern unsigned char C_CryptVerifySig; /* verify PGP signatures */
extern char *        C_DisplayFilter;
extern bool          C_PipeDecode;
extern char *        C_PipeSep;
extern bool          C_PipeSplit;
extern bool          C_PrintDecode;
extern bool          C_PrintSplit;
extern bool          C_PromptAfter;

void ci_bounce_message(struct Mailbox *m, struct EmailList *el);
void mutt_check_stats(void);
bool mutt_check_traditional_pgp(struct EmailList *el, MuttRedrawFlags *redraw);
void mutt_display_address(struct Envelope *env);
int  mutt_display_message(struct MuttWindow *win_index, struct MuttWindow *win_ibar, struct MuttWindow *win_pager, struct MuttWindow *win_pbar, struct Mailbox *m, struct Email *e);
bool mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp);
void mutt_enter_command(void);
void mutt_pipe_message(struct Mailbox *m, struct EmailList *el);
void mutt_print_message(struct Mailbox *m, struct EmailList *el);
int  mutt_save_message_ctx(struct Email *e, bool delete_original, bool decode, bool decrypt, struct Mailbox *m);
int  mutt_save_message(struct Mailbox *m, struct EmailList *el, bool delete_original, bool decode, bool decrypt);
int  mutt_select_sort(bool reverse);
void mutt_shell_escape(void);

#endif /* MUTT_COMMANDS_H */
