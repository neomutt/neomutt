/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */ 

/* Check various types for their respective length, and for Endianness */

#include <stdio.h>

#define CHECK_TYPE(a,b)					\
	if (!have_UINT##a && (sizeof (b) == a)) 	\
	{						\
	  have_UINT##a = 1;				\
	  puts (" #define UINT" #a " " #b);		\
	}

#define CHECK_ENDIAN(TYPE)				\
	if (!have_endian && sizeof (TYPE) == 2)		\
	{						\
	  TYPE end_test = (TYPE) 0x1234;		\
	  ep = (char *) &end_test;			\
	  have_endian = 1;				\
	  if (*ep == 0x34)				\
	    puts (" #define M_LITTLE_ENDIAN");		\
	  else						\
	    puts (" #define M_BIG_ENDIAN");		\
	}
	

int main (int argc, char *argv[])
{
  short have_UINT2 = 0;
  short have_UINT4 = 0;
  short have_endian = 0;

  char *ep;
  
  puts ("/* This is a generated file.  Don't edit! */");
  puts ("#ifndef _TYPES_H");
  puts (" #define _TYPES_H");

  CHECK_TYPE (2, unsigned short int)
  CHECK_TYPE (2, unsigned int)
  CHECK_TYPE (4, unsigned short int)
  CHECK_TYPE (4, unsigned int)
  CHECK_TYPE (4, unsigned long int)

  CHECK_ENDIAN (unsigned short int)
  CHECK_ENDIAN (unsigned int)
	    
  puts ("#endif");
  
  if (!have_UINT2 || !have_UINT4)
  {
    fputs ("Can't determine integer types.  Please edit " __FILE__ ",\n", stderr);
    fputs ("and submit a patch to <mutt-dev@mutt.org>\n", stderr);
    return 1;
  }
  
  return 0;
}
