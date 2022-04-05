#include "common.h"
#include "event.h"
#include "curl_event.h"
#include "config.h"
#include "urlchecker.h"
#include <getopt.h>

#include <signal.h>

int main(
    const int argc,
    const char **argv)
{
  struct sigaction act;
  config_t *c;
  int i;

  event_init();
  curl_event_init();
  run_script_init();

  if (argc < 2)
    c = config_load("/etc/httpwatch.ini");
  else
    c = config_load(argv[1]);

  for (i=0; i < c->total_checkers; i++) 
    urlgroup_init(c->checks[i], c);

  while (1)
   event_loop(1, -1);

  config_destroy();
}
