#include "config.h"
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "mutt/logging.h"
#include "config/set.h"
#include "email/body.h"
#include "email/email.h"
#include "email/envelope.h"
#include "email/mime.h"
#include "email/parse.h"
#include "core/neomutt.h"
#include "globals.h"
#include "init.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  MuttLogger = log_disp_null;
  struct ConfigSet *cs = cs_new(16);
  NeoMutt = neomutt_new(cs);
  init_config(cs);
  OptNoCurses = true;
  char file[] = "/tmp/mutt-fuzz";
  FILE *fp = fopen(file, "wb");
  if (fp != NULL)
  {
    fwrite(data, 1, size, fp);
    fclose(fp);
  }
  fp = fopen(file, "rb");
  struct Email *e = email_new();
  struct Envelope *env = mutt_rfc822_read_header(fp, e, 0, 0);
  mutt_parse_part(fp, e->body);
  email_free(&e);
  mutt_env_free(&env);
  fclose(fp);
  neomutt_free(&NeoMutt);
  cs_free(&cs);
  return 0;
}
