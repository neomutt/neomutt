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



#include "rxall.h"
#include "rxposix.h"
#include "rxgnucomp.h"
#include "rxbasic.h"
#include "rxsimp.h"

/* regcomp takes a regular expression as a string and compiles it.
 *
 * PATTERN is the address of the pattern string.
 *
 * CFLAGS is a series of bits which affect compilation.
 *
 *   If REG_EXTENDED is set, we use POSIX extended syntax; otherwise, we
 *   use POSIX basic syntax.
 *
 *   If REG_NEWLINE is set, then . and [^...] don't match newline.
 *   Also, regexec will try a match beginning after every newline.
 *
 *   If REG_ICASE is set, then we considers upper- and lowercase
 *   versions of letters to be equivalent when matching.
 *
 *   If REG_NOSUB is set, then when PREG is passed to regexec, that
 *   routine will report only success or failure, and nothing about the
 *   registers.
 *
 * It returns 0 if it succeeds, nonzero if it doesn't.  (See regex.h for
 * the return codes and their meanings.)  
 */


#ifdef __STDC__
int
regncomp (regex_t * preg, const char * pattern, int len, int cflags)
#else
int
regncomp (preg, pattern, len, cflags)
     regex_t * preg;
     const char * pattern;
     int len;
     int cflags;
#endif
{
  int ret;
  unsigned int syntax;

  rx_bzero ((char *)preg, sizeof (*preg));
  syntax = ((cflags & REG_EXTENDED)
	    ? RE_SYNTAX_POSIX_EXTENDED
	    : RE_SYNTAX_POSIX_BASIC);

  if (!(cflags & REG_ICASE))
    preg->translate = 0;
  else
    {
      unsigned i;

      preg->translate = (unsigned char *) malloc (256);
      if (!preg->translate)
        return (int) REG_ESPACE;

      /* Map uppercase characters to corresponding lowercase ones.  */
      for (i = 0; i < CHAR_SET_SIZE; i++)
        preg->translate[i] = isupper (i) ? tolower (i) : i;
    }


  /* If REG_NEWLINE is set, newlines are treated differently.  */
  if (!(cflags & REG_NEWLINE))
    preg->newline_anchor = 0;
  else
    {
      /* REG_NEWLINE implies neither . nor [^...] match newline.  */
      syntax &= ~RE_DOT_NEWLINE;
      syntax |= RE_HAT_LISTS_NOT_NEWLINE;
      /* It also changes the matching behavior.  */
      preg->newline_anchor = 1;
    }

  preg->no_sub = !!(cflags & REG_NOSUB);

  ret = rx_parse (&preg->pattern,
		  pattern, len,
		  syntax,
		  256,
		  preg->translate);

  /* POSIX doesn't distinguish between an unmatched open-group and an
   * unmatched close-group: both are REG_EPAREN.
   */
  if (ret == REG_ERPAREN)
    ret = REG_EPAREN;

  if (!ret)
    {
      preg->re_nsub = 1;
      preg->subexps = 0;
      rx_posix_analyze_rexp (&preg->subexps,
			     &preg->re_nsub,
			     preg->pattern,
			     0);
      preg->is_nullable = rx_fill_in_fastmap (256,
					      preg->fastmap,
					      preg->pattern);

      preg->is_anchored = rx_is_anchored_p (preg->pattern);
    }

  return (int) ret;
}


#ifdef __STDC__
int
regcomp (regex_t * preg, const char * pattern, int cflags)
#else
int
regcomp (preg, pattern, cflags)
     regex_t * preg;
     const char * pattern;
     int cflags;
#endif
{
  /* POSIX says a null character in the pattern terminates it, so we
   * can use strlen here in compiling the pattern.  
   */

  return regncomp (preg, pattern, strlen (pattern), cflags);
}




/* Returns a message corresponding to an error code, ERRCODE, returned
   from either regcomp or regexec.   */

#ifdef __STDC__
size_t
regerror (int errcode, const regex_t *preg,
	  char *errbuf, size_t errbuf_size)
#else
size_t
regerror (errcode, preg, errbuf, errbuf_size)
    int errcode;
    const regex_t *preg;
    char *errbuf;
    size_t errbuf_size;
#endif
{
  const char *msg;
  size_t msg_size;

  msg = rx_error_msg[errcode] == 0 ? "Success" : rx_error_msg[errcode];
  msg_size = strlen (msg) + 1; /* Includes the 0.  */
  if (errbuf_size != 0)
    {
      if (msg_size > errbuf_size)
        {
          strncpy (errbuf, msg, errbuf_size - 1);
          errbuf[errbuf_size - 1] = 0;
        }
      else
        strcpy (errbuf, msg);
    }
  return msg_size;
}



#ifdef __STDC__
int
rx_regmatch (regmatch_t pmatch[], const regex_t *preg, struct rx_context_rules * rules, int start, int end, const char *string)
#else
int
rx_regmatch (pmatch, preg, rules, start, end, string)
     regmatch_t pmatch[];
     const regex_t *preg;
     struct rx_context_rules * rules;
     int start;
     int end;
     const char *string;
#endif
{
  struct rx_solutions * solutions;
  enum rx_answers answer;
  struct rx_context_rules local_rules;
  int orig_end;
  int end_lower_bound;
  int end_upper_bound;
  
  local_rules = *rules;
  orig_end = end;

  if (!preg->pattern)
    {
      end_lower_bound = start;
      end_upper_bound = start;
    }
  else if (preg->pattern->len >= 0)
    {
      end_lower_bound = start + preg->pattern->len;
      end_upper_bound = start + preg->pattern->len;
    }
  else
    {
      end_lower_bound = start;
      end_upper_bound = end;
    }
  end = end_upper_bound;
  while (end >= end_lower_bound)
    {
      local_rules.not_eol = (rules->not_eol
			     ? (   (end == orig_end)
				|| !local_rules.newline_anchor
				|| (string[end] != '\n'))
			     : (   (end != orig_end)
				&& (!local_rules.newline_anchor
				    || (string[end] != '\n'))));
      solutions = rx_basic_make_solutions (pmatch, preg->pattern, preg->subexps,
					   start, end, &local_rules, string);
      if (!solutions)
	return REG_ESPACE;
      
      answer = rx_next_solution (solutions);

      if (answer == rx_yes)
	{
	  if (pmatch)
	    {
	      pmatch[0].rm_so = start;
	      pmatch[0].rm_eo = end;
	      pmatch[0].final_tag = solutions->final_tag;
	    }
	  rx_basic_free_solutions (solutions);
	  return 0;
	}
      else
	rx_basic_free_solutions (solutions);

      --end;
    }

  switch (answer)
    {
    default:
    case rx_bogus:
      return REG_ESPACE;

    case rx_no:
      return REG_NOMATCH;
    }
}


#ifdef __STDC__
int
rx_regexec (regmatch_t pmatch[], const regex_t *preg, struct rx_context_rules * rules, int start, int end, const char *string)
#else
int
rx_regexec (pmatch, preg, rules, start, end, string)
     regmatch_t pmatch[];
     const regex_t *preg;
     struct rx_context_rules * rules;
     int start;
     int end;
     const char *string;
#endif
{
  int x;
  int stat;
  int anchored;
  struct rexp_node * simplified;
  struct rx_unfa * unfa;
  struct rx_classical_system machine;

  anchored = preg->is_anchored;

  unfa = 0;
  if ((end - start) > RX_MANY_CASES)
    {
      if (0 > rx_simple_rexp (&simplified, 256, preg->pattern, preg->subexps))
	return REG_ESPACE;
      unfa = rx_unfa (rx_basic_unfaniverse (), simplified, 256);
      if (!unfa)
	{
	  rx_free_rexp (simplified);
	  return REG_ESPACE;
	}
      rx_init_system (&machine, unfa->nfa);
      rx_free_rexp (simplified);
    }

  for (x = start; x <= end; ++x)
    {
      if (preg->is_nullable
	  || ((x < end)
	      && (preg->fastmap[((unsigned char *)string)[x]])))
	{
	  if ((end - start) > RX_MANY_CASES)
	    {
	      int amt;
	      if (rx_start_superstate (&machine) != rx_yes)
		{
		  rx_free_unfa (unfa);
		  return REG_ESPACE;
		}
	      amt = rx_advance_to_final (&machine, string + x, end - start - x);
	      if (!machine.final_tag && (amt < (end - start - x)))
		goto nomatch;
	    }
	  stat = rx_regmatch (pmatch, preg, rules, x, end, string);
	  if (!stat || (stat != REG_NOMATCH))
	    {
	      rx_free_unfa (unfa);
	      return stat;
	    }
	}
    nomatch:
      if (anchored)
	if (!preg->newline_anchor)
	  {
	    rx_free_unfa (unfa);
	    return REG_NOMATCH;
	  }
	else
	  while (x < end)
	    if (string[x] == '\n')
	      break;
	    else
	      ++x;
    }
  rx_free_unfa (unfa);
  return REG_NOMATCH;
}



/* regexec searches for a given pattern, specified by PREG, in the
 * string STRING.
 *
 * If NMATCH is zero or REG_NOSUB was set in the cflags argument to
 * `regcomp', we ignore PMATCH.  Otherwise, we assume PMATCH has at
 * least NMATCH elements, and we set them to the offsets of the
 * corresponding matched substrings.
 *
 * EFLAGS specifies `execution flags' which affect matching: if
 * REG_NOTBOL is set, then ^ does not match at the beginning of the
 * string; if REG_NOTEOL is set, then $ does not match at the end.
 *
 * We return 0 if we find a match and REG_NOMATCH if not.  
 */

#ifdef __STDC__
int
regnexec (const regex_t *preg, const char *string, int len, size_t nmatch, regmatch_t **pmatch, int eflags)
#else
int
regnexec (preg, string, len, nmatch, pmatch, eflags)
     const regex_t *preg;
     const char *string;
     int len;
     size_t nmatch;
     regmatch_t **pmatch;
     int eflags;
#endif
{
  int want_reg_info;
  struct rx_context_rules rules;
  regmatch_t * regs;
  size_t nregs;
  int stat;

  want_reg_info = (!preg->no_sub && (nmatch > 0));

  rules.newline_anchor = preg->newline_anchor;
  rules.not_bol = !!(eflags & REG_NOTBOL);
  rules.not_eol = !!(eflags & REG_NOTEOL);
  rules.case_indep = !!(eflags & REG_ICASE);

  if (nmatch >= preg->re_nsub)
    {
      regs = *pmatch;
      nregs = nmatch;
    }
  else
    {
      regs = (regmatch_t *)malloc (preg->re_nsub * sizeof (*regs));
      if (!regs)
	return REG_ESPACE;
      nregs = preg->re_nsub;
    }

  {
    int x;
    for (x = 0; x < nregs; ++x)
      regs[x].rm_so = regs[x].rm_eo = -1;
  }


  stat = rx_regexec (regs, preg, &rules, 0, len, string);

  if (!stat && want_reg_info && pmatch && (regs != *pmatch))
    {
      size_t x;
      for (x = 0; x < nmatch; ++x)
	(*pmatch)[x] = regs[x];
    }

  if (!stat && (eflags & REG_ALLOC_REGS))
    *pmatch = regs;
  else if (regs && (!pmatch || (regs != *pmatch)))
    free (regs);
  
  return stat;
}

#ifdef __STDC__
int
regexec (const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
#else
int
regexec (preg, string, nmatch, pmatch, eflags)
     const regex_t *preg;
     const char *string;
     size_t nmatch;
     regmatch_t pmatch[];
     int eflags;
#endif
{
  return regnexec (preg,
		   string,
		   strlen (string),
		   nmatch,
		   &pmatch,
		   (eflags & ~REG_ALLOC_REGS));
}


/* Free dynamically allocated space used by PREG.  */

#ifdef __STDC__
void
regfree (regex_t *preg)
#else
void
regfree (preg)
    regex_t *preg;
#endif
{
  if (preg->pattern)
    {
      rx_free_rexp (preg->pattern);
      preg->pattern = 0;
    }
  if (preg->subexps)
    {
      free (preg->subexps);
      preg->subexps = 0;
    }
  if (preg->translate != 0)
    {
      free (preg->translate);
      preg->translate = 0;
    }
}
