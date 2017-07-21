/**
 * @file
 * Signing/encryption multiplexor
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _NCRYPT_CRYPT_H
#define _NCRYPT_CRYPT_H

#include <stdbool.h>

struct Body;
struct State;

void convert_to_7bit(struct Body *a);
void crypt_current_time(struct State *s, char *app_name);
int crypt_write_signed(struct Body *a, struct State *s, const char *tempfile);
const char *crypt_get_fingerprint_or_id(char *p, const char **pphint,
                                        const char **ppl, const char **pps);
bool crypt_is_numerical_keyid(const char *s);

#endif /* _NCRYPT_CRYPT_H */
