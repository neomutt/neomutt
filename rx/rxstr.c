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
#include "rxstr.h"



#ifdef __STDC__
enum rx_answers
rx_str_vmfn (void * closure, unsigned const char ** burstp, int * lenp, int * offsetp, int start, int end, int need)
#else
enum rx_answers
rx_str_vmfn (closure, burstp, lenp, offsetp, start, end, need)
     void * closure;
     unsigned const char ** burstp;
     int * lenp;
     int * offsetp;
     int start;
     int end;
     int need;
#endif
{
  struct rx_str_closure * strc;
  strc = (struct rx_str_closure *)closure;

  if (   (need < 0)
      || (need > strc->len))
    return rx_no;

  *burstp = strc->str;
  *lenp = strc->len;
  *offsetp = 0;
  return rx_yes;
}

#ifdef __STDC__
enum rx_answers
rx_str_contextfn (void * closure, struct rexp_node * node, int start, int end, struct rx_registers * regs)
#else
enum rx_answers
rx_str_contextfn (closure, node, start, end, regs)
     void * closure;
     struct rexp_node * node;
     int start;
     int end;
     struct rx_registers * regs;
#endif
{
  struct rx_str_closure * strc;

  strc = (struct rx_str_closure *)closure;
  switch (node->params.intval)
    {
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':
      {
	int cmp;
	int regn;
	regn = node->params.intval - '0';
	if (   (regs[regn].rm_so == -1)
	    || ((end - start) != (regs[regn].rm_eo - regs[regn].rm_so)))
	  return rx_no;
	else
	  {
	    if (strc->rules.case_indep)
	      cmp = strncasecmp (strc->str + start,
				 strc->str + regs[regn].rm_so,
				 end - start);
	    else
	      cmp = strncmp (strc->str + start,
			     strc->str + regs[regn].rm_so,
			     end - start);

	    return (!cmp
		    ? rx_yes
		    : rx_no);
	  }
      }

    case '^':
      {
	return ((   (start == end)
		 && (   ((start == 0) && !strc->rules.not_bol)
		     || (   (start > 0)
			 && strc->rules.newline_anchor
			 && (strc->str[start - 1] == '\n'))))
		? rx_yes
		: rx_no);
      }

    case '$':
      {
	return ((   (start == end)
		 && (   ((start == strc->len) && !strc->rules.not_eol)
		     || (   (start < strc->len)
			 && strc->rules.newline_anchor
			 && (strc->str[start] == '\n'))))
		? rx_yes
		: rx_no);
      }

    case '<':
    case '>':

    case 'B':
    case 'b':


    default:
      return rx_bogus;
    }
}
