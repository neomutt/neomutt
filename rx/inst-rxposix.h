/* classes: h_files */

#ifndef INST_RXPOSIXH
#define INST_RXPOSIXH
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



struct rx_posix_regex
{
  struct rexp_node * pattern;
  struct rexp_node ** subexps;
  size_t re_nsub;
  unsigned char * translate;
  unsigned int newline_anchor:1;/* If true, an anchor at a newline matches.*/
  unsigned int no_sub:1;	/* If set, don't  return register offsets. */
  unsigned int is_anchored:1;
  unsigned int is_nullable:1;
  unsigned char fastmap[256];
  void * owner_data;
};

typedef struct rx_posix_regex regex_t;

/* Type for byte offsets within the string.  POSIX mandates this.  */
typedef int regoff_t;

typedef struct rx_registers
{
  regoff_t rm_so; 		 /* Byte offset from string's start to substring's start.  */
  regoff_t rm_eo;  		/* Byte offset from string's start to substring's end.  */
  regoff_t final_tag;		/* data from the cut operator (only pmatch[0]) */
} regmatch_t;



/* If any error codes are removed, changed, or added, update the
 * `rx_error_msg' table.
 */
#define REG_NOERROR	0		/* Success.  */
#define REG_NOMATCH	1		/* Didn't find a match (for regexec).  */

/* POSIX regcomp return error codes.  
 * (In the order listed in the standard.)  
 */
#define REG_BADPAT	2		/* Invalid pattern.  */
#define REG_ECOLLATE	3		/* Not implemented.  */
#define REG_ECTYPE	4		/* Invalid character class name.  */
#define REG_EESCAPE	5		/* Trailing backslash.  */
#define REG_ESUBREG	6		/* Invalid back reference.  */
#define REG_EBRACK	7		/* Unmatched left bracket.  */
#define REG_EPAREN	8		/* Parenthesis imbalance.  */ 
#define REG_EBRACE	9		/* Unmatched \{.  */
#define REG_BADBR	10		/* Invalid contents of \{\}.  */
#define REG_ERANGE	11		/* Invalid range end.  */
#define REG_ESPACE	12		/* Ran out of memory.  */
#define REG_BADRPT	13		/* No preceding re for repetition op.  */

/* Error codes we've added.  
 */
#define REG_EEND	14		/* Premature end.  */
#define REG_ESIZE	15		/* Compiled pattern bigger than 2^16 bytes.  */
#define REG_ERPAREN	16		/* Unmatched ) or \); not returned from regcomp.  */


/*
 * POSIX `cflags' bits (i.e., information for `regcomp').
 */

/* If this bit is set, then use extended regular expression syntax.
 * If not set, then use basic regular expression syntax.  
 */
#define REG_EXTENDED 1

/* If this bit is set, then ignore case when matching.
 * If not set, then case is significant.
 */
#define REG_ICASE (REG_EXTENDED << 1)
 
/* If this bit is set, then anchors do not match at newline
 *   characters in the string.
 * If not set, then anchors do match at newlines.  
 */
#define REG_NEWLINE (REG_ICASE << 1)

/* If this bit is set, then report only success or fail in regexec.
 * If not set, then returns differ between not matching and errors.  
 */
#define REG_NOSUB (REG_NEWLINE << 1)


/*
 * POSIX `eflags' bits (i.e., information for regexec).  
 */

/* If this bit is set, then the beginning-of-line operator doesn't match
 *   the beginning of the string (presumably because it's not the
 *   beginning of a line).
 * If not set, then the beginning-of-line operator does match the
 *   beginning of the string.  
 */
#define REG_NOTBOL 1

/* Like REG_NOTBOL, except for the end-of-line.  
 */
#define REG_NOTEOL (REG_NOTBOL << 1)

/* For regnexec only.  Allocate register storage and return that. */
#define REG_ALLOC_REGS (REG_NOTEOL << 1)


#ifdef __STDC__
extern int regncomp (regex_t * preg,
		     const char * pattern, int len,
		     int cflags);
extern int regcomp (regex_t * preg, const char * pattern, int cflags);
extern size_t regerror (int errcode,
			const regex_t *preg,
			char *errbuf, size_t errbuf_size);
extern int regnexec (const regex_t *preg,
		     const char *string, int len,
		     size_t nmatch, regmatch_t **pmatch,
		     int eflags);
extern int regexec (const regex_t *preg,
		    const char *string,
		    size_t nmatch, regmatch_t pmatch[],
		    int eflags);
extern void regfree (regex_t *preg);

#else /* STDC */
extern int regncomp ();
extern int regcomp ();
extern size_t regerror ();
extern int regnexec ();
extern int regexec ();
extern void regfree ();

#endif /* STDC */
#endif  /* INST_RXPOSIXH */
