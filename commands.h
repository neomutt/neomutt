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

#include <stdio.h>

struct Body;
struct Context;
struct Envelope;
struct Header;

/* These Config Variables are only used in commands.c */
extern unsigned char CryptVerifySig; /* verify PGP signatures */
extern char *        DisplayFilter;
extern bool          PipeDecode;
extern char *        PipeSep;
extern bool          PipeSplit;
extern bool          PrintDecode;
extern bool          PrintSplit;
extern bool          PromptAfter;

void ci_bounce_message(struct Header *h);
void mutt_check_stats(void);
int  mutt_check_traditional_pgp(struct Header *h, int *redraw);
void mutt_display_address(struct Envelope *env);
int  mutt_display_message(struct Header *cur);
int  mutt_edit_content_type(struct Header *h, struct Body *b, FILE *fp);
void mutt_enter_command(void);
void mutt_pipe_message(struct Header *h);
void mutt_print_message(struct Header *h);
int mutt_save_message_ctx(struct Header *h, int delete, int decode, int decrypt, struct Context *ctx);
int  mutt_save_message(struct Header *h, int delete, int decode, int decrypt);
void mutt_shell_escape(void);
void mutt_version(void);

#endif /* MUTT_COMMANDS_H */
