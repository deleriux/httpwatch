#include "event.h"
#include "curl_event.h"
#include "common.h"
#include "urlchecker.h"
#include <curl/curl.h>
#include <sys/timerfd.h>

static struct global {
  CURLM *multi;
  int still_running;
  int tfd;
} g;

static void multi_check_done(void);


static int event_cb(
    int fd,
    int event,
    void *data)
{
  struct itimerspec its;
  CURLMcode rc;
  int action = ((event & EPOLLIN) ? CURL_CSELECT_IN : 0) |
               ((event & EPOLLOUT) ? CURL_CSELECT_OUT : 0);

  multi_check_done();
  rc = curl_multi_socket_action(g.multi, fd, action, &g.still_running);
  if (rc != CURLM_OK)
    warnx("Error performing a socket action: %s", curl_multi_strerror(rc));

  if(g.still_running <= 0) {
    memset(&its, 0, sizeof(struct itimerspec));
    timerfd_settime(g.tfd, 0, &its, NULL);
  }

  return 0;
}


static void multi_check_done(
    void)
{
  struct itimerspec its;
  CURLMsg *msg = NULL;
  int left;

  while ((msg = curl_multi_info_read(g.multi, &left))) {
    assert(msg->msg == CURLMSG_DONE);
    urlchecker_curl_done(msg->easy_handle, msg->data.result);
    curl_multi_remove_handle(g.multi, msg->easy_handle);
  }
}


static int fd_event_modify_cb(
    CURL *e,
    curl_socket_t fd,
    int what,
    void *globaldata,
    void *sockdata)
{
  int evwhat = ((what & CURL_POLL_IN) ? EPOLLIN : 0) |
               ((what & CURL_POLL_OUT) ? EPOLLOUT : 0);
  /* Remove handle from event manager */
  if (what == CURL_POLL_REMOVE) {
    event_del_fd(fd);
    //printf("fd_event_modify_cb: %d, what: %s\n", fd, "REMOVE");
  }
  /* This is an existing handle */
  else {
    if (sockdata) {
      if (event_mod_event(fd, evwhat) < 0)
        warn("FD %d was not modified by the event manager", fd);
      //printf("fd_event_modify_cb: %d, what: %s (%d)\n", fd, "MODIFY", evwhat);
    }
    /* This must be a new connection */
    else {
      if (event_add_fd(fd, event_cb, NULL, NULL, evwhat) < 0) 
        warn("FD %d was not added to the event manager", fd);
      curl_multi_assign(g.multi, fd, e);
      //printf("fd_event_modify_cb: %d, what: %s (%d)\n", fd, "ADD", evwhat);
    }
  }

  multi_check_done();

  return 0;
}


int multi_tfd_expired(
    int fd,
    int event,
    void *data)
{
  uint64_t count = 0;
  CURLMcode rc;
  int err;

  err = read(g.tfd, &count, sizeof(count));
  if (err < 0) {
    if (errno == EAGAIN) {
      printf("tfd_expired EAGAIN");
      return 0;
    }
  }

  assert(err == sizeof(uint64_t));
  rc = curl_multi_socket_action(g.multi, CURL_SOCKET_TIMEOUT, 0, &g.still_running);
  if (rc != CURLE_OK)
    warn("Multi timout socket action failed: %s", curl_multi_strerror(rc));
  multi_check_done();
  return 0;
}


int multi_adjust_tfd(
    CURLM *multi, 
    long timeout_ms,
    void *data)
{
  struct itimerspec its;
  memset(&its, 0, sizeof(its));

  if (timeout_ms < 0) {
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
  }
  else if (timeout_ms > 0) {
    its.it_value.tv_sec = timeout_ms / 1000;
    its.it_value.tv_nsec = (timeout_ms % 1000) * 1000000;
  }
  else
    its.it_value.tv_nsec = 1;

  if (timerfd_settime(g.tfd, 0, &its, NULL) < 0)
    err(EXIT_FAILURE, "timerfd_settime - multi_adjust_tfd, %d\n", timeout_ms);
  multi_check_done();
  return 0;
}


void curl_event_init(
    void)
{
  struct itimerspec its;
  curl_global_init(CURL_GLOBAL_ALL);
  g.multi = curl_multi_init();
  g.tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
  if (g.tfd < 0)
    err(EXIT_FAILURE, "Timerfd_create");

  memset(&its, 0, sizeof(its));
  timerfd_settime(g.tfd, 0, &its, NULL);
  event_add_fd(g.tfd, multi_tfd_expired, NULL, NULL, EPOLLIN); 

  curl_multi_setopt(g.multi, CURLMOPT_SOCKETFUNCTION, fd_event_modify_cb);
  curl_multi_setopt(g.multi, CURLMOPT_SOCKETDATA, NULL);
  curl_multi_setopt(g.multi, CURLMOPT_TIMERFUNCTION, multi_adjust_tfd);
  curl_multi_setopt(g.multi, CURLMOPT_TIMERDATA, NULL);

  return;
}


CURLM * curl_event_multi_get(
    void)
{
  return g.multi;
}
