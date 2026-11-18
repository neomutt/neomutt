#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "pattern/lib.h"

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
 * LLVMFuzzerTestOneInput - Fuzz the pattern compiler
 *
 * Threat model: pattern expressions are neomutt's filter grammar.
 * They reach the parser from score rules, hooks and limit prompts, and
 * several of them parse further untrusted payloads themselves (date
 * ranges "~d", header regexes "~h", message-ids "~e").  The compiler is
 * a recursive-descent parser over a token grammar (~X codes, ! | &
 * operators, quotes, parentheses) -- a classic grammar-parser fuzz
 * target for memory-safety and recursion-depth bug classes.
 *
 * mutt_pattern_comp() tolerates mv == NULL (pattern/compile.c:978),
 * so the harness compiles each input twice -- once with MUTT_PC_NONE
 * (score-rule mode) and once with body/header matching and dynamic
 * date ranges enabled (limit mode) -- and frees both results.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  MuttLogger = log_disp_null;

  NeoMutt = neomutt_new();
  char **tmp_env = MUTT_MEM_CALLOC(2, char *);
  neomutt_init(NeoMutt, tmp_env, Modules);
  FREE(&tmp_env);

  if (size > 4096)
    return -1;

  char *str = mutt_strn_dup((const char *) data, size);
  if (!str)
    return -1;

  struct Buffer err;
  buf_init(&err);

  // Score-rule mode: no special matching flags.
  struct PatternList *pat = mutt_pattern_comp(NULL, str, MUTT_PC_NONE, &err);
  if (pat)
    mutt_pattern_free(&pat);

  // Limit mode: body/header matching and dynamic date ranges enabled.
  buf_dealloc(&err);
  buf_init(&err);
  pat = mutt_pattern_comp(NULL, str, MUTT_PC_FULL_MSG | MUTT_PC_PATTERN_DYNAMIC, &err);
  if (pat)
    mutt_pattern_free(&pat);

  buf_dealloc(&err);
  FREE(&str);
  neomutt_cleanup(NeoMutt);
  neomutt_free(&NeoMutt);
  return 0;
}
