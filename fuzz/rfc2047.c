#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/queue.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"

// email/rfc2047.c: encoded-word decode; email/rfc2231.c: parameter continuations
extern void rfc2047_decode(char **pd);
extern void rfc2047_decode_addrlist(struct AddressList *al);
extern void rfc2047_decode_envelope(struct Envelope *env);
extern void rfc2231_decode_parameters(struct ParameterList *pl);

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
 * LLVMFuzzerTestOneInput - Fuzz the RFC 2047 / RFC 2231 decoders
 *
 * Threat model: encoded words and continuation parameters arrive in
 * every remote header (From/To/Subject/...) and Content-* parameter of
 * an attacker's mail.  The decoders tokenise =?charset?enc?...?=
 * framing, drive charset conversion (iconv paths) and reassemble
 * RFC 2231 continuations -- all on untrusted bytes.
 *
 * The harness runs the decode direction four ways: directly on the raw
 * bytes, through a parsed AddressList, through an Envelope field, and
 * through RFC 2231 parameter continuations.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  MuttLogger = log_disp_null;

  NeoMutt = neomutt_new();
  char **tmp_env = MUTT_MEM_CALLOC(2, char *);
  neomutt_init(NeoMutt, tmp_env, Modules);
  FREE(&tmp_env);

  if (size > 8192)
    size = 8192;

  // NUL-terminated copy for the C-string APIs below (raw `data` is not
  // NUL-terminated and must never be passed to them directly).
  char value[8192 + 1];
  memcpy(value, data, size);
  value[size] = '\0';

  // 1. Direct decode of arbitrary encoded-word text.
  char *str = mutt_strn_dup((const char *) data, size);
  rfc2047_decode(&str);
  FREE(&str);

  // 2. Decode through a parsed address list.
  str = mutt_strn_dup((const char *) data, size);
  if (str)
  {
    struct AddressList al;
    TAILQ_INIT(&al);
    mutt_addrlist_parse(&al, str);
    rfc2047_decode_addrlist(&al);
    mutt_addrlist_clear(&al);
    FREE(&str);
  }

  // 3. Decode through envelope fields.
  struct Envelope *env = mutt_env_new();
  mutt_env_set_subject(env, value);
  mutt_addrlist_parse(&env->from, value);
  rfc2047_decode_envelope(env);
  mutt_env_free(&env);

  // 4. RFC 2231 continuation parameters, extended and plain.
  struct ParameterList pl;
  TAILQ_INIT(&pl);
  mutt_param_set(&pl, "name", value);
  mutt_param_set(&pl, "name*0*", value);
  mutt_param_set(&pl, "filename*1", value);
  rfc2231_decode_parameters(&pl);
  mutt_param_free(&pl);

  neomutt_cleanup(NeoMutt);
  neomutt_free(&NeoMutt);
  return 0;
}
