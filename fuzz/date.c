#include "config.h"
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "mutt/lib.h"
#include "expando/lib.h"

bool StartupComplete = true;

/**
 * expando_stringify - Dummy
 */
void expando_stringify(const struct Expando *exp, const struct ExpandoDefinition *defs,
                       bool named, struct Buffer *buf)
{
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size > 512)
    return -1;

  struct Tz tz = { 0 };
  time_t t = mutt_date_parse_date((const char *) data, &tz);
  return 0;
}
