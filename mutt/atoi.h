/**
 * @file
 * Parse a number in a string
 *
 * @authors
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_LIB_ATOI_H
#define MUTT_LIB_ATOI_H

#include <stdbool.h>

const char *mutt_str_atoi  (const char *str, int *dst);
const char *mutt_str_atol  (const char *str, long *dst);
const char *mutt_str_atos  (const char *str, short *dst);
const char *mutt_str_atoui (const char *str, unsigned int *dst);
const char *mutt_str_atoul (const char *str, unsigned long *dst);
const char *mutt_str_atoull(const char *str, unsigned long long *dst);
const char *mutt_str_atous (const char *str, unsigned short *dst);

#define make_str_ato_wrappers(flavour, type) \
  static inline bool mutt_str_ato ## flavour ## _full(const char *src, type *dst) \
  { \
    const char * end = mutt_str_ato ## flavour(src, dst); \
    return end && !*end; \
  } \

make_str_ato_wrappers(i,   int)
make_str_ato_wrappers(l,   long)
make_str_ato_wrappers(s,   short)
make_str_ato_wrappers(ui,  unsigned int)
make_str_ato_wrappers(ul,  unsigned long)
make_str_ato_wrappers(ull, unsigned long long)
make_str_ato_wrappers(us,  unsigned short)

#endif /* MUTT_LIB_ATOI_H */
