/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA.
 */ 

/* 
 * Some simple dummies, so we can reuse the routines from
 * lib.c in external programs.
 */

#define WHERE
#define _EXTLIB_C

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include "lib.h"

void (*mutt_error) (const char *, ...) = mutt_nocurses_error;

void mutt_exit (int code)
{
  exit (code);
}

