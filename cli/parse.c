/**
 * @file
 * Parse the Command Line
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
 * @page cli_parse Parse the Command Line
 *
 * Parse the Command Line
 */

#include "config.h"
#include <stdbool.h>
#include <sys/param.h> // IWYU pragma: keep
#include <unistd.h>
#include "mutt/lib.h"
#include "objects.h"

/**
 * mop_up - Eat multiple arguments
 * @param[in]  argc  Size of argument array
 * @param[in]  argv  Array of argument strings
 * @param[in]  index Where to start eating
 * @param[out] sa    Array for the results
 * @retval num Number of arguments eaten
 *
 * Some options, like `-A`, can take multiple arguments.
 * e.g. `-A apple` or `-A apple banana cherry`
 *
 * Copy the arguments into an array.
 */
static int mop_up(int argc, char **argv, int index, struct StringArray *sa)
{
  int count = 0;
  for (int i = index; i < argc; i++, count++)
  {
    // Stop if we find '--' or '-X'
    if ((argv[i][0] == '-') && (argv[i][1] != '\0'))
      break;

    ARRAY_ADD(sa, mutt_str_dup(argv[i]));
  }

  return count;
}

/**
 * cli_parse - Parse the Command Line
 * @param[in]  argc Number of arguments
 * @param[in]  argv Arguments
 * @param[out] cli  Results
 * @retval true Success
 */
bool cli_parse(int argc, char **argv, struct CommandLine *cli)
{
  if ((argc < 1) || !argv || !cli)
    return false;

  // Any leading non-options must be addresses
  int count = mop_up(argc, argv, 1, &cli->send.addresses);
  if (count > 0)
  {
    cli->send.is_set = true;
    argc -= count;
    argv += count;
  }

  bool rc = true;

  opterr = 0; // We'll handle the errors
// Always initialise getopt() or the tests will fail
#if defined(BSD) || defined(__APPLE__)
  optreset = 1;
  optind = 1;
#else
  optind = 0;
#endif

  while (rc && (argc > 1) && (optind < argc))
  {
    int opt = getopt(argc, argv, "+:A:a:b:Cc:Dd:Ee:F:f:Gg:H:hi:l:m:nOpQ:RSs:TvyZz");
    switch (opt)
    {
      // ------------------------------------------------------------
      // Shared
      case 'F': // user config file
      {
        ARRAY_ADD(&cli->shared.user_files, mutt_str_dup(optarg));
        cli->shared.is_set = true;
        break;
      }
      case 'n': // no system config file
      {
        cli->shared.disable_system = true;
        cli->shared.is_set = true;
        break;
      }
      case 'e': // enter commands
      {
        ARRAY_ADD(&cli->shared.commands, mutt_str_dup(optarg));
        cli->shared.is_set = true;
        break;
      }
      case 'm': // mbox type
      {
        buf_strcpy(&cli->shared.mbox_type, optarg);
        cli->shared.is_set = true;
        break;
      }
      case 'd': // log level
      {
        buf_strcpy(&cli->shared.log_level, optarg);
        cli->shared.is_set = true;
        break;
      }
      case 'l': // log file
      {
        buf_strcpy(&cli->shared.log_file, optarg);
        cli->shared.is_set = true;
        break;
      }

      // ------------------------------------------------------------
      // Help
      case 'h': // help
      {
        cli->help.help = true;
        cli->help.is_set = true;
        break;
      }
      case 'v': // version, license
      {
        if (cli->help.version)
          cli->help.license = true;
        else
          cli->help.version = true;

        cli->help.is_set = true;
        break;
      }

      // ------------------------------------------------------------
      // Info
      case 'A': // alias lookup
      {
        ARRAY_ADD(&cli->info.alias_queries, mutt_str_dup(optarg));
        // '-A' can take multiple arguments
        optind += mop_up(argc, argv, optind, &cli->info.alias_queries);
        cli->info.is_set = true;
        break;
      }
      case 'D': // dump config, dump changed
      {
        if (cli->info.dump_config)
          cli->info.dump_changed = true;
        else
          cli->info.dump_config = true;

        cli->info.is_set = true;
        break;
      }
      case 'O': // one-liner help
      {
        cli->info.show_help = true;
        cli->info.is_set = true;
        break;
      }
      case 'Q': // query config&cli->send.attach
      {
        ARRAY_ADD(&cli->info.queries, mutt_str_dup(optarg));
        // '-Q' can take multiple arguments
        optind += mop_up(argc, argv, optind, &cli->info.queries);
        cli->info.is_set = true;
        break;
      }
      case 'S': // hide sensitive
      {
        cli->info.hide_sensitive = true;
        cli->info.is_set = true;
        break;
      }

      // ------------------------------------------------------------
      // Send
      case 'a': // attach file
      {
        ARRAY_ADD(&cli->send.attach, mutt_str_dup(optarg));
        // `-a` can take multiple arguments
        optind += mop_up(argc, argv, optind, &cli->send.attach);
        cli->send.is_set = true;
        break;
      }
      case 'b': // bcc:
      {
        ARRAY_ADD(&cli->send.bcc_list, mutt_str_dup(optarg));
        cli->send.is_set = true;
        break;
      }
      case 'C': // crypto
      {
        cli->send.use_crypto = true;
        cli->send.is_set = true;
        break;
      }
      case 'c': // cc:
      {
        ARRAY_ADD(&cli->send.cc_list, mutt_str_dup(optarg));
        cli->send.is_set = true;
        break;
      }
      case 'E': // edit file
      {
        cli->send.edit_infile = true;
        cli->send.is_set = true;
        break;
      }
      case 'H': // draft file
      {
        buf_strcpy(&cli->send.draft_file, optarg);
        cli->send.is_set = true;
        break;
      }
      case 'i': // include file
      {
        buf_strcpy(&cli->send.include_file, optarg);
        cli->send.is_set = true;
        break;
      }
      case 's': // subject:
      {
        buf_strcpy(&cli->send.subject, optarg);
        cli->send.is_set = true;
        break;
      }

      // ------------------------------------------------------------
      // TUI
      case 'f': // start folder
      {
        buf_strcpy(&cli->tui.folder, optarg);
        cli->tui.is_set = true;
        break;
      }
      case 'G': // list newsgroups
      {
        cli->tui.start_nntp = true;
        cli->tui.is_set = true;
        break;
      }
      case 'g': // news server
      {
        cli->tui.start_nntp = true;
        buf_strcpy(&cli->tui.nntp_server, optarg);
        cli->tui.is_set = true;
        break;
      }
      case 'p': // postponed
      {
        cli->tui.start_postponed = true;
        cli->tui.is_set = true;
        break;
      }
      case 'R': // read-only
      {
        cli->tui.read_only = true;
        cli->tui.is_set = true;
        break;
      }
      case 'y': // browser
      {
        cli->tui.start_browser = true;
        cli->tui.is_set = true;
        break;
      }
      case 'Z': // new mail
      {
        cli->tui.start_new_mail = true;
        cli->tui.is_set = true;
        break;
      }
      case 'z': // any mail
      {
        cli->tui.start_any_mail = true;
        cli->tui.is_set = true;
        break;
      }

      // ------------------------------------------------------------
      case -1: // end of options
      {
        for (int i = optind; i < argc; i++)
        {
          ARRAY_ADD(&cli->send.addresses, mutt_str_dup(argv[i]));
        }
        cli->send.is_set = true;
        optind = argc; // finish parsing
        break;
      }
      default: // error
      {
        if (opt == '?')
          mutt_warning("Invalid option: %c", optopt);
        else if (opt == ':')
          mutt_warning("Option %c requires an argument", optopt);

        cli->help.help = true;
        cli->help.is_set = true;
        rc = false; // stop parsing
        break;
      }
    }
  }

  return rc;
}
