/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* 
 * rfc1524 defines a format for the Multimedia Mail Configuration, which
 * is the standard mailcap file format under Unix which specifies what 
 * external programs should be used to view/compose/edit multimedia files
 * based on content type.
 *
 * This file contains various functions for implementing a fair subset of 
 * rfc1524.
 */

#include "mutt.h"
#include "rfc1524.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/* The command semantics include the following:
 * %s is the filename that contains the mail body data
 * %t is the content type, like text/plain
 * %{parameter} is replaced by the parameter value from the content-type field
 * \% is %
 * Unsupported rfc1524 parameters: these would probably require some doing
 * by mutt, and can probably just be done by piping the message to metamail
 * %n is the integer number of sub-parts in the multipart
 * %F is "content-type filename" repeated for each sub-part
 *
 * In addition, this function returns a 0 if the command works on a file,
 * and 1 if the command works on a pipe.
 */
int rfc1524_expand_command (BODY *a, char *filename, char *type, 
    char *command, int clen)
{
  int x=0,y=0;
  int needspipe = TRUE;
  char buf[LONG_STRING];

  while (command[x] && x<clen && y<sizeof(buf)) {
    if (command[x] == '\\') {
      x++;
      buf[y++] = command[x++];
    }
    else if (command[x] == '%') {
      x++;
      if (command[x] == '{') {
	char param[STRING];
	int z = 0;
	char *ret = NULL;

	x++;
	while (command[x] && command[x] != '}' && z<sizeof(param))
	  param[z++] = command[x++];
	param[z] = '\0';
	dprint(2,(debugfile,"Parameter: %s  Returns: %s\n",param,ret));
	ret = mutt_get_parameter(param,a->parameter);
	dprint(2,(debugfile,"Parameter: %s  Returns: %s\n",param,ret));
	z = 0;
	while (ret && ret[z] && y<sizeof(buf))
	  buf[y++] = ret[z++];
      }
      else if (command[x] == 's' && filename != NULL)
      {
	char *fn = filename;

	while (*fn && y < sizeof (buf))
	  buf[y++] = *fn++;
	needspipe = FALSE;
      }
      else if (command[x] == 't')
      {
	while (*type && y < sizeof (buf))
	  buf[y++] = *type++;
      }
      x++;
    }
    else
      buf[y++] = command[x++];
  }
  buf[y] = '\0';
  strfcpy (command, buf, clen);

  return needspipe;
}

/* NUL terminates a rfc 1524 field,
 * returns start of next field or NULL */
static char *get_field (char *s)
{
  char *ch;

  if (!s)
    return NULL;

  while ((ch = strpbrk (s, ";\\")) != NULL)
  {
    if (*ch == '\\')
    {
      s = ch + 1;
      if (*s)
	s++;
    }
    else
    {
      *ch++ = 0;
      SKIPWS (ch);
      break;
    }
  }
  mutt_remove_trailing_ws (s);
  return ch;
}

static int get_field_text (char *field, char **entry,
			   char *type, char *filename, int line)
{
  field = mutt_skip_whitespace (field);
  if (*field == '=')
  {
    if (entry)
    {
      field = mutt_skip_whitespace (++field);
      safe_free ((void **) entry);
      *entry = safe_strdup (field);
    }
    return 1;
  }
  else 
  {
    mutt_error ("Improperly formated entry for type %s in \"%s\" line %d",
		type, filename, line);
    return 0;
  }
}

static int rfc1524_mailcap_parse (BODY *a,
				  char *filename,
				  char *type, 
				  rfc1524_entry *entry,
				  int opt)
{
  FILE *fp;
  char *buf = NULL;
  size_t buflen;
  char *ch;
  char *field;
  int found = FALSE;
  int copiousoutput;
  int composecommand;
  int editcommand;
  int printcommand;
  int btlen;
  int line = 0;

  /* rfc1524 mailcap file is of the format:
   * base/type; command; extradefs
   * type can be * for matching all
   * base with no /type is an implicit wild
   * command contains a %s for the filename to pass, default to pipe on stdin
   * extradefs are of the form:
   *  def1="definition"; def2="define \;";
   * line wraps with a \ at the end of the line
   * # for comments
   */

  /* find length of basetype */
  if ((ch = strchr (type, '/')) == NULL)
    return FALSE;
  btlen = ch - type;

  if ((fp = fopen (filename, "r")) != NULL)
  {
    while (!found && (buf = mutt_read_line (buf, &buflen, fp, &line)) != NULL)
    {
      /* ignore comments */
      if (*buf == '#')
	continue;
      dprint (2, (debugfile, "mailcap entry: %s\n", buf));

      /* check type */
      ch = get_field (buf);
      if (strcasecmp (buf, type) &&
	  (strncasecmp (buf, type, btlen) ||
	   (buf[btlen] != 0 &&			/* implicit wild */
	    strcmp (buf + btlen, "/*"))))	/* wildsubtype */
	continue;

      /* next field is the viewcommand */
      field = ch;
      ch = get_field (ch);
      if (entry)
	entry->command = safe_strdup (field);

      /* parse the optional fields */
      found = TRUE;
      copiousoutput = FALSE;
      composecommand = FALSE;
      editcommand = FALSE;
      printcommand = FALSE;

      while (ch)
      {
	field = ch;
	ch = get_field (ch);
	dprint (2, (debugfile, "field: %s\n", field));

	if (!strcasecmp (field, "needsterminal"))
	{
	  if (entry)
	    entry->needsterminal = TRUE;
	}
	else if (!strcasecmp (field, "copiousoutput"))
	{
	  copiousoutput = TRUE;
	  if (entry)
	    entry->copiousoutput = TRUE;
	}
	else if (!strncasecmp (field, "composetyped", 12))
	{
	  /* this compare most occur before compose to match correctly */
	  if (get_field_text (field + 12, entry ? &entry->composetypecommand : NULL,
			      type, filename, line))
	    composecommand = TRUE;
	}
	else if (!strncasecmp (field, "compose", 7))
	{
	  if (get_field_text (field + 7, entry ? &entry->composecommand : NULL,
			      type, filename, line))
	    composecommand = TRUE;
	}
	else if (!strncasecmp (field, "print", 5))
	{
	  if (get_field_text (field + 5, entry ? &entry->printcommand : NULL,
			      type, filename, line))
	    printcommand = TRUE;
	}
	else if (!strncasecmp (field, "edit", 4))
	{
	  if (get_field_text (field + 4, entry ? &entry->editcommand : NULL,
			      type, filename, line))
	    editcommand = TRUE;
	}
	else if (!strncasecmp (field, "nametemplate", 12))
	{
	  get_field_text (field + 12, entry ? &entry->nametemplate : NULL,
			  type, filename, line);
	}
	else if (!strncasecmp (field, "x-convert", 9))
	{
	  get_field_text (field + 9, entry ? &entry->convert : NULL,
			  type, filename, line);
	}
	else if (!strncasecmp (field, "test", 4))
	{
	  /* 
	   * This routine executes the given test command to determine
	   * if this is the right entry.
	   */
	  char *test_command = NULL;
	  size_t len;

	  if (get_field_text (field + 4, &test_command, type, filename, line)
	      && test_command)
	  {
	    len = strlen (test_command) + STRING;
	    safe_realloc ((void **) &test_command, len);
	    rfc1524_expand_command (a, NULL, type, test_command, len);
	    if (mutt_system (test_command))
	    {
	      /* a non-zero exit code means test failed */
	      found = FALSE;
	    }
	    free (test_command);
	  }
	}
      } /* while (ch) */

      if (opt == M_AUTOVIEW)
      {
	if (!copiousoutput)
	  found = FALSE;
      }
      else if (opt == M_COMPOSE)
      {
	if (!composecommand)
	  found = FALSE;
      }
      else if (opt == M_EDIT)
      {
	if (!editcommand)
	  found = FALSE;
      }
      else if (opt == M_PRINT)
      {
	if (!printcommand)
	  found = FALSE;
      }
      
      if (!found)
      {
	/* reset */
	if (entry)
	{
	  safe_free ((void **) &entry->command);
	  safe_free ((void **) &entry->composecommand);
	  safe_free ((void **) &entry->composetypecommand);
	  safe_free ((void **) &entry->editcommand);
	  safe_free ((void **) &entry->printcommand);
	  safe_free ((void **) &entry->nametemplate);
	  safe_free ((void **) &entry->convert);
	  entry->needsterminal = 0;
	  entry->copiousoutput = 0;
	}
      }
    } /* while (!found && (buf = mutt_read_line ())) */
    fclose (fp);
  } /* if ((fp = fopen ())) */
  safe_free ((void **) &buf);
  return found;
}

rfc1524_entry *rfc1524_new_entry()
{
  rfc1524_entry *tmp;

  tmp = (rfc1524_entry *)safe_malloc(sizeof(rfc1524_entry));
  memset(tmp,0,sizeof(rfc1524_entry));

  return tmp;
}

void rfc1524_free_entry(rfc1524_entry **entry)
{
  rfc1524_entry *p = *entry;

  safe_free((void **)&p->command);
  safe_free((void **)&p->testcommand);
  safe_free((void **)&p->composecommand);
  safe_free((void **)&p->composetypecommand);
  safe_free((void **)&p->editcommand);
  safe_free((void **)&p->printcommand);
  safe_free((void **)&p->nametemplate);
  safe_free((void **)entry);
}

/*
 * rfc1524_mailcap_lookup attempts to find the given type in the
 * list of mailcap files.  On success, this returns the entry information
 * in *entry, and returns 1.  On failure (not found), returns 0.
 * If entry == NULL just return 1 if the given type is found.
 */
int rfc1524_mailcap_lookup (BODY *a, char *type, rfc1524_entry *entry, int opt)
{
  char path[_POSIX_PATH_MAX];
  int x;
  int found = FALSE;
  char *curr = MailcapPath;

  /* rfc1524 specifies that a path of mailcap files should be searched.
   * joy.  They say 
   * $HOME/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap, etc
   * and overriden by the MAILCAPS environment variable, and, just to be nice, 
   * we'll make it specifiable in .muttrc
   */
  if (!*curr)
  {
    mutt_error ("No mailcap path specified");
    return 0;
  }

  while (!found && *curr)
  {
    x = 0;
    while (*curr && *curr != ':' && x < sizeof (path) - 1)
    {
      path[x++] = *curr;
      curr++;
    }
    if (*curr)
      curr++;

    if (!x)
      continue;
    
    path[x] = '\0';
    mutt_expand_path (path, sizeof (path));

    dprint(2,(debugfile,"Checking mailcap file: %s\n",path));
    found = rfc1524_mailcap_parse (a, path, type, entry, opt);
  }

  if (entry && !found)
    mutt_error ("mailcap entry for type %s not found", type);

  return found;
}

/* Modified by blong to accept a "suggestion" for file name.  If
 * that file exists, then construct one with unique name but 
 * keep any extension.  This might fail, I guess.
 * Renamed to mutt_adv_mktemp so I only have to change where it's
 * called, and not all possible cases.
 */
void mutt_adv_mktemp (char *s)
{
  char buf[_POSIX_PATH_MAX];
  char tmp[_POSIX_PATH_MAX];
  char *period;

  strfcpy (buf, NONULL (Tempdir), sizeof (buf));
  mutt_expand_path (buf, sizeof (buf));
  if (s[0] == '\0')
  {
    sprintf (s, "%s/muttXXXXXX", buf);
    mktemp (s);
  }
  else
  {
    strfcpy (tmp, s, sizeof (tmp));
    sprintf (s, "%s/%s", buf, tmp);
    if (access (s, F_OK) != 0)
      return;
    if ((period = strrchr (tmp, '.')) != NULL)
      *period = 0;
    sprintf (s, "%s/%s.XXXXXX", buf, tmp);
    mktemp (s);
    if (period != NULL)
    {
      *period = '.';
      strcat (s, period);
    }
  }
}

/* This routine expands the filename given to match the format of the
 * nametemplate given.  It returns various values based on what operations
 * it performs.
 *
 * Returns 0 if oldfile is fine as is.
 * Returns 1 if newfile specified
 */

int rfc1524_expand_filename (char *nametemplate,
			     char *oldfile, 
			     char *newfile,
			     size_t nflen)
{
  int z = 0;
  int i = 0, j = 0;
  int lmatch = TRUE;
  int match = TRUE;
  size_t len = 0;
  char *s;

  newfile[0] = 0;

  if (nametemplate && (s = strrchr (nametemplate, '/')))
    nametemplate = s + 1;

  if (oldfile && (s = strrchr (oldfile, '/')))
  {
    len = s - oldfile + 1;
    if (len > nflen)
      len = nflen;
    strfcpy (newfile, oldfile, len + 1);
    oldfile += len;
  }

  /* If nametemplate is NULL, create a newfile from oldfile and return 0 */
  if (!nametemplate)
  {
    if (oldfile)
      strfcpy (newfile, oldfile, nflen);
    mutt_adv_mktemp (newfile);
    return 0;
  }

  /* If oldfile is NULL, just return a newfile name */
  if (!oldfile)
  {
    snprintf (newfile, nflen, nametemplate, "mutt");
    mutt_adv_mktemp (newfile);
    return 0;
  }

  /* Next, attempt to determine if the oldfile already matches nametemplate */
  /* Nametemplate is of the form pre%spost, only replace pre or post if
   * they don't already match the oldfilename */
  /* Test pre */

  if ((s = strrchr (nametemplate, '%')) != NULL)
  {
    newfile[len] = '\0';

    z = s - nametemplate;

    for (i = 0; i < z && i < nflen; i++)
    {
      if (oldfile[i] != nametemplate[i])
      {
	lmatch=FALSE;
	break;
      }
    }

    if (!lmatch)
    {
      match = FALSE;
      i = nflen - len;
      if (i > z)
	i = z;
      strfcpy (newfile + len, nametemplate, i);
    }

    strfcpy (newfile + strlen (newfile), 
	    oldfile, nflen - strlen (newfile));

    dprint (1, (debugfile,"template: %s, oldfile: %s, newfile: %s\n",
	      nametemplate, oldfile, newfile));

    /* test post */
    lmatch = TRUE;

    for (z += 2, i = strlen (oldfile) - 1, j = strlen (nametemplate) - 1; 
	i && j > z; i--, j--)
      if (oldfile[i] != nametemplate[j])
      {
	lmatch = FALSE;
	break;
      }

    if (!lmatch)
    {
      match = FALSE;
      strfcpy (newfile + strlen (newfile),
	      nametemplate + z, nflen - strlen (newfile));
    }

    if (match) 
      return 0;

    return 1;
  }
  else
  {
    /* no %s in nametemplate, graft unto path of oldfile */
    strfcpy (newfile, nametemplate, nflen);
    return 1;
  }
}

/* For nametemplate support, we may need to rename a file.
 * If rfc1524_expand_command() is used on a recv'd message, then
 * the filename doesn't exist yet, but if its used while sending a message,
 * then we need to rename the existing file.
 *
 * This function returns 0 on successful move, 1 on old file doesn't exist,
 * 2 on new file already exists, and 3 on other failure.
 */
int mutt_rename_file (char *oldfile, char *newfile)
{
  FILE *ofp, *nfp;

  if (access (oldfile, F_OK) != 0)
    return 1;
  if (access (newfile, F_OK) == 0)
    return 2;
  if ((ofp = fopen (oldfile,"r")) == NULL)
    return 3;
  if ((nfp = safe_fopen (newfile,"w")) == NULL)
  {
    fclose(ofp);
    return 3;
  }
  mutt_copy_stream (ofp,nfp);
  fclose (nfp);
  fclose (ofp);
  mutt_unlink (oldfile);
  return 0;
}
