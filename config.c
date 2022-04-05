#include "common.h"
#include "config.h"
#include "ini/iniparser.h"

config_t *conf;
char ret[256];

static char * makekey(
    char *sn,
    char *k)
{
  memset(ret, 0, 256);
  snprintf(ret, 255, "%s:%s", sn, k);

  return ret;
}


static checker_t * parse_checker(
    dictionary *d,
    char *s)
{
  char *p;
  checker_t *ch = calloc(1, sizeof(*ch));
  if (!ch)
    assert(ch);

  p = makekey(s, "timeout");
  ch->timeout_ms = iniparser_getint(d, p, DEFAULT_TIMEOUT);
  if (ch->timeout_ms <= 0)
    errx(EXIT_FAILURE, "%s must be a positive integer", p);
  ch->timeout_ms *= 1000;

  p = makekey(s, "interval");
  ch->interval_ms = iniparser_getint(d, p, DEFAULT_INTERVAL);
  if (ch->interval_ms <= 0)
    errx(EXIT_FAILURE, "%s must be a positive integer", p);
  ch->interval_ms *= 1000;

  p = makekey(s, "concurrency");
  ch->concurrency = iniparser_getint(d, p, DEFAULT_CONCURRENCY);
  if (ch->concurrency <= 0)
    errx(EXIT_FAILURE, "%s must be a positive integer", p);

  p = makekey(s, "http_response");
  ch->http_response_code = iniparser_getint(d, p, 0);
  if (ch->http_response_code < 0)
    errx(EXIT_FAILURE, "%s must be 0 or higher", p);

  p = makekey(s, "tls_ignore");
  ch->tls_ignore = iniparser_getboolean(d, p, 0);

  p = makekey(s, "url");
  ch->url = strdup(iniparser_getstring(d, p, NULL));
  if (!ch->url)
    errx(EXIT_FAILURE, "%s is not a valid URL", p);

  ch->section_name = strdup(s);
  return ch;
}


void config_destroy(
    void)
{
  int i;
  checker_t *ch = NULL;

  for (i=0; i < conf->total_checkers; i++) {
    ch = conf->checks[i];
    if (ch) {
      if (ch->url)
        free(ch->url);
      if (ch->section_name)
        free(ch->section_name);
      free(ch);
    }
  }
  free(conf);
  return;
}


config_t * config_load(
    const char *path)
{
  int i;
  config_t *c = NULL;
  checker_t *ch = NULL;
  dictionary *d = NULL;
  char *p;

  c = calloc(1, sizeof(*c));
  assert(c);

  d = iniparser_load(path);
  if (!d)
    err(EXIT_FAILURE, "Cannot load config file");

  /* Load main configuration values */
  if (!iniparser_find_entry(d, "main")) 
    errx(EXIT_FAILURE, "Config file requires a main section, but not found");
  if (strncmp(iniparser_getsecname(d, 0), "main", 5) != 0)
    errx(EXIT_FAILURE, "The first section must be a main section");

  c->verbose = iniparser_getboolean(d, "main:verbose", 0);

  c->run_script = iniparser_getstring(d, "main:runscript", "");
  if (c->run_script[0] != 0 && c->run_script[0] != '/')
    errx(EXIT_FAILURE, "Run script value must be an absolute path");
  if (access(c->run_script, X_OK) < 0)
    errx(EXIT_FAILURE, "Run script value must be executable");

  c->total_checkers = iniparser_getnsec(d)-1;
  if (c->total_checkers <= 0)
    errx(EXIT_FAILURE, "There are no checkers listed in the configuration file, aborting.");

  c->checks = calloc(c->total_checkers, sizeof(*c->checks));
  assert(c->checks);
  for (i=1; i < c->total_checkers+1; i++) {
    p = iniparser_getsecname(d, i);
    ch = parse_checker(d, p);
    c->checks[i-1] = ch;
  }

  if (conf)
    config_destroy();
  conf = c;

  return conf;
}


config_t * config_get()
{
  return conf;
}
