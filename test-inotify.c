// gcc -I. -o test-inotify test-inotify.c libmonitor.a libmutt.a -lpcre2-8

#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "monitor/lib.h" // IWYU pragma: keep

struct Monitor;

ARRAY_HEAD(IntArray, int);

struct NeoMutt *NeoMutt = NULL;

void callback1(struct Monitor *mon, int wd, MonitorEvent me, void *wdata)
{
  long num = (long) wdata;
  printf("\033[1;31mCallback1: %p, wd %d, me 0x%04x, %ld\033[0m\n",
         (void *) mon, wd, me, num);
}

void callback2(struct Monitor *mon, int wd, MonitorEvent me, void *wdata)
{
  long num = (long) wdata;
  printf("\033[1;32mCallback2: %p, wd %d, me 0x%04x, %ld\033[0m\n",
         (void *) mon, wd, me, num);
}

void callback3(struct Monitor *mon, int wd, MonitorEvent me, void *wdata)
{
  long num = (long) wdata;
  printf("\033[1;34mCallback3: %p, wd %d, me 0x%04x, %ld\033[0m\n",
         (void *) mon, wd, me, num);
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    return 1;

  static monitor_t *cb[] = { callback1, callback2, callback3 };

  struct Monitor *mon = monitor_init();
  if (!mon)
    return 1;

  struct IntArray ia = ARRAY_HEAD_INITIALIZER;

  for (int i = 1; i < argc; i++)
  {
    struct stat st = { 0 };
    if (stat(argv[i], &st) != 0)
    {
      printf("stat failed for '%s', errno=%d %s\n", argv[i], errno, strerror(errno));
      continue;
    }

    int wd;
    if (S_ISREG(st.st_mode))
    {
      printf("Add file: %s\n", argv[i]);
      wd = monitor_watch_file(mon, argv[i], cb[(i - 1) % mutt_array_size(cb)], (void *) 42);
    }
    else if (S_ISDIR(st.st_mode))
    {
      printf("Add dir: %s\n", argv[i]);
      wd = monitor_watch_dir(mon, argv[i], cb[(i - 1) % mutt_array_size(cb)], (void *) 42);
    }
    else
    {
      printf("Unknown type: %s\n", argv[i]);
      continue;
    }

    if (wd == -1)
      continue;

    ARRAY_ADD(&ia, wd);
    printf("\twatch: wd %d\n", wd);
  }

  if (ARRAY_SIZE(&ia) > 0)
  {
    monitor_poll(mon);

    int *ip = NULL;
    ARRAY_FOREACH(ip, &ia)
    {
      printf("removing: wd %d\n", *ip);
      monitor_remove_watch(mon, *ip);
    }
  }

  monitor_free(&mon);
  ARRAY_FREE(&ia);
  return 0;
}
