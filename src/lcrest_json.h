#ifndef LCREST_JSON_H
#define LCREST_JSON_H

#include <stdint.h>
#include <stdbool.h>

#include <jansson.h>

#include "lcrest.h"
#include "lcrest_conf.h"

void json_build_response(json_t *ret, CONF_JSON_ITEM_T *json);
void json_parse_request(json_t *inp, CONF_JSON_ITEM_T *json);

#endif

