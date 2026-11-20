#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mx.h"
#include "mutt.h"

bool StartupComplete = true;

// clang-format off
extern const struct Module ModuleMain;
extern const struct Module ModuleAddress;  extern const struct Module ModuleAlias;    extern const struct Module ModuleAttach;   extern const struct Module ModuleAutocrypt;
extern const struct Module ModuleBcache;   extern const struct Module ModuleBrowser;  extern const struct Module ModuleColor;    extern const struct Module ModuleCommands;
extern const struct Module ModuleComplete; extern const struct Module ModuleCompmbox; extern const struct Module ModuleCompose;  extern const struct Module ModuleCompress;
extern const struct Module ModuleConfig;   extern const struct Module ModuleConn;     extern const struct Module ModuleConvert;  extern const struct Module ModuleCore;
extern const struct Module ModuleEditor;   extern const struct Module ModuleEmail;    extern const struct Module ModuleEnvelope; extern const struct Module ModuleExpando;
extern const struct Module ModuleGui;      extern const struct Module ModuleHcache;   extern const struct Module ModuleHelpbar;  extern const struct Module ModuleHistory;
extern const struct Module ModuleHooks;    extern const struct Module ModuleImap;     extern const struct Module ModuleIndex;    extern const struct Module ModuleKey;
extern const struct Module ModuleLua;      extern const struct Module ModuleMaildir;  extern const struct Module ModuleMbox;     extern const struct Module ModuleMenu;
extern const struct Module ModuleMh;       extern const struct Module ModuleMutt;     extern const struct Module ModuleNcrypt;   extern const struct Module ModuleNntp;
extern const struct Module ModuleNotmuch;  extern const struct Module ModulePager;    extern const struct Module ModuleParse;    extern const struct Module ModulePattern;
extern const struct Module ModulePop;      extern const struct Module ModulePostpone; extern const struct Module ModuleProgress; extern const struct Module ModuleQuestion;
extern const struct Module ModuleSend;     extern const struct Module ModuleSidebar;  extern const struct Module ModuleStore;
// clang-format on

static const struct Module *Modules[] = {
  // clang-format off
  &ModuleMain,     &ModuleGui,
  &ModuleAddress,  &ModuleAlias,    &ModuleAttach,   &ModuleBcache,   &ModuleBrowser,
  &ModuleColor,    &ModuleCommands, &ModuleComplete, &ModuleCompmbox, &ModuleCompose,
  &ModuleConfig,   &ModuleConn,     &ModuleConvert,  &ModuleCore,     &ModuleEditor,
  &ModuleEmail,    &ModuleEnvelope, &ModuleExpando,  &ModuleHelpbar,  &ModuleHistory,
  &ModuleHooks,    &ModuleImap,     &ModuleIndex,    &ModuleKey,      &ModuleMaildir,
  &ModuleMbox,     &ModuleMenu,     &ModuleMh,       &ModuleMutt,     &ModuleNcrypt,
  &ModuleNntp,     &ModulePager,    &ModuleParse,    &ModulePattern,  &ModulePop,
  &ModulePostpone, &ModuleProgress, &ModuleQuestion, &ModuleSend,     &ModuleSidebar,
// clang-format on
#ifdef USE_AUTOCRYPT
  &ModuleAutocrypt,
#endif
#ifdef USE_HCACHE_COMPRESSION
  &ModuleCompress,
#endif
#ifdef USE_HCACHE
  &ModuleHcache,
#endif
#ifdef USE_LUA
  &ModuleLua,
#endif
#ifdef USE_NOTMUCH
  &ModuleNotmuch,
#endif
#ifdef USE_HCACHE
  &ModuleStore,
#endif
  NULL,
};

/**
 * LLVMFuzzerTestOneInput - Fuzz the mbox/MMDF mailbox parsers
 *
 * Threat model: a mailbox file is untrusted data.  mbox files arrive from
 * remote sources (saved mail, spool files, IMAP append) and their parsers
 * drive pointer arithmetic with From_ line scanning and Content-Length
 * offsets; MMDF adds its own separator state machine.  The harness writes
 * the raw bytes to a temp file and runs the real mx open path:
 * format probe, mbox_parse_mailbox()/mmdf_parse_mailbox(), then parses
 * every message body through the offset-based message stream.
 *
 * Random bytes rarely produce the "From " / Ctrl-A separators these
 * parsers require, so the OSS-Fuzz build ships a generated seed corpus
 * and a dictionary with those structure tokens (generate_mbox_seeds.py,
 * mbox-dict.txt).
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  MuttLogger = log_disp_null;

  NeoMutt = neomutt_new();
  char **tmp_env = MUTT_MEM_CALLOC(2, char *);
  neomutt_init(NeoMutt, tmp_env, Modules);
  FREE(&tmp_env);

  char file[] = "/tmp/mutt-fuzz-mbox";
  FILE *fp = fopen(file, "wb");
  if (!fp)
    return -1;
  fwrite(data, 1, size, fp);
  fclose(fp);

  // Let the format probe decide; keep both parsers reachable when the
  // probe cannot classify the bytes.
  enum MailboxType type = mx_path_probe(file);
  if ((type != MUTT_MBOX) && (type != MUTT_MMDF))
    type = ((size > 0) && (data[0] & 1)) ? MUTT_MMDF : MUTT_MBOX;

  struct Mailbox *m = mailbox_new();
  buf_addstr(&m->pathbuf, file);
  m->type = type;
  m->mx_ops = mx_get_ops(m->type);

  if (mx_mbox_open(m, MUTT_QUIET | MUTT_NOSORT | MUTT_PEEK))
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        continue;
      struct Message *msg = mx_msg_open(m, e);
      if (!msg)
        continue;
      mutt_parse_part(msg->fp, e->body);
      message_free(&msg);
    }
    mx_mbox_close(m);
  }
  mailbox_free(&m);

  neomutt_cleanup(NeoMutt);
  neomutt_free(&NeoMutt);
  return 0;
}
