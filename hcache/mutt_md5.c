/**
 * @file
 * Calculate the MD5 checksum of a buffer
 *
 * @authors
 * Copyright (C) 1995,1996,1997,1999,2000,2001,2005,2006,2008 Free Software Foundation, Inc.
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
 *
 * md5.c - Functions to compute MD5 message digest of files or memory blocks
 * according to the definition of MD5 in RFC1321 from April 1992.
 *
 * NOTE: The canonical source of this file is maintained with the GNU C
 * Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.
 */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.  */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "lib/md5.h"

/* local md5 equivalent for header cache versioning */
int main(void)
{
  unsigned char r[16];
  int rc;

  if ((rc = md5_stream(stdin, r)))
    return rc;

  printf("%02x%02x%02x%02x%02x%02x%02x%02x"
         "%02x%02x%02x%02x%02x%02x%02x%02x\n",
         r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10],
         r[11], r[12], r[13], r[14], r[15]);

  return 0;
}

