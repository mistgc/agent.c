#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <openai.h>

#include "call_llm.h"

extern char *api_key;
extern char *base_url;

int chat_complete(char **msg, const char *prompt, const char *model_id) {
  *msg = openai_chat_with_model(prompt, model_id);
  return *msg ? (int)strlen(*msg) : -1;
}

int chat_complete_stream(chat_complete_stream_callback cb, const char *prompt,
                         const char *model_id, void *userdata) {
  if (!api_key || !base_url || !model_id || !prompt)
    return -1;

  CURL *c = NULL;
  struct curl_slist *headers = NULL;
  cJSON *j = NULL;
  char *json = NULL;
  CURLcode res = CURLE_FAILED_INIT;

  c = curl_easy_init();
  if (!c)
    goto cleanup;

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: text/event-stream");
  char auth_header[512];
  snprintf(auth_header, sizeof auth_header,"Authorization: Bearer %s", api_key);
  headers = curl_slist_append(headers, auth_header);
  if (!headers)
    goto cleanup;

  j = cJSON_CreateObject();
  if (!j)
    goto cleanup;
  cJSON_AddStringToObject(j, "model", model_id);

  cJSON_AddBoolToObject(j, "stream", 1);

  cJSON* messages = cJSON_AddArrayToObject(j, "messages");
  cJSON* msg = cJSON_CreateObject();
  if (!msg)
    goto cleanup;
  cJSON_AddStringToObject(msg, "role", "user");
  cJSON_AddStringToObject(msg, "content", prompt);
  cJSON_AddItemToArray(messages, msg);

  json = cJSON_PrintUnformatted(j);
  if (!json)
    goto cleanup;

  curl_easy_setopt(c, CURLOPT_URL, base_url);
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, json);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, userdata);

  res = curl_easy_perform(c);

cleanup:
  if (json)
    free(json);
  if (j)
    cJSON_Delete(j);
  if (headers)
    curl_slist_free_all(headers);
  if (c)
    curl_easy_cleanup(c);

  return (int)res;
}
