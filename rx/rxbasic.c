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
#include "rxbasic.h"
#include "rxstr.h"




int rx_basic_unfaniverse_delay = RX_DEFAULT_NFA_DELAY;
static struct rx_unfaniverse * rx_basic_uv = 0;



static int
init_basic_once ()
{
  if (rx_basic_uv)
    return 0;
  rx_basic_uv = rx_make_unfaniverse (rx_basic_unfaniverse_delay);
  return (rx_basic_uv ? 0 : -1);
}


#ifdef __STDC__
struct rx_unfaniverse *
rx_basic_unfaniverse (void)
#else
struct rx_unfaniverse *
rx_basic_unfaniverse ()
#endif
{
  if (init_basic_once ())
    return 0;
  return rx_basic_uv;
}


static char * silly_hack = 0;

#ifdef __STDC__
struct rx_solutions *
rx_basic_make_solutions (struct rx_registers * regs, struct rexp_node * expression, struct rexp_node ** subexps, int start, int end, struct rx_context_rules * rules, const unsigned char * str)
#else
struct rx_solutions *
rx_basic_make_solutions (regs, expression, subexps, start, end, rules, str)
     struct rx_registers * regs;
     struct rexp_node * expression;
     struct rexp_node ** subexps;
     int start;
     int end;
     struct rx_context_rules * rules;
     const unsigned char * str;
#endif
{
  struct rx_str_closure * closure;
  if (init_basic_once ())
    return 0;			/* bogus but rare */
  if (   expression
      && (expression->len >= 0)
      && (expression->len != (end - start)))
    return &rx_no_solutions;
  if (silly_hack)
    {
      closure = (struct rx_str_closure *)silly_hack;
      silly_hack = 0;
    }
  else
    closure = (struct rx_str_closure *)malloc (sizeof (*closure));
  if (!closure)
    return 0;
  closure->str = str;
  closure->len = end;
  closure->rules = *rules;
  return rx_make_solutions (regs, rx_basic_uv, expression, subexps, 256,
			    start, end, rx_str_vmfn, rx_str_contextfn,
			    (void *)closure);
}



#ifdef __STDC__
void
rx_basic_free_solutions (struct rx_solutions * solns)
#else
     void
     rx_basic_free_solutions (solns)
     struct rx_solutions * solns;
#endif
{
  if (solns == &rx_no_solutions)
    return;

  if (!silly_hack)
    silly_hack = (char *)solns->closure;
  else
    free (solns->closure);
  solns->closure = 0;
  rx_free_solutions (solns);
}
