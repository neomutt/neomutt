/**
 * @file
 * Definition of the Email Module
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
 * @page email_module Definition of the Email Module
 *
 * Definition of the Email Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "globals.h"
#include "tags.h"

extern struct ConfigDef EmailVars[];

/**
 * email_init - Initialise a Module - Implements Module::init()
 */
static bool email_init(struct NeoMutt *n)
{
  driver_tags_init();
  return true;
}

/**
 * email_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool email_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, EmailVars);
}

/**
 * email_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void email_cleanup(struct NeoMutt *n)
{
  driver_tags_cleanup();

  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&NoSpamList);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&UnSubscribedLists);

  mutt_list_free(&Ignore);
  mutt_list_free(&MailToAllow);
  mutt_list_free(&UnIgnore);

  mutt_replacelist_free(&SpamList);

  mutt_regexlist_free(&NoSpamList);
}

/**
 * ModuleEmail - Module for the Email library
 */
const struct Module ModuleEmail = {
  "email",
  email_init,
  NULL, // config_define_types
  email_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  email_cleanup,
  NULL, // mod_data
};
