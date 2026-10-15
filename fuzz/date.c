#include "config.h"
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "mutt/lib.h"

bool StartupComplete = true;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size > 512)
    return -1;

  struct Tz tz = { 0 };
  char *str = mutt_strn_dup((const char *) data, size);
  time_t t = mutt_date_parse_date(str, &tz);
  (void) t;
  FREE(&str);
  return 0;
}
