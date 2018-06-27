/**
 * @file
 * Monitor files for changes
 *
 * @authors
 * Copyright (C) 2018 Gero Treuer <gero@70t.de>
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

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "monitor.h"
#include "context.h"
#include "curs_lib.h"
#include "globals.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"

int MonitorFilesChanged;
int MonitorContextChanged;

struct Monitor
{
  struct Monitor *next;
  char *mh_backup_path;
  dev_t st_dev;
  ino_t st_ino;
  short magic;
  int desc;
};

static int INotifyFd = -1;
static struct Monitor *Monitor = NULL;
static size_t PollFdsCount = 0;
static size_t PollFdsLen = 0;
static struct pollfd *PollFds;

static int MonitorContextDescriptor = -1;

struct MonitorInfo
{
  short magic;
  short isdir;
  const char *path;
  dev_t st_dev;
  ino_t st_ino;
  struct Monitor *monitor;
  char path_buf[PATH_MAX]; /* access via path only (maybe not initialized) */
};

#define INOTIFY_MASK_DIR (IN_MOVED_TO | IN_ATTRIB | IN_CLOSE_WRITE | IN_ISDIR)
#define INOTIFY_MASK_FILE IN_CLOSE_WRITE

static void mutt_poll_fd_add(int fd, short events)
{
  int i = 0;
  for (; i < PollFdsCount && PollFds[i].fd != fd; ++i)
    ;

  if (i == PollFdsCount)
  {
    if (PollFdsCount == PollFdsLen)
    {
      PollFdsLen += 2;
      mutt_mem_realloc(&PollFds, PollFdsLen * sizeof(struct pollfd));
    }
    PollFdsCount++;
    PollFds[i].fd = fd;
    PollFds[i].events = events;
  }
  else
    PollFds[i].events |= events;
}

static int mutt_poll_fd_remove(int fd)
{
  int i = 0, d;
  for (i = 0; i < PollFdsCount && PollFds[i].fd != fd; ++i)
    ;
  if (i == PollFdsCount)
    return -1;
  d = PollFdsCount - i - 1;
  if (d)
    memmove(&PollFds[i], &PollFds[i + 1], d * sizeof(struct pollfd));
  PollFdsCount--;
  return 0;
}

static int monitor_init(void)
{
  if (INotifyFd == -1)
  {
    INotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (INotifyFd == -1)
    {
      mutt_debug(2, "inotify_init1 failed, errno=%d %s\n", errno, strerror(errno));
      return -1;
    }
    mutt_poll_fd_add(0, POLLIN);
    mutt_poll_fd_add(INotifyFd, POLLIN);
  }
  return 0;
}

static void monitor_check_free(void)
{
  if (!Monitor && INotifyFd != -1)
  {
    mutt_poll_fd_remove(INotifyFd);
    close(INotifyFd);
    INotifyFd = -1;
    MonitorFilesChanged = 0;
  }
}

static struct Monitor *monitor_create(struct MonitorInfo *info, int descriptor)
{
  struct Monitor *monitor = mutt_mem_calloc(1, sizeof(struct Monitor));
  monitor->magic = info->magic;
  monitor->st_dev = info->st_dev;
  monitor->st_ino = info->st_ino;
  monitor->desc = descriptor;
  monitor->next = Monitor;
  if (info->magic == MUTT_MH)
    monitor->mh_backup_path = mutt_str_strdup(info->path);

  Monitor = monitor;

  return monitor;
}

static void monitor_delete(struct Monitor *monitor)
{
  struct Monitor **ptr = &Monitor;

  if (!monitor)
    return;

  while (true)
  {
    if (!*ptr)
      return;
    if (*ptr == monitor)
      break;
    ptr = &(*ptr)->next;
  }

  FREE(&monitor->mh_backup_path); /* __FREE_CHECKED__ */
  monitor = monitor->next;
  FREE(ptr); /* __FREE_CHECKED__ */
  *ptr = monitor;
}

static int monitor_handle_ignore(int desc)
{
  int new_desc = -1;
  struct Monitor *iter = Monitor;
  struct stat sb;

  while (iter && iter->desc != desc)
    iter = iter->next;

  if (iter)
  {
    if (iter->magic == MUTT_MH && stat(iter->mh_backup_path, &sb) == 0)
    {
      if ((new_desc = inotify_add_watch(INotifyFd, iter->mh_backup_path,
                                        INOTIFY_MASK_FILE)) == -1)
      {
        mutt_debug(2, "inotify_add_watch failed for '%s', errno=%d %s\n",
                   iter->mh_backup_path, errno, strerror(errno));
      }
      else
      {
        mutt_debug(3, "inotify_add_watch descriptor=%d for '%s'\n", desc, iter->mh_backup_path);
        iter->st_dev = sb.st_dev;
        iter->st_ino = sb.st_ino;
        iter->desc = new_desc;
      }
    }
    else
    {
      mutt_debug(3, "cleanup watch (implicitely removed) - descriptor=%d\n", desc);
    }

    if (MonitorContextDescriptor == desc)
      MonitorContextDescriptor = new_desc;

    if (new_desc == -1)
    {
      monitor_delete(iter);
      monitor_check_free();
    }
  }

  return new_desc;
}

#define EVENT_BUFLEN MAX(4096, sizeof(struct inotify_event) + NAME_MAX + 1)

/* mutt_monitor_poll: Waits for I/O ready file descriptors or signals.
 *
 * return values:
 *      -3   unknown/unexpected events: poll timeout / fds not handled by us
 *      -2   monitor detected changes, no STDIN input
 *      -1   error (see errno)
 *       0   (1) input ready from STDIN, or (2) monitoring inactive -> no poll()
 * MonitorFilesChanged also reflects changes to monitored files.
 *
 * Only STDIN and INotify file handles currently expected/supported.
 * More would ask for common infrastructur (sockets?).
 */
int mutt_monitor_poll(void)
{
  int rc = 0, fds, i, inputReady;
  char buf[EVENT_BUFLEN] __attribute__((aligned(__alignof__(struct inotify_event))));

  MonitorFilesChanged = 0;

  if (INotifyFd != -1)
  {
    fds = poll(PollFds, PollFdsLen, MuttGetchTimeout);

    if (fds == -1)
    {
      rc = -1;
      if (errno != EINTR)
      {
        mutt_debug(2, "poll() failed, errno=%d %s\n", errno, strerror(errno));
      }
    }
    else
    {
      inputReady = 0;
      for (i = 0; fds && i < PollFdsCount; ++i)
      {
        if (PollFds[i].revents)
        {
          fds--;
          if (PollFds[i].fd == 0)
          {
            inputReady = 1;
          }
          else if (PollFds[i].fd == INotifyFd)
          {
            MonitorFilesChanged = 1;
            mutt_debug(3, "file change(s) detected\n");
            int len;
            char *ptr = buf;
            const struct inotify_event *event;

            while (true)
            {
              len = read(INotifyFd, buf, sizeof(buf));
              if (len == -1)
              {
                if (errno != EAGAIN)
                  mutt_debug(2, "read inotify events failed, errno=%d %s\n",
                             errno, strerror(errno));
                break;
              }

              while (ptr < buf + len)
              {
                event = (const struct inotify_event *) ptr;
                mutt_debug(5, "+ detail: descriptor=%d mask=0x%x\n", event->wd,
                           event->mask);
                if (event->mask & IN_IGNORED)
                  monitor_handle_ignore(event->wd);
                else if (event->wd == MonitorContextDescriptor)
                  MonitorContextChanged = 1;
                ptr += sizeof(struct inotify_event) + event->len;
              }
            }
          }
        }
      }
      if (!inputReady)
        rc = MonitorFilesChanged ? -2 : -3;
    }
  }

  return rc;
}

#define RESOLVERES_OK_NOTEXISTING 0
#define RESOLVERES_OK_EXISTING 1
#define RESOLVERES_FAIL_NOMAILBOX -3
#define RESOLVERES_FAIL_NOMAGIC -2
#define RESOLVERES_FAIL_STAT -1

/* monitor_resolve: resolve monitor entry match by Mailbox, or - if NULL - by Context.
 *
 * return values:
 *      >=0   mailbox is valid and locally accessible:
 *              0: no monitor / 1: preexisting monitor
 *       -3   no mailbox (MONITORINFO: no fields set)
 *       -2   magic not set
 *       -1   stat() failed (see errno; MONITORINFO fields: magic, isdir, path)
 */
static int monitor_resolve(struct MonitorInfo *info, struct Mailbox *mailbox)
{
  struct Monitor *iter;
  char *fmt = NULL;
  struct stat sb;

  if (mailbox)
  {
    info->magic = mailbox->magic;
    info->path = mailbox->realpath;
  }
  else if (Context)
  {
    info->magic = Context->magic;
    info->path = Context->realpath;
  }
  else
  {
    return RESOLVERES_FAIL_NOMAILBOX;
  }

  if (!info->magic)
  {
    return RESOLVERES_FAIL_NOMAGIC;
  }
  else if (info->magic == MUTT_MAILDIR)
  {
    info->isdir = 1;
    fmt = "%s/new";
  }
  else
  {
    info->isdir = 0;
    if (info->magic == MUTT_MH)
      fmt = "%s/.mh_sequences";
  }

  if (fmt)
  {
    snprintf(info->path_buf, sizeof(info->path_buf), fmt, info->path);
    info->path = info->path_buf;
  }
  if (stat(info->path, &sb) != 0)
    return RESOLVERES_FAIL_STAT;

  iter = Monitor;
  while (iter && (iter->st_ino != sb.st_ino || iter->st_dev != sb.st_dev))
    iter = iter->next;

  info->st_dev = sb.st_dev;
  info->st_ino = sb.st_ino;
  info->monitor = iter;

  return iter ? RESOLVERES_OK_EXISTING : RESOLVERES_OK_NOTEXISTING;
}

/* mutt_monitor_add: add file monitor from Mailbox, or - if NULL - from Context.
 *
 * return values:
 *       0   success: new or already existing monitor
 *      -1   failed:  no mailbox, inaccessible file, create monitor/watcher failed
 */
int mutt_monitor_add(struct Mailbox *mailbox)
{
  struct MonitorInfo info;
  uint32_t mask;
  int desc;

  desc = monitor_resolve(&info, mailbox);
  if (desc != RESOLVERES_OK_NOTEXISTING)
  {
    if (!mailbox && (desc == RESOLVERES_OK_EXISTING))
      MonitorContextDescriptor = info.monitor->desc;
    return desc == RESOLVERES_OK_EXISTING ? 0 : -1;
  }

  mask = info.isdir ? INOTIFY_MASK_DIR : INOTIFY_MASK_FILE;
  if ((INotifyFd == -1 && monitor_init() == -1) ||
      (desc = inotify_add_watch(INotifyFd, info.path, mask)) == -1)
  {
    mutt_debug(2, "inotify_add_watch failed for '%s', errno=%d %s\n", info.path,
               errno, strerror(errno));
    return -1;
  }

  mutt_debug(3, "inotify_add_watch descriptor=%d for '%s'\n", desc, info.path);
  if (!mailbox)
    MonitorContextDescriptor = desc;

  monitor_create(&info, desc);
  return 0;
}

/* mutt_monitor_remove: remove file monitor from Mailbox, or - if NULL - from Context.
 *
 * return values:
 *       0   monitor removed (not shared)
 *       1   monitor not removed (shared)
 *       2   no monitor
 */
int mutt_monitor_remove(struct Mailbox *mailbox)
{
  struct MonitorInfo info, info2;

  if (!mailbox)
  {
    MonitorContextDescriptor = -1;
    MonitorContextChanged = 0;
  }

  if (monitor_resolve(&info, mailbox) != RESOLVERES_OK_EXISTING)
    return 2;

  if (Context)
  {
    if (mailbox)
    {
      if (monitor_resolve(&info2, NULL) == RESOLVERES_OK_EXISTING &&
          info.st_ino == info2.st_ino && info.st_dev == info2.st_dev)
        return 1;
    }
    else
    {
      if (mutt_find_mailbox(Context->realpath))
        return 1;
    }
  }

  inotify_rm_watch(info.monitor->desc, INotifyFd);
  mutt_debug(3, "inotify_rm_watch for '%s' descriptor=%d\n", info.path,
             info.monitor->desc);

  monitor_delete(info.monitor);
  monitor_check_free();
  return 0;
}
