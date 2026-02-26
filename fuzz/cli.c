/**
 * @file
 * Fuzz-test the Command Line Parser
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

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "cli/lib.h"

bool StartupComplete = true;

/**
 * parse_args - Parse fuzz input into argc/argv format
 * @param data    Raw input data
 * @param size    Size of input data
 * @param argc    Pointer to store argument count
 * @param argv    Pointer to store argument array
 * @retval true   Success
 * @retval false  Failure
 *
 * Input format: space-separated arguments (like command line).
 * Each corpus file contains a command line string.
 */
static bool parse_args(const uint8_t *data, size_t size, int *argc, char ***argv)
{
  // Limit input size to prevent excessive memory usage
  if (size > 4096)
    return false;

  // Create a null-terminated copy of the input
  char *input = MUTT_MEM_MALLOC(size + 1, char);
  memcpy(input, data, size);
  input[size] = '\0';

  // Count arguments (words separated by spaces/control chars)
  int count = 1; // Start with 1 for argv[0] = "neomutt"
  bool in_word = false;

  for (size_t i = 0; i < size; i++)
  {
    unsigned char c = (unsigned char) input[i];
    // Treat control characters and space as separators
    if (iscntrl(c) || (c == ' '))
    {
      input[i] = '\0';
      in_word = false;
    }
    else if (!in_word)
    {
      count++;
      in_word = true;
    }
  }

  // Allocate argv array
  char **args = MUTT_MEM_MALLOC(count + 1, char *);
  args[0] = mutt_str_dup("neomutt");

  // Fill in arguments
  int idx = 1;
  in_word = false;
  char *word_start = NULL;

  for (size_t i = 0; i < size; i++)
  {
    if (input[i] == '\0')
    {
      if (in_word && word_start)
      {
        args[idx++] = mutt_str_dup(word_start);
        word_start = NULL;
      }
      in_word = false;
    }
    else if (!in_word)
    {
      word_start = &input[i];
      in_word = true;
    }
  }

  // Handle last word if input doesn't end with separator
  if (in_word && word_start)
  {
    args[idx++] = mutt_str_dup(word_start);
  }

  args[idx] = NULL;
  FREE(&input);

  *argc = idx;
  *argv = args;
  return true;
}

/**
 * free_args - Free the allocated argc/argv
 * @param argc Argument count
 * @param argv Argument array
 */
static void free_args(int argc, char **argv)
{
  for (int i = 0; i < argc; i++)
  {
    FREE(&argv[i]);
  }
  FREE(&argv);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  MuttLogger = log_disp_null;

  int argc = 0;
  char **argv = NULL;

  if (!parse_args(data, size, &argc, &argv))
    return -1;

  // Disable getopt error output to stderr
  opterr = 0;

  struct CommandLine *cli = command_line_new();
  cli_parse(argc, (char *const *) argv, cli);
  command_line_free(&cli);

  free_args(argc, argv);

  return 0;
}
