#include <stdio.h>

#include "lcrest.h"
#include "lcrest_conf.h"
#include "lcrest_hal.h"

static int export_json_pins(CONF_JSON_ITEM_T *json, const char *pfx, void **hal_data_ptr);
static int export_json_pin(CONF_JSON_ITEM_T *json, const char *name, void **hal_data_ptr);

int hal_comp_id;

static int export_json_pins(CONF_JSON_ITEM_T *json, const char *pfx, void **hal_data_ptr) {
  char name[HAL_NAME_LEN];

  for (; json != NULL; json = json->next) {
    // process childs
    if (json->childs != NULL) {
      if (json->type == confTypeJsonArray) {
        if (snprintf(name, HAL_NAME_LEN, "%s.%s-%d", pfx, json->name, json->array_index) >= HAL_NAME_LEN) {
          goto name_len_exceeded;
        }
      } else {
        if (snprintf(name, HAL_NAME_LEN, "%s.%s", pfx, json->name) >= HAL_NAME_LEN) {
            goto name_len_exceeded;
        }
      }
      if (export_json_pins(json->childs, name, hal_data_ptr)) {
        return -1;
      }
    }

    // process pins and params
    if (json->type == confTypeJsonPin || json->type == confTypeJsonParam) {
      if (snprintf(name, HAL_NAME_LEN, "%s.%s", pfx, json->name) >= HAL_NAME_LEN) {
        goto name_len_exceeded;
      }
      if (export_json_pin(json, name, hal_data_ptr)) {
        fprintf(stderr, "%s: ERROR: failed to export param/pin '%s'\n", modname, name);
        return -1;
      }
    }
  }

  return 0;

name_len_exceeded:
  fprintf(stderr, "%s: ERROR: name of json param/pin too long: '%s.%s'\n", modname, pfx, json->name);
  return -1;
}

static int export_json_pin(CONF_JSON_ITEM_T *json, const char *name, void **hal_data_ptr) {
  if (json->type == confTypeJsonPin) {
    json->hal.pin.ptr.ptr = *hal_data_ptr;
    *hal_data_ptr += hal_get_pin_size(json->hal.type);
    switch (json->hal.type) {
      case HAL_BIT:
        if (hal_pin_bit_new(name, json->hal.pin.dir, json->hal.pin.ptr.bit, hal_comp_id) < 0) {
          return -1;
        }
        **(json->hal.pin.ptr.bit) = 0;
        return 0;
      case HAL_U32:
        if (hal_pin_u32_new(name, json->hal.pin.dir, json->hal.pin.ptr.u32, hal_comp_id) < 0) {
          return -1;
        }
        **(json->hal.pin.ptr.u32) = 0;
        return 0;
      case HAL_S32:
        if (hal_pin_s32_new(name, json->hal.pin.dir, json->hal.pin.ptr.s32, hal_comp_id) < 0) {
          return -1;
        }
        **(json->hal.pin.ptr.s32) = 0;
        return 0;
      case HAL_FLOAT:
        if (hal_pin_float_new(name, json->hal.pin.dir, json->hal.pin.ptr.flt, hal_comp_id) < 0) {
          return -1;
        }
        **(json->hal.pin.ptr.flt) = 0.0;
        return 0;
      default:
        return -1;
    }
  }

  if (json->type == confTypeJsonParam) {
    json->hal.param.ptr.ptr = *hal_data_ptr;
    *hal_data_ptr += hal_get_param_size(json->hal.type);
    switch (json->hal.type) {
      case HAL_BIT:
        if (hal_param_bit_new(name, json->hal.param.dir, json->hal.param.ptr.bit, hal_comp_id) < 0) {
          return -1;
        }
        *(json->hal.param.ptr.bit) = 0;
        return 0;
      case HAL_U32:
        if (hal_param_u32_new(name, json->hal.param.dir, json->hal.param.ptr.u32, hal_comp_id) < 0) {
          return -1;
        }
        *(json->hal.param.ptr.u32) = 0;
        return 0;
      case HAL_S32:
        if (hal_param_s32_new(name, json->hal.param.dir, json->hal.param.ptr.s32, hal_comp_id) < 0) {
          return -1;
        }
        *(json->hal.param.ptr.s32) = 0;
        return 0;
      case HAL_FLOAT:
        if (hal_param_float_new(name, json->hal.param.dir, json->hal.param.ptr.flt, hal_comp_id) < 0) {
          return -1;
        }
        *(json->hal.param.ptr.flt) = 0.0;
        return 0;
      default:
        return -1;
    }
  }

  return -1;
}

int hal_export_json_pins(CONF_ROOT_T *conf) {
  void *hal_data;

  // allocate hal memory
  hal_data = hal_malloc(conf->json_hal_size);
  if (hal_data == NULL) {
    fprintf(stderr, "%s: ERROR: unable to allocate HAL shared memory for json pins\n", modname);
    return -1;
  }

  // export pins
  return export_json_pins(conf->json, "json", &hal_data);
}

bool hal_validate_json_type(hal_type_t type, json_t *val) {
    switch (type) {
      case HAL_BIT:
        return json_is_boolean(val);
      case HAL_U32:
        return json_is_integer(val);
      case HAL_S32:
        return json_is_integer(val);
      case HAL_FLOAT:
        return json_is_number(val);
      default:
        return false;
    }
}

json_t *hal_read_json_pin(CONF_JSON_ITEM_T *json) {
  if (json->type == confTypeJsonPin) {
    switch (json->hal.type) {
      case HAL_BIT:
        return json_boolean(**(json->hal.pin.ptr.bit));
      case HAL_U32:
        return json_integer(**(json->hal.pin.ptr.u32));
      case HAL_S32:
        return json_integer(**(json->hal.pin.ptr.s32));
      case HAL_FLOAT:
        return json_real(**(json->hal.pin.ptr.flt));
      default:
        return NULL;
    }
  }

  if (json->type == confTypeJsonParam) {
    switch (json->hal.type) {
      case HAL_BIT:
        return json_boolean(*(json->hal.param.ptr.bit));
      case HAL_U32:
        return json_integer(*(json->hal.param.ptr.u32));
      case HAL_S32:
        return json_integer(*(json->hal.param.ptr.s32));
      case HAL_FLOAT:
        return json_real(*(json->hal.param.ptr.flt));
      default:
        return NULL;
    }
  }

  return NULL;
}

int hal_write_json_pin(CONF_JSON_ITEM_T *json, json_t *val) {
  if (!hal_validate_json_type(json->hal.type, val)) {
    return -1;
  }

  if (json->type == confTypeJsonPin) {
    if (json->hal.pin.dir != HAL_OUT && json->hal.pin.dir != HAL_IO) {
      return -1;
    }

    switch (json->hal.type) {
      case HAL_BIT:
        **(json->hal.pin.ptr.bit) = json_is_true(val);
        return 0;
      case HAL_U32:
        **(json->hal.pin.ptr.u32) = json_integer_value(val);
        return 0;
      case HAL_S32:
        **(json->hal.pin.ptr.s32) = json_integer_value(val);
        return 0;
      case HAL_FLOAT:
        **(json->hal.pin.ptr.flt) = json_number_value(val);
        return 0;
      default:
        return -1;
    }
  }

  if (json->type == confTypeJsonParam) {
    if (json->hal.param.dir != HAL_RW) {
      return -1;
    }

    switch (json->hal.type) {
      case HAL_BIT:
        *(json->hal.param.ptr.bit) = json_is_true(val);
        return 0;
      case HAL_U32:
        *(json->hal.param.ptr.u32) = json_integer_value(val);
        return 0;
      case HAL_S32:
        *(json->hal.param.ptr.s32) = json_integer_value(val);
        return 0;
      case HAL_FLOAT:
        *(json->hal.param.ptr.flt) = json_number_value(val);
        return 0;
      default:
        return -1;
    }
  }

  return -1;
}

size_t hal_get_pin_size(hal_type_t type) {
  switch (type) {
    case HAL_BIT:
      return sizeof(hal_bit_t *);
    case HAL_U32:
      return sizeof(hal_u32_t *);
    case HAL_S32:
      return sizeof(hal_s32_t *);
    case HAL_FLOAT:
      return sizeof(hal_float_t *);
    default:
      return 0;
  }
}

size_t hal_get_param_size(hal_type_t type) {
  switch (type) {
    case HAL_BIT:
      return sizeof(hal_bit_t);
    case HAL_U32:
      return sizeof(hal_u32_t);
    case HAL_S32:
      return sizeof(hal_s32_t);
    case HAL_FLOAT:
      return sizeof(hal_float_t);
    default:
      return 0;
  }
}

