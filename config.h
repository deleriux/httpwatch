#ifndef _CONFIG_H_
#define _CONFIG_H_
#include "common.h"

#define DEFAULT_TIMEOUT 4
#define DEFAULT_INTERVAL 30
#define DEFAULT_CONCURRENCY 1

typedef struct checker {
  char *url;
  int32_t timeout_ms;
  int32_t interval_ms;
  int32_t concurrency;
  int32_t http_response_code;
  bool tls_ignore;
  char *section_name;
} checker_t;

typedef struct config {
  int total_checkers;
  bool verbose;
  char *run_script;
  struct checker **checks;
} config_t;


config_t * config_load(const char *path);
config_t * config_get(void);
void config_destroy(void);
#endif
