/**
 * @file
 * Routines for adding user scores to emails
 *
 * @authors
 * Copyright (C) 2018-2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EMAIL_SCORE_H
#define MUTT_EMAIL_SCORE_H

#include <stdbool.h>
#include "core/lib.h"

struct Buffer;
struct Email;
struct ParseContext;
struct ParseError;

/**
 * struct Score - Scoring rule for email
 */
struct Score
{
  char *str;               ///< String to match
  struct PatternList *pat; ///< Pattern to match
  int val;                 ///< Score to add
  bool exact;              ///< If this rule matches, don't evaluate any more
  struct Score *next;      ///< Linked list
};

extern struct Score *ScoreList;

enum CommandResult parse_score  (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_unscore(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);

void mutt_check_rescore(struct Mailbox *m);
void mutt_score_message(struct Mailbox *m, struct Email *e, bool upd_mbox);

#endif /* MUTT_EMAIL_SCORE_H */
