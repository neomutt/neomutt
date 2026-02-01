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

#ifndef MUTT_CLI_OBJECTS_H
#define MUTT_CLI_OBJECTS_H

#include <stdbool.h>
#include "mutt/lib.h"

/**
 * enum HelpMode - Show detailed help
 */
enum HelpMode
{
  HM_NONE,         ///< No extra help
  HM_SHARED,       ///< Help about shared config options
  HM_HELP,         ///< Help about help
  HM_INFO,         ///< Help about info options
  HM_SEND,         ///< Help about sending email options
  HM_TUI,          ///< Help about starting the tui options
  HM_ALL,          ///< Help about all options
};

/**
 * struct CliShared - Shared Command Line options
 */
struct CliShared
{
  bool is_set;                      ///< This struct has been used

  struct StringArray user_files;    ///< `-F` Use these user config files
  bool disable_system;              ///< `-n` Don't read the system config file

  struct StringArray commands;      ///< `-e` Run these commands
  struct Buffer mbox_type;          ///< `-m` Set the default Mailbox type

  struct Buffer log_level;          ///< `-d` Debug log level
  struct Buffer log_file;           ///< `-l` Debug log file
};

/**
 * struct CliHelp - Help Mode Command Line options
 */
struct CliHelp
{
  bool is_set;                      ///< This struct has been used
  bool help;                        ///< `-h`  Print help
  bool version;                     ///< `-v`  Print version
  bool license;                     ///< `-vv` Print license

  enum HelpMode mode;               ///< Display detailed help
};

/**
 * struct CliInfo - Info Mode Command Line options
 */
struct CliInfo
{
  bool is_set;                      ///< This struct has been used
  bool dump_config;                 ///< `-D`  Dump the config options
  bool dump_changed;                ///< `-DD` Dump the changed config options
  bool show_help;                   ///< `-O`  Show one-liner help
  bool hide_sensitive;              ///< `-S`  Hide sensitive config

  struct StringArray alias_queries; ///< `-A`  Lookup an alias
  struct StringArray queries;       ///< `-Q`  Query a config option
};

/**
 * struct CliSend - Send Mode Command Line options
 */
struct CliSend
{
  bool is_set;                      ///< This struct has been used
  bool use_crypto;                  ///< `-C` Use CLI crypto
  bool edit_infile;                 ///< `-E` Edit the draft/include

  struct StringArray attach;        ///< `-a` Attach a file
  struct StringArray bcc_list;      ///< `-b` Add a Bcc:
  struct StringArray cc_list;       ///< `-c` Add a Cc:
  struct StringArray addresses;     ///< Send to these addresses

  struct Buffer draft_file;         ///< `-H` Use this draft file
  struct Buffer include_file;       ///< `-i` Use this include file
  struct Buffer subject;            ///< `-s` Use this Subject:
};

/**
 * struct CliTui - TUI Mode Command Line options
 */
struct CliTui
{
  bool is_set;                      ///< This struct has been used
  bool read_only;                   ///< `-R` Open Mailbox read-only
  bool start_postponed;             ///< `-p` Open Postponed emails
  bool start_browser;               ///< `-y` Open the Mailbox Browser
  bool start_nntp;                  ///< `-G` Open an NNTP Mailbox
  bool start_new_mail;              ///< `-Z` Check for New Mail
  bool start_any_mail;              ///< `-z` Check for Any Mail

  struct Buffer folder;             ///< `-f` Open this Mailbox
  struct Buffer nntp_server;        ///< `-g` Open this NNTP Mailbox
};

/**
 * struct CommandLine - Command Line options
 */
struct CommandLine
{
  struct CliShared  shared;        ///< Shared    command line options
  struct CliHelp    help;          ///< Help Mode command line options
  struct CliInfo    info;          ///< Info Mode command line options
  struct CliSend    send;          ///< Send Mode command line options
  struct CliTui     tui;           ///< Tui  Mode command line options
};

struct CommandLine *command_line_new(void);
void                command_line_free(struct CommandLine **ptr);

#endif /* MUTT_CLI_OBJECTS_H */
