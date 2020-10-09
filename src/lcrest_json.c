#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include <string.h>

#include "lcrest.h"
#include "lcrest_conf.h"
#include "lcrest_json.h"
#include "lcrest_hal.h"


static void parse_array(const char *key, json_t *inp, CONF_JSON_ITEM_T *json);
static void parse_value(const char *key, json_t *inp, CONF_JSON_ITEM_T *json);

static CONF_JSON_ITEM_T *find_conf_item(const char *key, CONF_JSON_ITEM_T *json);

void json_build_response(json_t *ret, CONF_JSON_ITEM_T *json) {
  json_t *obj, *arr = NULL;

  for (; json != NULL; json = json->next) {
    switch (json->type) {
      case confTypeJsonPin:
      case confTypeJsonParam:
        json_object_set_new(ret, json->name, hal_read_json_pin(json));
        break;

      case confTypeJsonObject:
        obj = json_object();
        json_build_response(obj, json->childs);
        json_object_set_new(ret, json->name, obj);
        break;

      case confTypeJsonArray:
        if (json->array_index == 0 || arr == NULL) {
          arr = json_array();
          json_object_set_new(ret, json->name, arr);
        }
        obj = json_object();
        json_build_response(obj, json->childs);
        json_array_append_new(arr, obj);
        break;

      default:
        break;
    }
  }
}

void json_parse_request(json_t *inp, CONF_JSON_ITEM_T *json) {
  const char *key;
  json_t *value;

  if (!json_is_object(inp)) {
    return;
  }

  json_object_foreach(inp, key, value) {
    parse_value(key, value, json);
  }
}

static void parse_array(const char *key, json_t *inp, CONF_JSON_ITEM_T *json) {
  int index;
  json_t *value;

  if (!json_is_array(inp)) {
    return;
  }

  json_array_foreach(inp, index, value) {
    // array must contain objects
    if (!json_is_object(value)) {
      return;
    }

    // check for corresponding index
    if (json->array_index != index) {
      return;
    }

    // check name for safety
    if (strcasecmp(key, json->name) != 0) {
      return;
    }

    // process object
    json_parse_request(value, json->childs);

    // go to next config item
    json = json->next;
    if (json == NULL) {
      return;
    }
  }
}

static void parse_value(const char *key, json_t *inp, CONF_JSON_ITEM_T *list) {
  CONF_JSON_ITEM_T *json;

  json = find_conf_item(key, list);
  if (json == NULL) {
    return;
  }

  switch (json->type) {
    case confTypeJsonPin:
    case confTypeJsonParam:
      // TODO: handle data type missmatch
      if (json_is_number(inp) || json_is_boolean(inp)) {
        hal_write_json_pin(json, inp);
      }
      return;

    case confTypeJsonObject:
      if (json_is_object(inp) && json->childs != NULL) {
        json_parse_request(inp, json->childs);
      }
      return;

    case confTypeJsonArray:
      if (json_is_array(inp)) {
        parse_array(key, inp, json);
      }
      return;

    default:
      return;
  }
}

static CONF_JSON_ITEM_T *find_conf_item(const char *key, CONF_JSON_ITEM_T *list) {
  CONF_JSON_ITEM_T *json;

  for (json = list; json != NULL; json = json->next) {
    // skip array copies
    if (json->array_index > 0) {
      continue;
    }

    // skip unmatching names
    if (strcasecmp(key, json->name) != 0) {
      continue;
    }

    return json;
  }

  return NULL;
}

