#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "config.h"
#include "call_llm.h"

static char _sse_buf[65536];
static size_t _sse_len = 0;

static size_t _stream_callback_for_sse(char *data, size_t size, size_t nmemb,
                                         void *sse_cb) {
    size_t realsize = size * nmemb;
    chat_complete_stream_sse_callback *cb = sse_cb;

    if (_sse_len + realsize > sizeof _sse_buf)
        _sse_len = 0;

    memcpy(_sse_buf + _sse_len, data, realsize);
    _sse_len += realsize;
    _sse_buf[_sse_len] = '\0';

    char *start = _sse_buf;
    while (_sse_len > (size_t)(start - _sse_buf)) {
        char *nl = memchr(start, '\n', _sse_len - (size_t)(start - _sse_buf));
        if (!nl)
            break;

        size_t line_len = (size_t)(nl - start);
        if (line_len > 0 && start[line_len - 1] == '\r')
            line_len--; /* CRLF */
        cb(start, line_len);

        start = nl + 1;
    }

    size_t rest = _sse_len - (size_t)(start - _sse_buf);
    if (rest > 0 && start != _sse_buf)
        memmove(_sse_buf, start, rest);
    _sse_len = rest;
    _sse_buf[_sse_len] = '\0';

    return realsize;
}

int chat_complete_stream(chat_complete_stream_sse_callback sse_cb, const char *prompt,
                         const char *model_id) {
  if (!config()->api_key || !config()->base_url || !model_id || !prompt)
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
  snprintf(auth_header, sizeof auth_header, "Authorization: Bearer %s",
           config()->api_key);
  headers = curl_slist_append(headers, auth_header);
  if (!headers)
    goto cleanup;

  j = cJSON_CreateObject();
  if (!j)
    goto cleanup;
  cJSON_AddStringToObject(j, "model", model_id);

  cJSON_AddBoolToObject(j, "stream", 1);

  cJSON *messages = cJSON_AddArrayToObject(j, "messages");
  cJSON *msg = cJSON_CreateObject();
  if (!msg)
    goto cleanup;
  cJSON_AddStringToObject(msg, "role", "user");
  cJSON_AddStringToObject(msg, "content", prompt);
  cJSON_AddItemToArray(messages, msg);

  json = cJSON_PrintUnformatted(j);
  if (!json)
    goto cleanup;

  curl_easy_setopt(c, CURLOPT_URL, config()->base_url);
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, json);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, _stream_callback_for_sse);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, sse_cb);

  res = curl_easy_perform(c);

  long http = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
  if (res == CURLE_OK && http >= 400)
    res = CURLE_HTTP_RETURNED_ERROR;

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
