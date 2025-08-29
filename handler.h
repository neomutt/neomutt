/**
 * @file
 * Decide how to display email content
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_HANDLER_H
#define MUTT_HANDLER_H

#include <iconv.h>
#include <stdbool.h>
#include <stddef.h>

struct Body;
struct State;

int  mutt_body_handler        (struct Body *b, struct State *state);
bool mutt_can_decode          (struct Body *b);
void mutt_decode_attachment   (const struct Body *b, struct State *state);
void mutt_decode_base64       (struct State *state, size_t len, bool istext, iconv_t cd);
bool mutt_prefer_as_attachment(struct Body *b);

#endif /* MUTT_HANDLER_H */
