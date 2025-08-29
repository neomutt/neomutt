/**
 * @file
 * Shared Editor Buffer Testing Code
 *
 * @authors
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef TEST_EDITOR_COMMON_H
#define TEST_EDITOR_COMMON_H

#include <stddef.h>

struct EnterState;

size_t editor_buffer_get_lastchar(struct EnterState *es);
size_t editor_buffer_get_cursor(struct EnterState *es);
size_t editor_buffer_set_cursor(struct EnterState *es, size_t pos);
int editor_buffer_set(struct EnterState *es, const char *str);

#endif /* TEST_EDITOR_COMMON_H */
