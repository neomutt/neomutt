/*!
The MIT License (MIT)

Copyright (c) 2016 Dmitriy Kubyshkin <dmitriy@kubyshkin.name>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN struct Connection WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef BDD_FOR_C_H
#define BDD_FOR_C_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <term.h>
#include <unistd.h>
#define __BDD_IS_ATTY__() isatty(fileno(stdin))
#else
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>
#define __BDD_IS_ATTY__() _isatty(_fileno(stdin))
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // _CRT_SECURE_NO_WARNINGS
#endif

#ifndef BDD_USE_COLOR
#define BDD_USE_COLOR 1
#endif

#ifndef BDD_USE_TAP
#define BDD_USE_TAP 0
#endif

#define __BDD_COLOR_RESET__ "\x1B[0m"
#define __BDD_COLOR_BLACK__ "\x1B[30m"              /* Black */
#define __BDD_COLOR_RED__ "\x1B[31m"                /* Red */
#define __BDD_COLOR_GREEN__ "\x1B[32m"              /* Green */
#define __BDD_COLOR_YELLOW__ "\x1B[33m"             /* Yellow */
#define __BDD_COLOR_BLUE__ "\x1B[34m"               /* Blue */
#define __BDD_COLOR_MAGENTA__ "\x1B[35m"            /* Magenta */
#define __BDD_COLOR_CYAN__ "\x1B[36m"               /* Cyan */
#define __BDD_COLOR_WHITE__ "\x1B[37m"              /* White */
#define __BDD_COLOR_BOLDBLACK__ "\x1B[1m\033[30m"   /* Bold Black */
#define __BDD_COLOR_BOLDRED__ "\x1B[1m\033[31m"     /* Bold Red */
#define __BDD_COLOR_BOLDGREEN__ "\x1B[1m\033[32m"   /* Bold Green */
#define __BDD_COLOR_BOLDYELLOW__ "\x1B[1m\033[33m"  /* Bold Yellow */
#define __BDD_COLOR_BOLDBLUE__ "\x1B[1m\033[34m"    /* Bold Blue */
#define __BDD_COLOR_BOLDMAGENTA__ "\x1B[1m\033[35m" /* Bold Magenta */
#define __BDD_COLOR_BOLDCYAN__ "\x1B[1m\033[36m"    /* Bold Cyan */
#define __BDD_COLOR_BOLD__ "\x1B[1m"                /* Bold White */

enum __bdd_run_type__
{
  __BDD_INIT_RUN__ = 1,
  __BDD_TEST_RUN__ = 2,
  __BDD_BEFORE_EACH_RUN__ = 3,
  __BDD_AFTER_EACH_RUN__ = 4,
  __BDD_BEFORE_RUN__ = 5,
  __BDD_AFTER_RUN__ = 6
};

typedef struct __bdd_config_type__
{
  enum __bdd_run_type__ run;
  unsigned int test_index;
  unsigned int test_tap_index;
  unsigned int failed_test_count;
  size_t test_list_size;
  char **test_list;
  char *error;
  unsigned int use_color;
  unsigned int use_tap;
} __bdd_config_type__;

const char *__bdd_describe_name__;
void __bdd_test_main__(__bdd_config_type__ *__bdd_config__);

void __bdd_run__(__bdd_config_type__ *config, char *name)
{
  __bdd_test_main__(config);

  if (config->error == NULL)
  {
    if (config->run == __BDD_TEST_RUN__)
    {
      if (config->use_tap)
      {
        // We only to report tests and not setup / teardown success
        if (config->test_tap_index)
        {
          printf("ok %i - %s\n", config->test_tap_index, name);
        }
      }
      else
      {
        printf("  %s %s(OK)%s\n", name, config->use_color ? __BDD_COLOR_GREEN__ : "",
               config->use_color ? __BDD_COLOR_RESET__ : "");
      }
    }
  }
  else
  {
    ++config->failed_test_count;
    if (config->use_tap)
    {
      // We only to report tests and not setup / teardown errors
      if (config->test_tap_index)
      {
        printf("not ok %i - %s\n", config->test_tap_index, name);
      }
    }
    else
    {
      printf("  %s %s(FAIL)%s\n", name, config->use_color ? __BDD_COLOR_RED__ : "",
             config->use_color ? __BDD_COLOR_RESET__ : "");
      printf("    %s\n", config->error);
    }
    free(config->error);
    config->error = NULL;
  }
}

char *__bdd_format__(const char *format, ...)
{
  va_list va;
  va_start(va, format);

  // First we over-allocate
  const size_t size = 2048;
  char *result = malloc(sizeof(char) * size);
  vsnprintf(result, size - 1, format, va);

  // Then clip to an actual size
  result = realloc(result, strlen(result) + 1);

  va_end(va);
  return result;
}

unsigned int __bdd_is_supported_term__()
{
  unsigned int result;
  const char *term = getenv("TERM");
  result = term && strcmp(term, "") != 0;
#ifndef _WIN32
  return result;
#else
  if (result)
  {
    return 1;
  }

  // Attempt to enable virtual terminal processing on Windows.
  // See: https://msdn.microsoft.com/en-us/library/windows/desktop/mt638032(v=vs.85).aspx
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE)
  {
    return 0;
  }

  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode))
  {
    return 0;
  }

  dwMode |= 0x4; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
  if (!SetConsoleMode(hOut, dwMode))
  {
    return 0;
  }

  return 1;
#endif
}

int main(void)
{
  struct __bdd_config_type__ config = {.run = __BDD_INIT_RUN__,
                                       .test_index = 0,
                                       .test_tap_index = 0,
                                       .failed_test_count = 0,
                                       .test_list_size = 8,
                                       .test_list = NULL,
                                       .error = NULL,
                                       .use_color = 0,
                                       .use_tap = 0};

  const char *tap_env = getenv("BDD_USE_TAP");
  if (BDD_USE_TAP || (tap_env && strcmp(tap_env, "") != 0 && strcmp(tap_env, "0") != 0))
  {
    config.use_tap = 1;
  }

  if (!config.use_tap && BDD_USE_COLOR && __BDD_IS_ATTY__() && __bdd_is_supported_term__())
  {
    config.use_color = 1;
  }

  // During the first run we just gather the
  // count of the tests and their descriptions
  config.test_list = malloc(sizeof(char *) * config.test_list_size);
  __bdd_test_main__(&config);

  const unsigned int test_count = config.test_index;

  // Outputting the name of the suite
  if (config.use_tap)
  {
    printf("TAP version 13\n1..%i\n", test_count);
  }
  else
  {
    printf("%s%s%s\n", config.use_color ? __BDD_COLOR_BOLD__ : "",
           __bdd_describe_name__, config.use_color ? __BDD_COLOR_RESET__ : "");
  }

  config.run = __BDD_BEFORE_RUN__;
  __bdd_run__(&config, "before");

  for (unsigned int i = 0; config.test_list[i]; ++i)
  {
    config.run = __BDD_BEFORE_EACH_RUN__;
    config.test_tap_index = 0;
    __bdd_run__(&config, "before each");

    config.run = __BDD_TEST_RUN__;
    config.test_index = i;
    config.test_tap_index = i + 1;
    __bdd_run__(&config, config.test_list[i]);

    config.run = __BDD_AFTER_EACH_RUN__;
    config.test_tap_index = 0;
    __bdd_run__(&config, "after each");
  }

  config.run = __BDD_AFTER_RUN__;
  __bdd_run__(&config, "after");

  if (config.failed_test_count > 0)
  {
    if (!config.use_tap)
    {
      printf("\n  %i test%s run, %i failed.\n", test_count,
             test_count == 1 ? "" : "s", config.failed_test_count);
    }
    return 1;
  }

  return 0;
}

#define describe(name)                                                         \
  const char *__bdd_describe_name__ = (name);                                  \
  void __bdd_test_main__(__bdd_config_type__ *__bdd_config__)


#define it(name)                                                                               \
  if (__bdd_config__->run == __BDD_INIT_RUN__)                                                 \
  {                                                                                            \
    while (__bdd_config__->test_index >= __bdd_config__->test_list_size)                       \
    {                                                                                          \
      __bdd_config__->test_list_size *= 2;                                                     \
      __bdd_config__->test_list =                                                              \
          realloc(__bdd_config__->test_list, sizeof(char *) * __bdd_config__->test_list_size); \
    }                                                                                          \
    __bdd_config__->test_list[__bdd_config__->test_index] = name;                              \
    __bdd_config__->test_list[++__bdd_config__->test_index] = 0;                               \
  }                                                                                            \
  else if (__bdd_config__->run == __BDD_TEST_RUN__ && __bdd_config__->test_index-- == 0)


#define before_each() if (__bdd_config__->run == __BDD_BEFORE_EACH_RUN__)
#define after_each() if (__bdd_config__->run == __BDD_AFTER_EACH_RUN__)
#define before() if (__bdd_config__->run == __BDD_BEFORE_RUN__)
#define after() if (__bdd_config__->run == __BDD_AFTER_RUN__)


#define __BDD_MACRO__(M, ...)                                                  \
  __BDD_OVERLOAD__(M, __BDD_COUNT_ARGS__(__VA_ARGS__))(__VA_ARGS__)
#define __BDD_OVERLOAD__(macro_name, suffix)                                   \
  __BDD_EXPAND_OVERLOAD__(macro_name, suffix)
#define __BDD_EXPAND_OVERLOAD__(macro_name, suffix) macro_name##suffix

#define __BDD_COUNT_ARGS__(...)                                                \
  __BDD_PATTERN_MATCH__(__VA_ARGS__, _, _, _, _, _, _, _, _, _, ONE__)
#define __BDD_PATTERN_MATCH__(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

void __bdd_sprintf__(char *buffer, const char *fmt, const char *message)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // _CRT_SECURE_NO_WARNINGS
#endif
  sprintf(buffer, fmt, message);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

#define __BDD_CHECK__(condition, ...)                                                   \
  if (!(condition))                                                                     \
  {                                                                                     \
    const char *message = __bdd_format__(__VA_ARGS__);                                  \
    const char *fmt =                                                                   \
        __bdd_config__->use_color ?                                                     \
            (__BDD_COLOR_RED__ "Check failed: %s" __BDD_COLOR_RESET__) :                \
            "Check failed: %s";                                                         \
    __bdd_config__->error = malloc(sizeof(char) * (strlen(fmt) + strlen(message) + 1)); \
    __bdd_sprintf__(__bdd_config__->error, fmt, message);                               \
    return;                                                                             \
  }

#define __BDD_CHECK_ONE__(condition) __BDD_CHECK__(condition, #condition)

#define check(...) __BDD_MACRO__(__BDD_CHECK_, __VA_ARGS__)

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif //BDD_FOR_C_H
