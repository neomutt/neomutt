/* classes: h_files */

#ifndef RXPOSIXH
#define RXPOSIXH
/*	Copyright (C) 1995, 1996 Tom Lord
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */


#include "rxspencer.h"
#include "rxcontext.h"
#include "inst-rxposix.h"

#ifdef __STDC__
extern int regncomp (regex_t * preg, const char * pattern, int len, int cflags);
extern int regcomp (regex_t * preg, const char * pattern, int cflags);
extern size_t regerror (int errcode, const regex_t *preg,
			char *errbuf, size_t errbuf_size);
extern int rx_regmatch (regmatch_t pmatch[], const regex_t *preg, struct rx_context_rules * rules, int start, int end, const char *string);
extern int rx_regexec (regmatch_t pmatch[], const regex_t *preg, struct rx_context_rules * rules, int start, int end, const char *string);
extern int regnexec (const regex_t *preg, const char *string, int len, size_t nmatch, regmatch_t **pmatch, int eflags);
extern int regexec (const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
extern void regfree (regex_t *preg);

#else /* STDC */
extern int regncomp ();
extern int regcomp ();
extern size_t regerror ();
extern int rx_regmatch ();
extern int rx_regexec ();
extern int regnexec ();
extern int regexec ();
extern void regfree ();

#endif /* STDC */

#endif /* RXPOSIXH */
