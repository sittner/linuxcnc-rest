#ifndef LCREST_REST_H
#define LCREST_REST_H

#include <stdint.h>
#include <stdbool.h>

#include <ulfius.h>

#include "lcrest.h"
#include "lcrest_conf.h"

int rest_start(CONF_ROOT_T *_conf);
int rest_stop(void);

#endif

