#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/eventfd.h>

#include "lcrest.h"
#include "lcrest_conf.h"
#include "lcrest_hal.h"
#include "lcrest_rest.h"

const char *modname = "lcrest";

static int exit_event;

static void exitHandler(int sig) {
  uint64_t u = 1;
  if (write(exit_event, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error writing exit event\n", modname);
  }
}

int main(int argc, char **argv) {
  int ret = 1;
  char *filename;
  CONF_ROOT_T *conf;
  uint64_t u;

  // get config file name
  if (argc != 2) {
    fprintf(stderr, "%s: ERROR: invalid arguments\n", modname);
    goto fail0;
  }
  filename = argv[1];

  // parse config
  conf = conf_parse(filename);
  if (conf == NULL) {
    goto fail0;
  }

  // initialize component
  hal_comp_id = hal_init(modname);
  if (hal_comp_id < 1) {
    fprintf(stderr, "%s: ERROR: hal_init failed\n", modname);
    goto fail1;
  }

  // export json pins
  if (hal_export_json_pins(conf)) {
    goto fail2;
  }

  // start rest server
  if (rest_start(conf) != U_OK) {
    goto fail2;
  }

  // initialize signal handling
  exit_event = eventfd(0, 0);
  if (exit_event == -1) {
    fprintf(stderr, "%s: ERROR: unable to create exit event\n", modname);
    goto fail3;
  }
  signal(SIGINT, exitHandler);
  signal(SIGTERM, exitHandler);








  // everything is fine
  ret = 0;
  hal_ready(hal_comp_id);

  // wait for SIGTERM
  if (read(exit_event, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error reading exit event\n", modname);
  }

  close(exit_event);
fail3:
  rest_stop();
fail2:
  hal_exit(hal_comp_id);
fail1:
  conf_free(conf);
fail0:
  return ret;
}
