/*
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/**
 ** This program parses mutt's init.h and generates documentation in
 ** three different formats:
 **
 ** -> a commented muttrc configuration file
 ** -> nroff, suitable for inclusion in a manual page
 ** -> docbook-xml, suitable for inclusion in the 
 **    SGML-based manual
 **
 **/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "makedoc-defs.h"

#ifndef HAVE_STRERROR
#ifndef STDC_HEADERS
extern int sys_nerr;
extern char *sys_errlist[];
#endif

#define strerror(x) ((x) > 0 && (x) < sys_nerr) ? sys_errlist[(x)] : 0
#endif /* !HAVE_STRERROR */

extern int optind;

#define BUFFSIZE 2048

enum output_formats_t
{
  F_CONF, F_MAN, F_SGML, F_NONE
};

#define D_NL 		(1 << 0)
#define D_EM		(1 << 1)
#define D_BF		(1 << 2)
#define D_TAB		(1 << 3)
#define D_NP		(1 << 4)
#define D_INIT		(1 << 5)
#define D_DL		(1 << 6)
#define D_DT		(1 << 7)
#define D_DD            (1 << 8)
#define D_PA            (1 << 9)
#define D_IL		(1 << 10)
#define D_TT            (1 << 11)

enum
{
  SP_START_EM,
  SP_START_BF,
  SP_START_TT,
  SP_END_FT,
  SP_NEWLINE,
  SP_NEWPAR,
  SP_END_PAR,
  SP_STR,
  SP_START_TAB,
  SP_END_TAB,
  SP_START_DL,
  SP_DT,
  SP_DD,
  SP_END_DD,
  SP_END_DL,
  SP_START_IL,
  SP_END_IL,
  SP_END_SECT,
  SP_REFER
};

enum output_formats_t OutputFormat = F_NONE;
char *Progname;
short Debug = 0;

static char *get_token (char *, size_t, char *);
static char *skip_ws (char *);
static const char *type2human (int);
static int buff2type (const char *);
static int flush_doc (int, FILE *);
static int handle_docline (char *, FILE *, int);
static int print_it (int, char *, FILE *, int);
static void print_confline (const char *, int, const char *, FILE *);
static void handle_confline (char *, FILE *);
static void makedoc (FILE *, FILE *);
static void pretty_default (char *, size_t, const char *, int);
static int sgml_fputc (int, FILE *);
static int sgml_fputs (const char *, FILE *);
static int sgml_id_fputs (const char *, FILE *);

int main (int argc, char *argv[])
{
  int c;
  FILE *f;

  if ((Progname = strrchr (argv[0], '/')))
    Progname++;
  else
    Progname = argv[0];
  
  while ((c = getopt (argc, argv, "cmsd")) != EOF)
  {
    switch (c)
    {
      case 'c': OutputFormat = F_CONF; break;
      case 'm': OutputFormat = F_MAN; break;
      case 's': OutputFormat = F_SGML; break;
      case 'd': Debug++; break;
      default: 
      {
	fprintf (stderr, "%s: bad command line parameter.\n", Progname);
	exit (1);
      }
    }
  }

  if (optind != argc)
  {
    if ((f = fopen (argv[optind], "r")) == NULL)
    {
      fprintf (stderr, "%s: Can't open %s (%s).\n",
	       Progname, argv[optind], strerror (errno));
      exit (1);
    }
  }
  else 
    f = stdin;

  switch (OutputFormat)
  {
    case F_CONF: 
    case F_MAN:  
    case F_SGML: makedoc (f, stdout); break;
    default:
    {
      fprintf (stderr, "%s: No output format specified.\n",
	       Progname);
      exit (1);
    }
  }
  
  if (f != stdin)
    fclose (f);

  return 0;
}


static void makedoc (FILE *in, FILE *out)
{
  char buffer[BUFFSIZE];
  char token[BUFFSIZE];
  char *p;
  int active = 0;
  int line = 0;
  int docstat = D_INIT;

  while ((fgets (buffer, sizeof (buffer), in)))
  {
    line++;
    if ((p = strchr (buffer, '\n')) == NULL)
    {
      fprintf (stderr, "%s: Line %d too long.  Ask a wizard to enlarge\n"
	               "%s: my buffer size.\n", Progname, line, Progname);
      exit (1);
    }
    else
      *p = '\0';

    if (!(p = get_token (token, sizeof (token), buffer)))
      continue;

    if (Debug)
    {
      fprintf (stderr, "%s: line %d.  first token: \"%s\".\n",
	       Progname, line, token);
    }
    
    if (!strcmp (token, "/*++*/"))
      active = 1;
    else if (!strcmp (token, "/*--*/"))
    {
      docstat = flush_doc (docstat, out);
      active = 0;
    }
    else if (active && (!strcmp (token, "/**") || !strcmp (token, "**")))
      docstat = handle_docline (p, out, docstat);
    else if (active && !strcmp (token, "{"))
    {
      docstat = flush_doc (docstat, out);
      handle_confline (p, out);
    }
  }
  flush_doc (docstat, out);
  fputs ("\n", out);
}

/* skip whitespace */

static char *skip_ws (char *s)
{
  while (*s && isspace ((unsigned char) *s))
    s++;

  return s;
}

/* isolate a token */

static char single_char_tokens[] = "[]{},;|";

static char *get_token (char *d, size_t l, char *s)
{
  char *t;
  short is_quoted = 0;
  char *dd = d;

  if (Debug)
     fprintf (stderr, "%s: get_token called for `%s'.\n",
	      Progname, s);
  
  s = skip_ws (s);

  if (Debug > 1)
    fprintf (stderr, "%s: argument after skip_ws():  `%s'.\n",
	     Progname, s);

  if (!*s)
  {
    if (Debug)
      fprintf (stderr, "%s: no more tokens on this line.\n", Progname);
    return NULL;
  }

  if (strchr (single_char_tokens, *s))
  {
    if (Debug)
    {
      fprintf (stderr, "%s: found single character token `%c'.\n",
	       Progname, *s);
    }
    d[0] = *s++;
    d[1] = 0;
    return s;
  }

  if (*s == '"')
  {
    if (Debug)
    {
      fprintf (stderr, "%s: found quote character.\n", Progname);
    }
      
    s++;
    is_quoted = 1;
  }

  for (t = s; *t && --l > 0; t++)
  {
    if (*t == '\\' && !t[1])
      break;

    if (is_quoted && *t == '\\')
    {
      switch ((*d = *++t))
      {
	case 'n': *d = '\n'; break;
	case 't': *d = '\t'; break;
	case 'r': *d = '\r'; break;
	case 'a': *d = '\a'; break;
      }

      d++;
      continue;
    }

    if (is_quoted && *t == '"')
    {
      t++;
      break;
    }
    else if (!is_quoted && strchr (single_char_tokens, *t))
      break;
    else if (!is_quoted && isspace ((unsigned char) *t))
      break;
    else
      *d++ = *t;
  }

  *d = '\0';

  if (Debug)
  {
    fprintf (stderr, "%s: Got %stoken: `%s'.\n",
	     Progname, is_quoted ? "quoted " : "", dd);
    fprintf (stderr, "%s: Remainder: `%s'.\n",
	     Progname, t);
  }
  
  return t;
}


/**
 ** Configuration line parser
 ** 
 ** The following code parses a line from init.h which declares
 ** a configuration variable.
 **
 **/

/* note: the following enum must be in the same order as the
 * following string definitions!
 */

enum 
{
  DT_NONE = 0,
  DT_BOOL,
  DT_NUM,
  DT_STR,
  DT_PATH,
  DT_QUAD,
  DT_SORT,
  DT_RX,
  DT_MAGIC,
  DT_SYN,
  DT_ADDR
};

struct 
{
  char *machine;
  char *human;
}
types[] = 
{
  { "DT_NONE",	"-none-" 	},
  { "DT_BOOL",  "boolean"	},
  { "DT_NUM",   "number"	},
  { "DT_STR",	"string"	},
  { "DT_PATH",	"path"		},
  { "DT_QUAD",	"quadoption"	},
  { "DT_SORT",	"sort order"	},
  { "DT_RX",	"regular expression" },
  { "DT_MAGIC",	"folder magic" },
  { "DT_SYN",	NULL },
  { "DT_ADDR",	"e-mail address" },
  { NULL, NULL }
};
    

static int buff2type (const char *s)
{
  int type;
  
  for (type = DT_NONE; types[type].machine; type++)
    if (!strcmp (types[type].machine, s))
	return type;
  
  return DT_NONE;
}

static const char *type2human (int type)
{
  return types[type].human;
}
static void handle_confline (char *s, FILE *out)
{
  char varname[BUFFSIZE];
  char buff[BUFFSIZE];
  char tmp[BUFFSIZE];
  int type;
  
  char val[BUFFSIZE];

  /* xxx - put this into an actual state machine? */

  /* variable name */
  if (!(s = get_token (varname, sizeof (varname), s))) return;
  
  /* comma */
  if (!(s = get_token (buff, sizeof (buff), s))) return;
    
  /* type */
  if (!(s = get_token (buff, sizeof (buff), s))) return;

  type = buff2type (buff);

  /* possibly a "|" or comma */
  if (!(s = get_token (buff, sizeof (buff), s))) return;

  if (!strcmp (buff, "|"))
  {
    if (Debug) fprintf (stderr, "%s: Expecting <subtype> <comma>.\n", Progname);
    /* ignore subtype and comma */
    if (!(s = get_token (buff, sizeof (buff), s))) return;
    if (!(s = get_token (buff, sizeof (buff), s))) return;
  }

  /* redraw, comma */
  
  while (1)
  {
    if (!(s = get_token (buff, sizeof (buff), s))) return;
    if (!strcmp (buff, ","))
      break;
  }

  /* option name or UL &address */
  if (!(s = get_token (buff, sizeof (buff), s))) return;
  if (!strcmp (buff, "UL"))
    if (!(s = get_token (buff, sizeof (buff), s))) return;

  /* comma */
  if (!(s = get_token (buff, sizeof (buff), s))) return;

  if (Debug) fprintf (stderr, "%s: Expecting default value.\n", Progname);
  
  /* <default value> or UL <default value> */
  if (!(s = get_token (buff, sizeof (buff), s))) return;
  if (!strcmp (buff, "UL"))
  {
    if (Debug) fprintf (stderr, "%s: Skipping UL.\n", Progname);
    if (!(s = get_token (buff, sizeof (buff), s))) return;
  }

  memset (tmp, 0, sizeof (tmp));

  do 
  {
    if (!strcmp (buff, "}"))
      break;

    strncpy (tmp + strlen (tmp), buff, sizeof (tmp) - strlen (tmp));
  }
  while ((s = get_token (buff, sizeof (buff), s)));

  pretty_default (val, sizeof (val), tmp, type);
  print_confline (varname, type, val, out);
}

static void pretty_default (char *t, size_t l, const char *s, int type)
{
  memset (t, 0, l);
  l--;

  switch (type)
  {
    case DT_QUAD:
    {    
      if (!strcasecmp (s, "M_YES")) strncpy (t, "yes", l);
      else if (!strcasecmp (s, "M_NO")) strncpy (t, "no", l);
      else if (!strcasecmp (s, "M_ASKYES")) strncpy (t, "ask-yes", l);
      else if (!strcasecmp (s, "M_ASKNO")) strncpy (t, "ask-no", l);
      break;
    }
    case DT_BOOL:
    {
      if (atoi (s))
	strncpy (t, "yes", l);
      else
	strncpy (t, "no", l);
      break;
    }
    case DT_SORT:
    {
      /* heuristic! */
      strncpy (t, s + 5, l);
      for (; *t; t++) *t = tolower ((unsigned char) *t);
      break;
    }
    case DT_MAGIC:
    {
      /* heuristic! */
      strncpy (t, s + 2, l);
      for (; *t; t++) *t = tolower ((unsigned char) *t);
      break;
    }
    case DT_STR:
    case DT_RX:
    case DT_ADDR:
    case DT_PATH:
    {
      if (!strcmp (s, "0"))
	break;
      /* fallthrough */
    }
    default:
    {
      strncpy (t, s, l);
      break;
    }
  }
}

static void char_to_escape (char *dest, unsigned int c)
{
  switch (c)
  {
    case '\r': strcpy (dest, "\\r"); break;	/* __STRCPY_CHECKED__ */
    case '\n': strcpy (dest, "\\n"); break;	/* __STRCPY_CHECKED__ */
    case '\t': strcpy (dest, "\\t"); break;	/* __STRCPY_CHECKED__ */
    case '\f': strcpy (dest, "\\f"); break;	/* __STRCPY_CHECKED__ */
    default: sprintf (dest, "\\%03o", c); break;
  }
}
static void conf_char_to_escape (unsigned int c , FILE *out)
{
  char buff[16];
  char_to_escape (buff, c);
  fputs (buff, out);
}

static void conf_print_strval (const char *v, FILE *out)
{
  for (; *v; v++)
  {
    if (*v < ' ' || *v & 0x80)
    {
      conf_char_to_escape ((unsigned int) *v, out);
      continue;
    }

    if (*v == '"'  || *v == '\\')
      fputc ('\\', out);
    fputc (*v, out);
  }
}

static void man_print_strval (const char *v, FILE *out)
{
  for (; *v; v++)
  {
    if (*v < ' ' || *v & 0x80)
    {
      fputc ('\\', out);
      conf_char_to_escape ((unsigned int) *v, out);
      continue;
    }
    
    if (*v == '"')
      fputs ("\\(rq", out);
    else if (*v == '\\')
      fputs ("\\\\", out);
    else if (*v == '-')
      fputs ("\\-", out);
    else
      fputc (*v, out);
  }
}

static void sgml_print_strval (const char *v, FILE *out)
{
  char buff[16];
  for (; *v; v++)
  {
    if (*v <  ' ' || *v & 0x80)
    {
      char_to_escape (buff, (unsigned int) *v);
      sgml_fputs (buff, out);
      continue;
    }
    sgml_fputc ((unsigned int) *v, out);
  }
}

static int sgml_fputc (int c, FILE *out)
{
  switch (c)
  {
    /* the bare minimum for escaping */
    case '<': return fputs ("&lt;", out);
    case '>': return fputs ("&gt;", out);
    case '&': return fputs ("&amp;", out);
    default:  return fputc (c, out);
  }
}

static int sgml_fputs (const char *s, FILE *out)
{
  for (; *s; s++)
    if (sgml_fputc ((unsigned int) *s, out) == EOF)
      return EOF;

  return 0;
}

/* reduce CDATA to ID */
static int sgml_id_fputs (const char *s, FILE* out)
{
  char id;

  if (*s == '<')
    s++;

  for (; *s; s++)
  {
    if (*s == '_')
      id = '-';
    else
      id = *s;
    if (*s == '>' && !*(s+1))
      break;

    if (fputc ((unsigned int) id, out) == EOF)
      return EOF;
  }

  return 0;
}

static void print_confline (const char *varname, int type, const char *val, FILE *out)
{
  if (type == DT_SYN) return;
  
  switch (OutputFormat)
  {
    /* configuration file */
    case F_CONF:
    {
      if (type == DT_STR || type == DT_RX || type == DT_ADDR || type == DT_PATH)
      {
	fprintf (out, "\n# set %s=\"", varname);
	conf_print_strval (val, out);
	fputs ("\"", out);
      }
      else if (type != DT_SYN)
	fprintf (out, "\n# set %s=%s", varname, val);
      
      fprintf (out, "\n#\n# Name: %s", varname);
      fprintf (out, "\n# Type: %s", type2human (type));
      if (type == DT_STR || type == DT_RX || type == DT_ADDR || type == DT_PATH)
      {
	fputs ("\n# Default: \"", out);
	conf_print_strval (val, out);
	fputs ("\"", out);
      }
      else
	fprintf (out, "\n# Default: %s", val);

      fputs ("\n# ", out);
      break;
    }

    /* manual page */
    case F_MAN:
    {
      fprintf (out, "\n.TP\n.B %s\n", varname);
      fputs (".nf\n", out);
      fprintf (out, "Type: %s\n", type2human (type));
      if (type == DT_STR || type == DT_RX || type == DT_ADDR || type == DT_PATH)
      {
	fputs ("Default: \\(lq", out);
	man_print_strval (val, out);
	fputs ("\\(rq\n", out);
      }
      else {
	fputs ("Default: ", out);
	man_print_strval (val, out);
	fputs ("\n", out);
      }

      fputs (".fi", out);

      break;
    }
    
    /* SGML based manual */
    case F_SGML:
    {
      fputs ("\n<sect2 id=\"", out);
      sgml_id_fputs(varname, out);
      fputs ("\">\n<title>", out);
      sgml_fputs (varname, out);
      fprintf (out, "</title>\n<literallayout>Type: %s", type2human (type));

      
      if (type == DT_STR || type == DT_RX || type == DT_ADDR || type == DT_PATH)
      {
	if (val && *val)
	{
	  fputs ("\nDefault: <quote><literal>", out);
	  sgml_print_strval (val, out);
	  fputs ("</literal></quote>", out);
	}
	else
	{
	  fputs ("\nDefault: (empty)", out);
	}
	fputs ("</literallayout>\n", out);
      }
      else
	fprintf (out, "\nDefault: %s</literallayout>\n", val);
      break;
    }
    /* make gcc happy */
    default:
      break;
  }
}

/**
 ** Documentation line parser
 **
 ** The following code parses specially formatted documentation 
 ** comments in init.h.
 **
 ** The format is very remotely inspired by nroff. Most important, it's
 ** easy to parse and convert, and it was easy to generate from the SGML 
 ** source of mutt's original manual.
 **
 ** - \fI switches to italics
 ** - \fB switches to boldface
 ** - \fP switches to normal display
 ** - .dl on a line starts a definition list (name taken taken from HTML).
 ** - .dt starts a term in a definition list.
 ** - .dd starts a definition in a definition list.
 ** - .de on a line finishes a definition list.
 ** - .il on a line starts an itemized list
 ** - .dd starts an item in an itemized list
 ** - .ie on a line finishes an itemized list
 ** - .ts on a line starts a "tscreen" environment (name taken from SGML).
 ** - .te on a line finishes this environment.
 ** - .pp on a line starts a paragraph.
 ** - \$word will be converted to a reference to word, where appropriate.
 **   Note that \$$word is possible as well.
 ** - '. ' in the beginning of a line expands to two space characters.
 **   This is used to protect indentations in tables.
 **/

/* close eventually-open environments. */

static int fd_recurse = 0;

static int flush_doc (int docstat, FILE *out)
{
  if (docstat & D_INIT)
    return D_INIT;

  if (fd_recurse++)
  {
    fprintf (stderr, "%s: Internal error, recursion in flush_doc()!\n", Progname);
    exit (1);
  }

  if (docstat & (D_PA))
    docstat = print_it (SP_END_PAR, NULL, out, docstat);

  if (docstat & (D_TAB))
    docstat = print_it (SP_END_TAB, NULL, out, docstat);

  if (docstat & (D_DL))
    docstat = print_it (SP_END_DL, NULL, out, docstat);

  if (docstat & (D_EM | D_BF | D_TT))
    docstat = print_it (SP_END_FT, NULL, out, docstat);

  docstat = print_it (SP_END_SECT, NULL, out, docstat);

  docstat = print_it (SP_NEWLINE, NULL, out, 0);

  fd_recurse--;
  return D_INIT;
}

/* print something. */

static int print_it (int special, char *str, FILE *out, int docstat)
{
  int onl = docstat & (D_NL|D_NP);
  
  docstat &= ~(D_NL|D_NP|D_INIT);

  switch (OutputFormat)
  {
    /* configuration file */
    case F_CONF:
    {
      switch (special)
      {
	static int Continuation = 0;

	case SP_END_FT: docstat &= ~(D_EM|D_BF|D_TT); break;
	case SP_START_BF: docstat |= D_BF; break;
	case SP_START_EM: docstat |= D_EM; break;
        case SP_START_TT: docstat |= D_TT; break;
	case SP_NEWLINE: 
	{
	  if (onl)
	    docstat |= onl;
	  else
	  {
	    fputs ("\n# ", out);
	    docstat |= D_NL;
	  }
	  if (docstat & D_DL)
	    ++ Continuation;
	  break;
	}
	case SP_NEWPAR:
	{
	  if (onl & D_NP)
	  {
	    docstat |= onl;
	    break;
	  }

	  if (!(onl & D_NL))
	    fputs ("\n# ", out);
	  fputs ("\n# ", out);
	  docstat |= D_NP;
	  break;
	}
	case SP_START_TAB: 
	{
	  if (!onl) 
	    fputs ("\n# ", out);
	  docstat |= D_TAB;
	  break;
	}
	case SP_END_TAB:
	{
	  docstat &= ~D_TAB;
	  docstat |= D_NL;
	  break;
	}
	case SP_START_DL:
	{
	  docstat |= D_DL;
	  break;
	}
	case SP_DT:
	{
	  Continuation = 0;
	  docstat |= D_DT;
	  break;
	}
	case SP_DD:
	{
	  if (docstat & D_IL)
	    fputs ("- ", out);
	  Continuation = 0;
	  break;
	}
	case SP_END_DL:
	{
	  Continuation = 0;
	  docstat &= ~D_DL;
	  break;
	}
	case SP_START_IL:
	{
	  docstat |= D_IL;
	  break;
	}
	case SP_END_IL:
	{
	  Continuation = 0;
	  docstat &= ~D_IL;
	  break;
	}
	case SP_STR:
	{
	  if (Continuation)
	  {
	    Continuation = 0;
	    fputs ("        ", out);
	  }
	  fputs (str, out);
	  if (docstat & D_DT)
	  { 
	    int i;

	    for (i = strlen (str) ; i < 8 ; i++)
	      putc (' ', out);
	    docstat &= ~D_DT;
	    docstat |= D_NL;
	  }
	  break;
	}
      }
      break;
    }

    /* manual page */
    case F_MAN:
    {
      switch (special)
      {
	case SP_END_FT: 
	{
	  fputs ("\\fP", out);
	  docstat &= ~(D_EM|D_BF|D_TT);
	  break;
	}
	case SP_START_BF: 
	{
	  fputs ("\\fB", out);
	  docstat |= D_BF;
	  docstat &= ~(D_EM|D_TT);
	  break;
	}
	case SP_START_EM:
	{
	  fputs ("\\fI", out);
	  docstat |= D_EM;
	  docstat &= ~(D_BF|D_TT);
	  break;
	}
        case SP_START_TT:
	{
	  fputs ("\\fC", out);
	  docstat |= D_TT;
	  docstat &= ~(D_BF|D_EM);
	  break;
        }
	case SP_NEWLINE:
	{
	  if (onl)
	    docstat |= onl;
	  else
	  {
	    fputc ('\n', out);
	    docstat |= D_NL;
	  }
	  break;
	}
	case SP_NEWPAR:
	{
	  if (onl & D_NP)
	  {
	    docstat |= onl;
	    break;
	  }

	  if (!(onl & D_NL))
	    fputc ('\n', out);
	  fputs (".IP\n", out);

	  docstat |= D_NP;
	  break;
	}
	case SP_START_TAB:
	{
	  fputs ("\n.IP\n.EX\n", out);
	  docstat |= D_TAB | D_NL;
	  break;
	}
	case SP_END_TAB:
	{
	  fputs ("\n.EE\n", out);
	  docstat &= ~D_TAB;
	  docstat |= D_NL;
	  break;
	}
	case SP_START_DL:
	{
	  fputs (".RS\n.PD 0\n", out);
	  docstat |= D_DL;
	  break;
	}
	case SP_DT:
	{
	  fputs (".TP\n", out);
	  break;
	}
	case SP_DD:
	{
	  if (docstat & D_IL)
	    fputs (".TP\n\\(hy ", out);
	  else
	    fputs ("\n", out);
	  break;
	}
	case SP_END_DL:
	{
	  fputs (".RE\n.PD 1", out);
	  docstat &= ~D_DL;
	  break;
	}
	case SP_START_IL:
	{
	  fputs (".RS\n.PD 0\n", out);
	  docstat |= D_IL;
	  break;
	}
	case SP_END_IL:
	{
	  fputs (".RE\n.PD 1", out);
	  docstat &= ~D_DL;
	  break;
	}
	case SP_STR:
	{
	  while (*str)
	  {
	    for (; *str; str++)
	    {
	      if (*str == '"')
		fputs ("\\(rq", out);
	      else if (*str == '\\')
		fputs ("\\\\", out);
              else if (*str == '-')
                fputs ("\\-", out);
	      else if (!strncmp (str, "``", 2))
	      {
		fputs ("\\(lq", out);
		str++;
	      }
	      else if (!strncmp (str, "''", 2))
	      {
		fputs ("\\(rq", out);
		str++;
	      }
	      else
		fputc (*str, out);
	    }
	  }
	  break;
	}
      }
      break;
    }

    /* SGML based manual */
    case F_SGML:
    {
      switch (special)
      {
	case SP_END_FT: 
	{
	  if (docstat & D_EM) fputs ("</emphasis>", out);
	  if (docstat & D_BF) fputs ("</emphasis>", out);
          if (docstat & D_TT) fputs ("</literal>", out);
	  docstat &= ~(D_EM|D_BF|D_TT);
	  break;
	}
	case SP_START_BF: 
	{
	  fputs ("<emphasis role=\"bold\">", out);
	  docstat |= D_BF;
	  docstat &= ~(D_EM|D_TT);
	  break;
	}
	case SP_START_EM:
	{
	  fputs ("<emphasis>", out);
	  docstat |= D_EM;
	  docstat &= ~(D_BF|D_TT);
	  break;
	}
        case SP_START_TT:
	{
	  fputs ("<literal>", out);
	  docstat |= D_TT;
	  docstat &= ~(D_BF|D_EM);
	  break;
        }
	case SP_NEWLINE:
	{
	  if (onl)
	    docstat |= onl;
	  else
	  {
	    fputc ('\n', out);
	    docstat |= D_NL;
	  }
	  break;
	}
	case SP_NEWPAR:
	{
	  if (onl & D_NP)
	  {
	    docstat |= onl;
	    break;
	  }

	  if (!(onl & D_NL))
	    fputc ('\n', out);
	  if (docstat & D_PA)
	    fputs("</para>\n", out);
	  fputs ("<para>\n", out);

	  docstat |= D_NP;
	  docstat |= D_PA;

	  break;
	}
        case SP_END_PAR:
        {
	  fputs ("</para>\n", out);
	  docstat &= ~D_PA;
	  break;
        }
	case SP_START_TAB:
	{
	  if (docstat & D_PA)
	  {
	    fputs ("\n</para>\n", out);
	    docstat &= ~D_PA;
	  }
	  fputs ("\n<screen>\n", out);
	  docstat |= D_TAB | D_NL;
	  break;
	}
	case SP_END_TAB:
	{
	  fputs ("</screen>", out);
	  docstat &= ~D_TAB;
	  docstat |= D_NL;
	  break;
	}
	case SP_START_DL:
	{
	  if (docstat & D_PA)
	  {
	    fputs ("\n</para>\n", out);
	    docstat &= ~D_PA;
	  }
	  fputs ("\n<informaltable>\n<tgroup cols=\"2\">\n<tbody>\n", out);
	  docstat |= D_DL;
	  break;
	}
	case SP_DT:
	{
	  fputs ("<row><entry>", out);
	  break;
	}
	case SP_DD:
	{
	  docstat |= D_DD;
	  if (docstat & D_DL)
	    fputs("</entry><entry>", out);
	  else
	    fputs ("<listitem><para>", out);
	  break;
	}
        case SP_END_DD:
        {
	  if (docstat & D_DL)
	    fputs ("</entry></row>\n", out);
	  else
	    fputs ("</para></listitem>", out);
	  docstat &= ~D_DD;
	  break;
        }
	case SP_END_DL:
	{
	  fputs ("</entry></row></tbody></tgroup></informaltable>\n", out);
	  docstat &= ~(D_DD|D_DL);
	  break;
	}
	case SP_START_IL:
	{
	  if (docstat & D_PA)
	  {
	    fputs ("\n</para>\n", out);
	    docstat &= ~D_PA;
	  }
	  fputs ("\n<itemizedlist>\n", out);
	  docstat |= D_IL;
	  break;
	}
	case SP_END_IL:
	{
	  fputs ("</para></listitem></itemizedlist>\n", out);
	  docstat &= ~(D_DD|D_DL);
	  break;
	}
        case SP_END_SECT:
        {
	  fputs ("</sect2>", out);
	  break;
        }
	case SP_STR:
	{
	  if (docstat & D_TAB)
	    sgml_fputs (str, out);
	  else
	  {
	    while (*str)
	    {
	      for (; *str; str++)
	      {
	        if (!strncmp (str, "``", 2))
	        {
		  fputs ("<quote>", out);
		  str++;
	        }
	        else if (!strncmp (str, "''", 2))
	        {
		  fputs ("</quote>", out);
		  str++;
	        }
	        else
		  sgml_fputc (*str, out);
	      }
	    }
	  }
	  break;
	}
      }
      break;
    }
    /* make gcc happy (unreached) */
    default:
      break;
  }

  return docstat;
}

void print_ref (FILE *out, int output_dollar, const char *ref)
{
  switch (OutputFormat)
  {
  case F_CONF:
  case F_MAN:
    if (output_dollar)
      putc ('$', out);
    fputs (ref, out);
    break;

  case F_SGML:
    fputs ("<link linkend=\"", out);
    sgml_id_fputs (ref, out);
    fputs ("\">", out);
    if (output_dollar)
      fputc ('$', out);
    sgml_fputs (ref, out);
    fputs ("</link>", out);
    break;

  default:
    break;
  }
}

static int commit_buff (char *buff, char **d, FILE *out, int docstat)
{
  if (*d > buff)
  {
    **d = '\0';
    docstat = print_it (SP_STR, buff, out, docstat);
    *d = buff;
  }

  return docstat;
}

static int handle_docline (char *l, FILE *out, int docstat)
{
  char buff[BUFFSIZE];
  char *s, *d;
  l = skip_ws (l);

  if (Debug)
    fprintf (stderr, "%s: handle_docline `%s'\n", Progname, l);
  
  if (!strncmp (l, ".pp", 3))
    return print_it (SP_NEWPAR, NULL, out, docstat);
  else if (!strncmp (l, ".ts", 3))
    return print_it (SP_START_TAB, NULL, out, docstat);
  else if (!strncmp (l, ".te", 3))
    return print_it (SP_END_TAB, NULL, out, docstat);
  else if (!strncmp (l, ".dl", 3))
    return print_it (SP_START_DL, NULL, out, docstat);
  else if (!strncmp (l, ".de", 3))
    return print_it (SP_END_DL, NULL, out, docstat);
  else if (!strncmp (l, ".il", 3))
    return print_it (SP_START_IL, NULL, out, docstat);
  else if (!strncmp (l, ".ie", 3))
    return print_it (SP_END_IL, NULL, out, docstat);
  else if (!strncmp (l, ". ", 2))
    *l = ' ';

  for (s = l, d = buff; *s; s++)
  {
    if (!strncmp (s, "\\(as", 4))
    {
      *d++ = '*';
      s += 3;
    }
    else if (!strncmp (s, "\\(rs", 4))
    {
      *d++ = '\\';
      s += 3;
    }
    else if (!strncmp (s, "\\fI", 3))
    {
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_START_EM, NULL, out, docstat);
      s += 2;
    }
    else if (!strncmp (s, "\\fB", 3))
    {
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_START_BF, NULL, out, docstat);
      s += 2;
    }
    else if (!strncmp (s, "\\fC", 3))
    {
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_START_TT, NULL, out, docstat);
      s += 2;
    }
    else if (!strncmp (s, "\\fP", 3))
    {
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_END_FT, NULL, out, docstat);
      s += 2;
    }
    else if (!strncmp (s, ".dt", 3))
    {
      if (docstat & D_DD)
      {
	docstat = commit_buff (buff, &d, out, docstat);
	docstat = print_it (SP_END_DD, NULL, out, docstat);
      }
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_DT, NULL, out, docstat);
      s += 3;
    }
    else if (!strncmp (s, ".dd", 3))
    {
      if ((docstat & D_IL) && (docstat & D_DD))
      {
	docstat = commit_buff (buff, &d, out, docstat);
	docstat = print_it (SP_END_DD, NULL, out, docstat);
      }
      docstat = commit_buff (buff, &d, out, docstat);
      docstat = print_it (SP_DD, NULL, out, docstat);
      s += 3;
    }
    else if (*s == '$')
    {
      int output_dollar = 0;
      char *ref;
      char save;

      ++s;
      if (*s == '$')
      {
	output_dollar = 1;
	++s;
      }
      if (*s == '$')
      {
	*d++ = '$';
      }
      else
      {
	ref = s;
	while (isalnum ((unsigned char) *s) || (*s && strchr("-_<>", *s)))
	  ++s;

	docstat = commit_buff (buff, &d, out, docstat);
	save = *s;
	*s = 0;
	print_ref (out, output_dollar, ref);
	*s = save;
	--s;
      }
    }
    else
      *d++ = *s;
  }

  docstat = commit_buff (buff, &d, out, docstat);
  return print_it (SP_NEWLINE, NULL, out, docstat);
}
