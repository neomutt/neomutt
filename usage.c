/**
 * @file
 * Display Usage Information for NeoMutt
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

/**
 * @page neo_usage Display Usage Information for NeoMutt
 *
 * Display Usage Information for NeoMutt
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "cli/lib.h"
#include "muttlib.h"

/**
 * print_header - Print a section header
 * @param section   Help section
 * @param desc      Description
 * @param use_color Highlight parts of the text
 */
static void print_header(const char *section, const char *desc, bool use_color)
{
  if (use_color)
    printf("\033[1;4m%s\033[0m: %s\n", section, desc);
  else
    printf("%s: %s\n", section, desc);
}

/**
 * show_cli_overview - Display NeoMutt command line
 * @param use_color Highlight parts of the text
 */
static void show_cli_overview(bool use_color)
{
  // L10N: Help Overview -- `neomutt -h`
  //       The modes are: help, info, send, tui
  //       Plus two helper modes: shared, all
  //       These should not be translated.
  printf("%s ", _("NeoMutt has four modes of operation:"));
  if (use_color)
    puts("\033[1;4mhelp\033[0m, \033[1;4minfo\033[0m, \033[1;4msend\033[0m, \033[1;4mtui\033[0m");
  else
    puts("help, info, send, tui");

  puts(_("The default mode, if no command line arguments are specified, is tui."));
  puts("");

  // L10N: Parameters in <>s may be translated
  print_header("shared", _("Options that apply to all modes"), use_color);
  puts(_("neomutt -n                           Don't read system config file"));
  puts(_("        -F <config>                  Use this user config file"));
  puts(_("        -e <command>                 Run extra commands"));
  puts(_("        -m <type>                    Set default mailbox type"));
  puts(_("        -d <level>                   Set logging level (1..5)"));
  puts(_("        -l <file>                    Set logging file"));
  puts("");

  print_header("help", _("Get command line help for NeoMutt"), use_color);
  puts(_("neomutt -h <mode>                    Detailed help for a mode"));
  puts(_("        -v[v]                        Version or license"));
  puts("");

  print_header("info", _("Ask NeoMutt for config information"), use_color);
  puts(_("neomutt -A <alias> [...]             Lookup email aliases"));
  puts(_("        -D [-D] [-O] [-S]            Dump the config"));
  puts(_("        -Q <option> [...] [-O] [-S]  Query config options"));
  puts("");

  print_header("send", _("Send an email from the command line"), use_color);
  puts(_("neomutt -a <file> [...]              Attach files"));
  puts(_("        -b <address>                 Add Bcc: address"));
  puts(_("        -C                           Use crypto (signing/encryption)"));
  puts(_("        -c <address>                 Add Cc: address"));
  puts(_("        -E                           Edit message"));
  puts(_("        -H <draft>                   Use draft email"));
  puts(_("        -i <include>                 Include body file"));
  puts(_("        -s <subject>                 Set Subject:"));
  puts(_("        -- <address> [...]           Add To: addresses"));
  puts("");

  print_header("tui", _("Start NeoMutt's TUI (Terminal User Interface)"), use_color);
  puts(_("neomutt                              Start NeoMutt's TUI"));
  puts(_("        -f <mailbox>                 Open this mailbox"));
  puts(_("        -G                           Open NNTP browser"));
  puts(_("        -g <server>                  Use this NNTP server"));
  puts(_("        -p                           Resume postponed email"));
  puts(_("        -R                           Open mailbox read-only"));
  puts(_("        -y                           Open mailbox browser"));
  puts(_("        -Z                           Check for new mail"));
  puts(_("        -z                           Check for any mail"));
  puts("");

  if (use_color)
    printf("%s \033[1m%s\033[0m\n", _("For detailed help, run:"), "neomutt -h all");
  else
    printf("%s %s\n", _("For detailed help, run:"), "neomutt -h all");
}

/**
 * show_cli_shared - Show Command Line Help for Shared
 * @param use_color Highlight parts of the text
 */
static void show_cli_shared(bool use_color)
{
  // L10N: Shared Help -- `neomutt -h shared`
  print_header("shared", _("Options that apply to all modes"), use_color);
  puts("");

  puts(_("By default NeoMutt loads one system and one user config file,"));
  puts(_("e.g. /etc/neomuttrc and ~/.neomuttrc"));

  // L10N: Parameters in <>s may be translated
  puts(_("  -n            Don't read system config file"));
  puts(_("  -F <config>   Use this user config file"));
  puts(_("                May be used multiple times"));
  puts("");

  puts(_("These options override the config:"));
  puts(_("  -m <type>     Set default mailbox type"));
  puts(_("                May be: maildir, mbox, mh, mmdf"));
  puts(_("  -e <command>  Run extra commands"));
  puts(_("                May be used multiple times"));
  puts("");

  puts(_("These logging options override the config:"));
  puts(_("  -d <level>    Set logging level"));
  puts(_("                0 (off), 1 (low) .. 5 (high)"));
  puts(_("  -l <file>     Set logging file"));
  puts(_("                Default file '~/.neomuttdebug0'"));
  puts("");

  if (use_color)
    printf("\033[1m%s\033[0m\n", _("Examples:"));
  else
    printf("%s\n", _("Examples:"));

  // L10N: Examples: Filenames may be translated
  puts(_("  neomutt -n"));
  puts(_("  neomutt -F work.rc"));
  puts(_("  neomutt -F work.rc -F colours.rc"));
  puts("");

  puts(_("  neomutt -m maildir"));
  puts(_("  neomutt -e 'set ask_cc = yes'"));
  puts("");

  puts(_("  neomutt -d 2"));
  puts(_("  neomutt -d 5 -l neolog"));
  puts("");

  puts(_("See also:"));
  puts(_("- Config files: https://neomutt.org/guide/configuration"));
}

/**
 * show_cli_help - Show Command Line Help for Help
 * @param use_color Highlight parts of the text
 */
static void show_cli_help(bool use_color)
{
  // L10N: Help Help -- `neomutt -h help`
  print_header("help", _("Get command line help for NeoMutt"), use_color);
  puts("");

  // L10N: Parameters in <>s may be translated
  puts(_("  -h         Overview of command line options"));
  puts(_("  -h <mode>  Detailed help for: shared, help, info, send, tui, all"));
  puts(_("  -v         NeoMutt version and build parameters"));
  puts(_("  -vv        NeoMutt Copyright and license"));
  puts("");

  if (use_color)
    printf("\033[1m%s\033[0m\n", _("Examples:"));
  else
    printf("%s\n", _("Examples:"));

  // L10N: Examples
  puts(_("  neomutt -h info"));
  puts(_("  neomutt -vv"));
}

/**
 * show_cli_info - Show Command Line Help for Info
 * @param use_color Highlight parts of the text
 */
static void show_cli_info(bool use_color)
{
  // L10N: Info Help -- `neomutt -h info`
  print_header("info", _("Ask NeoMutt for config information"), use_color);
  puts("");

  // L10N: Parameters in <>s may be translated
  puts(_("  -A <alias> [...]  Lookup email aliases"));
  puts(_("                    Multiple aliases can be looked up (space-separated)"));
  puts("");

  puts(_("  -D                Dump all the config options"));
  puts(_("  -D -D             (or -DD) Like -D, but only show changed config"));
  puts("");

  puts(_("  -Q <option> [...] Query config options"));
  puts(_("                    Multiple options can be looked up (space-separated)"));

  puts(_("Modify the -D and -Q options:"));
  puts(_("  -O                Add one-liner documentation"));
  puts(_("  -S                Hide the value of sensitive options"));
  puts("");

  if (use_color)
    printf("\033[1m%s\033[0m\n", _("Examples:"));
  else
    printf("%s\n", _("Examples:"));

  // L10N: Examples
  puts(_("  neomutt -A flatcap gahr"));
  puts(_("  neomutt -D -O"));
  puts(_("  neomutt -DD -S"));
  puts(_("  neomutt -O -Q alias_format index_format"));
}

/**
 * show_cli_send - Show Command Line Help for Send
 * @param use_color Highlight parts of the text
 */
static void show_cli_send(bool use_color)
{
  // L10N: Sending Help -- `neomutt -h send`
  print_header("send", _("Send an email from the command line"), use_color);
  puts("");

  puts(_("These options can supply everything NeoMutt needs to send an email."));
  puts(_("If any parts are missing, NeoMutt will start the TUI to ask for them."));
  puts(_("Addresses may be used before the options, or after a -- marker."));
  puts(_("Aliases may be used in place of addresses."));
  puts("");

  // L10N: Parameters in <>s may be translated
  puts(_("  -a <file> [...]     Attach files"));
  puts(_("                      Terminated by -- or another option"));
  puts(_("  -b <address>        Add Bcc: address"));
  puts(_("  -C                  Use crypto (signing/encryption)"));
  puts(_("                      Must be set up in the config file"));
  puts(_("  -c <address>        Add Cc: address"));
  puts(_("  -E                  Edit message"));
  puts(_("                      (supplied by -H or -i)"));
  puts(_("  -H <draft>          Use draft email"));
  puts(_("                      Full email with headers and body"));
  puts(_("  -i <include>        Include body file"));
  puts(_("  -s <subject>        Set Subject:"));
  puts(_("  -- <address> [...]  Add To: addresses"));
  puts("");

  if (use_color)
    printf("\033[1m%s\033[0m\n", _("Examples:"));
  else
    printf("%s\n", _("Examples:"));

  // L10N: Examples: Filenames and email subjects may be translated
  puts(_("  neomutt flatcap -s 'Meeting' < meeting.txt"));
  puts(_("  neomutt jim@example.com -c bob@example.com -s 'Party' -i party.txt"));
  puts(_("  neomutt -s 'Receipts' -a receipt1.pdf receipt2.pdf -- rocco"));
  puts(_("  cat secret.txt | neomutt gahr -s 'Secret' -C"));
}

/**
 * show_cli_tui - Show Command Line Help for Tui
 * @param use_color Highlight parts of the text
 */
static void show_cli_tui(bool use_color)
{
  // L10N: TUI Help -- `neomutt -h tui`
  print_header("tui", _("Start NeoMutt's TUI (Terminal User Interface)"), use_color);
  puts("");

  puts(_("Running NeoMutt with no options will read the config and start the TUI."));
  puts(_("By default, it will open the Index Dialog with the $spool_file Mailbox."));
  puts("");

  puts(_("These options cause NeoMutt to check a mailbox for mail."));
  puts(_("If the condition isn't matched, NeoMutt exits."));
  puts(_("  -p            Resume postponed email"));
  puts(_("  -Z            Check for new mail"));
  puts(_("  -z            Check for any mail"));
  puts("");

  // L10N: Parameters in <>s may be translated
  puts(_("These options change the starting behavior:"));
  puts(_("  -f <mailbox>  Open this mailbox"));
  puts(_("  -G            Open NNTP browser"));
  puts(_("  -g <server>   Use this NNTP server"));
  puts(_("  -R            Open mailbox read-only"));
  puts(_("  -y            Open mailbox browser"));
  puts("");

  if (use_color)
    printf("\033[1m%s\033[0m\n", _("Examples:"));
  else
    printf("%s\n", _("Examples:"));

  // L10N: Examples: Path may be translated
  puts(_("  neomutt -f ~/mail -Z"));
  puts(_("  neomutt -p"));
  puts(_("  neomutt -y"));
}

/**
 * show_cli - Show Instructions on how to run NeoMutt
 * @param mode      Details, e.g. #HM_SHARED
 * @param use_color Highlight parts of the text
 */
void show_cli(enum HelpMode mode, bool use_color)
{
  if (use_color)
    printf("\033[38;5;255m%s\033[0m\n\n", mutt_make_version());
  else
    printf("%s\n\n", mutt_make_version());

  switch (mode)
  {
    case HM_NONE:
    {
      show_cli_overview(use_color);
      break;
    }
    case HM_SHARED:
    {
      show_cli_shared(use_color);
      break;
    }
    case HM_HELP:
    {
      show_cli_help(use_color);
      break;
    }
    case HM_INFO:
    {
      show_cli_info(use_color);
      break;
    }
    case HM_SEND:
    {
      show_cli_send(use_color);
      break;
    }
    case HM_TUI:
    {
      show_cli_tui(use_color);
      break;
    }
    case HM_ALL:
    {
      printf("------------------------------------------------------------\n");
      show_cli_shared(use_color);
      printf("\n------------------------------------------------------------\n");
      show_cli_help(use_color);
      printf("\n------------------------------------------------------------\n");
      show_cli_info(use_color);
      printf("\n------------------------------------------------------------\n");
      show_cli_send(use_color);
      printf("\n------------------------------------------------------------\n");
      show_cli_tui(use_color);
      printf("\n------------------------------------------------------------\n");
      break;
    }
  }
}
