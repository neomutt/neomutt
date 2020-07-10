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
 * @page mutt_config Definitions of config variables
 *
 * Definitions of config variables
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "browser.h"
#include "commands.h"
#include "compose.h"
#include "handler.h"
#include "hdrline.h"
#include "hook.h"
#include "index.h"
#include "init.h"
#include "mailcap.h"
#include "main.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "pager.h"
#include "pattern.h"
#include "progress.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "remailer.h"
#include "rfc3676.h"
#include "score.h"
#include "sort.h"
#include "status.h"
#include "bcache/lib.h"
#include "maildir/lib.h"
#include "ncrypt/lib.h"
#include "nntp/lib.h"
#include "notmuch/lib.h"
#include "pop/lib.h"
#include "send/lib.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif

#ifndef ISPELL
#define ISPELL "ispell"
#endif

/* These options are deprecated */
char *C_Escape = NULL;
bool C_IgnoreLinearWhiteSpace = false;

// clang-format off
struct ConfigDef MainVars[] = {
  { "abort_backspace", DT_BOOL, &C_AbortBackspace, true },
  { "abort_key", DT_STRING|DT_NOT_EMPTY, &C_AbortKey, IP "\007" },
  { "abort_noattach", DT_QUAD, &C_AbortNoattach, MUTT_NO },
  { "abort_noattach_regex", DT_REGEX, &C_AbortNoattachRegex, IP "\\<(attach|attached|attachments?)\\>" },
  { "abort_nosubject", DT_QUAD, &C_AbortNosubject, MUTT_ASKYES },
  { "abort_unmodified", DT_QUAD, &C_AbortUnmodified, MUTT_YES },
  { "alias_file", DT_PATH|DT_PATH_FILE, &C_AliasFile, IP "~/.neomuttrc" },
  { "alias_format", DT_STRING|DT_NOT_EMPTY, &C_AliasFormat, IP "%3n %f%t %-15a %-56r | %c" },
  { "allow_8bit", DT_BOOL, &C_Allow8bit, true },
  { "allow_ansi", DT_BOOL, &C_AllowAnsi, false },
  { "arrow_cursor", DT_BOOL|R_MENU, &C_ArrowCursor, false },
  { "arrow_string", DT_STRING|DT_NOT_EMPTY, &C_ArrowString, IP "->" },
  { "ascii_chars", DT_BOOL|R_INDEX|R_PAGER, &C_AsciiChars, false },
#ifdef USE_NNTP
  { "ask_follow_up", DT_BOOL, &C_AskFollowUp, false },
  { "ask_x_comment_to", DT_BOOL, &C_AskXCommentTo, false },
#endif
  { "askbcc", DT_BOOL, &C_Askbcc, false },
  { "askcc", DT_BOOL, &C_Askcc, false },
  { "assumed_charset", DT_STRING, &C_AssumedCharset, 0, 0, charset_validator },
  { "attach_charset", DT_STRING, &C_AttachCharset, 0, 0, charset_validator },
  { "attach_format", DT_STRING|DT_NOT_EMPTY, &C_AttachFormat, IP "%u%D%I %t%4n %T%.40d%> [%.7m/%.10M, %.6e%?C?, %C?, %s] " },
  { "attach_save_dir", DT_PATH|DT_PATH_DIR, &C_AttachSaveDir, IP "./" },
  { "attach_save_without_prompting", DT_BOOL, &C_AttachSaveWithoutPrompting, false },
  { "attach_sep", DT_STRING, &C_AttachSep, IP "\n" },
  { "attach_split", DT_BOOL, &C_AttachSplit, true },
  { "attribution", DT_STRING, &C_Attribution, IP "On %d, %n wrote:" },
  { "attribution_locale", DT_STRING, &C_AttributionLocale, 0 },
  { "auto_subscribe", DT_BOOL, &C_AutoSubscribe, false },
  { "auto_tag", DT_BOOL, &C_AutoTag, false },
  { "autoedit", DT_BOOL, &C_Autoedit, false },
  { "beep", DT_BOOL, &C_Beep, true },
  { "beep_new", DT_BOOL, &C_BeepNew, false },
  { "bounce", DT_QUAD, &C_Bounce, MUTT_ASKYES },
  { "bounce_delivered", DT_BOOL, &C_BounceDelivered, true },
  { "braille_friendly", DT_BOOL, &C_BrailleFriendly, false },
  { "browser_abbreviate_mailboxes", DT_BOOL, &C_BrowserAbbreviateMailboxes, true },
#ifdef USE_NNTP
  { "catchup_newsgroup", DT_QUAD, &C_CatchupNewsgroup, MUTT_ASKYES },
#endif
  { "change_folder_next", DT_BOOL, &C_ChangeFolderNext, false },
  { "charset", DT_STRING|DT_NOT_EMPTY, &C_Charset, 0, 0, charset_validator },
  { "collapse_all", DT_BOOL, &C_CollapseAll, false },
  { "collapse_flagged", DT_BOOL, &C_CollapseFlagged, true },
  { "collapse_unread", DT_BOOL, &C_CollapseUnread, true },
  { "compose_format", DT_STRING|R_MENU, &C_ComposeFormat, IP "-- NeoMutt: Compose  [Approx. msg size: %l   Atts: %a]%>-" },
  { "config_charset", DT_STRING, &C_ConfigCharset, 0, 0, charset_validator },
  { "confirmappend", DT_BOOL, &C_Confirmappend, true },
  { "confirmcreate", DT_BOOL, &C_Confirmcreate, true },
  { "content_type", DT_STRING, &C_ContentType, IP "text/plain" },
  { "copy", DT_QUAD, &C_Copy, MUTT_YES },
  { "crypt_autoencrypt", DT_BOOL, &C_CryptAutoencrypt, false },
  { "crypt_autopgp", DT_BOOL, &C_CryptAutopgp, true },
  { "crypt_autosign", DT_BOOL, &C_CryptAutosign, false },
  { "crypt_autosmime", DT_BOOL, &C_CryptAutosmime, true },
  { "crypt_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_CryptChars, IP "SPsK " },
  { "date_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_DateFormat, IP "!%a, %b %d, %Y at %I:%M:%S%p %Z" },
  { "debug_file", DT_PATH|DT_PATH_FILE, &C_DebugFile, IP "~/.neomuttdebug" },
  { "debug_level", DT_NUMBER, &C_DebugLevel, 0, 0, level_validator },
  { "default_hook", DT_STRING, &C_DefaultHook, IP "~f %s !~P | (~P ~C %s)" },
  { "delete", DT_QUAD, &C_Delete, MUTT_ASKYES },
  { "delete_untag", DT_BOOL, &C_DeleteUntag, true },
  { "digest_collapse", DT_BOOL, &C_DigestCollapse, true },
  { "display_filter", DT_STRING|DT_COMMAND|R_PAGER, &C_DisplayFilter, 0 },
  { "dsn_notify", DT_STRING, &C_DsnNotify, 0 },
  { "dsn_return", DT_STRING, &C_DsnReturn, 0 },
  { "duplicate_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_DuplicateThreads, true, 0, pager_validator },
  { "edit_headers", DT_BOOL, &C_EditHeaders, false },
  { "editor", DT_STRING|DT_NOT_EMPTY|DT_COMMAND, &C_Editor, IP "vi" },
  { "empty_subject", DT_STRING, &C_EmptySubject, IP "Re: your mail" },
  { "encode_from", DT_BOOL, &C_EncodeFrom, false },
  { "external_search_command", DT_STRING|DT_COMMAND, &C_ExternalSearchCommand, 0 },
  { "fast_reply", DT_BOOL, &C_FastReply, false },
  { "fcc_attach", DT_QUAD, &C_FccAttach, MUTT_YES },
  { "fcc_before_send", DT_BOOL, &C_FccBeforeSend, false },
  { "fcc_clear", DT_BOOL, &C_FccClear, false },
  { "flag_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FlagChars, IP "*!DdrONon- " },
  { "flag_safe", DT_BOOL, &C_FlagSafe, false },
  { "folder", DT_STRING|DT_MAILBOX, &C_Folder, IP "~/Mail" },
  { "folder_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_FolderFormat, IP "%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %i" },
  { "followup_to", DT_BOOL, &C_FollowupTo, true },
#ifdef USE_NNTP
  { "followup_to_poster", DT_QUAD, &C_FollowupToPoster, MUTT_ASKYES },
#endif
  { "force_name", DT_BOOL, &C_ForceName, false },
  { "forward_attachments", DT_QUAD, &C_ForwardAttachments, MUTT_ASKYES },
  { "forward_attribution_intro", DT_STRING, &C_ForwardAttributionIntro, IP "----- Forwarded message from %f -----" },
  { "forward_attribution_trailer", DT_STRING, &C_ForwardAttributionTrailer, IP "----- End forwarded message -----" },
  { "forward_decode", DT_BOOL, &C_ForwardDecode, true },
  { "forward_decrypt", DT_BOOL, &C_ForwardDecrypt, true },
  { "forward_edit", DT_QUAD, &C_ForwardEdit, MUTT_YES },
  { "forward_format", DT_STRING|DT_NOT_EMPTY, &C_ForwardFormat, IP "[%a: %s]" },
  { "forward_quote", DT_BOOL, &C_ForwardQuote, false },
  { "forward_references", DT_BOOL, &C_ForwardReferences, false },
  { "from", DT_ADDRESS, &C_From, 0 },
  { "from_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FromChars, 0 },
  { "gecos_mask", DT_REGEX, &C_GecosMask, IP "^[^,]*" },
#ifdef USE_NNTP
  { "group_index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, &C_GroupIndexFormat, IP "%4C %M%N %5s  %-45.45f %d" },
#endif
  { "hdrs", DT_BOOL, &C_Hdrs, true },
  { "header", DT_BOOL, &C_Header, false },
  { "header_color_partial", DT_BOOL|R_PAGER_FLOW, &C_HeaderColorPartial, false },
  { "help", DT_BOOL|R_REFLOW, &C_Help, true },
  { "hidden_host", DT_BOOL, &C_HiddenHost, false },
  { "hidden_tags", DT_SLIST|SLIST_SEP_COMMA, &C_HiddenTags, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted" },
  { "hide_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideLimited, false },
  { "hide_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideMissing, true },
  { "hide_thread_subject", DT_BOOL|R_TREE|R_INDEX, &C_HideThreadSubject, true },
  { "hide_top_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideTopLimited, false },
  { "hide_top_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideTopMissing, true },
  { "honor_disposition", DT_BOOL, &C_HonorDisposition, false },
  { "honor_followup_to", DT_QUAD, &C_HonorFollowupTo, MUTT_YES },
  { "hostname", DT_STRING, &C_Hostname, 0 },
#ifdef HAVE_LIBIDN
  { "idn_decode", DT_BOOL|R_MENU, &C_IdnDecode, true },
  { "idn_encode", DT_BOOL|R_MENU, &C_IdnEncode, true },
#endif
  { "ignore_list_reply_to", DT_BOOL, &C_IgnoreListReplyTo, false },
  { "implicit_autoview", DT_BOOL, &C_ImplicitAutoview, false },
  { "include", DT_QUAD, &C_Include, MUTT_ASKYES },
  { "include_encrypted", DT_BOOL, &C_IncludeEncrypted, false },
  { "include_onlyfirst", DT_BOOL, &C_IncludeOnlyfirst, false },
  { "indent_string", DT_STRING, &C_IndentString, IP "> " },
  { "index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, &C_IndexFormat, IP "%4C %Z %{%b %d} %-15.15L (%?l?%4l&%4c?) %s" },
#ifdef USE_NNTP
  { "inews", DT_STRING|DT_COMMAND, &C_Inews, 0 },
#endif
  { "ispell", DT_STRING|DT_COMMAND, &C_Ispell, IP ISPELL },
  { "keep_flagged", DT_BOOL, &C_KeepFlagged, false },
  { "mail_check", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheck, 5 },
  { "mail_check_recent", DT_BOOL, &C_MailCheckRecent, true },
  { "mail_check_stats", DT_BOOL, &C_MailCheckStats, false },
  { "mail_check_stats_interval", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheckStatsInterval, 60 },
  { "mailcap_path", DT_SLIST|SLIST_SEP_COLON, &C_MailcapPath, IP "~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap" },
  { "mailcap_sanitize", DT_BOOL, &C_MailcapSanitize, true },
  { "mark_macro_prefix", DT_STRING, &C_MarkMacroPrefix, IP "'" },
  { "mark_old", DT_BOOL|R_INDEX|R_PAGER, &C_MarkOld, true },
  { "markers", DT_BOOL|R_PAGER_FLOW, &C_Markers, true },
  { "mask", DT_REGEX|DT_REGEX_MATCH_CASE|DT_REGEX_ALLOW_NOT|DT_REGEX_NOSUB, &C_Mask, IP "!^\\.[^.]" },
  { "mbox", DT_STRING|DT_MAILBOX|R_INDEX|R_PAGER, &C_Mbox, IP "~/mbox" },
  { "mbox_type", DT_ENUM, &C_MboxType, MUTT_MBOX, IP &MboxTypeDef },
  { "menu_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_MenuContext, 0 },
  { "menu_move_off", DT_BOOL, &C_MenuMoveOff, true },
  { "menu_scroll", DT_BOOL, &C_MenuScroll, false },
  { "message_cache_clean", DT_BOOL, &C_MessageCacheClean, false },
  { "message_cachedir", DT_PATH|DT_PATH_DIR, &C_MessageCachedir, 0 },
  { "message_format", DT_STRING|DT_NOT_EMPTY, &C_MessageFormat, IP "%s" },
  { "meta_key", DT_BOOL, &C_MetaKey, false },
  { "metoo", DT_BOOL, &C_Metoo, false },
  { "mime_forward", DT_QUAD, &C_MimeForward, MUTT_NO },
  { "mime_forward_decode", DT_BOOL, &C_MimeForwardDecode, false },
  { "mime_forward_rest", DT_QUAD, &C_MimeForwardRest, MUTT_YES },
#ifdef USE_NNTP
  { "mime_subject", DT_BOOL, &C_MimeSubject, true },
#endif
  { "mime_type_query_command", DT_STRING|DT_COMMAND, &C_MimeTypeQueryCommand, 0 },
  { "mime_type_query_first", DT_BOOL, &C_MimeTypeQueryFirst, false },
#ifdef MIXMASTER
  { "mix_entry_format", DT_STRING|DT_NOT_EMPTY, &C_MixEntryFormat, IP "%4n %c %-16s %a" },
  { "mixmaster", DT_STRING|DT_COMMAND, &C_Mixmaster, IP MIXMASTER },
#endif
  { "move", DT_QUAD, &C_Move, MUTT_NO },
  { "narrow_tree", DT_BOOL|R_TREE|R_INDEX, &C_NarrowTree, false },
  { "net_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_NetInc, 10 },
  { "new_mail_command", DT_STRING|DT_COMMAND, &C_NewMailCommand, 0 },
#ifdef USE_NNTP
  { "news_cache_dir", DT_PATH|DT_PATH_DIR, &C_NewsCacheDir, IP "~/.neomutt" },
  { "news_server", DT_STRING, &C_NewsServer, 0 },
  { "newsgroups_charset", DT_STRING, &C_NewsgroupsCharset, IP "utf-8", 0, charset_validator },
  { "newsrc", DT_PATH|DT_PATH_FILE, &C_Newsrc, IP "~/.newsrc" },
#endif
#ifdef USE_NOTMUCH
  { "nm_db_limit", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmDbLimit, 0 },
  { "nm_default_url", DT_STRING, &C_NmDefaultUrl, 0 },
  { "nm_exclude_tags", DT_STRING, &C_NmExcludeTags, 0 },
  { "nm_flagged_tag", DT_STRING, &C_NmFlaggedTag, IP "flagged" },
  { "nm_open_timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmOpenTimeout, 5 },
  { "nm_query_type", DT_STRING, &C_NmQueryType, IP "messages" },
  { "nm_query_window_current_position", DT_NUMBER, &C_NmQueryWindowCurrentPosition, 0 },
  { "nm_query_window_current_search", DT_STRING, &C_NmQueryWindowCurrentSearch, 0 },
  { "nm_query_window_duration", DT_NUMBER|DT_NOT_NEGATIVE, &C_NmQueryWindowDuration, 0 },
  { "nm_query_window_timebase", DT_STRING, &C_NmQueryWindowTimebase, IP "week" },
  { "nm_record", DT_BOOL, &C_NmRecord, false },
  { "nm_record_tags", DT_STRING, &C_NmRecordTags, 0 },
  { "nm_replied_tag", DT_STRING, &C_NmRepliedTag, IP "replied" },
  { "nm_unread_tag", DT_STRING, &C_NmUnreadTag, IP "unread" },
#endif
#ifdef USE_NNTP
  { "nntp_authenticators", DT_STRING, &C_NntpAuthenticators, 0 },
  { "nntp_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_NntpContext, 1000 },
  { "nntp_listgroup", DT_BOOL, &C_NntpListgroup, true },
  { "nntp_load_description", DT_BOOL, &C_NntpLoadDescription, true },
  { "nntp_pass", DT_STRING|DT_SENSITIVE, &C_NntpPass, 0 },
  { "nntp_poll", DT_NUMBER|DT_NOT_NEGATIVE, &C_NntpPoll, 60 },
  { "nntp_user", DT_STRING|DT_SENSITIVE, &C_NntpUser, 0 },
#endif
  { "pager", DT_STRING|DT_COMMAND, &C_Pager, IP "builtin" },
  { "pager_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_PagerContext, 0 },
  { "pager_format", DT_STRING|R_PAGER, &C_PagerFormat, IP "-%Z- %C/%m: %-20.20n   %s%*  -- (%P)" },
  { "pager_index_lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER|R_REFLOW, &C_PagerIndexLines, 0 },
  { "pager_stop", DT_BOOL, &C_PagerStop, false },
  { "pipe_decode", DT_BOOL, &C_PipeDecode, false },
  { "pipe_sep", DT_STRING, &C_PipeSep, IP "\n" },
  { "pipe_split", DT_BOOL, &C_PipeSplit, false },
#ifdef USE_POP
  { "pop_auth_try_all", DT_BOOL, &C_PopAuthTryAll, true },
  { "pop_authenticators", DT_SLIST|SLIST_SEP_COLON, &C_PopAuthenticators, 0 },
  { "pop_checkinterval", DT_NUMBER|DT_NOT_NEGATIVE, &C_PopCheckinterval, 60 },
  { "pop_delete", DT_QUAD, &C_PopDelete, MUTT_ASKNO },
  { "pop_host", DT_STRING, &C_PopHost, 0 },
  { "pop_last", DT_BOOL, &C_PopLast, false },
  { "pop_oauth_refresh_command", DT_STRING|DT_COMMAND|DT_SENSITIVE, &C_PopOauthRefreshCommand, 0 },
  { "pop_pass", DT_STRING|DT_SENSITIVE, &C_PopPass, 0 },
  { "pop_reconnect", DT_QUAD, &C_PopReconnect, MUTT_ASKYES },
  { "pop_user", DT_STRING|DT_SENSITIVE, &C_PopUser, 0 },
#endif
  { "post_indent_string", DT_STRING, &C_PostIndentString, 0 },
#ifdef USE_NNTP
  { "post_moderated", DT_QUAD, &C_PostModerated, MUTT_ASKYES },
#endif
  { "postpone", DT_QUAD, &C_Postpone, MUTT_ASKYES },
  { "postpone_encrypt", DT_BOOL, &C_PostponeEncrypt, false },
  { "postpone_encrypt_as", DT_STRING, &C_PostponeEncryptAs, 0 },
  { "postponed", DT_STRING|DT_MAILBOX|R_INDEX, &C_Postponed, IP "~/postponed" },
  { "preferred_languages", DT_SLIST|SLIST_SEP_COMMA, &C_PreferredLanguages, 0 },
  { "print", DT_QUAD, &C_Print, MUTT_ASKNO },
  { "print_command", DT_STRING|DT_COMMAND, &C_PrintCommand, IP "lpr" },
  { "print_decode", DT_BOOL, &C_PrintDecode, true },
  { "print_split", DT_BOOL, &C_PrintSplit, false },
  { "prompt_after", DT_BOOL, &C_PromptAfter, true },
  { "query_command", DT_STRING|DT_COMMAND, &C_QueryCommand, 0 },
  { "query_format", DT_STRING|DT_NOT_EMPTY, &C_QueryFormat, IP "%3c %t %-25.25n %-25.25a | %e" },
  { "quit", DT_QUAD, &C_Quit, MUTT_YES },
  { "quote_regex", DT_REGEX|R_PAGER, &C_QuoteRegex, IP "^([ \t]*[|>:}#])+" },
  { "read_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_ReadInc, 10 },
  { "read_only", DT_BOOL, &C_ReadOnly, false },
  { "realname", DT_STRING|R_INDEX|R_PAGER, &C_Realname, 0 },
  { "recall", DT_QUAD, &C_Recall, MUTT_ASKYES },
  { "record", DT_STRING|DT_MAILBOX, &C_Record, IP "~/sent" },
  { "reflow_space_quotes", DT_BOOL, &C_ReflowSpaceQuotes, true },
  { "reflow_text", DT_BOOL, &C_ReflowText, true },
  { "reflow_wrap", DT_NUMBER, &C_ReflowWrap, 78 },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, &C_ReplyRegex, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, reply_validator },
  { "reply_self", DT_BOOL, &C_ReplySelf, false },
  { "reply_to", DT_QUAD, &C_ReplyTo, MUTT_ASKYES },
  { "reply_with_xorig", DT_BOOL, &C_ReplyWithXorig, false },
  { "resolve", DT_BOOL, &C_Resolve, true },
  { "resume_draft_files", DT_BOOL, &C_ResumeDraftFiles, false },
  { "resume_edited_draft_files", DT_BOOL, &C_ResumeEditedDraftFiles, true },
  { "reverse_alias", DT_BOOL|R_INDEX|R_PAGER, &C_ReverseAlias, false },
  { "reverse_name", DT_BOOL|R_INDEX|R_PAGER, &C_ReverseName, false },
  { "reverse_realname", DT_BOOL|R_INDEX|R_PAGER, &C_ReverseRealname, true },
  { "rfc2047_parameters", DT_BOOL, &C_Rfc2047Parameters, false },
  { "save_address", DT_BOOL, &C_SaveAddress, false },
  { "save_empty", DT_BOOL, &C_SaveEmpty, true },
  { "save_name", DT_BOOL, &C_SaveName, false },
#ifdef USE_NNTP
  { "save_unsubscribed", DT_BOOL, &C_SaveUnsubscribed, false },
#endif
  { "score", DT_BOOL, &C_Score, true },
  { "score_threshold_delete", DT_NUMBER, &C_ScoreThresholdDelete, -1 },
  { "score_threshold_flag", DT_NUMBER, &C_ScoreThresholdFlag, 9999 },
  { "score_threshold_read", DT_NUMBER, &C_ScoreThresholdRead, -1 },
  { "search_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_SearchContext, 0 },
  { "send_charset", DT_STRING, &C_SendCharset, IP "us-ascii:iso-8859-1:utf-8", 0, charset_validator },
  { "sendmail", DT_STRING|DT_COMMAND, &C_Sendmail, IP SENDMAIL " -oem -oi" },
  { "sendmail_wait", DT_NUMBER, &C_SendmailWait, 0 },
  { "shell", DT_STRING|DT_COMMAND, &C_Shell, IP "/bin/sh" },
  { "show_multipart_alternative", DT_STRING, &C_ShowMultipartAlternative, 0, 0, multipart_validator },
#ifdef USE_NNTP
  { "show_new_news", DT_BOOL, &C_ShowNewNews, true },
  { "show_only_unread", DT_BOOL, &C_ShowOnlyUnread, false },
#endif
#ifdef USE_SIDEBAR
  { "sidebar_component_depth", DT_NUMBER|R_SIDEBAR, &C_SidebarComponentDepth, 0 },
  { "sidebar_delim_chars", DT_STRING|R_SIDEBAR, &C_SidebarDelimChars, IP "/." },
  { "sidebar_divider_char", DT_STRING|R_SIDEBAR, &C_SidebarDividerChar, 0 },
  { "sidebar_folder_indent", DT_BOOL|R_SIDEBAR, &C_SidebarFolderIndent, false },
  { "sidebar_format", DT_STRING|DT_NOT_EMPTY|R_SIDEBAR, &C_SidebarFormat, IP "%D%*  %n" },
  { "sidebar_indent_string", DT_STRING|R_SIDEBAR, &C_SidebarIndentString, IP "  " },
  { "sidebar_new_mail_only", DT_BOOL|R_SIDEBAR, &C_SidebarNewMailOnly, false },
  { "sidebar_next_new_wrap", DT_BOOL, &C_SidebarNextNewWrap, false },
  { "sidebar_non_empty_mailbox_only", DT_BOOL|R_SIDEBAR, &C_SidebarNonEmptyMailboxOnly, false },
  { "sidebar_on_right", DT_BOOL|R_INDEX|R_PAGER|R_REFLOW, &C_SidebarOnRight, false },
  { "sidebar_short_path", DT_BOOL|R_SIDEBAR, &C_SidebarShortPath, false },
  { "sidebar_sort_method", DT_SORT|DT_SORT_SIDEBAR|R_SIDEBAR, &C_SidebarSortMethod, SORT_ORDER },
  { "sidebar_visible", DT_BOOL|R_REFLOW, &C_SidebarVisible, false },
  { "sidebar_width", DT_NUMBER|DT_NOT_NEGATIVE|R_REFLOW, &C_SidebarWidth, 30 },
#endif
  { "sig_dashes", DT_BOOL, &C_SigDashes, true },
  { "sig_on_top", DT_BOOL, &C_SigOnTop, false },
  { "signature", DT_PATH|DT_PATH_FILE, &C_Signature, IP "~/.signature" },
  { "simple_search", DT_STRING, &C_SimpleSearch, IP "~f %s | ~s %s" },
  { "size_show_bytes", DT_BOOL|R_MENU, &C_SizeShowBytes, false },
  { "size_show_fractions", DT_BOOL|R_MENU, &C_SizeShowFractions, true },
  { "size_show_mb", DT_BOOL|R_MENU, &C_SizeShowMb, true },
  { "size_units_on_left", DT_BOOL|R_MENU, &C_SizeUnitsOnLeft, false },
  { "skip_quoted_offset", DT_NUMBER|DT_NOT_NEGATIVE, &C_SkipQuotedOffset, 0 },
  { "sleep_time", DT_NUMBER|DT_NOT_NEGATIVE, &C_SleepTime, 1 },
  { "smart_wrap", DT_BOOL|R_PAGER_FLOW, &C_SmartWrap, true },
  { "smileys", DT_REGEX|R_PAGER, &C_Smileys, IP "(>From )|(:[-^]?[][)(><}{|/DP])" },
#ifdef USE_SMTP
  { "smtp_authenticators", DT_SLIST|SLIST_SEP_COLON, &C_SmtpAuthenticators, 0 },
  { "smtp_oauth_refresh_command", DT_STRING|DT_COMMAND|DT_SENSITIVE, &C_SmtpOauthRefreshCommand, 0 },
  { "smtp_pass", DT_STRING|DT_SENSITIVE, &C_SmtpPass, 0 },
  { "smtp_user", DT_STRING|DT_SENSITIVE, &C_SmtpUser, 0 },
  { "smtp_url", DT_STRING|DT_SENSITIVE, &C_SmtpUrl, 0 },
#endif
  { "sort", DT_SORT|R_INDEX|R_RESORT, &C_Sort, SORT_DATE, 0, pager_validator },
  { "sort_alias", DT_SORT|DT_SORT_ALIAS, &C_SortAlias, SORT_ALIAS },
  { "sort_aux", DT_SORT|DT_SORT_AUX|R_INDEX|R_RESORT|R_RESORT_SUB, &C_SortAux, SORT_DATE },
  { "sort_browser", DT_SORT|DT_SORT_BROWSER, &C_SortBrowser, SORT_ALPHA },
  { "sort_re", DT_BOOL|R_INDEX|R_RESORT|R_RESORT_INIT, &C_SortRe, true, 0, pager_validator },
  { "spam_separator", DT_STRING, &C_SpamSeparator, IP "," },
  { "spoolfile", DT_STRING|DT_MAILBOX, &C_Spoolfile, 0 },
  { "status_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_StatusChars, IP "-*%A" },
  { "status_format", DT_STRING|R_INDEX|R_PAGER, &C_StatusFormat, IP "-%r-NeoMutt: %D [Msgs:%?M?%M/?%m%?n? New:%n?%?o? Old:%o?%?d? Del:%d?%?F? Flag:%F?%?t? Tag:%t?%?p? Post:%p?%?b? Inc:%b?%?l? %l?]---(%s/%S)-%>-(%P)---" },
  { "status_on_top", DT_BOOL|R_REFLOW, &C_StatusOnTop, false },
  { "strict_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_StrictThreads, false, 0, pager_validator },
  { "suspend", DT_BOOL, &C_Suspend, true },
  { "text_flowed", DT_BOOL, &C_TextFlowed, false },
  { "thorough_search", DT_BOOL, &C_ThoroughSearch, true },
  { "thread_received", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_ThreadReceived, false, 0, pager_validator },
  { "tilde", DT_BOOL|R_PAGER, &C_Tilde, false },
  { "time_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_TimeInc, 0 },
  { "timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_Timeout, 600 },
  { "tmpdir", DT_PATH|DT_PATH_DIR|DT_NOT_EMPTY, &C_Tmpdir, IP "/tmp" },
  { "toggle_quoted_show_levels", DT_NUMBER|DT_NOT_NEGATIVE, &C_ToggleQuotedShowLevels, 0 },
  { "to_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_ToChars, IP " +TCFLR" },
  { "trash", DT_STRING|DT_MAILBOX, &C_Trash, 0 },
  { "ts_enabled", DT_BOOL|R_INDEX|R_PAGER, &C_TsEnabled, false },
  { "ts_icon_format", DT_STRING|R_INDEX|R_PAGER, &C_TsIconFormat, IP "M%?n?AIL&ail?" },
  { "ts_status_format", DT_STRING|R_INDEX|R_PAGER, &C_TsStatusFormat, IP "NeoMutt with %?m?%m messages&no messages?%?n? [%n NEW]?" },
  { "uncollapse_jump", DT_BOOL, &C_UncollapseJump, false },
  { "uncollapse_new", DT_BOOL, &C_UncollapseNew, true },
  { "use_8bitmime", DT_BOOL, &C_Use8bitmime, false },
  { "use_domain", DT_BOOL, &C_UseDomain, true },
  { "use_envelope_from", DT_BOOL, &C_UseEnvelopeFrom, false },
  { "use_from", DT_BOOL, &C_UseFrom, true },
  { "user_agent", DT_BOOL, &C_UserAgent, false },
#ifdef USE_NOTMUCH
  { "vfolder_format", DT_STRING|DT_NOT_EMPTY|R_INDEX, &C_VfolderFormat, IP "%2C %?n?%4n/&     ?%4m %f" },
  { "virtual_spoolfile", DT_BOOL, &C_VirtualSpoolfile, false },
#endif
  { "visual", DT_STRING|DT_COMMAND, &C_Visual, IP "vi" },
  { "wait_key", DT_BOOL, &C_WaitKey, true },
  { "weed", DT_BOOL, &C_Weed, true },
  { "wrap", DT_NUMBER|R_PAGER_FLOW, &C_Wrap, 0 },
  { "wrap_headers", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER, &C_WrapHeaders, 78, 0, wrapheaders_validator },
  { "wrap_search", DT_BOOL, &C_WrapSearch, true },
  { "write_bcc", DT_BOOL, &C_WriteBcc, false },
  { "write_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_WriteInc, 10 },
#ifdef USE_NNTP
  { "x_comment_to", DT_BOOL, &C_XCommentTo, false },
#endif
  { "escape", DT_DEPRECATED|DT_STRING, &C_Escape, IP "~" },

  { "ignore_linear_white_space", DT_DEPRECATED|DT_BOOL,            &C_IgnoreLinearWhiteSpace, false   },

  { "abort_noattach_regexp",  DT_SYNONYM, NULL, IP "abort_noattach_regex",     },
  { "attach_keyword",         DT_SYNONYM, NULL, IP "abort_noattach_regex",     },
  { "edit_hdrs",              DT_SYNONYM, NULL, IP "edit_headers",             },
  { "envelope_from",          DT_SYNONYM, NULL, IP "use_envelope_from",        },
  { "forw_decode",            DT_SYNONYM, NULL, IP "forward_decode",           },
  { "forw_decrypt",           DT_SYNONYM, NULL, IP "forward_decrypt",          },
  { "forw_format",            DT_SYNONYM, NULL, IP "forward_format",           },
  { "forw_quote",             DT_SYNONYM, NULL, IP "forward_quote",            },
  { "hdr_format",             DT_SYNONYM, NULL, IP "index_format",             },
  { "indent_str",             DT_SYNONYM, NULL, IP "indent_string",            },
  { "mime_fwd",               DT_SYNONYM, NULL, IP "mime_forward",             },
  { "msg_format",             DT_SYNONYM, NULL, IP "message_format",           },
#ifdef USE_NOTMUCH
  { "nm_default_uri",         DT_SYNONYM, NULL, IP "nm_default_url",           },
#endif
  { "pgp_autoencrypt",        DT_SYNONYM, NULL, IP "crypt_autoencrypt",        },
  { "pgp_autosign",           DT_SYNONYM, NULL, IP "crypt_autosign",           },
  { "pgp_auto_traditional",   DT_SYNONYM, NULL, IP "pgp_replyinline",          },
  { "pgp_replyencrypt",       DT_SYNONYM, NULL, IP "crypt_replyencrypt",       },
  { "pgp_replysign",          DT_SYNONYM, NULL, IP "crypt_replysign",          },
  { "pgp_replysignencrypted", DT_SYNONYM, NULL, IP "crypt_replysignencrypted", },
  { "post_indent_str",        DT_SYNONYM, NULL, IP "post_indent_string",       },
  { "print_cmd",              DT_SYNONYM, NULL, IP "print_command",            },
  { "quote_regexp",           DT_SYNONYM, NULL, IP "quote_regex",              },
  { "reply_regexp",           DT_SYNONYM, NULL, IP "reply_regex",              },
  { "xterm_icon",             DT_SYNONYM, NULL, IP "ts_icon_format",           },
  { "xterm_set_titles",       DT_SYNONYM, NULL, IP "ts_enabled",               },
  { "xterm_title",            DT_SYNONYM, NULL, IP "ts_status_format",         },

  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_main - Register main config variables
 */
bool config_init_main(struct ConfigSet *cs)
{
  return cs_register_variables(cs, MainVars, 0);
}
