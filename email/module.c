/**
 * @file
 * Definition of the Email Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
#include "module_data.h"
#include "tags.h"

extern struct ConfigDef EmailVars[];

extern const struct Command EmailCommands[];

/**
 * email_init - Initialise a Module - Implements Module::init()
 */
static bool email_init(struct NeoMutt *n)
{
  struct EmailModuleData *md = MUTT_MEM_CALLOC(1, struct EmailModuleData);
  neomutt_set_module_data(n, MODULE_ID_EMAIL, md);

  md->auto_subscribe_cache = NULL;

  STAILQ_INIT(&md->alternative_order);
  STAILQ_INIT(&md->auto_view);
  STAILQ_INIT(&md->header_order);
  STAILQ_INIT(&md->ignore);
  STAILQ_INIT(&md->mail);
  STAILQ_INIT(&md->mail_to_allow);
  STAILQ_INIT(&md->no_spam);
  STAILQ_INIT(&md->spam);
  STAILQ_INIT(&md->subscribed);
  STAILQ_INIT(&md->unignore);
  STAILQ_INIT(&md->unmail);
  STAILQ_INIT(&md->unsubscribed);

  /* RFC2368, "4. Unsafe headers"
   * The creator of a mailto URL can't expect the resolver of a URL to
   * understand more than the "subject" and "body" headers. Clients that
   * resolve mailto URLs into mail messages should be able to correctly create
   * RFC822-compliant mail messages using the "subject" and "body" headers. */
  add_to_stailq(&md->mail_to_allow, "body");
  add_to_stailq(&md->mail_to_allow, "subject");
  // Cc, In-Reply-To, and References help with not breaking threading on mailing lists
  add_to_stailq(&md->mail_to_allow, "cc");
  add_to_stailq(&md->mail_to_allow, "in-reply-to");
  add_to_stailq(&md->mail_to_allow, "references");

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
 * email_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool email_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, EmailCommands);
}

/**
 * email_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool email_cleanup(struct NeoMutt *n)
{
  struct EmailModuleData *md = neomutt_get_module_data(n, MODULE_ID_EMAIL);
  ASSERT(md);

  mutt_hash_free(&md->auto_subscribe_cache);

  mutt_list_free(&md->alternative_order);
  mutt_list_free(&md->auto_view);
  mutt_list_free(&md->header_order);
  mutt_list_free(&md->ignore);
  mutt_list_free(&md->mail_to_allow);
  mutt_list_free(&md->unignore);

  mutt_regexlist_free(&md->mail);
  mutt_regexlist_free(&md->no_spam);
  mutt_regexlist_free(&md->subscribed);
  mutt_regexlist_free(&md->unmail);
  mutt_regexlist_free(&md->unsubscribed);

  mutt_replacelist_free(&md->spam);

  driver_tags_cleanup();

  FREE(&md);
  return true;
}

/**
 * ModuleEmail - Module for the Email library
 */
const struct Module ModuleEmail = {
  MODULE_ID_EMAIL,
  "email",
  email_init,
  NULL, // config_define_types
  email_config_define_variables,
  email_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  email_cleanup,
};
