/**
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
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

/* Print the current time. */
void crypt_current_time(struct State *s, char *app_name);

/* Write the message body/part A described by state S to the given
   TEMPFILE.  */
int crypt_write_signed(struct Body *a, struct State *s, const char *tempfile);

/* Obtain pointers to fingerprint or short or long key ID, if any.

   Upon return, at most one of return, *ppl and *pps pointers is non-NULL,
   indicating the longest fingerprint or ID found, if any.

   Return:  Copy of fingerprint, if any, stripped of all spaces, else NULL.
            Must be FREE'd by caller.
   *pphint  Start of string to be passed to pgp_add_string_to_hints() or
            crypt_add_string_to_hints().
   *ppl     Start of long key ID if detected, else NULL.
   *pps     Start of short key ID if detected, else NULL. */
const char *crypt_get_fingerprint_or_id(char *p, const char **pphint,
                                        const char **ppl, const char **pps);

/* Check if a string contains a numerical key */
bool crypt_is_numerical_keyid(const char *s);

#endif /* _NCRYPT_CRYPT_H */
