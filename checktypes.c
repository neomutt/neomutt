/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@guug.de>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* Check various types for their respective length */

#include <stdio.h>

#define CHECK_TYPE(a,b)					\
	if (!have_UINT##a && (sizeof (b) == a)) 	\
	{						\
	  have_UINT##a = 1;				\
	  puts (" #define UINT" #a " " #b);		\
	}

int main (int argc, char *argv[])
{
  short have_UINT2 = 0;
  short have_UINT4 = 0;
  
  puts ("/* This is a generated file.  Don't edit! */");
  puts ("#ifndef _TYPES_H");
  puts (" #define _TYPES_H");

  CHECK_TYPE (2, unsigned short int)
  CHECK_TYPE (2, unsigned int)
  CHECK_TYPE (4, unsigned short int)
  CHECK_TYPE (4, unsigned int)
  CHECK_TYPE (4, unsigned long int)
  
  puts ("#endif");
  
  if (!have_UINT2 || !have_UINT4)
  {
    fputs ("Can't determine integer types.  Please edit " __FILE__ ",\n", stderr);
    fputs ("and submit a patch to <mutt-dev@mutt.org>\n", stderr);
    return 1;
  }
  
  return 0;
}
