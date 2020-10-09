#include <stdio.h>
#include <string.h>
#include <ulfius.h>
#include <jansson.h>
#include <string.h>
#include <netinet/in.h>

#include "lcrest.h"
#include "lcrest_conf.h"
#include "lcrest_rest.h"
#include "lcrest_json.h"

#define PORT 8080

static int callback_json_get(const struct _u_request * request, struct _u_response * response, void * user_data);
static int callback_json_post(const struct _u_request * request, struct _u_response * response, void * user_data);

static struct _u_instance instance;

static int callback_json_get(const struct _u_request * request, struct _u_response * response, void * user_data) {
  CONF_JSON_ITEM_T *json = (CONF_JSON_ITEM_T *) user_data;
  json_t *ret;

  ret = json_object();
  json_build_response(ret, json);
  ulfius_set_json_body_response(response, 200, ret);
  json_decref(ret);

  return U_CALLBACK_CONTINUE;
}

static int callback_json_post(const struct _u_request * request, struct _u_response * response, void * user_data) {
  CONF_JSON_ITEM_T *json = (CONF_JSON_ITEM_T *) user_data;
  json_t *root;
  json_error_t error;

  root = json_loadb(request->binary_body, request->binary_body_length, 0, &error);
  if (!root) {
    fprintf(stderr, "json error on line %d: %s\n", error.line, error.text);
    ulfius_set_string_body_response(response, 400, "JSON parsing error.");
    return U_CALLBACK_ERROR;
  }

  json_parse_request(root, json);
  json_decref(root);

  ulfius_set_string_body_response(response, 200, "OK");
  return U_CALLBACK_CONTINUE;
}

int rest_start(CONF_ROOT_T *conf) {
  int err;
  struct sockaddr_in lsnr;
  CONF_JSON_ITEM_T *json;

  // build listener address
  memset(&lsnr, 0, sizeof(lsnr));
  lsnr.sin_family = AF_INET;
  lsnr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  lsnr.sin_port = htons(PORT);

  // Initialize instance with the port number
  if ((err = ulfius_init_instance(&instance, PORT, &lsnr, NULL)) != U_OK) {
    fprintf(stderr, "%s: ERROR: unable to initialize ulfius instance\n", modname);
    goto fail0;
  }

  // setup json endpoints
  for (json = conf->json; json != NULL; json = json->next) {
    ulfius_add_endpoint_by_val(&instance, "GET", "/hal/json", json->name, 0, &callback_json_get, json->childs);
    ulfius_add_endpoint_by_val(&instance, "POST", "/hal/json", json->name, 0, &callback_json_post, json->childs);
  }

  // Start the framework
  if ((err = ulfius_start_framework(&instance)) != U_OK) {
    fprintf(stderr, "%s: ERROR: unable to start ulfius instance\n", modname);
    goto fail1;
  }

  return U_OK;

fail1:
  ulfius_clean_instance(&instance);
fail0:
  return err;
}

int rest_stop(void) {
  return ulfius_stop_framework(&instance);
}

