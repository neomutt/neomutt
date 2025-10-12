/**
 * @file
 * Ask the user a question
 *
 * @authors
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_question Question
 *
 * Ask the user a question
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | question/module.c   | @subpage question_module   |
 * | question/question.c | @subpage question_question |
 */

#ifndef MUTT_QUESTION_LIB_H
#define MUTT_QUESTION_LIB_H

#include "config/lib.h"

int             mw_multi_choice            (const char *prompt, const char *letters);
enum QuadOption query_yesorno              (const char *prompt, enum QuadOption def);
enum QuadOption query_yesorno_ignore_macro (const char *prompt, enum QuadOption def);
enum QuadOption query_yesorno_help         (const char *prompt, enum QuadOption def, struct ConfigSubset *sub, const char *name);
enum QuadOption query_quadoption           (const char *prompt, struct ConfigSubset *sub, const char *name);

#endif /* MUTT_QUESTION_LIB_H */
