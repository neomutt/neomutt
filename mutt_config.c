/**
 * @file
 * Definitions of config variables
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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
 * @page neo_mutt_config Definitions of config variables
 *
 * Definitions of config variables
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "init.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "mx.h"
#include "options.h"

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

#define CONFIG_INIT_VARS(CS, NAME)                                             \
  bool config_init_##NAME(struct ConfigSet *cs);                               \
  config_init_##NAME(CS)

/**
 * SortAuxMethods - Sort methods for '$sort_aux' for the index
 */
static const struct Mapping SortAuxMethods[] = {
  // clang-format off
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "threads",       SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "to",            SORT_TO },
  { NULL, 0 },
  // clang-format on
};

/**
 * SortMethods - Sort methods for '$sort' for the index
 */
const struct Mapping SortMethods[] = {
  // clang-format off
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_THREADS },
  { "to",            SORT_TO },
  { NULL, 0 },
  // clang-format on
};

/**
 * SortBrowserMethods - Sort methods for the folder/dir browser
 */
const struct Mapping SortBrowserMethods[] = {
  // clang-format off
  { "alpha",    SORT_SUBJECT },
  { "count",    SORT_COUNT },
  { "date",     SORT_DATE },
  { "desc",     SORT_DESC },
  { "new",      SORT_UNREAD },
  { "unread",   SORT_UNREAD },
  { "size",     SORT_SIZE },
  { "unsorted", SORT_ORDER },
  { NULL, 0 },
  // clang-format on
};

/**
 * multipart_validator - Validate the "show_multipart_alternative" config variable - Implements ConfigDef::validator() - @ingroup cfg_def_validator
 */
int multipart_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                        intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (mutt_str_equal(str, "inline") || mutt_str_equal(str, "info"))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

/**
 * reply_validator - Validate the "reply_regex" config variable - Implements ConfigDef::validator() - @ingroup cfg_def_validator
 */
int reply_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                    intptr_t value, struct Buffer *err)
{
  if (!OptAttachMsg)
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Option %s may not be set when in attach-message mode"),
                     cdef->name);
  return CSR_ERR_INVALID;
}

static struct ConfigDef MainVars[] = {
  // clang-format off
  { "abort_backspace", DT_BOOL, true, 0, NULL,
    "Hitting backspace against an empty prompt aborts the prompt"
  },
  { "abort_key", DT_STRING|DT_NOT_EMPTY, IP "\007", 0, NULL,
    "String representation of key to abort prompts"
  },
  { "arrow_cursor", DT_BOOL|R_MENU, false, 0, NULL,
    "Use an arrow '->' instead of highlighting in the index"
  },
  { "arrow_string", DT_STRING|DT_NOT_EMPTY, IP "->", 0, NULL,
    "Use an custom string for arrow_cursor"
  },
  { "ascii_chars", DT_BOOL|R_INDEX|R_PAGER, false, 0, NULL,
    "Use plain ASCII characters, when drawing email threads"
  },
  { "ask_bcc", DT_BOOL, false, 0, NULL,
    "Ask the user for the blind-carbon-copy recipients"
  },
  { "ask_cc", DT_BOOL, false, 0, NULL,
    "Ask the user for the carbon-copy recipients"
  },
  { "assumed_charset", DT_STRING, 0, 0, charset_validator,
    "If a message is missing a character set, assume this character set"
  },
  { "attach_format", DT_STRING|DT_NOT_EMPTY, IP "%u%D%I %t%4n %T%.40d%> [%.7m/%.10M, %.6e%?C?, %C?, %s] ", 0, NULL,
    "printf-like format string for the attachment menu"
  },
  { "attach_save_dir", DT_PATH|DT_PATH_DIR, IP "./", 0, NULL,
    "Default directory where attachments are saved"
  },
  { "attach_save_without_prompting", DT_BOOL, false, 0, NULL,
    "If true, then don't prompt to save"
  },
  { "attach_sep", DT_STRING, IP "\n", 0, NULL,
    "Separator to add between saved/printed/piped attachments"
  },
  { "attach_split", DT_BOOL, true, 0, NULL,
    "Save/print/pipe tagged messages individually"
  },
  { "attribution", DT_STRING, IP "On %d, %n wrote:", 0, NULL,
    "Message to start a reply, 'On DATE, PERSON wrote:'"
  },
  { "attribution_locale", DT_STRING, 0, 0, NULL,
    "Locale for dates in the attribution message"
  },
  { "auto_edit", DT_BOOL, false, 0, NULL,
    "Skip the initial compose menu and edit the email"
  },
  { "auto_subscribe", DT_BOOL, false, 0, NULL,
    "Automatically check if the user is subscribed to a mailing list"
  },
  { "auto_tag", DT_BOOL, false, 0, NULL,
    "Automatically apply actions to all tagged messages"
  },
  { "beep", DT_BOOL, true, 0, NULL,
    "Make a noise when an error occurs"
  },
  { "beep_new", DT_BOOL, false, 0, NULL,
    "Make a noise when new mail arrives"
  },
  { "bounce", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Confirm before bouncing a message"
  },
  { "braille_friendly", DT_BOOL, false, 0, NULL,
    "Move the cursor to the beginning of the line"
  },
  { "charset", DT_STRING|DT_NOT_EMPTY|DT_CHARSET_SINGLE, 0, 0, charset_validator,
    "Default character set for displaying text on screen"
  },
  { "collapse_flagged", DT_BOOL, true, 0, NULL,
    "Prevent the collapse of threads with flagged emails"
  },
  { "collapse_unread", DT_BOOL, true, 0, NULL,
    "Prevent the collapse of threads with unread emails"
  },
  { "config_charset", DT_STRING, 0, 0, charset_validator,
    "Character set that the config files are in"
  },
  { "confirm_append", DT_BOOL, true, 0, NULL,
    "Confirm before appending emails to a mailbox"
  },
  { "confirm_create", DT_BOOL, true, 0, NULL,
    "Confirm before creating a new mailbox"
  },
  { "copy_decode_weed", DT_BOOL, false, 0, NULL,
    "Controls whether to weed headers when copying or saving emails"
  },
  { "count_alternatives", DT_BOOL, false, 0, NULL,
    "Recurse inside multipart/alternatives while counting attachments"
  },
  { "crypt_chars", DT_MBTABLE|R_INDEX|R_PAGER, IP "SPsK ", 0, NULL,
    "User-configurable crypto flags: signed, encrypted etc."
  },
  { "date_format", DT_STRING|DT_NOT_EMPTY|R_MENU, IP "!%a, %b %d, %Y at %I:%M:%S%p %Z", 0, NULL,
    "strftime format string for the `%d` expando"
  },
  { "debug_file", DT_PATH|DT_PATH_FILE, IP "~/.neomuttdebug", 0, NULL,
    "File to save debug logs"
  },
  { "debug_level", DT_NUMBER, 0, 0, level_validator,
    "Logging level for debug logs"
  },
  { "default_hook", DT_STRING, IP "~f %s !~P | (~P ~C %s)", 0, NULL,
    "Pattern to use for hooks that only have a simple regex"
  },
  { "delete", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Really delete messages, when the mailbox is closed"
  },
  { "delete_untag", DT_BOOL, true, 0, NULL,
    "Untag messages when they are marked for deletion"
  },
  { "digest_collapse", DT_BOOL, true, 0, NULL,
    "Hide the subparts of a multipart/digest"
  },
  { "display_filter", DT_STRING|DT_COMMAND|R_PAGER, 0, 0, NULL,
    "External command to pre-process an email before display"
  },
  { "duplicate_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, true, 0, NULL,
    "Highlight messages with duplicated message IDs"
  },
  { "editor", DT_STRING|DT_NOT_EMPTY|DT_COMMAND, 0, 0, NULL,
    "External command to use as an email editor"
  },
  { "flag_chars", DT_MBTABLE|R_INDEX|R_PAGER, IP "*!DdrONon- ", 0, NULL,
    "User-configurable index flags: tagged, new, etc"
  },
  { "flag_safe", DT_BOOL, false, 0, NULL,
    "Protect flagged messages from deletion"
  },
  { "folder", DT_STRING|DT_MAILBOX, IP "~/Mail", 0, NULL,
    "Base folder for a set of mailboxes"
  },
  { "force_name", DT_BOOL, false, 0, NULL,
    "Save outgoing mail in a folder of their name"
  },
  { "forward_attachments", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Forward attachments when forwarding a message"
  },
  { "forward_decode", DT_BOOL, true, 0, NULL,
    "Decode the message when forwarding it"
  },
  { "forward_quote", DT_BOOL, false, 0, NULL,
    "Automatically quote a forwarded message using `$indent_string`"
  },
  { "from", DT_ADDRESS, 0, 0, NULL,
    "Default 'From' address to use, if isn't otherwise set"
  },
  { "from_chars", DT_MBTABLE|R_INDEX|R_PAGER, 0, 0, NULL,
    "User-configurable index flags: to address, cc address, etc"
  },
  { "gecos_mask", DT_REGEX, IP "^[^,]*", 0, NULL,
    "Regex for parsing GECOS field of /etc/passwd"
  },
  { "greeting", DT_STRING, 0, 0, NULL,
    "Greeting string added to the top of all messages"
  },
  { "header", DT_BOOL, false, 0, NULL,
    "Include the message headers in the reply email (Weed applies)"
  },
  { "hidden_tags", DT_SLIST|SLIST_SEP_COMMA, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted", 0, NULL,
    "List of tags that shouldn't be displayed on screen (comma-separated)"
  },
  { "hide_limited", DT_BOOL|R_TREE|R_INDEX, false, 0, NULL,
    "Don't indicate hidden messages, in the thread tree"
  },
  { "hide_missing", DT_BOOL|R_TREE|R_INDEX, true, 0, NULL,
    "Don't indicate missing messages, in the thread tree"
  },
  { "hide_thread_subject", DT_BOOL|R_TREE|R_INDEX, true, 0, NULL,
    "Hide subjects that are similar to that of the parent message"
  },
  { "hide_top_limited", DT_BOOL|R_TREE|R_INDEX, false, 0, NULL,
    "Don't indicate hidden top message, in the thread tree"
  },
  { "hide_top_missing", DT_BOOL|R_TREE|R_INDEX, true, 0, NULL,
    "Don't indicate missing top message, in the thread tree"
  },
  { "honor_disposition", DT_BOOL, false, 0, NULL,
    "Don't display MIME parts inline if they have a disposition of 'attachment'"
  },
  { "hostname", DT_STRING, 0, 0, NULL,
    "Fully-qualified domain name of this machine"
  },
  { "implicit_autoview", DT_BOOL, false, 0, NULL,
    "Display MIME attachments inline if a 'copiousoutput' mailcap entry exists"
  },
  { "include_encrypted", DT_BOOL, false, 0, NULL,
    "Whether to include encrypted content when replying"
  },
  { "include_only_first", DT_BOOL, false, 0, NULL,
    "Only include the first attachment when replying"
  },
  { "indent_string", DT_STRING, IP "> ", 0, NULL,
    "String used to indent 'reply' text"
  },
  { "index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, IP "%4C %Z %{%b %d} %-15.15L (%?l?%4l&%4c?) %s", 0, NULL,
    "printf-like format string for the index menu (emails)"
  },
  { "keep_flagged", DT_BOOL, false, 0, NULL,
    "Don't move flagged messages from `$spool_file` to `$mbox`"
  },
  { "local_date_header", DT_BOOL, true, 0, NULL,
    "Convert the date in the Date header of sent emails into local timezone, UTC otherwise"
  },
  { "mail_check", DT_NUMBER|DT_NOT_NEGATIVE, 5, 0, NULL,
    "Number of seconds before NeoMutt checks for new mail"
  },
  { "mail_check_recent", DT_BOOL, true, 0, NULL,
    "Notify the user about new mail since the last time the mailbox was opened"
  },
  { "mail_check_stats", DT_BOOL, false, 0, NULL,
    "Periodically check for new mail"
  },
  { "mail_check_stats_interval", DT_NUMBER|DT_NOT_NEGATIVE, 60, 0, NULL,
    "How often to check for new mail"
  },
  { "mailcap_path", DT_SLIST|SLIST_SEP_COLON, IP "~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap", 0, NULL,
    "List of mailcap files (colon-separated)"
  },
  { "mailcap_sanitize", DT_BOOL, true, 0, NULL,
    "Restrict the possible characters in mailcap expandos"
  },
  { "mark_old", DT_BOOL|R_INDEX|R_PAGER, true, 0, NULL,
    "Mark new emails as old when leaving the mailbox"
  },
  { "markers", DT_BOOL|R_PAGER_FLOW, true, 0, NULL,
    "Display a '+' at the beginning of wrapped lines in the pager"
  },
  { "mbox", DT_STRING|DT_MAILBOX|R_INDEX|R_PAGER, IP "~/mbox", 0, NULL,
    "Folder that receives read emails (see Move)"
  },
  { "mbox_type", DT_ENUM, MUTT_MBOX, IP &MboxTypeDef, NULL,
    "Default type for creating new mailboxes"
  },
  { "message_cache_clean", DT_BOOL, false, 0, NULL,
    "(imap/pop) Clean out obsolete entries from the message cache"
  },
  { "message_cachedir", DT_PATH|DT_PATH_DIR, 0, 0, NULL,
    "(imap/pop) Directory for the message cache"
  },
  { "message_format", DT_STRING|DT_NOT_EMPTY, IP "%s", 0, NULL,
    "printf-like format string for listing attached messages"
  },
  { "meta_key", DT_BOOL, false, 0, NULL,
    "Interpret 'ALT-x' as 'ESC-x'"
  },
  { "mime_forward", DT_QUAD, MUTT_NO, 0, NULL,
    "Forward a message as a 'message/RFC822' MIME part"
  },
  { "mime_forward_rest", DT_QUAD, MUTT_YES, 0, NULL,
    "Forward all attachments, even if they can't be decoded"
  },
  { "move", DT_QUAD, MUTT_NO, 0, NULL,
    "Move emails from `$spool_file` to `$mbox` when read"
  },
  { "narrow_tree", DT_BOOL|R_TREE|R_INDEX, false, 0, NULL,
    "Draw a narrower thread tree in the index"
  },
  { "net_inc", DT_NUMBER|DT_NOT_NEGATIVE, 10, 0, NULL,
    "(socket) Update the progress bar after this many KB sent/received (0 to disable)"
  },
  { "new_mail_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "External command to run when new mail arrives"
  },
  { "pager", DT_STRING|DT_COMMAND, IP "builtin", 0, NULL,
    "External command for viewing messages, or 'builtin' to use NeoMutt's"
  },
  { "pager_format", DT_STRING|R_PAGER, IP "-%Z- %C/%m: %-20.20n   %s%*  -- (%P)", 0, NULL,
    "printf-like format string for the pager's status bar"
  },
  { "pipe_decode", DT_BOOL, false, 0, NULL,
    "Decode the message when piping it"
  },
  { "pipe_decode_weed", DT_BOOL, true, 0, NULL,
    "Control whether to weed headers when piping an email"
  },
  { "pipe_sep", DT_STRING, IP "\n", 0, NULL,
    "Separator to add between multiple piped messages"
  },
  { "pipe_split", DT_BOOL, false, 0, NULL,
    "Run the pipe command on each message separately"
  },
  { "postponed", DT_STRING|DT_MAILBOX|R_INDEX, IP "~/postponed", 0, NULL,
    "Folder to store postponed messages"
  },
  { "preferred_languages", DT_SLIST|SLIST_SEP_COMMA, 0, 0, NULL,
    "List of Preferred Languages for multilingual MIME (comma-separated)"
  },
  { "print", DT_QUAD, MUTT_ASKNO, 0, NULL,
    "Confirm before printing a message"
  },
  { "print_command", DT_STRING|DT_COMMAND, IP "lpr", 0, NULL,
    "External command to print a message"
  },
  { "print_decode", DT_BOOL, true, 0, NULL,
    "Decode message before printing it"
  },
  { "print_decode_weed", DT_BOOL, true, 0, NULL,
    "Control whether to weed headers when printing an email "
  },
  { "print_split", DT_BOOL, false, 0, NULL,
    "Print multiple messages separately"
  },
  { "prompt_after", DT_BOOL, true, 0, NULL,
    "Pause after running an external pager"
  },
  { "quit", DT_QUAD, MUTT_YES, 0, NULL,
    "Prompt before exiting NeoMutt"
  },
  { "quote_regex", DT_REGEX|R_PAGER, IP "^([ \t]*[|>:}#])+", 0, NULL,
    "Regex to match quoted text in a reply"
  },
  { "read_inc", DT_NUMBER|DT_NOT_NEGATIVE, 10, 0, NULL,
    "Update the progress bar after this many records read (0 to disable)"
  },
  { "read_only", DT_BOOL, false, 0, NULL,
    "Open folders in read-only mode"
  },
  { "real_name", DT_STRING|R_INDEX|R_PAGER, 0, 0, NULL,
    "Real name of the user"
  },
  { "record", DT_STRING|DT_MAILBOX, IP "~/sent", 0, NULL,
    "Folder to save 'sent' messages"
  },
  { "reflow_space_quotes", DT_BOOL, true, 0, NULL,
    "Insert spaces into reply quotes for 'format=flowed' messages"
  },
  { "reflow_text", DT_BOOL, true, 0, NULL,
    "Reformat paragraphs of 'format=flowed' text"
  },
  { "reflow_wrap", DT_NUMBER, 78, 0, NULL,
    "Maximum paragraph width for reformatting 'format=flowed' text"
  },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, reply_validator,
    "Regex to match message reply subjects like 're: '"
  },
  { "resolve", DT_BOOL, true, 0, NULL,
    "Move to the next email whenever a command modifies an email"
  },
  { "resume_draft_files", DT_BOOL, false, 0, NULL,
    "Process draft files like postponed messages"
  },
  { "resume_edited_draft_files", DT_BOOL, true, 0, NULL,
    "Resume editing previously saved draft files"
  },
  { "reverse_alias", DT_BOOL|R_INDEX|R_PAGER, false, 0, NULL,
    "Display the alias in the index, rather than the message's sender"
  },
  { "rfc2047_parameters", DT_BOOL, true, 0, NULL,
    "Decode RFC2047-encoded MIME parameters"
  },
  { "save_address", DT_BOOL, false, 0, NULL,
    "Use sender's full address as a default save folder"
  },
  { "save_empty", DT_BOOL, true, 0, NULL,
    "(mbox,mmdf) Preserve empty mailboxes"
  },
  { "save_name", DT_BOOL, false, 0, NULL,
    "Save outgoing message to mailbox of recipient's name if it exists"
  },
  { "score", DT_BOOL, true, 0, NULL,
    "Use message scoring"
  },
  { "score_threshold_delete", DT_NUMBER, -1, 0, NULL,
    "Messages with a lower score will be automatically deleted"
  },
  { "score_threshold_flag", DT_NUMBER, 9999, 0, NULL,
    "Messages with a greater score will be automatically flagged"
  },
  { "score_threshold_read", DT_NUMBER, -1, 0, NULL,
    "Messages with a lower score will be automatically marked read"
  },
  { "send_charset", DT_STRING|DT_CHARSET_STRICT, IP "us-ascii:iso-8859-1:utf-8", 0, charset_validator,
    "Character sets for outgoing mail"
  },
  { "shell", DT_STRING|DT_COMMAND, IP "/bin/sh", 0, NULL,
    "External command to run subshells in"
  },
  { "show_multipart_alternative", DT_STRING, 0, 0, multipart_validator,
    "How to display 'multipart/alternative' MIME parts"
  },
  { "simple_search", DT_STRING, IP "~f %s | ~s %s", 0, NULL,
    "Pattern to search for when search doesn't contain ~'s"
  },
  { "size_show_bytes", DT_BOOL|R_MENU, false, 0, NULL,
    "Show smaller sizes in bytes"
  },
  { "size_show_fractions", DT_BOOL|R_MENU, true, 0, NULL,
    "Show size fractions with a single decimal place"
  },
  { "size_show_mb", DT_BOOL|R_MENU, true, 0, NULL,
    "Show sizes in megabytes for sizes greater than 1 megabyte"
  },
  { "size_units_on_left", DT_BOOL|R_MENU, false, 0, NULL,
    "Show the units as a prefix to the size"
  },
  { "sleep_time", DT_NUMBER|DT_NOT_NEGATIVE, 1, 0, NULL,
    "Time to pause after certain info messages"
  },
  { "sort", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT, SORT_DATE, IP SortMethods, sort_validator,
    "Sort method for the index"
  },
  { "sort_aux", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT|R_RESORT_SUB, SORT_DATE, IP SortAuxMethods, NULL,
    "Secondary sort method for the index"
  },
  { "sort_re", DT_BOOL|R_INDEX|R_RESORT|R_RESORT_INIT, true, 0, NULL,
    "Whether $reply_regex must be matched when not $strict_threads"
  },
  { "spam_separator", DT_STRING, IP ",", 0, NULL,
    "Separator for multiple spam headers"
  },
  { "spool_file", DT_STRING|DT_MAILBOX, 0, 0, NULL,
    "Inbox"
  },
  { "status_chars", DT_MBTABLE|R_INDEX|R_PAGER, IP "-*%A", 0, NULL,
    "Indicator characters for the status bar"
  },
  { "status_format", DT_STRING|R_INDEX|R_PAGER, IP "-%r-NeoMutt: %D [Msgs:%?M?%M/?%m%?n? New:%n?%?o? Old:%o?%?d? Del:%d?%?F? Flag:%F?%?t? Tag:%t?%?p? Post:%p?%?b? Inc:%b?%?l? %l?]---(%?T?%T/?%s/%S)-%>-(%P)---", 0, NULL,
    "printf-like format string for the index's status line"
  },
  { "status_on_top", DT_BOOL, false, 0, NULL,
    "Display the status bar at the top"
  },
  { "strict_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, false, 0, NULL,
    "Thread messages using 'In-Reply-To' and 'References' headers"
  },
  { "suspend", DT_BOOL, true, 0, NULL,
    "Allow the user to suspend NeoMutt using '^Z'"
  },
  { "text_flowed", DT_BOOL, false, 0, NULL,
    "Generate 'format=flowed' messages"
  },
  { "thread_received", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, false, 0, NULL,
    "Sort threaded messages by their received date"
  },
  { "time_inc", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Frequency of progress bar updates (milliseconds)"
  },
  { "timeout", DT_NUMBER|DT_NOT_NEGATIVE, 600, 0, NULL,
    "Time to wait for user input in menus"
  },
  { "tmpdir", DT_PATH|DT_PATH_DIR|DT_NOT_EMPTY, IP TMPDIR, 0, NULL,
    "Directory for temporary files"
  },
  { "to_chars", DT_MBTABLE|R_INDEX|R_PAGER, IP " +TCFLR", 0, NULL,
    "Indicator characters for the 'To' field in the index"
  },
  { "toggle_quoted_show_levels", DT_NUMBER|DT_NOT_NEGATIVE, 0, 0, NULL,
    "Number of quote levels to show with toggle-quoted"
  },
  { "trash", DT_STRING|DT_MAILBOX, 0, 0, NULL,
    "Folder to put deleted emails"
  },
  { "ts_enabled", DT_BOOL|R_INDEX|R_PAGER, false, 0, NULL,
    "Allow NeoMutt to set the terminal status line and icon"
  },
  { "ts_icon_format", DT_STRING|R_INDEX|R_PAGER, IP "M%?n?AIL&ail?", 0, NULL,
    "printf-like format string for the terminal's icon title"
  },
  { "ts_status_format", DT_STRING|R_INDEX|R_PAGER, IP "NeoMutt with %?m?%m messages&no messages?%?n? [%n NEW]?", 0, NULL,
    "printf-like format string for the terminal's status (window title)"
  },
  { "use_domain", DT_BOOL, true, 0, NULL,
    "Qualify local addresses using this domain"
  },
  { "use_threads", DT_ENUM|R_INDEX|R_RESORT, UT_UNSET, IP &UseThreadsTypeDef, NULL,
    "Whether to use threads for the index"
  },
  { "wait_key", DT_BOOL, true, 0, NULL,
    "Prompt to press a key after running external commands"
  },
  { "weed", DT_BOOL, true, 0, NULL,
    "Filter headers when displaying/forwarding/printing/replying"
  },
  { "wrap", DT_NUMBER|R_PAGER_FLOW, 0, 0, NULL,
    "Width to wrap text in the pager"
  },
  { "wrap_search", DT_BOOL, true, 0, NULL,
    "Wrap around when the search hits the end"
  },
  { "write_bcc", DT_BOOL, false, 0, NULL,
    "Write out the 'Bcc' field when preparing to send a mail"
  },
  { "write_inc", DT_NUMBER|DT_NOT_NEGATIVE, 10, 0, NULL,
    "Update the progress bar after this many records written (0 to disable)"
  },

  { "escape",                    DT_DEPRECATED|DT_STRING,            IP "~", IP "2021-03-18" },
  { "ignore_linear_white_space", DT_DEPRECATED|DT_BOOL,              false,  IP "2021-03-18" },
  { "visual",                    DT_DEPRECATED|DT_STRING|DT_COMMAND, 0,      IP "2021-03-18" },

  { "askbcc",                    DT_SYNONYM, IP "ask_bcc",                    IP "2021-03-21" },
  { "askcc",                     DT_SYNONYM, IP "ask_cc",                     IP "2021-03-21" },
  { "autoedit",                  DT_SYNONYM, IP "auto_edit",                  IP "2021-03-21" },
  { "confirmappend",             DT_SYNONYM, IP "confirm_append",             IP "2021-03-21" },
  { "confirmcreate",             DT_SYNONYM, IP "confirm_create",             IP "2021-03-21" },
  { "edit_hdrs",                 DT_SYNONYM, IP "edit_headers",               IP "2021-03-21" },
  { "forw_decode",               DT_SYNONYM, IP "forward_decode",             IP "2021-03-21" },
  { "forw_quote",                DT_SYNONYM, IP "forward_quote",              IP "2021-03-21" },
  { "hdr_format",                DT_SYNONYM, IP "index_format",               IP "2021-03-21" },
  { "include_onlyfirst",         DT_SYNONYM, IP "include_only_first",         IP "2021-03-21" },
  { "indent_str",                DT_SYNONYM, IP "indent_string",              IP "2021-03-21" },
  { "mime_fwd",                  DT_SYNONYM, IP "mime_forward",               IP "2021-03-21" },
  { "msg_format",                DT_SYNONYM, IP "message_format",             IP "2021-03-21" },
  { "print_cmd",                 DT_SYNONYM, IP "print_command",              IP "2021-03-21" },
  { "quote_regexp",              DT_SYNONYM, IP "quote_regex",                IP "2021-03-21" },
  { "realname",                  DT_SYNONYM, IP "real_name",                  IP "2021-03-21" },
  { "reply_regexp",              DT_SYNONYM, IP "reply_regex",                IP "2021-03-21" },
  { "spoolfile",                 DT_SYNONYM, IP "spool_file",                 IP "2021-03-21" },
  { "xterm_icon",                DT_SYNONYM, IP "ts_icon_format",             IP "2021-03-21" },
  { "xterm_set_titles",          DT_SYNONYM, IP "ts_enabled",                 IP "2021-03-21" },
  { "xterm_title",               DT_SYNONYM, IP "ts_status_format",           IP "2021-03-21" },

  { NULL },
  // clang-format on
};

#if defined(MIXMASTER)
#ifdef MIXMASTER
#define MIXMASTER_DEFAULT MIXMASTER
#else
#define MIXMASTER_DEFAULT ""
#endif
static struct ConfigDef MainVarsMixmaster[] = {
  // clang-format off
  { "mix_entry_format", DT_STRING|DT_NOT_EMPTY, IP "%4n %c %-16s %a", 0, NULL,
    "(mixmaster) printf-like format string for the mixmaster chain"
  },
  { "mixmaster", DT_STRING|DT_COMMAND, IP MIXMASTER_DEFAULT, 0, NULL,
    "(mixmaster) External command to route a mixmaster message"
  },
  { NULL },
  // clang-format on
};
#endif

#if defined(HAVE_LIBIDN)
static struct ConfigDef MainVarsIdn[] = {
  // clang-format off
  { "idn_decode", DT_BOOL|R_MENU, true, 0, NULL,
    "(idn) Decode international domain names"
  },
  { "idn_encode", DT_BOOL|R_MENU, true, 0, NULL,
    "(idn) Encode international domain names"
  },
  { NULL },
  // clang-format on
};
#endif

/**
 * config_init_main - Register main config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
static bool config_init_main(struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, MainVars, 0);

#if defined(MIXMASTER)
  rc |= cs_register_variables(cs, MainVarsMixmaster, 0);
#endif

#if defined(HAVE_LIBIDN)
  rc |= cs_register_variables(cs, MainVarsIdn, 0);
#endif

  return rc;
}

/**
 * init_types - Create the config types
 * @param cs Config items
 *
 * Define the config types, e.g. #DT_STRING.
 */
static void init_types(struct ConfigSet *cs)
{
  CONFIG_INIT_TYPE(cs, Address);
  CONFIG_INIT_TYPE(cs, Bool);
  CONFIG_INIT_TYPE(cs, Enum);
  CONFIG_INIT_TYPE(cs, Long);
  CONFIG_INIT_TYPE(cs, Mbtable);
  CONFIG_INIT_TYPE(cs, Number);
  CONFIG_INIT_TYPE(cs, Path);
  CONFIG_INIT_TYPE(cs, Quad);
  CONFIG_INIT_TYPE(cs, Regex);
  CONFIG_INIT_TYPE(cs, Slist);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, String);
}

/**
 * init_variables - Define the config variables
 * @param cs Config items
 */
static void init_variables(struct ConfigSet *cs)
{
  // Define the config variables
  config_init_main(cs);
  CONFIG_INIT_VARS(cs, alias);
#if defined(USE_AUTOCRYPT)
  CONFIG_INIT_VARS(cs, autocrypt);
#endif
  CONFIG_INIT_VARS(cs, browser);
  CONFIG_INIT_VARS(cs, compose);
  CONFIG_INIT_VARS(cs, conn);
#if defined(USE_HCACHE)
  CONFIG_INIT_VARS(cs, hcache);
#endif
  CONFIG_INIT_VARS(cs, helpbar);
  CONFIG_INIT_VARS(cs, history);
#if defined(USE_IMAP)
  CONFIG_INIT_VARS(cs, imap);
#endif
  CONFIG_INIT_VARS(cs, index);
  CONFIG_INIT_VARS(cs, maildir);
  CONFIG_INIT_VARS(cs, mbox);
  CONFIG_INIT_VARS(cs, menu);
  CONFIG_INIT_VARS(cs, ncrypt);
#if defined(USE_NNTP)
  CONFIG_INIT_VARS(cs, nntp);
#endif
#if defined(USE_NOTMUCH)
  CONFIG_INIT_VARS(cs, notmuch);
#endif
  CONFIG_INIT_VARS(cs, pager);
  CONFIG_INIT_VARS(cs, pattern);
#if defined(USE_POP)
  CONFIG_INIT_VARS(cs, pop);
#endif
  CONFIG_INIT_VARS(cs, send);
#if defined(USE_SIDEBAR)
  CONFIG_INIT_VARS(cs, sidebar);
#endif
}

/**
 * init_config - Initialise the config system
 * @param cs Config items
 */
void init_config(struct ConfigSet *cs)
{
  init_types(cs);
  init_variables(cs);
}
