/**
 * @file
 * GUI basic built-in text editor
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

/* Close approximation of the mailx(1) builtin editor for sending mail. */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/mutt.h"
#include "alias.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "options.h"
#include "protos.h"

/*
 * SLcurses_waddnstr() can't take a "const char *", so this is only
 * declared "static" (sigh)
 */
static char *EditorHelp1 =
    N_("~~              insert a line beginning with a single ~\n"
       "~b users        add users to the Bcc: field\n"
       "~c users        add users to the Cc: field\n"
       "~f messages     include messages\n"
       "~F messages     same as ~f, except also include headers\n"
       "~h              edit the message header\n"
       "~m messages     include and quote messages\n"
       "~M messages     same as ~m, except include headers\n"
       "~p              print the message\n");

static char *EditorHelp2 =
    N_("~q              write file and quit editor\n"
       "~r file         read a file into the editor\n"
       "~t users        add users to the To: field\n"
       "~u              recall the previous line\n"
       "~v              edit message with the $visual editor\n"
       "~w file         write message to file\n"
       "~x              abort changes and quit editor\n"
       "~?              this message\n"
       ".               on a line by itself ends input\n");

static char **be_snarf_data(FILE *f, char **buf, int *bufmax, int *buflen,
                            LOFF_T offset, int bytes, int prefix)
{
  char tmp[HUGE_STRING];
  char *p = tmp;
  int tmplen = sizeof(tmp);

  tmp[sizeof(tmp) - 1] = '\0';
  if (prefix)
  {
    mutt_str_strfcpy(tmp, NONULL(IndentString), sizeof(tmp));
    tmplen = mutt_str_strlen(tmp);
    p = tmp + tmplen;
    tmplen = sizeof(tmp) - tmplen;
  }

  fseeko(f, offset, SEEK_SET);
  while (bytes > 0)
  {
    if (fgets(p, tmplen - 1, f) == NULL)
      break;
    bytes -= mutt_str_strlen(p);
    if (*bufmax == *buflen)
      mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
    buf[(*buflen)++] = mutt_str_strdup(tmp);
  }
  if (buf && *bufmax == *buflen)
  { /* Do not smash memory past buf */
    mutt_mem_realloc(&buf, sizeof(char *) * (++*bufmax));
  }
  if (buf)
    buf[*buflen] = NULL;
  return buf;
}

static char **be_snarf_file(const char *path, char **buf, int *max, int *len, int verbose)
{
  char tmp[LONG_STRING];
  struct stat sb;

  FILE *f = fopen(path, "r");
  if (f)
  {
    fstat(fileno(f), &sb);
    buf = be_snarf_data(f, buf, max, len, 0, sb.st_size, 0);
    if (verbose)
    {
      snprintf(tmp, sizeof(tmp), "\"%s\" %lu bytes\n", path, (unsigned long) sb.st_size);
      addstr(tmp);
    }
    mutt_file_fclose(&f);
  }
  else
  {
    snprintf(tmp, sizeof(tmp), "%s: %s\n", path, strerror(errno));
    addstr(tmp);
  }
  return buf;
}

static int be_barf_file(const char *path, char **buf, int buflen)
{
  FILE *f = fopen(path, "w");
  if (!f)
  {
    addstr(strerror(errno));
    addch('\n');
    return -1;
  }
  for (int i = 0; i < buflen; i++)
    fputs(buf[i], f);
  if (fclose(f) == 0)
    return 0;
  printw("fclose: %s\n", strerror(errno));
  return -1;
}

static void be_free_memory(char **buf, int buflen)
{
  while (buflen-- > 0)
    FREE(&buf[buflen]);
  if (buf)
    FREE(&buf);
}

static char **be_include_messages(char *msg, char **buf, int *bufmax,
                                  int *buflen, int pfx, int inc_hdrs)
{
  int offset, bytes, n;
  char tmp[LONG_STRING];

  if (!msg || !buf || !bufmax || !buflen)
    return buf;

  while ((msg = strtok(msg, " ,")) != NULL)
  {
    if (mutt_str_atoi(msg, &n) == 0 && n > 0 && n <= Context->msgcount)
    {
      n--;

      /* add the attribution */
      if (Attribution)
      {
        setlocale(LC_TIME, NONULL(AttributionLocale));
        mutt_make_string(tmp, sizeof(tmp) - 1, Attribution, Context, Context->hdrs[n]);
        setlocale(LC_TIME, "");
        strcat(tmp, "\n");
      }

      if (*bufmax == *buflen)
        mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
      buf[(*buflen)++] = mutt_str_strdup(tmp);

      bytes = Context->hdrs[n]->content->length;
      if (inc_hdrs)
      {
        offset = Context->hdrs[n]->offset;
        bytes += Context->hdrs[n]->content->offset - offset;
      }
      else
        offset = Context->hdrs[n]->content->offset;
      buf = be_snarf_data(Context->fp, buf, bufmax, buflen, offset, bytes, pfx);

      if (*bufmax == *buflen)
        mutt_mem_realloc(&buf, sizeof(char *) * (*bufmax += 25));
      buf[(*buflen)++] = mutt_str_strdup("\n");
    }
    else
      printw(_("%d: invalid message number.\n"), n);
    msg = NULL;
  }
  return buf;
}

static void be_print_header(struct Envelope *env)
{
  char tmp[HUGE_STRING];

  if (env->to)
  {
    addstr("To: ");
    tmp[0] = '\0';
    mutt_addr_write(tmp, sizeof(tmp), env->to, true);
    addstr(tmp);
    addch('\n');
  }
  if (env->cc)
  {
    addstr("Cc: ");
    tmp[0] = '\0';
    mutt_addr_write(tmp, sizeof(tmp), env->cc, true);
    addstr(tmp);
    addch('\n');
  }
  if (env->bcc)
  {
    addstr("Bcc: ");
    tmp[0] = '\0';
    mutt_addr_write(tmp, sizeof(tmp), env->bcc, true);
    addstr(tmp);
    addch('\n');
  }
  if (env->subject)
  {
    addstr("Subject: ");
    addstr(env->subject);
    addch('\n');
  }
  addch('\n');
}

/**
 * be_edit_header - Edit the message headers
 * @param e     Message headers
 * @param force override the $ask* vars (used for the ~h command)
 */
static void be_edit_header(struct Envelope *e, int force)
{
  char tmp[HUGE_STRING];

  mutt_window_move(MuttMessageWindow, 0, 0);

  addstr("To: ");
  tmp[0] = '\0';
  mutt_addrlist_to_local(e->to);
  mutt_addr_write(tmp, sizeof(tmp), e->to, false);
  if (!e->to || force)
  {
    if (mutt_enter_string(tmp, sizeof(tmp), 4, 0) == 0)
    {
      mutt_addr_free(&e->to);
      e->to = mutt_addr_parse_list2(e->to, tmp);
      e->to = mutt_expand_aliases(e->to);
      mutt_addrlist_to_intl(e->to, NULL); /* XXX - IDNA error reporting? */
      tmp[0] = '\0';
      mutt_addr_write(tmp, sizeof(tmp), e->to, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 4, tmp);
    }
  }
  else
  {
    mutt_addrlist_to_intl(e->to, NULL); /* XXX - IDNA error reporting? */
    addstr(tmp);
  }
  addch('\n');

  if (!e->subject || force)
  {
    addstr("Subject: ");
    mutt_str_strfcpy(tmp, e->subject ? e->subject : "", sizeof(tmp));
    if (mutt_enter_string(tmp, sizeof(tmp), 9, 0) == 0)
      mutt_str_replace(&e->subject, tmp);
    addch('\n');
  }

  if ((!e->cc && Askcc) || force)
  {
    addstr("Cc: ");
    tmp[0] = '\0';
    mutt_addrlist_to_local(e->cc);
    mutt_addr_write(tmp, sizeof(tmp), e->cc, false);
    if (mutt_enter_string(tmp, sizeof(tmp), 4, 0) == 0)
    {
      mutt_addr_free(&e->cc);
      e->cc = mutt_addr_parse_list2(e->cc, tmp);
      e->cc = mutt_expand_aliases(e->cc);
      tmp[0] = '\0';
      mutt_addrlist_to_intl(e->cc, NULL);
      mutt_addr_write(tmp, sizeof(tmp), e->cc, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 4, tmp);
    }
    else
      mutt_addrlist_to_intl(e->cc, NULL);
    addch('\n');
  }

  if (Askbcc || force)
  {
    addstr("Bcc: ");
    tmp[0] = '\0';
    mutt_addrlist_to_local(e->bcc);
    mutt_addr_write(tmp, sizeof(tmp), e->bcc, false);
    if (mutt_enter_string(tmp, sizeof(tmp), 5, 0) == 0)
    {
      mutt_addr_free(&e->bcc);
      e->bcc = mutt_addr_parse_list2(e->bcc, tmp);
      e->bcc = mutt_expand_aliases(e->bcc);
      mutt_addrlist_to_intl(e->bcc, NULL);
      tmp[0] = '\0';
      mutt_addr_write(tmp, sizeof(tmp), e->bcc, true);
      mutt_window_mvaddstr(MuttMessageWindow, 0, 5, tmp);
    }
    else
      mutt_addrlist_to_intl(e->bcc, NULL);
    addch('\n');
  }
}

int mutt_builtin_editor(const char *path, struct Header *msg, struct Header *cur)
{
  char **buf = NULL;
  int bufmax = 0, buflen = 0;
  char tmp[LONG_STRING];
  bool abort = false;
  bool done = false;
  char *p = NULL;

  scrollok(stdscr, true);

  be_edit_header(msg->env, 0);

  addstr(_("(End message with a . on a line by itself)\n"));

  buf = be_snarf_file(path, buf, &bufmax, &buflen, 0);

  tmp[0] = '\0';
  while (!done)
  {
    if (mutt_enter_string(tmp, sizeof(tmp), 0, 0) == -1)
    {
      tmp[0] = '\0';
      continue;
    }
    addch('\n');

    if (Escape && tmp[0] == Escape[0] && tmp[1] != Escape[0])
    {
      /* remove trailing whitespace from the line */
      p = tmp + mutt_str_strlen(tmp) - 1;
      while (p >= tmp && ISSPACE(*p))
        *p-- = '\0';

      p = tmp + 2;
      SKIPWS(p);

      switch (tmp[1])
      {
        case '?':
          addstr(_(EditorHelp1));
          addstr(_(EditorHelp2));
          break;
        case 'b':
          msg->env->bcc = mutt_addr_parse_list2(msg->env->bcc, p);
          msg->env->bcc = mutt_expand_aliases(msg->env->bcc);
          break;
        case 'c':
          msg->env->cc = mutt_addr_parse_list2(msg->env->cc, p);
          msg->env->cc = mutt_expand_aliases(msg->env->cc);
          break;
        case 'h':
          be_edit_header(msg->env, 1);
          break;
        case 'F':
        case 'f':
        case 'm':
        case 'M':
          if (Context)
          {
            if (!*p && cur)
            {
              /* include the current message */
              p = tmp + mutt_str_strlen(tmp) + 1;
              snprintf(tmp + mutt_str_strlen(tmp),
                       sizeof(tmp) - mutt_str_strlen(tmp), " %d", cur->msgno + 1);
            }
            buf = be_include_messages(p, buf, &bufmax, &buflen, (tolower(tmp[1]) == 'm'),
                                      (isupper((unsigned char) tmp[1])));
          }
          else
            addstr(_("No mailbox.\n"));
          break;
        case 'p':
          addstr("-----\n");
          addstr(_("Message contains:\n"));
          be_print_header(msg->env);
          for (int i = 0; i < buflen; i++)
            addstr(buf[i]);
          /* L10N:
             This entry is shown AFTER the message content,
             not IN the middle of the content.
             So it doesn't mean "(message will continue)"
             but means "(press any key to continue using neomutt)". */
          addstr(_("(continue)\n"));
          break;
        case 'q':
          done = true;
          break;
        case 'r':
          if (*p)
          {
            strncpy(tmp, p, sizeof(tmp));
            mutt_expand_path(tmp, sizeof(tmp));
            buf = be_snarf_file(tmp, buf, &bufmax, &buflen, 1);
          }
          else
            addstr(_("missing filename.\n"));
          break;
        case 's':
          mutt_str_replace(&msg->env->subject, p);
          break;
        case 't':
          msg->env->to = mutt_addr_parse_list(msg->env->to, p);
          msg->env->to = mutt_expand_aliases(msg->env->to);
          break;
        case 'u':
          if (buflen)
          {
            buflen--;
            mutt_str_strfcpy(tmp, buf[buflen], sizeof(tmp));
            tmp[mutt_str_strlen(tmp) - 1] = '\0';
            FREE(&buf[buflen]);
            buf[buflen] = NULL;
            continue;
          }
          else
            addstr(_("No lines in message.\n"));
          break;

        case 'e':
        case 'v':
          if (be_barf_file(path, buf, buflen) == 0)
          {
            char *tag = NULL, *err = NULL;
            be_free_memory(buf, buflen);
            buf = NULL;
            bufmax = buflen = 0;

            if (EditHeaders)
            {
              mutt_env_to_local(msg->env);
              mutt_edit_headers(NONULL(Visual), path, msg, NULL, 0);
              if (mutt_env_to_intl(msg->env, &tag, &err))
                printw(_("Bad IDN in %s: '%s'\n"), tag, err);
              /* tag is a statically allocated string and should not be freed */
              FREE(&err);
            }
            else
              mutt_edit_file(NONULL(Visual), path);

            buf = be_snarf_file(path, buf, &bufmax, &buflen, 0);

            addstr(_("(continue)\n"));
          }
          break;
        case 'w':
          be_barf_file(*p ? p : path, buf, buflen);
          break;
        case 'x':
          abort = true;
          done = true;
          break;
        default:
          printw(_("%s: unknown editor command (~? for help)\n"), tmp);
          break;
      }
    }
    else if (mutt_str_strcmp(".", tmp) == 0)
      done = true;
    else
    {
      mutt_str_strcat(tmp, sizeof(tmp), "\n");
      if (buflen == bufmax)
        mutt_mem_realloc(&buf, sizeof(char *) * (bufmax += 25));
      buf[buflen++] = mutt_str_strdup(tmp[1] == '~' ? tmp + 1 : tmp);
    }

    tmp[0] = '\0';
  }

  if (!abort)
    be_barf_file(path, buf, buflen);
  be_free_memory(buf, buflen);

  return (abort ? -1 : 0);
}
