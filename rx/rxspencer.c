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



#include <stdio.h>
#include "rxall.h"
#include "rxspencer.h"
#include "rxsimp.h"



static char * silly_hack_2 = 0;

struct rx_solutions rx_no_solutions;

#ifdef __STDC__
struct rx_solutions *
rx_make_solutions (struct rx_registers * regs, struct rx_unfaniverse * verse, struct rexp_node * expression, struct rexp_node ** subexps, int cset_size, int start, int end, rx_vmfn vmfn, rx_contextfn contextfn, void * closure)
#else
struct rx_solutions *
rx_make_solutions (regs, verse, expression, subexps, cset_size, 
		   start, end, vmfn, contextfn, closure)
     struct rx_registers * regs;
     struct rx_unfaniverse * verse;
     struct rexp_node * expression;
     struct rexp_node ** subexps;
     int cset_size;
     int start;
     int end;
     rx_vmfn vmfn;
     rx_contextfn contextfn;
     void * closure;
#endif
{
  struct rx_solutions * solns;

  if (   expression
      && (expression->len >= 0)
      && (expression->len != (end - start)))
    return &rx_no_solutions;

  if (silly_hack_2)
    {
      solns = (struct rx_solutions *)silly_hack_2;
      silly_hack_2 = 0;
    }
  else
    solns = (struct rx_solutions *)malloc (sizeof (*solns));
  if (!solns)
    return 0;
  rx_bzero ((char *)solns, sizeof (*solns));

  solns->step = 0;
  solns->cset_size = cset_size;
  solns->subexps = subexps;
  solns->exp = expression;
  rx_save_rexp (expression);
  solns->verse = verse;
  solns->regs = regs;
  solns->start = start;
  solns->end = end;
  solns->vmfn = vmfn;
  solns->contextfn = contextfn;
  solns->closure = closure;

  if (!solns->exp || !solns->exp->observed)
    {
      solns->dfa = rx_unfa (verse, expression, cset_size);
      if (!solns->dfa)
	goto err_return;
      rx_init_system (&solns->match_engine, solns->dfa->nfa);

      if (rx_yes != rx_start_superstate (&solns->match_engine))
	goto err_return;
    }
  else
    {
      struct rexp_node * simplified;
      int status;
      status = rx_simple_rexp (&simplified, cset_size, solns->exp, subexps);
      if (status)
	goto err_return;
      solns->dfa = rx_unfa (verse, simplified, cset_size);
      if (!solns->dfa)
	{
	  rx_free_rexp (simplified);
	  goto err_return;
	}
      rx_init_system (&solns->match_engine, solns->dfa->nfa);
      if (rx_yes != rx_start_superstate (&solns->match_engine))
	goto err_return;
      rx_free_rexp (simplified);
    }

  if (expression && (   (expression->type == r_concat)
		     || (expression->type == r_plus)
		     || (expression->type == r_star)
		     || (expression->type == r_interval)))
    {
      struct rexp_node * subexp;

      subexp = solns->exp->params.pair.left;

      if (!subexp || !subexp->observed)
	{
	  solns->left_dfa = rx_unfa (solns->verse, subexp, solns->cset_size);
	}
      else
	{
	  struct rexp_node * simplified;
	  int status;
	  status = rx_simple_rexp (&simplified, solns->cset_size, subexp, solns->subexps);
	  if (status)
	    goto err_return;
	  solns->left_dfa = rx_unfa (solns->verse, simplified, solns->cset_size);
	  rx_free_rexp (simplified);
	}
      
      if (!solns->left_dfa)
	goto err_return;
      rx_bzero ((char *)&solns->left_match_engine, sizeof (solns->left_match_engine));
      rx_init_system (&solns->left_match_engine, solns->left_dfa->nfa);
    }
  
  return solns;

 err_return:
  rx_free_rexp (solns->exp);
  free (solns);
  return 0;
}



#ifdef __STDC__
void
rx_free_solutions (struct rx_solutions * solns)
#else
void
rx_free_solutions (solns)
     struct rx_solutions * solns;
#endif
{
  if (!solns)
    return;

  if (solns == &rx_no_solutions)
    return;

  if (solns->left)
    {
      rx_free_solutions (solns->left);
      solns->left = 0;
    }

  if (solns->right)
    {
      rx_free_solutions (solns->right);
      solns->right = 0;
    }

  if (solns->dfa)
    {
      rx_free_unfa (solns->dfa);
      solns->dfa = 0;
    }
  if (solns->left_dfa)
    {
      rx_terminate_system (&solns->left_match_engine);
      rx_free_unfa (solns->left_dfa);
      solns->left_dfa = 0;
    }

  rx_terminate_system (&solns->match_engine);

  if (solns->exp)
    {
      rx_free_rexp (solns->exp);
      solns->exp = 0;
    }

  if (!silly_hack_2)
    silly_hack_2 = (char *)solns;
  else
    free (solns);
}


#ifdef __STDC__
static enum rx_answers
rx_solution_fit_p (struct rx_solutions * solns)
#else
static enum rx_answers
rx_solution_fit_p (solns)
     struct rx_solutions * solns;
#endif
{
  unsigned const char * burst;
  int burst_addr;
  int burst_len;
  int burst_end_addr;
  int rel_pos_in_burst;
  enum rx_answers vmstat;
  int current_pos;
	  
  current_pos = solns->start;
 next_burst:
  vmstat = solns->vmfn (solns->closure,
			&burst, &burst_len, &burst_addr,
			current_pos, solns->end,
			current_pos);

  if (vmstat != rx_yes)
    return vmstat;

  rel_pos_in_burst = current_pos - burst_addr;
  burst_end_addr = burst_addr + burst_len;

  if (burst_end_addr >= solns->end)
    {
      enum rx_answers fit_status;
      fit_status = rx_fit_p (&solns->match_engine,
			     burst + rel_pos_in_burst,
			     solns->end - current_pos);
      return fit_status;
    }
  else
    {
      enum rx_answers fit_status;
      fit_status = rx_advance (&solns->match_engine,
			       burst + rel_pos_in_burst,
			       burst_len - rel_pos_in_burst);
      if (fit_status != rx_yes)
	{
	  return fit_status;
	}
      else
	{
	  current_pos += burst_len - rel_pos_in_burst;
	  goto next_burst;
	}
    }
}


#ifdef __STDC__
static enum rx_answers
rx_solution_fit_str_p (struct rx_solutions * solns)
#else
static enum rx_answers
rx_solution_fit_str_p (solns)
     struct rx_solutions * solns;
#endif
{
  int current_pos;
  unsigned const char * burst;
  int burst_addr;
  int burst_len;
  int burst_end_addr;
  int rel_pos_in_burst;
  enum rx_answers vmstat;
  int count;
  unsigned char * key;


  current_pos = solns->start;
  count = solns->exp->params.cstr.len;
  key = (unsigned char *)solns->exp->params.cstr.contents;

 next_burst:
  vmstat = solns->vmfn (solns->closure,
			&burst, &burst_len, &burst_addr,
			current_pos, solns->end,
			current_pos);

  if (vmstat != rx_yes)
    return vmstat;

  rel_pos_in_burst = current_pos - burst_addr;
  burst_end_addr = burst_addr + burst_len;

  {
    unsigned const char * pos;

    pos = burst + rel_pos_in_burst;

    if (burst_end_addr >= solns->end)
      {
	while (count)
	  {
	    if (*pos != *key)
	      return rx_no;
	    ++pos;
	    ++key;
	    --count;
	  }
	return rx_yes;
      }
    else
      {
	int part_count;
	int part_count_init;

	part_count_init = burst_len - rel_pos_in_burst;
	part_count = part_count_init;
	while (part_count)
	  {
	    if (*pos != *key)
	      return rx_no;
	    ++pos;
	    ++key;
	    --part_count;
	  }
	count -= part_count_init;
	current_pos += burst_len - rel_pos_in_burst;
	goto next_burst;
      }
  }
}




#if 0
#ifdef __STDC__
int
rx_best_end_guess (struct rx_solutions * solns, struct rexp_node *  exp, int bound)
#else
int
rx_best_end_guess (solns, exp, bound)
     struct rx_solutions * solns;
     struct rexp_node *  exp;
     int bound;
#endif
{
  int current_pos;
  unsigned const char * burst;
  int burst_addr;
  int burst_len;
  int burst_end_addr;
  int rel_pos_in_burst;
  int best_guess;
  enum rx_answers vmstat;

#if 0
  unparse_print_rexp (256, solns->exp);
  printf ("\n");
  unparse_print_rexp (256, exp);
  printf ("\nbound %d \n", bound);
#endif

  if (rx_yes != rx_start_superstate (&solns->left_match_engine))
    {
      return bound - 1;
    }
  best_guess = current_pos = solns->start;

 next_burst:
#if 0
  printf ("  best_guess %d\n", best_guess);
#endif
  vmstat = solns->vmfn (solns->closure,
			&burst, &burst_len, &burst_addr,
			current_pos, bound,
			current_pos);
#if 0
  printf ("  str>%s\n", burst);
#endif
  if (vmstat != rx_yes)
    {
      return bound - 1;
    }

  rel_pos_in_burst = current_pos - burst_addr;
  burst_end_addr = burst_addr + burst_len;

  if (burst_end_addr > bound)
    {
      burst_end_addr = bound;
      burst_len = bound - burst_addr;
    }

  {
    int amt_advanced;

#if 0
    printf ("  rel_pos_in_burst %d burst_len %d\n", rel_pos_in_burst, burst_len);
#endif
    while (rel_pos_in_burst < burst_len)
      {
	amt_advanced= rx_advance_to_final (&solns->left_match_engine,
					   burst + rel_pos_in_burst,
					   burst_len - rel_pos_in_burst);
#if 0
	printf ("  amt_advanced %d", amt_advanced);
#endif
	if (amt_advanced < 0)
	  {
	    return bound - 1;
	  }
	current_pos += amt_advanced;
	rel_pos_in_burst += amt_advanced;
	if (solns->left_match_engine.final_tag)
	  best_guess = current_pos;
#if 0
	printf ("  best_guess %d\n", best_guess);
	printf ("  current_pos %d\n", current_pos);
#endif
	if (amt_advanced == 0)
	  {
	    return best_guess;
	  }
      }
    if (current_pos == bound)
      {
	return best_guess;
      }
    goto next_burst;
  }
}
#endif



#ifdef __STDC__
enum rx_answers
rx_next_solution (struct rx_solutions * solns)
#else
enum rx_answers
rx_next_solution (solns)
     struct rx_solutions * solns;
#endif
{
  if (!solns)
    return rx_bogus;

  if (solns == &rx_no_solutions)
    {
      return rx_no;
    }

  if (!solns->exp)
    {
      if (solns->step != 0)
	{
	  return rx_no;
	}
      else
	{
	  solns->step = 1;
	  solns->final_tag = 1;
	  return (solns->start == solns->end
		  ? rx_yes
		  : rx_no);
	}
    }
  else if (   (solns->exp->len >= 0)
	   && (solns->exp->len != (solns->end - solns->start)))
    {
      return rx_no;
    }
  else if (!solns->exp->observed)
    {
      if (solns->step != 0)
	{
	  return rx_no;
	}
      else if (solns->exp->type == r_string)
	{
	  enum rx_answers ans;
	  ans = rx_solution_fit_str_p (solns);
	  solns->final_tag = 1;
	  solns->step = -1;
	  return ans;
	}
      else
	{
	  enum rx_answers ans;
	  ans = rx_solution_fit_p (solns);
	  solns->final_tag = solns->match_engine.final_tag;
	  solns->step = -1;
	  return ans;
	}
    }
  else if (solns->exp->observed)
    {
      enum rx_answers fit_p;
      switch (solns->step)
	{
	case -2:
	  if (solns->exp->params.intval)
	    {
	      solns->regs[solns->exp->params.intval].rm_so = solns->saved_rm_so;
	      solns->regs[solns->exp->params.intval].rm_eo = solns->saved_rm_eo;
	    }
	  return rx_no;

	case -1:
	  return rx_no;

	case 0:
	  fit_p = rx_solution_fit_p (solns);
	  /* Set final_tag here because this rough fit test
	   * may be all the matching that gets done.
	   * For example, consider a paren node containing
	   * a true regular expression ending with a cut
	   * operator.
	   */
	  solns->final_tag = solns->match_engine.final_tag;
	  switch (fit_p)
	    {
	    case rx_no:
	      solns->step = -1;
	      return rx_no;
	    case rx_yes:
	      solns->step = 1;
	      goto resolve_fit;
	    case rx_bogus:
	    default:
	      solns->step = -1;
	      return fit_p;
	    }

	default:
	resolve_fit:
	  switch (solns->exp->type)
	    {
	    case r_cset:
	    case r_string:
	    case r_cut:
	      solns->step = -1;
	      return rx_bogus;
	      
	    case r_parens:
	      {
		enum rx_answers paren_stat;
		switch (solns->step)
		  {
		  case 1:
		    if (solns->exp->params.intval)
		      {
			solns->saved_rm_so = solns->regs[solns->exp->params.intval].rm_so;
			solns->saved_rm_eo = solns->regs[solns->exp->params.intval].rm_eo;
		      }

		    if (   !solns->exp->params.pair.left
			|| !solns->exp->params.pair.left->observed)
		      {
			if (solns->exp->params.intval)
			  {
			    solns->regs[solns->exp->params.intval].rm_so = solns->start;
			    solns->regs[solns->exp->params.intval].rm_eo = solns->end;
			  }
			solns->step = -2;
			/* Keep the final_tag from the fit_p test. */
			return rx_yes;
		      }
		    else
		      {
			solns->left = rx_make_solutions (solns->regs,
							 solns->verse,
							 solns->exp->params.pair.left,
							 solns->subexps,
							 solns->cset_size,
							 solns->start,
							 solns->end,
							 solns->vmfn,
							 solns->contextfn,
							 solns->closure);
			if (!solns->left)
			  {
			    solns->step = -1;
			    return rx_bogus;
			  }
		      }
		    solns->step = 2;
		    /* fall through */

		  case 2:
		    if (solns->exp->params.intval)
		      {
			solns->regs[solns->exp->params.intval].rm_so = solns->saved_rm_so;
			solns->regs[solns->exp->params.intval].rm_eo = solns->saved_rm_eo;
		      }

		    paren_stat = rx_next_solution (solns->left);
		    if (paren_stat == rx_yes)
		      {
			if (solns->exp->params.intval)
			  {
			    solns->regs[solns->exp->params.intval].rm_so = solns->start;
			    solns->regs[solns->exp->params.intval].rm_eo = solns->end;
			  }
			solns->final_tag = solns->left->final_tag;
			return rx_yes;
		      }
		    else 
		      {
			solns->step = -1;
			rx_free_solutions (solns->left);
			solns->left = 0;
			if (solns->exp->params.intval)
			  {
			    solns->regs[solns->exp->params.intval].rm_so = solns->saved_rm_so;
			    solns->regs[solns->exp->params.intval].rm_eo = solns->saved_rm_eo;
			  }
			return paren_stat;
		      }
		  }
	      }


	    case r_opt:
	      {
		enum rx_answers opt_stat;
		switch (solns->step)
		  {
		  case 1:
		    solns->left = rx_make_solutions (solns->regs,
						     solns->verse,
						     solns->exp->params.pair.left,
						     solns->subexps,
						     solns->cset_size,
						     solns->start,
						     solns->end,
						     solns->vmfn,
						     solns->contextfn,	
						     solns->closure);
		    if (!solns->left)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 2;
		    /* fall through */
		    
		  case 2:
		    opt_stat = rx_next_solution (solns->left);
		    if (opt_stat == rx_yes)
		      {
			solns->final_tag = solns->left->final_tag;
			return rx_yes;
		      }
		    else 
		      {
			solns->step = -1;
			rx_free_solutions (solns->left);
			solns->left = 0;
			return ((solns->start == solns->end)
				? rx_yes
				: rx_no);
		      }

		  }
	     }

	    case r_alternate:
	      {
		enum rx_answers alt_stat;
		switch (solns->step)
		  {
		  case 1:
		    solns->left = rx_make_solutions (solns->regs,
						     solns->verse,
						     solns->exp->params.pair.left,
						     solns->subexps,
						     solns->cset_size,
						     solns->start,
						     solns->end,
						     solns->vmfn,
						     solns->contextfn,
						     solns->closure);
		    if (!solns->left)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 2;
		    /* fall through */
		    
		  case 2:
		    alt_stat = rx_next_solution (solns->left);

		    if (alt_stat == rx_yes)
		      {
			solns->final_tag = solns->left->final_tag;
			return alt_stat;
		      }
		    else 
		      {
			solns->step = 3;
			rx_free_solutions (solns->left);
			solns->left = 0;
			/* fall through */
		      }

		  case 3:
		    solns->right = rx_make_solutions (solns->regs,
						      solns->verse,
						      solns->exp->params.pair.right,
						      solns->subexps,
						      solns->cset_size,
						      solns->start,
						      solns->end,
						      solns->vmfn,
						      solns->contextfn,
						      solns->closure);
		    if (!solns->right)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 4;
		    /* fall through */
		    
		  case 4:
		    alt_stat = rx_next_solution (solns->right);

		    if (alt_stat == rx_yes)
		      {
			solns->final_tag = solns->right->final_tag;
			return alt_stat;
		      }
		    else 
		      {
			solns->step = -1;
			rx_free_solutions (solns->right);
			solns->right = 0;
			return alt_stat;
		      }
		  }
	     }

	    case r_concat:
	      {
		switch (solns->step)
		  {
		    enum rx_answers concat_stat;
		  case 1:
		    solns->split_guess = solns->end;
#if 0
		    solns->split_guess = ((solns->end - solns->start) > RX_MANY_CASES
					  ? rx_best_end_guess (solns,
							       solns->exp->params.pair.left, solns->end)
					  : solns->end);
#endif

		  concat_split_guess_loop:
		    solns->left = rx_make_solutions (solns->regs,
						     solns->verse,
						     solns->exp->params.pair.left,
						     solns->subexps,
						     solns->cset_size,
						     solns->start,
						     solns->split_guess,
						     solns->vmfn,
						     solns->contextfn,
						     solns->closure);
		    if (!solns->left)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 2;

		  case 2:
		  concat_try_next_left_match:

		    concat_stat = rx_next_solution (solns->left);
		    if (concat_stat != rx_yes)
		      {
			rx_free_solutions (solns->left);
			rx_free_solutions (solns->right);
			solns->left = solns->right = 0;
			solns->split_guess = solns->split_guess - 1;
#if 0
			solns->split_guess = ((solns->split_guess - solns->start) > RX_MANY_CASES
					      ? rx_best_end_guess (solns,
								   solns->exp->params.pair.left,
								   solns->split_guess - 1)
					      : solns->split_guess - 1);
#endif
			if (solns->split_guess >= solns->start)
			  goto concat_split_guess_loop;
			else
			  {
			    solns->step = -1;
			    return concat_stat;
			  }
		      }
		    else
		      {
			solns->step = 3;
			/* fall through */
		      }

		  case 3:
		    solns->right = rx_make_solutions (solns->regs,
						      solns->verse,
						      solns->exp->params.pair.right,
						      solns->subexps,
						      solns->cset_size,
						      solns->split_guess,
						      solns->end,
						      solns->vmfn,
						      solns->contextfn,
						      solns->closure);
		    if (!solns->right)
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			solns->step = -1;
			return rx_bogus;
		      }

		    solns->step = 4;
		    /* fall through */

		  case 4:
		  /* concat_try_next_right_match: */

		    concat_stat = rx_next_solution (solns->right);
		    if (concat_stat == rx_yes)
		      {
			solns->final_tag = solns->right->final_tag;
			return concat_stat;
		      }
		    else if (concat_stat == rx_no)
		      {
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = 2;
			goto concat_try_next_left_match;
		      }
		    else /*  concat_stat == rx_bogus */
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = -1;
			return concat_stat;
		      }
		  }
	      }


	    case r_plus:
	    case r_star:
	      {
		switch (solns->step)
		  {
		    enum rx_answers star_stat;
		  case 1:
		    solns->split_guess = solns->end;
#if 0
		    solns->split_guess = ((solns->end - solns->start) > RX_MANY_CASES
					  ? rx_best_end_guess (solns,
							       solns->exp->params.pair.left, solns->end)
					  : solns->end);
#endif

		  star_split_guess_loop:
		    solns->left = rx_make_solutions (solns->regs,
						     solns->verse,
						     solns->exp->params.pair.left,
						     solns->subexps,
						     solns->cset_size,
						     solns->start,
						     solns->split_guess,
						     solns->vmfn,
						     solns->contextfn,
						     solns->closure);
		    if (!solns->left)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 2;

		  case 2:
		  star_try_next_left_match:

		    star_stat = rx_next_solution (solns->left);
		    if (star_stat != rx_yes)
		      {
			rx_free_solutions (solns->left);
			rx_free_solutions (solns->right);
			solns->left = solns->right = 0;
			solns->split_guess = solns->split_guess - 1;
#if 0
			solns->split_guess = ((solns->split_guess - solns->start) > RX_MANY_CASES
					      ? rx_best_end_guess (solns,
								   solns->exp->params.pair.left,
								   solns->split_guess - 1)
					      : solns->split_guess - 1);
#endif
			if (solns->split_guess >= solns->start)
			  goto star_split_guess_loop;
			else
			  {
			    solns->step = -1;

			    if (   (solns->exp->type == r_star)
				&& (solns->start == solns->end)
				&& (star_stat == rx_no))
			      {
				solns->final_tag = 1;
				return rx_yes;
			      }
			    else
			      return star_stat;
			  }
		      }
		    else
		      {
			solns->step = 3;
			/* fall through */
		      }


		    if (solns->split_guess == solns->end)
		      {
			solns->final_tag = solns->left->final_tag;
			return rx_yes;
		      }
		    
		  case 3:
		    solns->right = rx_make_solutions (solns->regs,
						      solns->verse,
						      solns->exp,
						      solns->subexps,
						      solns->cset_size,
						      solns->split_guess,
						      solns->end,
						      solns->vmfn,
						      solns->contextfn,
						      solns->closure);
		    if (!solns->right)
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			solns->step = -1;
			return rx_bogus;
		      }

		    solns->step = 4;
		    /* fall through */

		  case 4:
		  /* star_try_next_right_match: */
		    
		    star_stat = rx_next_solution (solns->right);
		    if (star_stat == rx_yes)
		      {
			solns->final_tag = solns->right->final_tag;
			return star_stat;
		      }
		    else if (star_stat == rx_no)
		      {
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = 2;
			goto star_try_next_left_match;
		      }
		    else /*  star_stat == rx_bogus */
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = -1;
			return star_stat;
		      }
		  }
	      }

	    case r_interval:
	      {
		switch (solns->step)
		  {
		    enum rx_answers interval_stat;

		  case 1:
		    /* If the interval permits nothing, 
		     * return immediately.
		     */
		    if (solns->exp->params.intval2 < solns->interval_x)
		      {
			solns->step = -1;
			return rx_no;
		      }

		    /* If the interval permits only 0 iterations,
		     * return immediately.  Success depends on the
		     * emptiness of the match.
		     */
		    if (   (solns->exp->params.intval2 == solns->interval_x)
			&& (solns->exp->params.intval <= solns->interval_x))
		      {
			solns->step = -1;
			solns->final_tag = 1;
			return ((solns->start == solns->end)
				? rx_yes
				: rx_no);
		      }

		    /* The interval permits at most 0 iterations, 
		     * but also requires more.  A bug.
		     */
		    if (solns->exp->params.intval2 == solns->interval_x)
		      {
			/* indicates a regexp compilation error, actually */
			solns->step = -1;
			return rx_bogus;
		      }

		    solns->split_guess = solns->end;
#if 0
		    solns->split_guess = ((solns->end - solns->start) > RX_MANY_CASES
					  ? rx_best_end_guess (solns,
							       solns->exp->params.pair.left, solns->end)
					  : solns->end);
#endif

		    /* The interval permits more than 0 iterations.
		     * If it permits 0 and the match is to be empty, 
		     * the trivial match is the most preferred answer. 
		     */
		    if (solns->exp->params.intval <= solns->interval_x)
		      {
			solns->step = 2;
			if (solns->start == solns->end)
			  {
			    solns->final_tag = 1;
			    return rx_yes;
			  }
			/* If this isn't a trivial match, or if the trivial match
			 * is rejected, look harder. 
			 */
		      }
		    
		  case 2:
		  interval_split_guess_loop:
		    /* The match requires at least one iteration, either because
		     * there are characters to match, or because the interval starts
		     * above 0.
		     *
		     * Look for the first iteration:
		     */
		    solns->left = rx_make_solutions (solns->regs,
						     solns->verse,
						     solns->exp->params.pair.left,
						     solns->subexps,
						     solns->cset_size,
						     solns->start,
						     solns->split_guess,
						     solns->vmfn,
						     solns->contextfn,
						     solns->closure);
		    if (!solns->left)
		      {
			solns->step = -1;
			return rx_bogus;
		      }
		    solns->step = 3;

		  case 3:
		  interval_try_next_left_match:

		    interval_stat = rx_next_solution (solns->left);
		    if (interval_stat != rx_yes)
		      {
			rx_free_solutions (solns->left);
			rx_free_solutions (solns->right);
			solns->left = solns->right = 0;
			solns->split_guess = solns->split_guess - 1;
#if 0
			solns->split_guess = ((solns->split_guess - solns->start) > RX_MANY_CASES
					      ? rx_best_end_guess (solns,
								   solns->exp->params.pair.left,
								   solns->split_guess - 1)
					      : solns->split_guess - 1);
#endif
			if (solns->split_guess >= solns->start)
			  goto interval_split_guess_loop;
			else
			  {
			    solns->step = -1;
			    return interval_stat;
			  }
		      }
		    else
		      {
			solns->step = 4;
			/* fall through */
		      }

		  case 4:
		    {
		      /* After matching one required iteration, construct a smaller
		       * interval and try to match that against the rest.
		       *
		       * To avoid thwarting unfa caching, instead of building a new
		       * rexp node with different interval extents, we keep interval_x
		       * in each solns structure to keep track of the number of 
		       * iterations matched so far.
		       */
		      solns->right = rx_make_solutions (solns->regs,
							solns->verse,
							solns->exp,
							solns->subexps,
							solns->cset_size,
							solns->split_guess,
							solns->end,
							solns->vmfn,
							solns->contextfn,
							solns->closure);
		      solns->right->interval_x = solns->interval_x + 1;
		    }
		    if (!solns->right)
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			solns->step = -1;
			return rx_bogus;
		      }

		    solns->step = 5;
		    /* fall through */

		  case 5:
		  /* interval_try_next_right_match: */
		    
		    interval_stat = rx_next_solution (solns->right);
		    if (interval_stat == rx_yes)
		      {
			solns->final_tag = solns->right->final_tag;
			return interval_stat;
		      }
		    else if (interval_stat == rx_no)
		      {
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = 2;
			goto interval_try_next_left_match;
		      }
		    else /*  interval_stat == rx_bogus */
		      {
			rx_free_solutions (solns->left);
			solns->left = 0;
			rx_free_solutions (solns->right);
			solns->right = 0;
			solns->step = -1;
			return interval_stat;
		      }
		  }
	      }

	    case r_context:
	      {
		solns->step = -1;
		solns->final_tag = 1;
		return solns->contextfn (solns->closure,
					 solns->exp,
					 solns->start, solns->end,
					 solns->regs);
	      }


	    }
	}
      return rx_bogus;
    }
}
