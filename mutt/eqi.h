/**
 * @file
 * Case-insensitive fixed-chunk comparisons
 *
 * @authors
 * Copyright (C) 2023 Steinar H. Gunderson <steinar+neomutt@gunderson.no>
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
 * @page mutt_eqi Case-insensitive fixed-chunk comparisons
 *
 * These functions are much faster for short strings than calling
 * mutt_istr_equal(), and are not affected by locale in any way. But you will
 * need to do length checking yourself, and the right-hand side (b) is assumed
 * to already be lowercased. It also is assumed to be constant, so that the
 * generated 0x20 mask (for lowercasing) will be generated compile-time.
 *
 * In general, you want the fewest possible comparison calls; on most
 * platforms, these will all generally be the same speed. So if you e.g. have
 * an 11-byte value, it's cheaper to call eqi8() and eqi4() with a one-byte
 * overlap than calling eqi8(), eqi2() and eqi1(). Similarly, if your value is
 * 8 bytes, you can ignore the fact that you know what the first byte is, and
 * do a full eqi8() compare to save time. There are helpers (e.g. eqi11()) that
 * can help with the former.
 */

#ifndef MUTT_MUTT_EQI_H
#define MUTT_MUTT_EQI_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * make_lowercase_mask - Create a mask to lowercase alphabetic characters
 * @param x       Character to test
 * @param bytenum Which byte position to set
 * @retval num Bitmask
 *
 * Utility for the functions below
 *
 * If the character is alphabetic, then the position determines where to shift
 * the 0x20 mask.
 * - Position 0: 0x00000020
 * - Position 1: 0x00002000
 * - Position 2: 0x00200000
 * - Position 3: 0x20000000
 */
static inline uint64_t make_lowercase_mask(uint64_t x, int bytenum)
{
  const char ch = x >> (bytenum * 8);
  const uint64_t mask = ((ch >= 'a') && (ch <= 'z')) ? 0x20 : 0;
  return mask << (bytenum * 8);
}

/**
 * eqi1 - Compare two 1-byte strings, ignoring case - See: @subpage mutt_eqi
 * @param a First string
 * @param b Second string, must be lower case
 * @retval true Strings match
 */
static inline bool eqi1(const char *a, const char b[1])
{
  uint8_t b8 = b[0];
  return (*a | make_lowercase_mask(b8, 0)) == (unsigned char) b[0];
}

/**
 * eqi2 - Compare two 2-byte strings, ignoring case - See: @subpage mutt_eqi
 * @param a First string
 * @param b Second string, must be lower case
 * @retval true Strings match
 */
static inline bool eqi2(const char *a, const char b[2])
{
  uint16_t a16, b16;
  memcpy(&a16, a, sizeof(a16));
  memcpy(&b16, b, sizeof(b16));
  const uint16_t lowercase_mask = make_lowercase_mask(b16, 0) |
                                  make_lowercase_mask(b16, 1);
  return (a16 | lowercase_mask) == b16;
}

/**
 * eqi4 - Compare two 4-byte strings, ignoring case - See: @subpage mutt_eqi
 * @param a First string
 * @param b Second string, must be lower case
 * @retval true Strings match
 */
static inline bool eqi4(const char *a, const char b[4])
{
  uint32_t a32, b32;
  memcpy(&a32, a, sizeof(a32));
  memcpy(&b32, b, sizeof(b32));
  const uint32_t lowercase_mask = make_lowercase_mask(b32, 0) |
                                  make_lowercase_mask(b32, 1) |
                                  make_lowercase_mask(b32, 2) |
                                  make_lowercase_mask(b32, 3);
  return (a32 | lowercase_mask) == b32;
}

/**
 * eqi8 - Compare two 8-byte strings, ignoring case - See: @subpage mutt_eqi
 * @param a First string
 * @param b Second string, must be lower case
 * @retval true Strings match
 */
static inline bool eqi8(const char *a, const char b[8])
{
  uint64_t a64, b64;
  memcpy(&a64, a, sizeof(a64));
  memcpy(&b64, b, sizeof(b64));
  const uint64_t lowercase_mask = make_lowercase_mask(b64, 0) |
                                  make_lowercase_mask(b64, 1) |
                                  make_lowercase_mask(b64, 2) |
                                  make_lowercase_mask(b64, 3) |
                                  make_lowercase_mask(b64, 4) |
                                  make_lowercase_mask(b64, 5) |
                                  make_lowercase_mask(b64, 6) |
                                  make_lowercase_mask(b64, 7);
  return (a64 | lowercase_mask) == b64;
}

/* various helpers for increased readability */

/* there's no eqi3(); consider using eqi4() instead if you can */

/// eqi5 - Compare two 5-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi5(const char *a, const char b[5])
{
  return eqi4(a, b) && eqi1(a + 1, b + 1);
}

/// eqi6 - Compare two 6-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi6(const char *a, const char b[6])
{
  return eqi4(a, b) && eqi2(a + 4, b + 4);
}

/* there's no eqi7(); consider using eqi8() instead if you can */

/// eqi9 - Compare two 9-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi9(const char *a, const char b[9])
{
  return eqi8(a, b) && eqi1(a + 8, b + 8);
}

/// eqi10 - Compare two 10-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi10(const char *a, const char b[10])
{
  return eqi8(a, b) && eqi2(a + 8, b + 8);
}

/// eqi11 - Compare two 11-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi11(const char *a, const char b[11])
{
  return eqi8(a, b) && eqi4(a + 7, b + 7);
}

/// eqi12 - Compare two 12-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi12(const char *a, const char b[12])
{
  return eqi8(a, b) && eqi4(a + 8, b + 8);
}

/// eqi13 - Compare two 13-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi13(const char *a, const char b[13])
{
  return eqi8(a, b) && eqi8(a + 5, b + 5);
}

/// eqi14 - Compare two 14-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi14(const char *a, const char b[14])
{
  return eqi8(a, b) && eqi8(a + 6, b + 6);
}

/// eqi15 - Compare two 15-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi15(const char *a, const char b[15])
{
  return eqi8(a, b) && eqi8(a + 7, b + 7);
}

/// eqi16 - Compare two 16-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi16(const char *a, const char b[16])
{
  return eqi8(a, b) && eqi8(a + 8, b + 8);
}

/// eqi17 - Compare two 17-byte strings, ignoring case - See: @subpage mutt_eqi
static inline bool eqi17(const char *a, const char b[17])
{
  return eqi16(a, b) && eqi1(a + 16, b + 16);
}

#endif /* MUTT_MUTT_EQI_H */
