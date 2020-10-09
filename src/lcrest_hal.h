#ifndef LCREST_HAL_H
#define LCREST_HAL_H

#include <stdint.h>
#include <stdbool.h>

#include <jansson.h>

#include "hal.h"

#include "lcrest.h"
#include "lcrest_conf.h"

extern int hal_comp_id;

int hal_export_json_pins(CONF_ROOT_T *conf);

bool hal_validate_json_type(hal_type_t type, json_t *val);
json_t *hal_read_json_pin(CONF_JSON_ITEM_T *json);
int hal_write_json_pin(CONF_JSON_ITEM_T *json, json_t *val);

size_t hal_get_pin_size(hal_type_t type);
size_t hal_get_param_size(hal_type_t type);

#endif
