/*	Copyright (C) 1992, 1993, 1994, 1995 Free Software Foundation, Inc.
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


#include <sys/types.h>
#include "rxall.h"
#include "rxgnucomp.h"
#include "inst-rxposix.h"


/* {A Syntax Table} 
 */

/* Define the syntax basics for \<, \>, etc.
 */

#ifndef emacs
#define CHARBITS 8
#define CHAR_SET_SIZE (1 << CHARBITS)
#define Sword 1
#define SYNTAX(c) re_syntax_table[c]
char re_syntax_table[CHAR_SET_SIZE];

#ifdef __STDC__
static void
init_syntax_once (void)
#else
static void
init_syntax_once ()
#endif
{
   register int c;
   static int done = 0;

   if (done)
     return;

   rx_bzero ((char *)re_syntax_table, sizeof re_syntax_table);

   for (c = 'a'; c <= 'z'; c++)
     re_syntax_table[c] = Sword;

   for (c = 'A'; c <= 'Z'; c++)
     re_syntax_table[c] = Sword;

   for (c = '0'; c <= '9'; c++)
     re_syntax_table[c] = Sword;

   re_syntax_table['_'] = Sword;

   done = 1;
}
#endif /* not emacs */





const char *rx_error_msg[] =
{
  0,						/* REG_NOUT */
  "No match",					/* REG_NOMATCH */
  "Invalid regular expression",			/* REG_BADPAT */
  "Invalid collation character",		/* REG_ECOLLATE */
  "Invalid character class name",		/* REG_ECTYPE */
  "Trailing backslash",				/* REG_EESCAPE */
  "Invalid back reference",			/* REG_ESUBREG */
  "Unmatched [ or [^",				/* REG_EBRACK */
  "Unmatched ( or \\(",				/* REG_EPAREN */
  "Unmatched \\{",				/* REG_EBRACE */
  "Invalid content of \\{\\}",			/* REG_BADBR */
  "Invalid range end",				/* REG_ERANGE */
  "Memory exhausted",				/* REG_ESPACE */
  "Invalid preceding regular expression",	/* REG_BADRPT */
  "Premature end of regular expression",	/* REG_EEND */
  "Regular expression too big",			/* REG_ESIZE */
  "Unmatched ) or \\)",				/* REG_ERPAREN */
};



/* 
 * Macros used while compiling patterns.
 *
 * By convention, PEND points just past the end of the uncompiled pattern,
 * P points to the read position in the pattern.  `translate' is the name
 * of the translation table (`TRANSLATE' is the name of a macro that looks
 * things up in `translate').
 */


/*
 * Fetch the next character in the uncompiled pattern---translating it 
 * if necessary. *Also cast from a signed character in the constant
 * string passed to us by the user to an unsigned char that we can use
 * as an array index (in, e.g., `translate').
 */
#define PATFETCH(c)							\
 do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++;						\
    c = translate[c];		 					\
 } while (0)

/* 
 * Fetch the next character in the uncompiled pattern, with no
 * translation.
 */
#define PATFETCH_RAW(c)							\
  do {if (p == pend) return REG_EEND;					\
    c = (unsigned char) *p++; 						\
  } while (0)

/* Go backwards one character in the pattern.  */
#define PATUNFETCH p--


#define TRANSLATE(d) translate[(unsigned char) (d)]

typedef int regnum_t;

/* Since offsets can go either forwards or backwards, this type needs to
 * be able to hold values from -(MAX_BUF_SIZE - 1) to MAX_BUF_SIZE - 1.
 */
typedef int pattern_offset_t;

typedef struct
{
  struct rexp_node ** top_expression;
  struct rexp_node ** last_expression;
  struct rexp_node ** last_non_regular_expression;
  pattern_offset_t inner_group_offset;
  regnum_t regnum;
} compile_stack_elt_t;

typedef struct
{
  compile_stack_elt_t *stack;
  unsigned size;
  unsigned avail;			/* Offset of next open position.  */
} compile_stack_type;


#define INIT_COMPILE_STACK_SIZE 32

#define COMPILE_STACK_EMPTY  (compile_stack.avail == 0)
#define COMPILE_STACK_FULL  (compile_stack.avail == compile_stack.size)

/* The next available element.  */
#define COMPILE_STACK_TOP (compile_stack.stack[compile_stack.avail])


/* Set the bit for character C in a list.  */
#define SET_LIST_BIT(c)                               \
  (b[((unsigned char) (c)) / CHARBITS]               \
   |= 1 << (((unsigned char) c) % CHARBITS))

/* Get the next unsigned number in the uncompiled pattern.  */
#define GET_UNSIGNED_NUMBER(num) 					\
  { if (p != pend)							\
     {									\
       PATFETCH (c); 							\
       while (isdigit (c)) 						\
         { 								\
           if (num < 0)							\
              num = 0;							\
           num = num * 10 + c - '0'; 					\
           if (p == pend) 						\
              break; 							\
           PATFETCH (c);						\
         } 								\
       } 								\
    }		

#define CHAR_CLASS_MAX_LENGTH  64

#define IS_CHAR_CLASS(string)						\
   (!strcmp (string, "alpha") || !strcmp (string, "upper")		\
    || !strcmp (string, "lower") || !strcmp (string, "digit")		\
    || !strcmp (string, "alnum") || !strcmp (string, "xdigit")		\
    || !strcmp (string, "space") || !strcmp (string, "print")		\
    || !strcmp (string, "punct") || !strcmp (string, "graph")		\
    || !strcmp (string, "cntrl") || !strcmp (string, "blank"))


/* These predicates are used in regex_compile. */

/* P points to just after a ^ in PATTERN.  Return true if that ^ comes
 * after an alternative or a begin-subexpression.  We assume there is at
 * least one character before the ^.  
 */

#ifdef __STDC__
static int
at_begline_loc_p (const char *pattern, const char * p, unsigned long syntax)
#else
static int
at_begline_loc_p (pattern, p, syntax)
     const char *pattern;
     const char * p;
     unsigned long syntax;
#endif
{
  const char *prev = p - 2;
  int prev_prev_backslash = ((prev > pattern) && (prev[-1] == '\\'));
  
    return
      
      (/* After a subexpression?  */
       ((*prev == '(') && ((syntax & RE_NO_BK_PARENS) || prev_prev_backslash))
       ||
       /* After an alternative?  */
       ((*prev == '|') && ((syntax & RE_NO_BK_VBAR) || prev_prev_backslash))
       );
}

/* The dual of at_begline_loc_p.  This one is for $.  We assume there is
 * at least one character after the $, i.e., `P < PEND'.
 */

#ifdef __STDC__
static int
at_endline_loc_p (const char *p, const char *pend, int syntax)
#else
static int
at_endline_loc_p (p, pend, syntax)
     const char *p;
     const char *pend;
     int syntax;
#endif
{
  const char *next = p;
  int next_backslash = (*next == '\\');
  const char *next_next = (p + 1 < pend) ? (p + 1) : 0;
  
  return
    (
     /* Before a subexpression?  */
     ((syntax & RE_NO_BK_PARENS)
      ? (*next == ')')
      : (next_backslash && next_next && (*next_next == ')')))
    ||
     /* Before an alternative?  */
     ((syntax & RE_NO_BK_VBAR)
      ? (*next == '|')
      : (next_backslash && next_next && (*next_next == '|')))
     );
}


unsigned char rx_id_translation[256] =
{
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,

 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
 120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
 130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
 180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
 190, 191, 192, 193, 194, 195, 196, 197, 198, 199,

 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
 240, 241, 242, 243, 244, 245, 246, 247, 248, 249,
 250, 251, 252, 253, 254, 255
};

/* The compiler keeps an inverted translation table.
 * This looks up/inititalize elements.
 * VALID is an array of booleans that validate CACHE.
 */

#ifdef __STDC__
static rx_Bitset
inverse_translation (int * n_members, int cset_size, char * valid, rx_Bitset cache, unsigned char * translate, int c)
#else
static rx_Bitset
inverse_translation (n_members, cset_size, valid, cache, translate, c)
     int * n_members;
     int cset_size;
     char * valid;
     rx_Bitset cache;
     unsigned char * translate;
     int c;
#endif
{
  rx_Bitset cs;
  cs = cache + c * rx_bitset_numb_subsets (cset_size); 

  if (!valid[c])
    {
      int x;
      int c_tr;
      int membs;

      c_tr = TRANSLATE(c);
      rx_bitset_null (cset_size, cs);
      membs = 0;
      for (x = 0; x < 256; ++x)
	if (TRANSLATE(x) == c_tr)
	  {
	    RX_bitset_enjoin (cs, x);
	    membs++;
	  }
      valid[c] = 1;
      n_members[c] = membs;
    }
  return cs;
}




/* More subroutine declarations and macros for regex_compile.  */

/* Returns true if REGNUM is in one of COMPILE_STACK's elements and 
 * false if it's not.
 */

#ifdef __STDC__
static int
group_in_compile_stack (compile_stack_type compile_stack, regnum_t regnum)
#else
static int
group_in_compile_stack (compile_stack, regnum)
    compile_stack_type compile_stack;
    regnum_t regnum;
#endif
{
  int this_element;

  for (this_element = compile_stack.avail - 1;  
       this_element >= 0; 
       this_element--)
    if (compile_stack.stack[this_element].regnum == regnum)
      return 1;

  return 0;
}


/*
 * Read the ending character of a range (in a bracket expression) from the
 * uncompiled pattern *P_PTR (which ends at PEND).  We assume the
 * starting character is in `P[-2]'.  (`P[-1]' is the character `-'.)
 * Then we set the translation of all bits between the starting and
 * ending characters (inclusive) in the compiled pattern B.
 * 
 * Return an error code.
 * 
 * We use these short variable names so we can use the same macros as
 * `regex_compile' itself.  
 */

#ifdef __STDC__
static int
compile_range (int * n_members, int cset_size, rx_Bitset cs, const char ** p_ptr, const char * pend, unsigned char * translate, unsigned long syntax, rx_Bitset inv_tr, char * valid_inv_tr)
#else
static int
compile_range (n_members, cset_size, cs, p_ptr, pend, translate, syntax, inv_tr, valid_inv_tr)
     int * n_members;
     int cset_size;
     rx_Bitset cs;
     const char ** p_ptr;
     const char * pend;
     unsigned char * translate;
     unsigned long syntax;
     rx_Bitset inv_tr;
     char * valid_inv_tr;
#endif
{
  unsigned this_char;

  const char *p = *p_ptr;

  unsigned char range_end;
  unsigned char range_start = TRANSLATE(p[-2]);

  if (p == pend)
    return REG_ERANGE;

  PATFETCH (range_end);

  (*p_ptr)++;

  if (range_start > range_end)
    return syntax & RE_NO_EMPTY_RANGES ? REG_ERANGE : REG_NOERROR;

  for (this_char = range_start; this_char <= range_end; this_char++)
    {
      rx_Bitset it =
	inverse_translation (n_members, cset_size, valid_inv_tr, inv_tr, translate, this_char);
      rx_bitset_union (cset_size, cs, it);
    }
  
  return REG_NOERROR;
}


#ifdef __STDC__
static int
pointless_if_repeated (struct rexp_node * node)
#else
static int
pointless_if_repeated (node)
     struct rexp_node * node;
#endif
{
  if (!node)
    return 1;
  switch (node->type)
    {
    case r_cset:
    case r_string:
    case r_cut:
      return 0;
    case r_concat:
    case r_alternate:
      return (pointless_if_repeated (node->params.pair.left)
	      && pointless_if_repeated (node->params.pair.right));
    case r_opt:
    case r_star:
    case r_interval:
    case r_parens:
      return pointless_if_repeated (node->params.pair.left);
    case r_context:
      switch (node->params.intval)
	{
	case '=':
	case '<':
	case '^':
	case 'b':
	case 'B':
	case '`':
	case '\'':
	  return 1;
	default:
	  return 0;
	}
    default:
      return 0;
    }
}



#ifdef __STDC__
static int
factor_string (struct rexp_node *** lastp, int cset_size)
#else
static int
factor_string (lastp, cset_size)
     struct rexp_node *** lastp;
     int cset_size;
#endif
{
  struct rexp_node ** expp;
  struct rexp_node * exp;
  rx_Bitset cs;
  struct rexp_node * cset_node;

  expp = *lastp;
  exp = *expp;			/* presumed r_string */

  cs = rx_cset (cset_size);
  if (!cs)
    return -1;
  RX_bitset_enjoin (cs, exp->params.cstr.contents[exp->params.cstr.len - 1]);
  cset_node = rx_mk_r_cset (r_cset, cset_size, cs);
  if (!cset_node)
    {
      rx_free_cset (cs);
      return -1;
    }
  if (exp->params.cstr.len == 1)
    {
      rx_free_rexp (exp);
      *expp = cset_node;
      /* lastp remains the same */
      return 0;
    }
  else
    {
      struct rexp_node * concat_node;
      exp->params.cstr.len--;
      concat_node = rx_mk_r_binop (r_concat, exp, cset_node);
      if (!concat_node)
	{
	  rx_free_rexp (cset_node);
	  return -1;
	}
      *expp = concat_node;
      *lastp = &concat_node->params.pair.right;
      return 0;
    }
}



#define isa_blank(C) (((C) == ' ') || ((C) == '\t'))

#ifdef __STDC__
int
rx_parse (struct rexp_node ** rexp_p,
	  const char *pattern,
	  int size,
	  unsigned long syntax,
	  int cset_size,
	  unsigned char *translate)
#else
int
rx_parse (rexp_p, pattern, size, syntax, cset_size, translate)
     struct rexp_node ** rexp_p;
     const char *pattern;
     int size;
     unsigned long syntax;
     int cset_size;
     unsigned char *translate;
#endif
{
  int compile_error;
  RX_subset
    inverse_translate [CHAR_SET_SIZE * rx_bitset_numb_subsets(CHAR_SET_SIZE)];
  char validate_inv_tr [CHAR_SET_SIZE];
  int n_members [CHAR_SET_SIZE];

  /* We fetch characters from PATTERN here.  Even though PATTERN is
   * `char *' (i.e., signed), we declare these variables as unsigned, so
   * they can be reliably used as array indices.  
   */
  register unsigned char c;
  register unsigned char c1;
  
  /* A random tempory spot in PATTERN.  */
  const char *p1;
  
  /* Keeps track of unclosed groups.  */
  compile_stack_type compile_stack;

  /* Points to the current (ending) position in the pattern.  */
  const char *p;
  const char *pend;
  
  /* When parsing is done, this will hold the expression tree. */
  struct rexp_node * rexp;

  /* This and top_expression are saved on the compile stack. */
  struct rexp_node ** top_expression;
  struct rexp_node ** last_non_regular_expression;
  struct rexp_node ** last_expression;
  
  /* Parameter to `goto append_node' */
  struct rexp_node * append;

  /* Counts open-groups as they are encountered.  This is the index of the
   * innermost group being compiled.
   */
  regnum_t regnum;

  /* True iff the sub-expression just started
   * is purely syntactic.  Otherwise, a regmatch data 
   * slot is allocated for the subexpression.
   */
  int syntax_only_parens;

  /* Place in the uncompiled pattern (i.e., the {) to
   * which to go back if the interval is invalid.  
   */
  const char *beg_interval;

  int side;



  if (!translate)
    translate = rx_id_translation;

  /* Points to the current (ending) position in the pattern.  */
  p = pattern;
  pend = pattern + size;
  
  /* When parsing is done, this will hold the expression tree. */
  rexp = 0;

  /* In the midst of compilation, this holds onto the regexp 
   * first parst while rexp goes on to aquire additional constructs.
   */
  top_expression = &rexp;
  last_non_regular_expression = top_expression;
  last_expression = top_expression;
  
  /* Counts open-groups as they are encountered.  This is the index of the
   * innermost group being compiled.
   */
  regnum = 0;

  rx_bzero ((char *)validate_inv_tr, sizeof (validate_inv_tr));


  /* Initialize the compile stack.  */
  compile_stack.stack =  (( compile_stack_elt_t *) malloc ((INIT_COMPILE_STACK_SIZE) * sizeof ( compile_stack_elt_t)));
  if (compile_stack.stack == 0)
    return REG_ESPACE;

  compile_stack.size = INIT_COMPILE_STACK_SIZE;
  compile_stack.avail = 0;

#if !defined (emacs) && !defined (SYNTAX_TABLE)
  /* Initialize the syntax table.  */
   init_syntax_once ();
#endif

  /* Loop through the uncompiled pattern until we're at the end.  */
  while (p != pend)
    {
      PATFETCH (c);

      switch (c)
        {
        case '^':
          {
            if (   /* If at start of pattern, it's an operator.  */
                   p == pattern + 1
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's come before.  */
                || at_begline_loc_p (pattern, p, syntax))
	      {
		struct rexp_node * n
		  = rx_mk_r_int (r_context, '^');
		if (!n)
		  goto space_error;
		append = n;
		goto append_node;
	      }
            else
              goto normal_char;
          }
          break;


        case '$':
          {
            if (   /* If at end of pattern, it's an operator.  */
                   p == pend 
                   /* If context independent, it's an operator.  */
                || syntax & RE_CONTEXT_INDEP_ANCHORS
                   /* Otherwise, depends on what's next.  */
                || at_endline_loc_p (p, pend, syntax))
	      {
		struct rexp_node * n
		  = rx_mk_r_int (r_context, '$');
		if (!n)
		  goto space_error;
		append = n;
		goto append_node;
	      }
             else
               goto normal_char;
           }
           break;


	case '+':
        case '?':
          if ((syntax & RE_BK_PLUS_QM)
              || (syntax & RE_LIMITED_OPS))
            goto normal_char;

        handle_plus:
        case '*':
          /* If there is no previous pattern... */
          if (pointless_if_repeated (*last_expression))
            {
              if (syntax & RE_CONTEXT_INVALID_OPS)
		{
		  compile_error = REG_BADRPT;
		  goto error_return;
		}
              else if (!(syntax & RE_CONTEXT_INDEP_OPS))
                goto normal_char;
            }

          {
            /* 1 means zero (many) matches is allowed.  */
            char zero_times_ok = 0, many_times_ok = 0;

            /* If there is a sequence of repetition chars, collapse it
               down to just one (the right one).  We can't combine
               interval operators with these because of, e.g., `a{2}*',
               which should only match an even number of `a's.  */

            for (;;)
              {
                zero_times_ok |= c != '+';
                many_times_ok |= c != '?';

                if (p == pend)
                  break;

                PATFETCH (c);

                if (c == '*'
                    || (!(syntax & RE_BK_PLUS_QM) && (c == '+' || c == '?')))
                  ;

                else if (syntax & RE_BK_PLUS_QM  &&  c == '\\')
                  {
                    if (p == pend)
		      {
			compile_error = REG_EESCAPE;
			goto error_return;
		      }

                    PATFETCH (c1);
                    if (!(c1 == '+' || c1 == '?'))
                      {
                        PATUNFETCH;
                        PATUNFETCH;
                        break;
                      }

                    c = c1;
                  }
                else
                  {
                    PATUNFETCH;
                    break;
                  }

                /* If we get here, we found another repeat character.  */
               }

	    /* Now we know whether or not zero matches is allowed
	     * and also whether or not two or more matches is allowed.
	     */

	    {
	      struct rexp_node * inner_exp;
	      struct rexp_node * star;

	      if (*last_expression && ((*last_expression)->type == r_string))
		if (factor_string (&last_expression, cset_size))
		  goto space_error;
	      inner_exp = *last_expression;
	      star = rx_mk_r_monop ((many_times_ok
				     ? (zero_times_ok ? r_star : r_plus)
				     : r_opt),
				    inner_exp);
	      if (!star)
		goto space_error;
	      *last_expression = star;
	    }
	  }
	  break;


	case '.':
	  {
	    rx_Bitset cs;
	    struct rexp_node * n;
	    cs = rx_cset (cset_size);
	    if (!cs)
	      goto space_error;
	    n = rx_mk_r_cset (r_cset, cset_size, cs);
	    if (!n)
	      {
		rx_free_cset (cs);
		goto space_error;
	      }
	    rx_bitset_universe (cset_size, cs);
	    if (!(syntax & RE_DOT_NEWLINE))
	      RX_bitset_remove (cs, '\n');
	    if (syntax & RE_DOT_NOT_NULL)
	      RX_bitset_remove (cs, 0);

	    append = n;
	    goto append_node;
	    break;
	  }


        case '[':
	  if (p == pend)
	    {
	      compile_error = REG_EBRACK;
	      goto error_return;
	    }
          {
            int had_char_class;
	    rx_Bitset cs;
	    struct rexp_node * node;
	    int is_inverted;

            had_char_class = 0;
	    is_inverted = *p == '^';
	    cs = rx_cset (cset_size);
	    if (!cs)
	      goto space_error;
	    node = rx_mk_r_cset (r_cset, cset_size ,cs);
	    if (!node)
	      {
		rx_free_cset (cs);
		goto space_error;
	      }
	    
	    /* This branch of the switch is normally exited with
	     *`goto append_node'
	     */
	    append = node;
	    
            if (is_inverted)
	      p++;
	    
            /* Remember the first position in the bracket expression.  */
            p1 = p;
	    
            /* Read in characters and ranges, setting map bits.  */
            for (;;)
              {
                if (p == pend)
		  {
		    compile_error = REG_EBRACK;
		    goto error_return;
		  }
		
                PATFETCH (c);
		
                /* \ might escape characters inside [...] and [^...].  */
                if ((syntax & RE_BACKSLASH_ESCAPE_IN_LISTS) && c == '\\')
                  {
                    if (p == pend)
		      {
			compile_error = REG_EESCAPE;
			goto error_return;
		      }
		    
                    PATFETCH (c1);
		    {
		      rx_Bitset it = inverse_translation (n_members,
							  cset_size,
							  validate_inv_tr,
							  inverse_translate,
							  translate,
							  c1);
		      rx_bitset_union (cset_size, cs, it);
		    }
                    continue;
                  }
		
                /* Could be the end of the bracket expression.  If it's
                   not (i.e., when the bracket expression is `[]' so
                   far), the ']' character bit gets set way below.  */
                if (c == ']' && p != p1 + 1)
                  goto finalize_class_and_append;
		
                /* Look ahead to see if it's a range when the last thing
                   was a character class.  */
                if (had_char_class && c == '-' && *p != ']')
                  {
		    compile_error = REG_ERANGE;
		    goto error_return;
		  }
		
                /* Look ahead to see if it's a range when the last thing
                   was a character: if this is a hyphen not at the
                   beginning or the end of a list, then it's the range
                   operator.  */
                if (c == '-' 
                    && !(p - 2 >= pattern && p[-2] == '[') 
                    && !(p - 3 >= pattern && p[-3] == '[' && p[-2] == '^')
                    && *p != ']')
                  {
                    int ret
                      = compile_range (n_members, cset_size, cs, &p, pend, translate, syntax,
				       inverse_translate, validate_inv_tr);
                    if (ret != REG_NOERROR)
		      {
			compile_error = ret;
			goto error_return;
		      }
                  }
		
                else if (p[0] == '-' && p[1] != ']')
                  { /* This handles ranges made up of characters only.  */
                    int ret;
		    
		    /* Move past the `-'.  */
                    PATFETCH (c1);
                    
                    ret = compile_range (n_members, cset_size, cs, &p, pend, translate, syntax,
					 inverse_translate, validate_inv_tr);
                    if (ret != REG_NOERROR)
		      {
			compile_error = ret;
			goto error_return;
		      }
                  }
		
                /* See if we're at the beginning of a possible character
                   class.  */
		
		else if ((syntax & RE_CHAR_CLASSES)
			 && (c == '[') && (*p == ':'))
                  {
                    char str[CHAR_CLASS_MAX_LENGTH + 1];
		    
                    PATFETCH (c);
                    c1 = 0;
		    
                    /* If pattern is `[[:'.  */
                    if (p == pend)
		      {
			compile_error = REG_EBRACK;
			goto error_return;
		      }
		    
                    for (;;)
                      {
                        PATFETCH (c);
                        if (c == ':' || c == ']' || p == pend
                            || c1 == CHAR_CLASS_MAX_LENGTH)
			  break;
                        str[c1++] = c;
                      }
                    str[c1] = '\0';
		    
                    /* If isn't a word bracketed by `[:' and:`]':
                       undo the ending character, the letters, and leave 
                       the leading `:' and `[' (but set bits for them).  */
                    if (c == ':' && *p == ']')
                      {
			if (!strncmp (str, "cut", 3))
			  {
			    int val;
			    if (1 != sscanf (str + 3, " %d", &val))
			      {
				compile_error = REG_ECTYPE;
				goto error_return;
			      }
			    /* Throw away the ]] */
			    PATFETCH (c);
			    PATFETCH (c);
			    {
			      struct rexp_node * cut;
			      cut = rx_mk_r_int (r_cut, val);
			      append = cut;
			      goto append_node;
			    }
			  }
			else if (!strncmp (str, "(", 1))
			  {
			    /* Throw away the ]] */
			    PATFETCH (c);
			    PATFETCH (c);
			    syntax_only_parens = 1;
			    goto handle_open;
			  }
			else if (!strncmp (str, ")", 1))
			  {
			    /* Throw away the ]] */
			    PATFETCH (c);
			    PATFETCH (c);
			    syntax_only_parens = 1;
			    goto handle_close;
			  }
			else
			  {
			    int ch;
			    int is_alnum = !strcmp (str, "alnum");
			    int is_alpha = !strcmp (str, "alpha");
			    int is_blank = !strcmp (str, "blank");
			    int is_cntrl = !strcmp (str, "cntrl");
			    int is_digit = !strcmp (str, "digit");
			    int is_graph = !strcmp (str, "graph");
			    int is_lower = !strcmp (str, "lower");
			    int is_print = !strcmp (str, "print");
			    int is_punct = !strcmp (str, "punct");
			    int is_space = !strcmp (str, "space");
			    int is_upper = !strcmp (str, "upper");
			    int is_xdigit = !strcmp (str, "xdigit");
                        
			    if (!IS_CHAR_CLASS (str))
			      {
				compile_error = REG_ECTYPE;
				goto error_return;
			      }
			
			    /* Throw away the ] at the end of the character
			       class.  */
			    PATFETCH (c);					
			
			    if (p == pend) { compile_error = REG_EBRACK; goto error_return; }
			
			    for (ch = 0; ch < 1 << CHARBITS; ch++)
			      {
				if (   (is_alnum  && isalnum (ch))
				    || (is_alpha  && isalpha (ch))
				    || (is_blank  && isa_blank (ch))
				    || (is_cntrl  && iscntrl (ch))
				    || (is_digit  && isdigit (ch))
				    || (is_graph  && isgraph (ch))
				    || (is_lower  && islower (ch))
				    || (is_print  && isprint (ch))
				    || (is_punct  && ispunct (ch))
				    || (is_space  && isspace (ch))
				    || (is_upper  && isupper (ch))
				    || (is_xdigit && isxdigit (ch)))
				  {
				    rx_Bitset it =
				      inverse_translation (n_members,
							   cset_size,
							   validate_inv_tr,
							   inverse_translate,
							   translate,
							   ch);
				    rx_bitset_union (cset_size,
						     cs, it);
				  }
			      }
			    had_char_class = 1;
			  }
		      }
                    else
                      {
                        c1++;
                        while (c1--)    
                          PATUNFETCH;
			{
			  rx_Bitset it =
			    inverse_translation (n_members,
						 cset_size,
						 validate_inv_tr,
						 inverse_translate,
						 translate,
						 '[');
			  rx_bitset_union (cset_size,
					   cs, it);
			}
			{
			  rx_Bitset it =
			    inverse_translation (n_members,
						 cset_size,
						 validate_inv_tr,
						 inverse_translate,
						 translate,
						 ':');
			  rx_bitset_union (cset_size,
					   cs, it);
			}
                        had_char_class = 0;
                      }
                  }
                else
                  {
                    had_char_class = 0;
		    {
		      rx_Bitset it = inverse_translation (n_members,
							  cset_size,
							  validate_inv_tr,
							  inverse_translate,
							  translate,
							  c);
		      rx_bitset_union (cset_size, cs, it);
		    }
                  }
              }

	  finalize_class_and_append:
	    if (is_inverted)
	      {
		rx_bitset_complement (cset_size, cs);
		if (syntax & RE_HAT_LISTS_NOT_NEWLINE)
		  RX_bitset_remove (cs, '\n');
	      }
	    goto append_node;
          }
          break;


	case '(':
          if (syntax & RE_NO_BK_PARENS)
	    {
	      syntax_only_parens = 0;
	      goto handle_open;
	    }
          else
            goto normal_char;


        case ')':
          if (syntax & RE_NO_BK_PARENS)
	    {
	      syntax_only_parens = 0;
	      goto handle_close;
	    }
          else
            goto normal_char;


        case '\n':
          if (syntax & RE_NEWLINE_ALT)
            goto handle_alt;
          else
            goto normal_char;


	case '|':
          if (syntax & RE_NO_BK_VBAR)
            goto handle_alt;
          else
            goto normal_char;


        case '{':
	  if ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
	    goto handle_interval;
	  else
	    goto normal_char;


        case '\\':
          if (p == pend) { compile_error = REG_EESCAPE; goto error_return; }

          /* Do not translate the character after the \, so that we can
             distinguish, e.g., \B from \b, even if we normally would
             translate, e.g., B to b.  */
          PATFETCH_RAW (c);

          switch (c)
            {
            case '(':
              if (syntax & RE_NO_BK_PARENS)
                goto normal_backslash;

	      syntax_only_parens = 0;

            handle_open:
	      if (!syntax_only_parens)
		regnum++;

              if (COMPILE_STACK_FULL)
                { 
                  compile_stack.stack
		    = ((compile_stack_elt_t *)
		       realloc (compile_stack.stack,
				(compile_stack.size << 1) * sizeof (compile_stack_elt_t)));
		  if (compile_stack.stack == 0)
		    goto space_error;
		  compile_stack.size <<= 1;
		}

	      if (*last_non_regular_expression)
		{
		  struct rexp_node * concat;
		  concat = rx_mk_r_binop (r_concat, *last_non_regular_expression, 0);
		  if (!concat)
		    goto space_error;
		  *last_non_regular_expression = concat;
		  last_non_regular_expression = &concat->params.pair.right;
		  last_expression = last_non_regular_expression;
		}

              /*
	       * These are the values to restore when we hit end of this
               * group.  
	       */
	      COMPILE_STACK_TOP.top_expression = top_expression;
	      COMPILE_STACK_TOP.last_expression = last_expression;
	      COMPILE_STACK_TOP.last_non_regular_expression = last_non_regular_expression;

	      if (syntax_only_parens)
		COMPILE_STACK_TOP.regnum = -1;
	      else
		COMPILE_STACK_TOP.regnum = regnum;
	      
              compile_stack.avail++;
	      
	      top_expression = last_non_regular_expression;
	      break;


            case ')':
              if (syntax & RE_NO_BK_PARENS) goto normal_backslash;
	      syntax_only_parens = 0;

            handle_close:
              /* See similar code for backslashed left paren above.  */
              if (COMPILE_STACK_EMPTY)
                if (syntax & RE_UNMATCHED_RIGHT_PAREN_ORD)
                  goto normal_char;
                else
                  { compile_error = REG_ERPAREN; goto error_return; }

              /* Since we just checked for an empty stack above, this
               * ``can't happen''. 
	       */

              {
                /* We don't just want to restore into `regnum', because
                 * later groups should continue to be numbered higher,
                 * as in `(ab)c(de)' -- the second group is #2.
		 */
                regnum_t this_group_regnum;
		struct rexp_node ** inner;
		struct rexp_node * parens;

		inner = top_expression;
                compile_stack.avail--;

		if (!!syntax_only_parens != (COMPILE_STACK_TOP.regnum == -1))
		  { compile_error = REG_ERPAREN; goto error_return; }

		top_expression = COMPILE_STACK_TOP.top_expression;
		last_expression = COMPILE_STACK_TOP.last_expression;
		last_non_regular_expression = COMPILE_STACK_TOP.last_non_regular_expression;
                this_group_regnum = COMPILE_STACK_TOP.regnum;

		{
		  parens = rx_mk_r_monop (r_parens, *inner);
		  if (!parens)
		    goto space_error;
		  parens->params.intval = this_group_regnum;
		  *inner = parens;
		  break;
		}
	      }

            case '|':					/* `\|'.  */
              if ((syntax & RE_LIMITED_OPS) || (syntax & RE_NO_BK_VBAR))
                goto normal_backslash;
            handle_alt:
              if (syntax & RE_LIMITED_OPS)
                goto normal_char;

	      {
		struct rexp_node * alt;

		alt = rx_mk_r_binop (r_alternate, *top_expression, 0);
		if (!alt)
		  goto space_error;
		*top_expression = alt;
		last_expression = &alt->params.pair.right;
		last_non_regular_expression = &alt->params.pair.right;
	      }
              break;


            case '{': 
              /* If \{ is a literal.  */
              if (!(syntax & RE_INTERVALS)
                     /* If we're at `\{' and it's not the open-interval 
                        operator.  */
                  || ((syntax & RE_INTERVALS) && (syntax & RE_NO_BK_BRACES))
                  || (p - 2 == pattern  &&  p == pend))
                goto normal_backslash;

            handle_interval:
              {
                /* If got here, then the syntax allows intervals. 
		 */

                /* At least (most) this many matches must be made.  
		 */
                int lower_bound;
		int upper_bound;

		lower_bound = -1;
		upper_bound = -1;

		/* We're about to parse the bounds of the interval.
		 * It may turn out that this isn't an interval after
		 * all, in which case these same characters will have
		 * to be reparsed as literals.   This remembers
		 * the backtrack point in the parse:
		 */
                beg_interval = p - 1;

                if (p == pend)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else
                      { compile_error = REG_EBRACE; goto error_return; }
                  }

                GET_UNSIGNED_NUMBER (lower_bound);

                if (c == ',')
                  {
                    GET_UNSIGNED_NUMBER (upper_bound);
                    if (upper_bound < 0) upper_bound = RE_DUP_MAX;
                  }
                else
                  /* Interval such as `{n}' => match exactly n times.
		   */
                  upper_bound = lower_bound;

                if (lower_bound < 0
		    || upper_bound > RE_DUP_MAX
                    || lower_bound > upper_bound)
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else 
                      { compile_error = REG_BADBR; goto error_return; }
                  }

                if (!(syntax & RE_NO_BK_BRACES)) 
                  {
                    if (c != '\\') { compile_error = REG_EBRACE; goto error_return; }
                    PATFETCH (c);
                  }

                if (c != '}')
                  {
                    if (syntax & RE_NO_BK_BRACES)
                      goto unfetch_interval;
                    else 
                      { compile_error = REG_BADBR; goto error_return; }
                  }

                /* We just parsed a valid interval.
		 * lower_bound and upper_bound are set.
		 */

                /* If it's invalid to have no preceding re.
		 */
                if (pointless_if_repeated (*last_expression))
                  {
                    if (syntax & RE_CONTEXT_INVALID_OPS)
                      { compile_error = REG_BADRPT; goto error_return; }
                    else if (!(syntax & RE_CONTEXT_INDEP_OPS))
		      /* treat the interval spec as literal chars. */
                      goto unfetch_interval; 
                  }

		{
		  struct rexp_node * interval;

		  if (*last_expression && ((*last_expression)->type == r_string))
		    if (factor_string (&last_expression, cset_size))
		      goto space_error;
		  interval = rx_mk_r_monop (r_interval, *last_expression);
		  if (!interval)
		    goto space_error;
		  interval->params.intval = lower_bound;
		  interval->params.intval2 = upper_bound;
		  *last_expression = interval;
		  last_non_regular_expression = last_expression;
		}
                beg_interval = 0;
              }
              break;

            unfetch_interval:
              /* If an invalid interval, match the characters as literals.  */
               p = beg_interval;
               beg_interval = 0;

               /* normal_char and normal_backslash need `c'.  */
               PATFETCH (c);	

               if (!(syntax & RE_NO_BK_BRACES))
                 {
                   if ((p > pattern)  &&  (p[-1] == '\\'))
                     goto normal_backslash;
                 }
               goto normal_char;

#ifdef emacs
            /* There is no way to specify the before_dot and after_dot
             * operators.  rms says this is ok.  --karl
	     */
            case '=':
	      side = '=';
	      goto add_side_effect;
              break;

            case 's':
	    case 'S':
	      {
		rx_Bitset cs;
		struct rexp_node * set;

		cs = rx_cset (&cset_size);
		if (!cs)
		  goto space_error;
		set = rx_mk_r_cset (r_cset, cset_size, cs);
		if (!set)
		  {
		    rx_free_cset (cs);
		    goto space_error;
		  }
		if (c == 'S')
		  rx_bitset_universe (cset_size, cs);

		PATFETCH (c);
		{
		  int x;
		  enum syntaxcode code = syntax_spec_code [c];
		  for (x = 0; x < 256; ++x)
		    {
		      
		      if (SYNTAX (x) == code)
			{
			  rx_Bitset it =
			    inverse_translation (n_members,
						 cset_size, validate_inv_tr,
						 inverse_translate,
						 translate, x);
			  rx_bitset_xor (cset_size, cs, it);
			}
		    }
		}
		append = set;
		goto append_node;
	      }
              break;
#endif /* emacs */


            case 'w':
            case 'W':
	      {
		rx_Bitset cs;
		struct rexp_node * n;

		cs = rx_cset (cset_size);
		n = (cs ? rx_mk_r_cset (r_cset, cset_size, cs) : 0);
		if (!(cs && n))
		  {
		    if (cs)
		      rx_free_cset (cs);
		    goto space_error;
		  }
		if (c == 'W')
		  rx_bitset_universe (cset_size ,cs);
		{
		  int x;
		  for (x = cset_size - 1; x > 0; --x)
		    if (SYNTAX(x) & Sword)
		      RX_bitset_toggle (cs, x);
		}
		append = n;
		goto append_node;
	      }
              break;

            case '<':
	      side = '<';
	      goto add_side_effect;
              break;

            case '>':
              side = '>';
	      goto add_side_effect;
              break;

            case 'b':
              side = 'b';
	      goto add_side_effect;
              break;

            case 'B':
              side = 'B';
	      goto add_side_effect;
              break;

            case '`':
	      side = '`';
	      goto add_side_effect;
	      break;
	      
            case '\'':
	      side = '\'';
	      goto add_side_effect;
              break;

	    add_side_effect:
	      {
		struct rexp_node * se;
		se = rx_mk_r_int (r_context, side);
		if (!se)
		  goto space_error;
		append = se;
		goto append_node;
	      }
	      break;

            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9':
              if (syntax & RE_NO_BK_REFS)
                goto normal_char;

              c1 = c - '0';

              /* Can't back reference to a subexpression if inside of it.  */
              if (group_in_compile_stack (compile_stack, c1))
                goto normal_char;

              if (c1 > regnum)
                { compile_error = REG_ESUBREG; goto error_return; }

	      side = c;
	      goto add_side_effect;
              break;

            case '+':
            case '?':
              if (syntax & RE_BK_PLUS_QM)
                goto handle_plus;
              else
                goto normal_backslash;

            default:
            normal_backslash:
              /* You might think it would be useful for \ to mean
               * not to translate; but if we don't translate it
               * it will never match anything.
	       */
              c = TRANSLATE (c);
              goto normal_char;
            }
          break;


	default:
        /* Expects the character in `c'.  */
	normal_char:
	    {
	      rx_Bitset cs;
	      struct rexp_node * match;
	      rx_Bitset it;

	      it = inverse_translation (n_members,
					cset_size, validate_inv_tr,
					inverse_translate, translate, c);

	      if (1 != n_members[c])
		{
		  cs = rx_cset (cset_size);
		  match = (cs ? rx_mk_r_cset (r_cset, cset_size, cs) : 0);
		  if (!(cs && match))
		    {
		      if (cs)
			rx_free_cset (cs);
		      goto space_error;
		    }
		  rx_bitset_union (CHAR_SET_SIZE, cs, it);
		  append = match;
		  goto append_node;
		}
	      else
		{
		  if (*last_expression && (*last_expression)->type == r_string)
		    {		
		      if (rx_adjoin_string (&((*last_expression)->params.cstr), c))
			goto space_error;
		      break;
		    }
		  else
		    {
		      append = rx_mk_r_str (r_string, c);
		      if(!append)
			goto space_error;
		      goto append_node;
		    }
		}
	      break;

	    append_node:
	      /* This genericly appends the rexp APPEND to *LAST_EXPRESSION
	       * and then parses the next character normally.
	       */
	      if (RX_regular_node_type (append->type))
		{
		  if (!*last_expression)
		    *last_expression = append;
		  else
		    {
		      struct rexp_node * concat;
		      concat = rx_mk_r_binop (r_concat,
					      *last_expression, append);
		      if (!concat)
			goto space_error;
		      *last_expression = concat;
		      last_expression = &concat->params.pair.right;
		    }
		}
	      else
		{
		  if (!*last_non_regular_expression)
		    {
		      *last_non_regular_expression = append;
		      last_expression = last_non_regular_expression;
		    }
		  else
		    {
		      struct rexp_node * concat;
		      concat = rx_mk_r_binop (r_concat,
					      *last_non_regular_expression, append);
		      if (!concat)
			goto space_error;
		      *last_non_regular_expression = concat;
		      last_non_regular_expression = &concat->params.pair.right;
		      last_expression = last_non_regular_expression;
		    }
		}
	    }
	} /* switch (c) */
    } /* while p != pend */

  
  /* Through the pattern now.  */

  if (!COMPILE_STACK_EMPTY) 
    { compile_error = REG_EPAREN; goto error_return; }
  free (compile_stack.stack);


  *rexp_p = rexp;
  return REG_NOERROR;

 space_error:
  compile_error = REG_ESPACE;

 error_return:
  free (compile_stack.stack);
  /* Free expressions pushed onto the compile stack! */
  if (rexp)
    rx_free_rexp (rexp);
  return compile_error;
}


