/**
 * @file
 * Determine who the email is from
 *
 * @authors
 * Copyright (C) 1996-2000,2013 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_EMAIL_FROM_H
#define MUTT_EMAIL_FROM_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

bool is_from(const char *s, char *path, size_t pathlen, time_t *tp);

#endif /* MUTT_EMAIL_FROM_H */
