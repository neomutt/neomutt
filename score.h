/**
 * @file
 * Routines for adding user scores to emails
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

#ifndef MUTT_SCORE_H
#define MUTT_SCORE_H

#include <stdbool.h>
#include "mutt_commands.h"

struct Buffer;
struct Email;
struct Mailbox;

/* These Config Variables are only used in score.c */
extern short C_ScoreThresholdDelete;
extern short C_ScoreThresholdFlag;
extern short C_ScoreThresholdRead;

void mutt_check_rescore(struct Mailbox *m);
enum CommandResult mutt_parse_score(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_parse_unscore(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
void mutt_score_message(struct Mailbox *m, struct Email *e, bool upd_ctx);

#endif /* MUTT_SCORE_H */
