#ifndef _URLCHECKER_H_
#define _URLCHECKER_H_
#include <curl/curl.h>
#include "config.h"


typedef struct urlgroup urlgroup_t;
typedef struct urlchecker urlchecker_t;

/* Need an interval timer.
 * Need a timeout timer?
 * Need a method to cancel an active request.
*/
struct urlchecker {
  CURL *curl;
  double time_start;
  double time_end;
  int id;
  char errbuf[512];
  int curlcode;
  int rc;
  bool done;
  urlgroup_t *group;

  struct data {
    char *data;
    off_t off;
    size_t len;
  } out;
};

struct urlgroup {
  CURL *tplt;
  int instances;
  int done;
  int success;
  checker_t *ch;
  urlchecker_t **checks;
  int interval_tfd;
};


void urlchecker_curl_done(CURL *easy, CURLcode result);
urlgroup_t * urlgroup_init(checker_t *ch, config_t *conf);
#endif
