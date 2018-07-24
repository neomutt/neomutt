#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/logging.h"
#include "dump/dump.h"
#include "test/config/account2.h"
#include "test/config/address.h"
#include "test/config/bool.h"
#include "test/config/command.h"
#include "test/config/initial.h"
#include "test/config/long.h"
#include "test/config/magic.h"
#include "test/config/mbtable.h"
#include "test/config/number.h"
#include "test/config/path.h"
#include "test/config/quad.h"
#include "test/config/regex3.h"
#include "test/config/set.h"
#include "test/config/sort.h"
#include "test/config/string4.h"
#include "test/config/synonym.h"

typedef bool (*test_fn)(void);

int log_disp_stdout(time_t stamp, const char *file, int line,
                    const char *function, int level, ...)
{
  int err = errno;

  va_list ap;
  va_start(ap, level);
  const char *fmt = va_arg(ap, const char *);
  int ret = vprintf(fmt, ap);
  va_end(ap);

  if (level == LL_PERROR)
    ret += printf("%s", strerror(err));

  return ret;
}

struct Test
{
  const char *name;
  test_fn function;
};

// clang-format off
struct Test test[] = {
  { "set",       set_test       },
  { "account",   account_test   },
  { "initial",   initial_test   },
  { "synonym",   synonym_test   },
  { "address",   address_test   },
  { "bool",      bool_test      },
  { "command",   command_test   },
  { "long",      long_test      },
  { "magic",     magic_test     },
  { "mbtable",   mbtable_test   },
  { "number",    number_test    },
  { "path",      path_test      },
  { "quad",      quad_test      },
  { "regex",     regex_test     },
  { "sort",      sort_test      },
  { "string",    string_test    },
  { "dump",      dump_test      },
  { NULL },
};
// clang-format on

int main(int argc, char *argv[])
{
  int result = 0;

  if (argc < 2)
  {
    printf("Usage: %s TEST ...\n", argv[0]);
    for (int i = 0; test[i].name; i++)
      printf("    %s\n", test[i].name);
    return 1;
  }

  MuttLogger = log_disp_stdout;

  for (; --argc > 0; argv++)
  {
    struct Test *t = NULL;

    for (int i = 0; test[i].name; i++)
    {
      if (strcmp(argv[1], test[i].name) == 0)
      {
        t = &test[i];
        break;
      }
    }
    if (!t)
    {
      printf("Unknown test '%s'\n", argv[1]);
      result = 1;
      continue;
    }

    if (!t->function())
    {
      printf("%s_test() failed\n", t->name);
      result = 1;
    }
  }

  return result;
}
