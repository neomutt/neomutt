/**
 * @file
 * Use inotify to monitor files/dirs for change
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page monitor_inotify Use inotify to monitor files/dirs for change
 *
 * Use inotify to monitor files/dirs for change
 */

#include "config.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "inotify.h"
#include "lib.h"

#define INOTIFY_MASK_DIR (IN_MOVED_TO | IN_ATTRIB | IN_CLOSE_WRITE | IN_ISDIR)
#define INOTIFY_MASK_FILE IN_CLOSE_WRITE

#define EVENT_BUFLEN MAX(4096, sizeof(struct inotify_event) + NAME_MAX + 1)

#define CHECK_FLAG(F)                                                          \
  if (flags & F)                                                               \
  {                                                                            \
    chars = snprintf(desc + off, sizeof(desc) - off, #F " ");                  \
    flags &= ~F;                                                               \
    off += chars;                                                              \
  }

/**
 * monitor_new - Create a new Filesystem Monitor
 * @retval ptr New Monitor
 */
static struct Monitor *monitor_new(void)
{
  struct Monitor *mon = mutt_mem_calloc(1, sizeof(struct Monitor));
  mon->fd_inotify = -1;
  return mon;
}

/**
 * monitor_free - Free a Filesystem Monitor
 * @param ptr Monitor to free
 */
void monitor_free(struct Monitor **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Monitor *mon = *ptr;

  if (mon->fd_inotify != -1)
    close(mon->fd_inotify);

  ARRAY_FREE(&mon->watches);

  FREE(ptr);
}

/**
 * monitor_init - Set up file monitoring
 * @retval ptr Filesystem Monitor
 */
struct Monitor *monitor_init(void)
{
  if (NeoMutt && NeoMutt->mon)
    return NeoMutt->mon;

  int fd = -1;

#ifdef HAVE_INOTIFY_INIT1
  fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (fd == -1)
  {
    mutt_debug(LL_DEBUG2, "inotify_init1 failed, errno=%d %s\n", errno, strerror(errno));
    return NULL;
  }
#else
  fd = inotify_init();
  if (fd == -1)
  {
    mutt_debug(LL_DEBUG2, "inotify_init failed, errno=%d %s\n", errno, strerror(errno));
    return NULL;
  }
  fcntl(fd, F_SETFL, O_NONBLOCK);
  fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

  struct Monitor *mon = monitor_new();

  // Monitor stdin and the Inotify fd
  mon->polls[0] = (struct pollfd){ STDIN_FILENO, POLLIN, 0 };
  mon->polls[1] = (struct pollfd){ fd, POLLIN, 0 };

  mon->fd_inotify = fd;

  if (NeoMutt)
    NeoMutt->mon = mon;

  mutt_debug(LL_DEBUG2, "monitor on fd %d\n", fd);
  return mon;
}

/**
 * watch_new - Create a new Watch
 * @retval ptr New Watch
 */
struct Watch *watch_new(void)
{
  struct Watch *watch = mutt_mem_calloc(1, sizeof(struct Watch));
  watch->wd = -1;
  return watch;
}

/**
 * watch_free - Free a Watch
 * @param ptr Watch to free
 */
void watch_free(struct Watch **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * watch_find - Locate a Watch by its Watch Descriptor
 * @param mon Filesystem Monitor
 * @param wd  Watch Descriptor
 * @retval ptr Matching Watch
 */
struct Watch *watch_find(struct Monitor *mon, int wd)
{
  if (!mon || (wd == -1))
    return NULL;

  struct Watch **wp = NULL;
  ARRAY_FOREACH(wp, &mon->watches)
  {
    struct Watch *watch = *wp;
    if (watch->wd == wd)
      return watch;
  }

  return NULL;
}

/**
 * monitor_watch_dir - Add a Watch for a directory
 * @param mon    Filesystem Monitor
 * @param dir    Directory to Watch
 * @param cb     Callback function
 * @param wdata  Data for callback function
 * @retval num Watch Descriptor
 */
int monitor_watch_dir(struct Monitor *mon, const char *dir, monitor_t *cb, void *wdata)
{
  if (!mon || !dir || !cb)
    return -1;

  struct stat st = { 0 };
  if (stat(dir, &st) != 0)
  {
    mutt_debug(LL_DEBUG2, "stat failed for '%s', errno=%d %s\n", dir, errno,
               strerror(errno));
    return -1;
  }

  if (!S_ISDIR(st.st_mode))
  {
    mutt_debug(LL_DEBUG2, "not a dir: '%s'\n", dir);
    return -1;
  }

  uint32_t mask = INOTIFY_MASK_DIR;
  mask = IN_ALL_EVENTS;
  int wd = inotify_add_watch(mon->fd_inotify, dir, mask);
  if (wd == -1)
  {
    mutt_debug(LL_DEBUG2, "inotify_add_watch failed for '%s', errno=%d %s\n",
               dir, errno, strerror(errno));
    return -1;
  }

  struct Watch *watch = watch_new();
  watch->st_dev = st.st_dev;
  watch->st_ino = st.st_ino;
  watch->cb = cb;
  watch->wdata = wdata;
  watch->wd = wd;

  ARRAY_ADD(&mon->watches, watch);

  mutt_debug(LL_DEBUG2, "watching: wd %d, mask 0x%x, (%lu,%lu) %s\n", wd, mask,
             st.st_dev, st.st_ino, dir);

  return wd;
}

/**
 * monitor_watch_file - Add a Watch for a file
 * @param mon    Filesystem Monitor
 * @param file   File to Watch
 * @param cb     Callback function
 * @param wdata  Data for callback function
 * @retval num Watch Descriptor
 */
int monitor_watch_file(struct Monitor *mon, const char *file, monitor_t *cb, void *wdata)
{
  if (!mon || !file || !cb)
    return -1;

  struct stat st = { 0 };
  if (stat(file, &st) != 0)
  {
    mutt_debug(LL_DEBUG2, "stat failed for '%s', errno=%d %s\n", file, errno,
               strerror(errno));
    return -1;
  }

  if (!S_ISREG(st.st_mode))
  {
    mutt_debug(LL_DEBUG2, "not a file: '%s'\n", file);
    return -1;
  }

  uint32_t mask = INOTIFY_MASK_FILE;
  mask = IN_ALL_EVENTS;
  int wd = inotify_add_watch(mon->fd_inotify, file, mask);
  if (wd == -1)
  {
    mutt_debug(LL_DEBUG2, "inotify_add_watch failed for '%s', errno=%d %s\n",
               file, errno, strerror(errno));
    return -1;
  }

  struct Watch *watch = watch_new();
  watch->st_dev = st.st_dev;
  watch->st_ino = st.st_ino;
  watch->cb = cb;
  watch->wdata = wdata;
  watch->wd = wd;

  ARRAY_ADD(&mon->watches, watch);

  mutt_debug(LL_DEBUG2, "watching: wd %d, mask 0x%x, (%lu,%lu) %s\n", wd, mask,
             st.st_dev, st.st_ino, file);

  return wd;
}

/**
 * monitor_remove_watch - Remove a Watch
 * @param mon Filesystem Monitor
 * @param wd  Watch Descriptor
 */
void monitor_remove_watch(struct Monitor *mon, int wd)
{
  if (!mon || (wd == -1))
    return;

  struct Watch **wp = NULL;
  ARRAY_FOREACH(wp, &mon->watches)
  {
    struct Watch *watch = *wp;
    if (watch->wd != wd)
      continue;

    mutt_debug(LL_DEBUG2, "removing watch: wd %d, (%lu,%lu)\n", watch->wd,
               watch->st_dev, watch->st_ino);
    inotify_rm_watch(watch->wd, mon->fd_inotify);
    ARRAY_REMOVE(&mon->watches, wp);
    watch_free(&watch);
    break;
  }
}

/**
 * inotify_name - XXX
 */
const char *inotify_name(int flags)
{
  static char desc[128] = { 0 };

  size_t off = 0;
  int chars = 0;

  CHECK_FLAG(IN_ACCESS)
  CHECK_FLAG(IN_MODIFY)
  CHECK_FLAG(IN_ATTRIB)
  CHECK_FLAG(IN_CLOSE_WRITE)
  CHECK_FLAG(IN_CLOSE_NOWRITE)
  CHECK_FLAG(IN_OPEN)
  CHECK_FLAG(IN_MOVED_FROM)
  CHECK_FLAG(IN_MOVED_TO)
  CHECK_FLAG(IN_CREATE)
  CHECK_FLAG(IN_DELETE)
  CHECK_FLAG(IN_DELETE_SELF)
  CHECK_FLAG(IN_MOVE_SELF)
  CHECK_FLAG(IN_UNMOUNT)
  CHECK_FLAG(IN_Q_OVERFLOW)
  CHECK_FLAG(IN_IGNORED)
  CHECK_FLAG(IN_ONLYDIR)
  CHECK_FLAG(IN_DONT_FOLLOW)
  CHECK_FLAG(IN_EXCL_UNLINK)
  CHECK_FLAG(IN_MASK_CREATE)
  CHECK_FLAG(IN_MASK_ADD)
  CHECK_FLAG(IN_ISDIR)
  CHECK_FLAG(IN_ONESHOT)

  return desc;
}

/**
 * monitor_poll - Check the Monitor for changes
 * @param mon Filesystem Monitor
 * @retval num Number of Events
 * @retval  -1 Error
 */
int monitor_poll(struct Monitor *mon)
{
  if (!mon)
    return -1;

  char buf[EVENT_BUFLEN] __attribute__((aligned(__alignof__(struct inotify_event))));

  int fds = poll(mon->polls, mutt_array_size(mon->polls), 5000);
  if (fds == -1)
  {
    if (errno != EINTR)
    {
      mutt_debug(LL_DEBUG2, "poll() failed, errno=%d %s\n", errno, strerror(errno));
    }
    return -1;
  }

  assert(fds <= 2);

  for (int i = 0; fds > 0; i++)
  {
    if (mon->polls[i].revents == 0)
      continue;

    fds--;

    if (mon->polls[i].fd == STDIN_FILENO) // stdin ready
      continue;

    if (mon->polls[i].fd != mon->fd_inotify)
      continue;

    printf("file change(s) detected\n");
    char *ptr = buf;
    const struct inotify_event *event = NULL;

    while (true)
    {
      int bytes = 0;
      if (ioctl(mon->fd_inotify, FIONREAD, &bytes) == 0)
      {
        printf("ioctl expects: %d bytes\n", bytes);
      }
      // int len = read(mon->fd_inotify, buf, sizeof(buf));
      int len = read(mon->fd_inotify, buf, bytes);
      printf("read returns: %d bytes\n", len);
      if (len == -1)
      {
        if (errno != EAGAIN)
        {
          mutt_debug(LL_DEBUG2, "read inotify events failed, errno=%d %s\n",
                     errno, strerror(errno));
        }
        break;
      }

      while (ptr < (buf + len))
      {
        event = (const struct inotify_event *) ptr;
        printf("+ detail: wd=%d mask=0x%x\n", event->wd, event->mask);

        struct Watch *watch = watch_find(mon, event->wd);
        if (!watch)
          continue;

        printf("EVENT on %p (%lu,%lu)\n", (void *) watch, watch->st_dev, watch->st_ino);
        printf("\t\033[1;33m%s\033[0m\n", inotify_name(event->mask));
        if (event->len > 0)
          printf("\t%s\n", event->name);
        if (event->cookie != 0)
          printf("\t\033[1;35mcookie: %d\033[0m\n", event->cookie);
        watch->cb(mon, watch->wd, event->mask, watch->wdata);

        // if (event->mask & IN_IGNORED)
        //   monitor_handle_ignore(event->wd);
        // else if (event->wd == MonitorContextDescriptor)
        //   MonitorContextChanged = true;
        ptr += sizeof(struct inotify_event) + event->len;
      }
    }
  }

  return 0;
}
