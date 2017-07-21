/**
 * @file
 * RFC1524 Mailcap routines
 *
 * @authors
 * Copyright (C) 1996-2000,2003,2012 Michael R. Elkins <me@mutt.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt.h"
#include "rfc1524.h"
#include "ascii.h"
#include "body.h"
#include "globals.h"
#include "lib.h"
#include "options.h"
#include "protos.h"

/**
 * rfc1524_expand_command - Expand expandos in a command
 *
 * The command semantics include the following:
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
int rfc1524_expand_command(struct Body *a, char *filename, char *_type, char *command, int clen)
{
  int x = 0, y = 0;
  int needspipe = true;
  char buf[LONG_STRING];
  char type[LONG_STRING];

  strfcpy(type, _type, sizeof(type));

  if (option(OPTMAILCAPSANITIZE))
    mutt_sanitize_filename(type, 0);

  while (x < clen - 1 && command[x] && y < sizeof(buf) - 1)
  {
    if (command[x] == '\\')
    {
      x++;
      buf[y++] = command[x++];
    }
    else if (command[x] == '%')
    {
      x++;
      if (command[x] == '{')
      {
        char param[STRING];
        char pvalue[STRING];
        char *_pvalue = NULL;
        int z = 0;

        x++;
        while (command[x] && command[x] != '}' && z < sizeof(param) - 1)
          param[z++] = command[x++];
        param[z] = '\0';

        _pvalue = mutt_get_parameter(param, a->parameter);
        strfcpy(pvalue, NONULL(_pvalue), sizeof(pvalue));
        if (option(OPTMAILCAPSANITIZE))
          mutt_sanitize_filename(pvalue, 0);

        y += mutt_quote_filename(buf + y, sizeof(buf) - y, pvalue);
      }
      else if (command[x] == 's' && filename != NULL)
      {
        y += mutt_quote_filename(buf + y, sizeof(buf) - y, filename);
        needspipe = false;
      }
      else if (command[x] == 't')
      {
        y += mutt_quote_filename(buf + y, sizeof(buf) - y, type);
      }
      x++;
    }
    else
      buf[y++] = command[x++];
  }
  buf[y] = '\0';
  strfcpy(command, buf, clen);

  return needspipe;
}

/**
 * get_field - NUL terminate a RFC1524 field
 * @param s String to alter
 * @retval ptr  Start of next field
 * @retval NULL Error
 */
static char *get_field(char *s)
{
  char *ch = NULL;

  if (!s)
    return NULL;

  while ((ch = strpbrk(s, ";\\")) != NULL)
  {
    if (*ch == '\\')
    {
      s = ch + 1;
      if (*s)
        s++;
    }
    else
    {
      *ch = 0;
      ch = skip_email_wsp(ch + 1);
      break;
    }
  }
  mutt_remove_trailing_ws(s);
  return ch;
}

static int get_field_text(char *field, char **entry, char *type, char *filename, int line)
{
  field = mutt_skip_whitespace(field);
  if (*field == '=')
  {
    if (entry)
    {
      field++;
      field = mutt_skip_whitespace(field);
      mutt_str_replace(entry, field);
    }
    return 1;
  }
  else
  {
    mutt_error(_("Improperly formatted entry for type %s in \"%s\" line %d"),
               type, filename, line);
    return 0;
  }
}

static int rfc1524_mailcap_parse(struct Body *a, char *filename, char *type,
                                 struct Rfc1524MailcapEntry *entry, int opt)
{
  FILE *fp = NULL;
  char *buf = NULL;
  size_t buflen;
  char *ch = NULL;
  char *field = NULL;
  int found = false;
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
  if ((ch = strchr(type, '/')) == NULL)
    return false;
  btlen = ch - type;

  if ((fp = fopen(filename, "r")) != NULL)
  {
    while (!found && (buf = mutt_read_line(buf, &buflen, fp, &line, MUTT_CONT)) != NULL)
    {
      /* ignore comments */
      if (*buf == '#')
        continue;
      mutt_debug(2, "mailcap entry: %s\n", buf);

      /* check type */
      ch = get_field(buf);
      if ((ascii_strcasecmp(buf, type) != 0) &&
          ((ascii_strncasecmp(buf, type, btlen) != 0) ||
           (buf[btlen] != 0 &&                       /* implicit wild */
            (mutt_strcmp(buf + btlen, "/*") != 0)))) /* wildsubtype */
        continue;

      /* next field is the viewcommand */
      field = ch;
      ch = get_field(ch);
      if (entry)
        entry->command = safe_strdup(field);

      /* parse the optional fields */
      found = true;
      copiousoutput = false;
      composecommand = false;
      editcommand = false;
      printcommand = false;

      while (ch)
      {
        field = ch;
        ch = get_field(ch);
        mutt_debug(2, "field: %s\n", field);

        if (ascii_strcasecmp(field, "needsterminal") == 0)
        {
          if (entry)
            entry->needsterminal = true;
        }
        else if (ascii_strcasecmp(field, "copiousoutput") == 0)
        {
          copiousoutput = true;
          if (entry)
            entry->copiousoutput = true;
        }
        else if (ascii_strncasecmp(field, "composetyped", 12) == 0)
        {
          /* this compare most occur before compose to match correctly */
          if (get_field_text(field + 12, entry ? &entry->composetypecommand : NULL,
                             type, filename, line))
            composecommand = true;
        }
        else if (ascii_strncasecmp(field, "compose", 7) == 0)
        {
          if (get_field_text(field + 7, entry ? &entry->composecommand : NULL,
                             type, filename, line))
            composecommand = true;
        }
        else if (ascii_strncasecmp(field, "print", 5) == 0)
        {
          if (get_field_text(field + 5, entry ? &entry->printcommand : NULL,
                             type, filename, line))
            printcommand = true;
        }
        else if (ascii_strncasecmp(field, "edit", 4) == 0)
        {
          if (get_field_text(field + 4, entry ? &entry->editcommand : NULL, type, filename, line))
            editcommand = true;
        }
        else if (ascii_strncasecmp(field, "nametemplate", 12) == 0)
        {
          get_field_text(field + 12, entry ? &entry->nametemplate : NULL, type,
                         filename, line);
        }
        else if (ascii_strncasecmp(field, "x-convert", 9) == 0)
        {
          get_field_text(field + 9, entry ? &entry->convert : NULL, type, filename, line);
        }
        else if (ascii_strncasecmp(field, "test", 4) == 0)
        {
          /*
           * This routine executes the given test command to determine
           * if this is the right entry.
           */
          char *test_command = NULL;
          size_t len;

          if (get_field_text(field + 4, &test_command, type, filename, line) && test_command)
          {
            len = mutt_strlen(test_command) + STRING;
            safe_realloc(&test_command, len);
            rfc1524_expand_command(a, a->filename, type, test_command, len);
            if (mutt_system(test_command))
            {
              /* a non-zero exit code means test failed */
              found = false;
            }
            FREE(&test_command);
          }
        }
      } /* while (ch) */

      if (opt == MUTT_AUTOVIEW)
      {
        if (!copiousoutput)
          found = false;
      }
      else if (opt == MUTT_COMPOSE)
      {
        if (!composecommand)
          found = false;
      }
      else if (opt == MUTT_EDIT)
      {
        if (!editcommand)
          found = false;
      }
      else if (opt == MUTT_PRINT)
      {
        if (!printcommand)
          found = false;
      }

      if (!found)
      {
        /* reset */
        if (entry)
        {
          FREE(&entry->command);
          FREE(&entry->composecommand);
          FREE(&entry->composetypecommand);
          FREE(&entry->editcommand);
          FREE(&entry->printcommand);
          FREE(&entry->nametemplate);
          FREE(&entry->convert);
          entry->needsterminal = false;
          entry->copiousoutput = false;
        }
      }
    } /* while (!found && (buf = mutt_read_line ())) */
    safe_fclose(&fp);
  } /* if ((fp = fopen ())) */
  FREE(&buf);
  return found;
}

/**
 * rfc1524_new_entry - Allocate memory for a new rfc1524 entry
 * @retval ptr An un-initialized struct Rfc1524MailcapEntry
 */
struct Rfc1524MailcapEntry *rfc1524_new_entry(void)
{
  return safe_calloc(1, sizeof(struct Rfc1524MailcapEntry));
}

/**
 * rfc1524_free_entry - Deallocate an struct Rfc1524MailcapEntry
 * @param entry Rfc1524MailcapEntry to deallocate
 */
void rfc1524_free_entry(struct Rfc1524MailcapEntry **entry)
{
  struct Rfc1524MailcapEntry *p = *entry;

  FREE(&p->command);
  FREE(&p->testcommand);
  FREE(&p->composecommand);
  FREE(&p->composetypecommand);
  FREE(&p->editcommand);
  FREE(&p->printcommand);
  FREE(&p->nametemplate);
  FREE(entry);
}

/**
 * rfc1524_mailcap_lookup - Find given type in the list of mailcap files
 * @param a      Message body
 * @param type   Text type in "type/subtype" format
 * @param entry  struct Rfc1524MailcapEntry to populate with results
 * @param opt    Type of mailcap entry to lookup
 * @retval 1 on success. If *entry is not NULL it poplates it with the mailcap entry
 * @retval 0 if no matching entry is found
 *
 * opt can be one of: #MUTT_EDIT, #MUTT_COMPOSE, #MUTT_PRINT, #MUTT_AUTOVIEW
 *
 * Find the given type in the list of mailcap files.
 */
int rfc1524_mailcap_lookup(struct Body *a, char *type,
                           struct Rfc1524MailcapEntry *entry, int opt)
{
  char path[_POSIX_PATH_MAX];
  int x;
  int found = false;
  char *curr = MailcapPath;

  /* rfc1524 specifies that a path of mailcap files should be searched.
   * joy.  They say
   * $HOME/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap, etc
   * and overridden by the MAILCAPS environment variable, and, just to be nice,
   * we'll make it specifiable in .muttrc
   */
  if (!curr || !*curr)
  {
    mutt_error(_("No mailcap path specified"));
    return 0;
  }

  mutt_check_lookup_list(a, type, SHORT_STRING);

  while (!found && *curr)
  {
    x = 0;
    while (*curr && *curr != ':' && x < sizeof(path) - 1)
    {
      path[x++] = *curr;
      curr++;
    }
    if (*curr)
      curr++;

    if (!x)
      continue;

    path[x] = '\0';
    mutt_expand_path(path, sizeof(path));

    mutt_debug(2, "Checking mailcap file: %s\n", path);
    found = rfc1524_mailcap_parse(a, path, type, entry, opt);
  }

  if (entry && !found)
    mutt_error(_("mailcap entry for type %s not found"), type);

  return found;
}

static void strnfcpy(char *d, char *s, size_t siz, size_t len)
{
  if (len > siz)
    len = siz - 1;
  strfcpy(d, s, len);
}

/**
 * rfc1524_expand_filename - Expand a new filename from a template or existing filename
 * @param nametemplate Template
 * @param oldfile      Original filename
 * @param newfile      Buffer for new filename
 * @param nflen        Buffer length
 * @retval 0 if the left and right components of the oldfile and newfile match
 * @retval 1 otherwise
 *
 * If there is no nametemplate, the stripped oldfile name is used as the
 * template for newfile.
 *
 * If there is no oldfile, the stripped nametemplate name is used as the
 * template for newfile.
 *
 * If both a nametemplate and oldfile are specified, the template is checked
 * for a "%s". If none is found, the nametemplate is used as the template for
 * newfile.  The first path component of the nametemplate and oldfile are ignored.
 *
 */
int rfc1524_expand_filename(char *nametemplate, char *oldfile, char *newfile, size_t nflen)
{
  int i, j, k, ps;
  char *s = NULL;
  bool lmatch = false, rmatch = false;
  char left[_POSIX_PATH_MAX];
  char right[_POSIX_PATH_MAX];

  newfile[0] = 0;

  /* first, ignore leading path components */

  if (nametemplate && (s = strrchr(nametemplate, '/')))
    nametemplate = s + 1;

  if (oldfile && (s = strrchr(oldfile, '/')))
    oldfile = s + 1;

  if (!nametemplate)
  {
    if (oldfile)
      strfcpy(newfile, oldfile, nflen);
  }
  else if (!oldfile)
  {
    mutt_expand_fmt(newfile, nflen, nametemplate, "mutt");
  }
  else /* oldfile && nametemplate */
  {
    /* first, compare everything left from the "%s"
     * (if there is one).  */

    lmatch = true;
    ps = 0;
    for (i = 0; nametemplate[i]; i++)
    {
      if (nametemplate[i] == '%' && nametemplate[i + 1] == 's')
      {
        ps = 1;
        break;
      }

      /* note that the following will _not_ read beyond oldfile's end. */

      if (lmatch && nametemplate[i] != oldfile[i])
        lmatch = false;
    }

    if (ps)
    {
      /* If we had a "%s", check the rest. */

      /* now, for the right part: compare everything right from
       * the "%s" to the final part of oldfile.
       *
       * The logic here is as follows:
       *
       * - We start reading from the end.
       * - There must be a match _right_ from the "%s",
       *   thus the i + 2.
       * - If there was a left hand match, this stuff
       *   must not be counted again.  That's done by the
       *   condition (j >= (lmatch ? i : 0)).
       */

      rmatch = true;

      for (j = mutt_strlen(oldfile) - 1, k = mutt_strlen(nametemplate) - 1;
           j >= (lmatch ? i : 0) && k >= i + 2; j--, k--)
      {
        if (nametemplate[k] != oldfile[j])
        {
          rmatch = false;
          break;
        }
      }

      /* Now, check if we had a full match. */

      if (k >= i + 2)
        rmatch = false;

      if (lmatch)
        *left = 0;
      else
        strnfcpy(left, nametemplate, sizeof(left), i);

      if (rmatch)
        *right = 0;
      else
        strfcpy(right, nametemplate + i + 2, sizeof(right));

      snprintf(newfile, nflen, "%s%s%s", left, oldfile, right);
    }
    else
    {
      /* no "%s" in the name template. */
      strfcpy(newfile, nametemplate, nflen);
    }
  }

  mutt_adv_mktemp(newfile, nflen);

  if (rmatch && lmatch)
    return 0;
  else
    return 1;
}

/* If rfc1524_expand_command() is used on a recv'd message, then
 * the filename doesn't exist yet, but if it's used while sending a message,
 * then we need to rename the existing file.
 *
 * This function returns 0 on successful move, 1 on old file doesn't exist,
 * 2 on new file already exists, and 3 on other failure.
 */

/* note on access(2) use: No dangling symlink problems here due to
 * safe_fopen().
 */

int mutt_rename_file(char *oldfile, char *newfile)
{
  FILE *ofp = NULL, *nfp = NULL;

  if (access(oldfile, F_OK) != 0)
    return 1;
  if (access(newfile, F_OK) == 0)
    return 2;
  if ((ofp = fopen(oldfile, "r")) == NULL)
    return 3;
  if ((nfp = safe_fopen(newfile, "w")) == NULL)
  {
    safe_fclose(&ofp);
    return 3;
  }
  mutt_copy_stream(ofp, nfp);
  safe_fclose(&nfp);
  safe_fclose(&ofp);
  mutt_unlink(oldfile);
  return 0;
}
