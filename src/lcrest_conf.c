#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>

#include "lcrest.h"
#include "lcrest_conf.h"
#include "lcrest_hal.h"

#define BUFFSIZE 8192
#define XML_MAX_LEVELS 32

struct CONF_XML_HANLDER;

typedef struct CONF_XML_INST {
  XML_Parser parser;
  const struct CONF_XML_HANLDER *states;
  int state;
  int state_stack[XML_MAX_LEVELS];
  int state_stack_pos;
  CONF_ROOT_T *conf;

  CONF_JSON_ITEM_T *json_parent;
  CONF_JSON_ITEM_T *json_last;
  long json_array_factor;

} CONF_XML_INST_T;

typedef struct CONF_XML_HANLDER {
  const char *el;
  int state_from;
  int state_to;
  void (*start_handler)(struct CONF_XML_INST *inst, int next, const char **attr);
  void (*end_handler)(struct CONF_XML_INST *inst, int next);
} CONF_XML_HANLDER_T;

static int initXmlInst(CONF_XML_INST_T *inst, const CONF_XML_HANLDER_T *states);
static void xml_start_handler(void *data, const char *el, const char **attr);
static void xml_end_handler(void *data, const char *el);

static CONF_JSON_ITEM_T *createJsonItem(struct CONF_XML_INST *inst, CONF_TYPE_T type, const char *name);
static CONF_JSON_ITEM_T *cloneJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *src);
static void addJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *json);
static void closeJsonItem(struct CONF_XML_INST *inst);

static CONF_JSON_ITEM_T *deepCopyJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *src, int level);

static void closeJsonContainer(struct CONF_XML_INST *inst, int next);
static void closeJsonArrayContainer(struct CONF_XML_INST *inst, int next);
static void parseHalJsonRoot(struct CONF_XML_INST *inst, int next, const char **attr);
static void parseHalJsonPin(struct CONF_XML_INST *inst, int next, const char **attr);
static void parseHalJsonParam(struct CONF_XML_INST *inst, int next, const char **attr);
static void parseHalJsonObject(struct CONF_XML_INST *inst, int next, const char **attr);
static void parseHalJsonArray(struct CONF_XML_INST *inst, int next, const char **attr);

static void conf_free_json(CONF_JSON_ITEM_T *json, bool parent_cloned);

static const CONF_XML_HANLDER_T xml_states[] = {
  { "halJson", confTypeNone, confTypeJson, NULL, NULL },
  { "halJsonRoot", confTypeJson, confTypeJsonRoot, parseHalJsonRoot, closeJsonContainer },
  { "halJsonPin", confTypeJsonRoot, confTypeJsonPin, parseHalJsonPin, NULL },
  { "halJsonRaram", confTypeJsonRoot, confTypeJsonParam, parseHalJsonParam, NULL },
  { "halJsonObject", confTypeJsonRoot, confTypeJsonObject, parseHalJsonObject, closeJsonContainer },
  { "halJsonArray", confTypeJsonRoot, confTypeJsonArray, parseHalJsonArray, closeJsonArrayContainer },
  { "halJsonPin", confTypeJsonObject, confTypeJsonPin, parseHalJsonPin, NULL },
  { "halJsonRaram", confTypeJsonObject, confTypeJsonParam, parseHalJsonParam, NULL },
  { "halJsonObject", confTypeJsonObject, confTypeJsonObject, parseHalJsonObject, closeJsonContainer },
  { "halJsonArray", confTypeJsonObject, confTypeJsonArray, parseHalJsonArray, closeJsonArrayContainer },
  { "halJsonPin", confTypeJsonArray, confTypeJsonPin, parseHalJsonPin, NULL },
  { "halJsonRaram", confTypeJsonArray, confTypeJsonParam, parseHalJsonParam, NULL },
  { "halJsonObject", confTypeJsonArray, confTypeJsonObject, parseHalJsonObject, closeJsonContainer },
  { "halJsonArray", confTypeJsonArray, confTypeJsonArray, parseHalJsonArray, closeJsonArrayContainer },
  { "NULL", -1, -1, NULL, NULL }
};

static int initXmlInst(CONF_XML_INST_T *inst, const CONF_XML_HANLDER_T *states) {
  // create xml parser
  inst->parser = XML_ParserCreate(NULL);
  if (inst->parser == NULL) {
    return 1;
  }

  // setup data
  inst->states = states;
  inst->state = 0;
  inst->state_stack_pos = 0;
  XML_SetUserData(inst->parser, inst);

  // setup handlers
  XML_SetElementHandler(inst->parser, xml_start_handler, xml_end_handler);

  return 0;
}

static void xml_start_handler(void *data, const char *el, const char **attr) {
  CONF_XML_INST_T *inst = (CONF_XML_INST_T *) data;
  const CONF_XML_HANLDER_T *state;

  if (inst->state_stack_pos >= XML_MAX_LEVELS) {
    fprintf(stderr, "%s: ERROR: XML level limit exceeded\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  for (state = inst->states; state->el != NULL; state++) {
    if (inst->state == state->state_from && (strcmp(el, state->el) == 0)) {
      if (state->start_handler != NULL) {
        state->start_handler(inst, state->state_to, attr);
      }
      inst->state_stack[(inst->state_stack_pos)++] = inst->state;
      inst->state = state->state_to;
      return;
    } 
  }

  fprintf(stderr, "%s: ERROR: unexpected node %s found\n", modname, el);
  XML_StopParser(inst->parser, 0);
}

static void xml_end_handler(void *data, const char *el) {
  CONF_XML_INST_T *inst = (CONF_XML_INST_T *) data;
  const CONF_XML_HANLDER_T *state;
  int prev_state;

  if (inst->state_stack_pos > 0) {
    prev_state = inst->state_stack[--(inst->state_stack_pos)];
    for (state = inst->states; state->el != NULL; state++) {
      if (prev_state == state->state_from && inst->state == state->state_to && (strcmp(el, state->el) == 0)) {
        if (state->end_handler != NULL) {
          state->end_handler(inst, state->state_from);
        }
        inst->state = state->state_from;
        return;
      } 
    }
  }

  fprintf(stderr, "%s: ERROR: unexpected close tag %s found\n", modname, el);
  XML_StopParser(inst->parser, 0);
}

static CONF_JSON_ITEM_T *createJsonItem(struct CONF_XML_INST *inst, CONF_TYPE_T type, const char *name) {
  CONF_JSON_ITEM_T *json;

  // alloc memory
  json = calloc(1, sizeof(CONF_JSON_ITEM_T));
  if (json == NULL) {
    fprintf(stderr, "%s: ERROR: unable to alloc memory for jsonItem\n", modname);
    goto fail0;
  }

  // set type
  json->type = type;

  // set name
  json->name = strdup(name);
  if (json->name == NULL) {
    fprintf(stderr, "%s: ERROR: unable to alloc memory for jsonItem name\n", modname);
    goto fail1;
  }

  // add to hierarchy
  addJsonItem(inst, json);

  return json;

fail1:
  free(json);
fail0:
  return NULL;
}

static CONF_JSON_ITEM_T *cloneJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *src) {
  CONF_JSON_ITEM_T *json;

  // alloc memory
  json = calloc(1, sizeof(CONF_JSON_ITEM_T));
  if (json == NULL) {
    fprintf(stderr, "%s: ERROR: unable to alloc memory for jsonItem\n", modname);
    goto fail0;
  }

  // copy data (NOTE: name gets reused)
  memcpy(json, src, sizeof(CONF_JSON_ITEM_T));

  // add to hierarchy
  addJsonItem(inst, json);

  return json;

fail0:
  return NULL;
}

static void addJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *json) {
  // initialize next and child pointers
  json->next = NULL;
  json->childs = NULL;

  // set parent
  json->parent = inst->json_parent;

  // append to list of last item
  if (inst->json_last != NULL) {
    inst->json_last->next = json;
  }

  // append to child list of parent
  if (inst->json_parent != NULL && inst->json_parent->childs == NULL) {
    inst->json_parent->childs = json;
  }

  // setup container struct
  if (CONF_TYPE_IS_CONTAINER(json->type)) {
    inst->json_last = NULL;
    inst->json_parent = json;
  } else {
    inst->json_last = json;
  }
}

static void closeJsonItem(struct CONF_XML_INST *inst) {
  // go one level up
  inst->json_last = inst->json_parent;
  if (inst->json_parent != NULL) {
    inst->json_parent = inst->json_parent->parent;
  }
}

static CONF_JSON_ITEM_T *deepCopyJsonItem(struct CONF_XML_INST *inst, CONF_JSON_ITEM_T *src, int level) {
  CONF_JSON_ITEM_T *json, *child;

  json = cloneJsonItem(inst, src);
  if (json == NULL) {
    return NULL;
  }

  for (child = src->childs; child != NULL; child = child->next) {
    if (deepCopyJsonItem(inst, child, level + 1) == NULL) {
      return NULL;
    }
  }

  if (CONF_TYPE_IS_CONTAINER(src->type)) {
    closeJsonItem(inst);
  }

  return json;
}

static void closeJsonContainer(struct CONF_XML_INST *inst, int next) {
  // close container
  closeJsonItem(inst);
}

static void closeJsonArrayContainer(struct CONF_XML_INST *inst, int next) {
  int i;
  CONF_JSON_ITEM_T *base = inst->json_parent;
  CONF_JSON_ITEM_T *json;

  // close array (index 0)
  closeJsonItem(inst);

  // revert array factor
  inst->json_array_factor /= base->array_size;

  // create other array instances
  for (i=1; i<base->array_size; i++) {
    json = deepCopyJsonItem(inst, base, 0);
    if (json == NULL) {
      XML_StopParser(inst->parser, 0);
      return;
    }

    // update array index
    json->array_index = i;
  }
}

static void parseHalJsonRoot(struct CONF_XML_INST *inst, int next, const char **attr) {
  CONF_ROOT_T *conf = inst->conf;
  const char *iname = NULL;
  CONF_JSON_ITEM_T *json;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse path
    if (strcmp(name, "path") == 0) {
      iname = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid halJsonRoot attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // path is required
  if (iname == NULL || iname[0] == 0) {
    fprintf(stderr, "%s: ERROR: halJsonRoot has no/empty path attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // add item
  json = createJsonItem(inst, confTypeJsonRoot, iname);
  if (json == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  // first root is head of list
  if (conf->json == NULL) {
    conf->json = json;
  }
}

static void parseHalJsonPin(struct CONF_XML_INST *inst, int next, const char **attr) {
  CONF_ROOT_T *conf = inst->conf;
  const char *iname = NULL;
  hal_type_t type = -1;
  hal_pin_dir_t dir = -1;
  CONF_JSON_ITEM_T *json;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      iname = val;
      continue;
    }

    // parse type
    if (strcmp(name, "type") == 0) {
      if (strcmp(val, "bit") == 0) {
        type = HAL_BIT;
        continue;
      }
      if (strcmp(val, "float") == 0) {
        type = HAL_FLOAT;
        continue;
      }
      if (strcmp(val, "s32") == 0) {
        type = HAL_S32;
        continue;
      }
      if (strcmp(val, "u32") == 0) {
        type = HAL_U32;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid halJsonPin type %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse dir
    if (strcmp(name, "dir") == 0) {
      if (strcmp(val, "in") == 0) {
        dir = HAL_IN;
        continue;
      }
      if (strcmp(val, "out") == 0) {
        dir = HAL_OUT;
        continue;
      }
      if (strcmp(val, "io") == 0) {
        dir = HAL_IO;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid halJsonPin dir %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid halJsonPin attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (iname == NULL || iname[0] == 0) {
    fprintf(stderr, "%s: ERROR: halJsonPin has no/empty name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // type is required
  if (type < 0) {
    fprintf(stderr, "%s: ERROR: halJsonPin has no type attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // dir is required
  if (dir < 0) {
    fprintf(stderr, "%s: ERROR: halJsonPin has no dir attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // add item
  json = createJsonItem(inst, confTypeJsonPin, iname);
  if (json == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set pin attributes
  json->hal.type = type;
  json->hal.pin.dir = dir;

  // increase hal data size
  conf->json_hal_size += hal_get_pin_size(type) * inst->json_array_factor;
}

static void parseHalJsonParam(struct CONF_XML_INST *inst, int next, const char **attr) {
  CONF_ROOT_T *conf = inst->conf;
  const char *iname = NULL;
  hal_type_t type = -1;
  hal_param_dir_t dir = -1;
  CONF_JSON_ITEM_T *json;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      iname = val;
      continue;
    }

    // parse type
    if (strcmp(name, "type") == 0) {
      if (strcmp(val, "bit") == 0) {
        type = HAL_BIT;
        continue;
      }
      if (strcmp(val, "float") == 0) {
        type = HAL_FLOAT;
        continue;
      }
      if (strcmp(val, "s32") == 0) {
        type = HAL_S32;
        continue;
      }
      if (strcmp(val, "u32") == 0) {
        type = HAL_U32;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid halJsonParam type %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse dir
    if (strcmp(name, "dir") == 0) {
      if (strcmp(val, "ro") == 0) {
        dir = HAL_RO;
        continue;
      }
      if (strcmp(val, "rw") == 0) {
        dir = HAL_RW;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid halJsonParam dir %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid halJsonParam attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (iname == NULL || iname[0] == 0) {
    fprintf(stderr, "%s: ERROR: halJsonParam has no/empty name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // type is required
  if (type < 0) {
    fprintf(stderr, "%s: ERROR: halJsonParam has no type attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // dir is required
  if (dir < 0) {
    fprintf(stderr, "%s: ERROR: halJsonParam has no dir attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // add item
  json = createJsonItem(inst, confTypeJsonParam, iname);
  if (json == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set pin attributes
  json->hal.type = type;
  json->hal.param.dir = dir;

  // increase hal data size
  conf->json_hal_size += hal_get_param_size(type) * inst->json_array_factor;
}

static void parseHalJsonObject(struct CONF_XML_INST *inst, int next, const char **attr) {
  const char *iname = NULL;
  CONF_JSON_ITEM_T *json;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      iname = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid halJsonObject attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (iname == NULL || iname[0] == 0) {
    fprintf(stderr, "%s: ERROR: halJsonObject has no/empty name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // add item
  json = createJsonItem(inst, confTypeJsonObject, iname);
  if (json == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }
}

static void parseHalJsonArray(struct CONF_XML_INST *inst, int next, const char **attr) {
  const char *iname = NULL;
  int size = 0;
  CONF_JSON_ITEM_T *json;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      iname = val;
      continue;
    }

    // parse size
    if (strcmp(name, "size") == 0) {
      size = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid halJsonArray attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (iname == NULL || iname[0] == 0) {
    fprintf(stderr, "%s: ERROR: halJsonArray has no/empty name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // size is required
  if (size <= 0) {
    fprintf(stderr, "%s: ERROR: halJsonArray has no size attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // add item
  json = createJsonItem(inst, confTypeJsonArray, iname);
  if (json == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set array attributes
  json->array_size = size;

  // update array factor
  inst->json_array_factor *= size;
}

static void conf_free_json(CONF_JSON_ITEM_T *json, bool parent_cloned) {
  CONF_JSON_ITEM_T *json_next;
  bool cloned;

  while (json != NULL) {
    // get next
    json_next = json->next;
    cloned = parent_cloned || (json->array_index > 0);

    // free childs
    conf_free_json(json->childs, cloned);

    // free name only on first instance, since they are reused
    if (!cloned) {
      free(json->name);
    }

    // free object
    free(json);

    // process next
    json = json_next;
  }
}

CONF_ROOT_T *conf_parse(const char *filename) {
  CONF_ROOT_T *ret = NULL;
  int done;
  char buffer[BUFFSIZE];
  FILE *file;
  CONF_XML_INST_T inst;

  // open file
  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "%s: ERROR: unable to open config file %s\n", modname, filename);
    goto fail0;
  }

  // create xml parser
  memset(&inst, 0, sizeof(inst));
  if (initXmlInst((CONF_XML_INST_T *) &inst, xml_states)) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for parser\n", modname);
    goto fail1;
  }

  // allocate conf header
  inst.conf = calloc(1, sizeof(CONF_ROOT_T));
  if (inst.conf == NULL) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for conf header\n", modname);
    goto fail2;
  }

  inst.json_array_factor = 1;
  for (done=0; !done;) {
    // read block
    int len = fread(buffer, 1, BUFFSIZE, file);
    if (ferror(file)) {
      fprintf(stderr, "%s: ERROR: Couldn't read from file %s\n", modname, filename);
      goto fail3;
    }

    // check for EOF
    done = feof(file);

    // parse current block
    if (!XML_Parse(inst.parser, buffer, len, done)) {
      fprintf(stderr, "%s: ERROR: Parse error at line %u: %s\n", modname,
        (unsigned int)XML_GetCurrentLineNumber(inst.parser),
        XML_ErrorString(XML_GetErrorCode(inst.parser)));
      goto fail3;
    }
  }

  // everything is fine
  ret = inst.conf;
  inst.conf = NULL;

fail3:
  conf_free(inst.conf);
fail2:
  XML_ParserFree(inst.parser);
fail1:
  fclose(file);
fail0:
  return ret;
}

void conf_free(CONF_ROOT_T *conf) {
  if (conf == NULL) {
    return;
  }

  conf_free_json(conf->json, false);
  free(conf);
}

