#ifndef LCREST_CONF_H
#define LCREST_CONF_H

#include <stdint.h>
#include <stdbool.h>

#include "hal.h"

typedef enum {
  confTypeNone = 0,
  confTypeJson,
  confTypeJsonRoot,
  confTypeJsonPin,
  confTypeJsonParam,
  confTypeJsonObject,
  confTypeJsonArray
} CONF_TYPE_T;

#define CONF_TYPE_IS_CONTAINER(t) (t == confTypeJsonRoot || t == confTypeJsonObject || t == confTypeJsonArray)

typedef union {
  void *ptr;
  hal_bit_t *bit;
  hal_u32_t *u32;
  hal_s32_t *s32;
  hal_float_t *flt;
} CONF_JSON_HAL_PARAM_PTR_T;

typedef struct {
  hal_param_dir_t dir;
  CONF_JSON_HAL_PARAM_PTR_T ptr;
} CONF_JSON_HAL_PARAM_T;

typedef union {
  void *ptr;
  hal_bit_t **bit;
  hal_u32_t **u32;
  hal_s32_t **s32;
  hal_float_t **flt;
} CONF_JSON_HAL_PIN_PTR_T;

typedef struct {
  hal_pin_dir_t dir;
  CONF_JSON_HAL_PIN_PTR_T ptr;
} CONF_JSON_HAL_PIN_T;

typedef struct {
  hal_type_t type;
  union {
    CONF_JSON_HAL_PARAM_T param;
    CONF_JSON_HAL_PIN_T pin;
  };
} CONF_JSON_HAL_T;

typedef struct CONF_JSON_ITEM {
  char *name;
  CONF_TYPE_T type;
  struct CONF_JSON_ITEM *next;
  struct CONF_JSON_ITEM *parent;
  struct CONF_JSON_ITEM *childs;
  CONF_JSON_HAL_T hal;
  int array_size;
  int array_index;
} CONF_JSON_ITEM_T;

typedef struct CONF_ROOT {
  CONF_JSON_ITEM_T *json;
  size_t json_hal_size;
} CONF_ROOT_T;

CONF_ROOT_T *conf_parse(const char *filename);
void conf_free(CONF_ROOT_T *conf);

#endif

