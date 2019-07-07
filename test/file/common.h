/**
 * @file
 * Common code for file tests
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#ifndef TEST_FILE_COMMON_H
#define TEST_FILE_COMMON_H

#include "acutest.h"
#include "config.h"
#include <stdio.h>
#include "mutt/mutt.h"

extern const char *file_lines[];

FILE *file_set_up(const char *funcname);
void file_tear_down(FILE *fp, const char *funcname);
size_t file_num_test_lines(void);

#define SET_UP() (file_set_up(__func__))
#define TEAR_DOWN(fp) (file_tear_down((fp), __func__))

#endif /* TEST_FILE_COMMON_H */
