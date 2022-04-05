#include "urlchecker.h"
#include "event.h"
#include "curl_event.h"
#include <sys/timerfd.h>

#define CS(h,v) curl_easy_setopt(ug->tplt, h, v)

static void urlgroup_report(urlgroup_t *ug);
static urlchecker_t * urlchecker_init(urlgroup_t *ug, int i);
static double timestamp(void);

/*
struct urlgroup {
  int instances;
  int done;
  checker_t *ch;
  urlchecker_t **checks;
  int interval_tfd;
};

*/
/*
typedef struct checker {
  char *url;
  int32_t timeout_ms;
  int32_t interval_ms;
  int32_t concurrency;
  int32_t http_response_code;
  bool tls_ignore;
  char *regex;
} checker_t;
*/

static double timestamp(
    void)
{
  double d;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  d = ts.tv_sec + ((double)ts.tv_nsec / 1000000000);
  return d;
}

static void urlchecker_destroy(
    urlchecker_t *uch)
{
  CURLM *m;
  if (!uch)
    return;
  free(uch->out.data);
  if (!uch->done) {
    m = curl_event_multi_get();
    curl_multi_remove_handle(m, uch->curl);
  }
  uch->done = 1;
  curl_easy_cleanup(uch->curl);
}


static void urlchecker_finish(
    urlchecker_t *uch)
{
  int code;
  uch->done = 1;
  uch->time_end = timestamp();
  curl_easy_getinfo(uch->curl, CURLINFO_RESPONSE_CODE, &code);
  uch->group->done++;
  const char *p = curl_easy_strerror(uch->curlcode);
  if (uch->curlcode == 0 && code != uch->group->ch->http_response_code) {
    strcat(uch->errbuf, "HTTP Response mismatch");
    uch->curlcode = 8;
  }
  else
    strcpy(uch->errbuf, p);
  if (uch->group->done == uch->group->instances) 
    urlgroup_report(uch->group);
  /* Instances and group are freed here. dont call */
  return;
}

static int interval_timeout(
    int fd,
    int event,
    void *data)
{
  CURLM *m;
  int i;
  uint64_t count;
  urlgroup_t *ug = (urlgroup_t *)data;

  if (read(fd, &count, sizeof(uint64_t)) != sizeof(uint64_t))
    return -1;  

  for (i=0; i < ug->instances; i++) {
    if (ug->checks[i]) {
      urlchecker_finish(ug->checks[i]);
      urlchecker_destroy(ug->checks[i]);
      ug->checks[i] = NULL;
    }
    ug->checks[i] = urlchecker_init(ug, i);
    m = curl_event_multi_get();
    curl_multi_add_handle(m, ug->checks[i]->curl);
    ug->checks[i]->time_start = timestamp();
  }
  return 0;
}


static size_t write_data(
    char *data,
    size_t sz,
    size_t nmemb,
    void *user)
{
  char *p;
  urlchecker_t *uch = (urlchecker_t *)user;
  uch->out.len += (sz*nmemb);
  uch->out.data = realloc(uch->out.data, uch->out.len+1);
  assert(uch->out.data);
  p = &uch->out.data[uch->out.off];
  memset(p, 0, (sz*nmemb)+1);
  memcpy(p, data, sz*nmemb);
  uch->out.off += (sz*nmemb);

  return (sz*nmemb);
}
 


static urlchecker_t * urlchecker_init(
    urlgroup_t *ug,
    int i)
{
  urlchecker_t *uch = calloc(1, sizeof(urlchecker_t));
  if (!uch)
    return NULL;

  uch->id = i;
  memset(uch->errbuf, 0, sizeof(uch->errbuf));
  uch->curlcode = 0;
  uch->rc = 0;
  uch->group = ug;
  uch->time_start = -1;
  uch->time_end = -1;
  uch->done = 0;

  uch->out.data = NULL;
  uch->out.len = 0;
  uch->out.off = 0;

  uch->curl = curl_easy_duphandle(ug->tplt);
  curl_easy_setopt(uch->curl, CURLOPT_WRITEDATA, uch);
  curl_easy_setopt(uch->curl, CURLOPT_PRIVATE, uch);
  curl_easy_setopt(uch->curl, CURLOPT_ERRORBUFFER, uch->errbuf);

  return uch;
}


static void nowstr(
    char *p, 
    size_t s)
{
  struct tm now;
  struct timespec ts;
  off_t off;

  memset(p, 0, s);
  clock_gettime(CLOCK_REALTIME, &ts);
  localtime_r(&ts.tv_sec, &now);
  off = strftime(p, s-1, "%Y-%m-%d:%H:%M:%S.", &now);
  snprintf(&p[off], s-off, "%06d", ts.tv_nsec/1000);
  return;
}

static void urlgroup_report(
    urlgroup_t *ug)
{
  int i;
  int code;
  char now[128];
  urlchecker_t *uch;
  bool ok = true;
  
  /* Fetch the time now */
  nowstr(now, 128);
  printf("%s: %10s ", now, ug->ch->section_name);
  for (i=0; i < ug->instances; i++) {
    uch = ug->checks[i];
    curl_easy_getinfo(uch->curl, CURLINFO_RESPONSE_CODE, &code);
    printf("[%s/%.3f/%d] ", uch->errbuf, uch->time_end-uch->time_start, code);
  }
  printf("\n");
  fflush(stdout);
 
  for (i=0; i < ug->instances; i++) {
    if (ug->checks[i]->curlcode != 0)
      ok = false;
    urlchecker_destroy(ug->checks[i]);
    ug->checks[i] = NULL;
  }
  ug->done = 0;
  ug->success = 0;

  if (!ok)
    run_script();
}


urlgroup_t * urlgroup_init(
    checker_t *ch,
    config_t *co)
{
  int i;
  struct itimerspec its;

  memset(&its, 0, sizeof(struct itimerspec));

  urlgroup_t *ug = calloc(1, sizeof(*ug));
  assert(ug);
  ug->instances = ch->concurrency;
  ug->done = 0;
  ug->success = 0;
  ug->ch = ch;

  ug->checks = calloc(ug->instances, sizeof(checker_t *));
  assert(ug->checks);

  ug->interval_tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
  if (ug->interval_tfd < 0)
    err(EXIT_FAILURE, "timerfd create failure");
//  its.it_value.tv_sec = ch->interval_ms / 1000;
//  its.it_value.tv_nsec = (ch->interval_ms % 1000) * 1000000;
  its.it_value.tv_sec = 0;
  its.it_value.tv_nsec = 500000000;
  its.it_interval.tv_sec = ch->interval_ms / 1000;
  its.it_interval.tv_nsec = (ch->interval_ms % 1000) * 1000000;
  if (timerfd_settime(ug->interval_tfd, 0, &its, NULL) < 0)
    err(EXIT_FAILURE, "timerfd settime");
  if (event_add_fd(ug->interval_tfd, interval_timeout, NULL, ug, EPOLLIN) < 0)
    err(EXIT_FAILURE, "event add fd");

  ug->tplt = curl_easy_init();
  CS(CURLOPT_WRITEFUNCTION, write_data);
  CS(CURLOPT_LOW_SPEED_TIME, ug->ch->timeout_ms/1000/2);
  CS(CURLOPT_LOW_SPEED_LIMIT, 30*1024);
  CS(CURLOPT_TIMEOUT_MS, ug->ch->timeout_ms);
  CS(CURLOPT_URL, ch->url);
  if (ug->ch->tls_ignore) {
    CS(CURLOPT_SSL_VERIFYHOST, 0);
    CS(CURLOPT_SSL_VERIFYPEER, 0);
  }
  CS(CURLOPT_VERBOSE, co->verbose);
  return ug;
}


void urlgroup_destroy(
    urlgroup_t *ug)
{
  if (!ug)
    return;

  close(ug->interval_tfd);
  free(ug->checks);
}


void urlchecker_curl_done(
    CURL *easy,
    CURLcode result)
{
  urlchecker_t *uch = NULL;
  curl_easy_getinfo(easy, CURLINFO_PRIVATE, &uch);
  uch->curlcode = result;
  urlchecker_finish(uch);
  return;
}
