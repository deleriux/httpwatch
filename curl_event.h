#ifndef _CURL_EVENT_H_
#define _CURL_EVENT_H_
#include <curl/curl.h>

void curl_event_init(void);
CURLM * curl_event_multi_get(void);
#endif
