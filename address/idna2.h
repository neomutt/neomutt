/**
 * @file
 * Handling of international domain names
 *
 * @authors
 * Copyright (C) 2018-2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ADDRESS_IDNA2_H
#define MUTT_ADDRESS_IDNA2_H

#include <stdbool.h>
#include <stdint.h>

#define MI_NO_FLAGS                  0
#define MI_MAY_BE_IRREVERSIBLE (1 << 0)

char *      mutt_idna_intl_to_local(const char *user, const char *domain, uint8_t flags);
char *      mutt_idna_local_to_intl(const char *user, const char *domain);
const char *mutt_idna_print_version(void);
int         mutt_idna_to_ascii_lz  (const char *input, char **output, uint8_t flags);

#endif /* MUTT_ADDRESS_IDNA2_H */
