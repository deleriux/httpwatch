#include "event.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <sysexits.h>
#include <sys/epoll.h>
#include <sys/queue.h>

/* Event */
struct callback {
  void *data;
  int fd;
  int (*callback)(int fd, int event, void *data);
  void (*destroy)(int fd, void *data);
  bool inset;
  LIST_ENTRY(callback) list;
};

/* Global event handle */
struct event_handle {
  int epollfd;
  int curfds;
  int maxfds;
  LIST_HEAD(evlist_head, callback) head;
};


/* Static function prototypes */
static struct callback * event_search(int fd);

static struct event_handle eh = { -1, 0, EVENT_MAXFDS };

/* Returns an event handle from searching by fd */ 
static struct callback * event_search(
    int fd)
{
  struct callback *ev;
  for (ev = eh.head.lh_first; ev != NULL; ev = ev->list.le_next) {
    if (ev->fd == fd)
      return ev;
  }

  return NULL;
}


/* Initialize the event handle */
void event_init(
    void)
{
  int fd = epoll_create1(EPOLL_CLOEXEC);
  if (fd < 0)
    err(EX_OSERR, "Cannot initialize event handler");

  eh.epollfd = fd; 
  LIST_INIT(&eh.head); 
}


/* Perform the event loop */
int event_loop(
    int max,
    int timeout)
{
  int cnt = 0;
  int rc, i;
  struct callback *cb;
  assert(max < EVENT_MAXFDS && max > 0);
  assert(timeout >= -1);

  struct epoll_event *events = calloc(max, sizeof(struct epoll_event));
  if (!events) {
    warn("Cannot allocate memory for events");
    goto fail;
  }

restart:
  /* Do the epoll, safely handle interrupts */
  rc = epoll_wait(eh.epollfd, events, max, timeout);
  if (rc < 0) {
    if (errno == EINTR)
      goto restart;
    else {
      goto fail;
    }
  }

  for (i=0; i < rc; i++) {
    cb = (struct callback *)events[i].data.ptr;
    assert(cb->callback);
    if (cb->callback(cb->fd, events[i].events, cb->data) < 0) {
      event_del_fd(cb->fd);
    }
    else {
      cnt++;
    }
  }

  free(events);
  return cnt;
  
fail:
  if (events)
    free(events);
  return -1;
}


/* Delete an FD from the events, is idempotent */
void event_del_fd(
    int fd)
{
  struct callback *ev = event_search(fd); 
  if (!ev)
    return;

  /* Remove from the list */
  LIST_REMOVE(ev, list);

  /* Remove from the epoll */
  epoll_ctl(eh.epollfd, EPOLL_CTL_DEL, ev->fd, NULL);
  /* WARNING WARNING, cb->data MAY BE ALLOCATED */

  /* Call the objects destructor */
  if (ev->destroy)
    ev->destroy(fd, ev->data);

  free(ev);
}

/* Modify the event mask of an existing FD */
int event_mod_event(
    int fd,
    int event)
{
    assert(fd > -1);

    struct epoll_event ev;
    struct callback *cb;

    if ((cb = event_search(fd)) == NULL) {
      return 0;
    }

    ev.events = event;
    ev.data.ptr = cb;
    if (epoll_ctl(eh.epollfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
      warn("Unable to modify event mask");
      return -1;
    }

    return 0;
}


/* Modify callback of existing FD */
void event_mod_cb(
    int fd,
    int (*callback)(int fd, int event, void *data))
{
    assert(fd > -1);

    struct epoll_event ev;
    struct callback *cb;

    if ((cb = event_search(fd)) == NULL) 
      return;

    cb->callback = callback;
    return;
}


/* Add a FD onto the events */
int event_add_fd(
    int fd,
    int (*callback)(int fd, int event, void *data),
    void (*destructor)(int fd, void *data),
    void *data,
    int event)
{
  assert(fd >= 0);
  assert(callback);
  struct epoll_event ep_ev;
  struct callback *ev = malloc(sizeof(*ev));
  struct callback *tmp;
  if (!ev) {
    warn("Cannot allocate memory for callback");
    goto fail;
  }
  memset(&ep_ev, 0, sizeof(ep_ev));

  /* Initialize callback and epoll event */
  ev->fd = fd;
  ev->data = data;
  ev->callback = callback;
  ev->destroy = destructor;

  ep_ev.events = event;
  ep_ev.data.ptr = ev;

  /* Insert the FD onto our list */
  LIST_INSERT_HEAD(&eh.head, ev, list);

  /* Register the fd with the epoll handler */
  if (epoll_ctl(eh.epollfd, EPOLL_CTL_ADD, fd, &ep_ev) < 0) {
    warn("Cannot add event to epoll");
    goto fail;
  }

  if (eh.curfds + 1 > eh.maxfds) {
    warnx("Adding FD maximum number of descriptors to monitor");
    goto fail;
  }

  eh.curfds++;
  return 0;

fail:
  tmp = event_search(fd);
  if (tmp) {
    LIST_REMOVE(tmp, list);
  }

  if (ev)
    free(ev);
  return -1;
}
