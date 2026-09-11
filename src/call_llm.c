#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "config.h"
#include "call_llm.h"

static char _sse_buf[65536];
static size_t _sse_len = 0;

static void _strapp(char **dst, const char *src) {
  if (!src)
    return;
  size_t a = *dst ? strlen(*dst) : 0;
  size_t b = strlen(src);
  char *np = realloc(*dst, a + b + 1);
  if (!np)
    return;
  memcpy(np + a, src, b + 1);
  *dst = np;
}

struct _sse_ctx {
  chat_result_t *out;
  chat_delta_cb delta;
};

static void _on_line(struct _sse_ctx *ctx, const char *line, size_t len) {
  if (len < 5 || strncmp(line, "data:", 5) != 0)
    return;
  line += 5;
  len -= 5;
  while (len > 0 && (*line == ' ' || *line == '\t')) {
    line++;
    len--;
  }

  if (len == 6 && memcmp(line, "[DONE]", 6) == 0)
    return;

  cJSON *root = cJSON_ParseWithLength(line, len);
  if (!root)
    return;

  cJSON *choice = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "choices"), 0);
  if (choice) {
    cJSON *delta = cJSON_GetObjectItem(choice, "delta");

    cJSON *content = delta ? cJSON_GetObjectItem(delta, "content") : NULL;
    if (cJSON_IsString(content) && content->valuestring) {
      _strapp(&ctx->out->content, content->valuestring);
      if (ctx->delta)
        ctx->delta(content->valuestring, strlen(content->valuestring));
    }

    /* tool_calls 的 arguments 会跨多个分片, 按 index 累积拼接 */
    cJSON *tcs = delta ? cJSON_GetObjectItem(delta, "tool_calls") : NULL;
    if (cJSON_IsArray(tcs)) {
      cJSON *tc;
      cJSON_ArrayForEach(tc, tcs) {
        cJSON *idx = cJSON_GetObjectItem(tc, "index");
        int i = cJSON_IsNumber(idx) ? idx->valueint : ctx->out->n_tool_calls;
        if (i < 0 || i >= CHAT_MAX_TOOL_CALLS)
          continue;
        if (i + 1 > ctx->out->n_tool_calls)
          ctx->out->n_tool_calls = i + 1;

        chat_tool_call_t *c = &ctx->out->tool_calls[i];
        cJSON *id = cJSON_GetObjectItem(tc, "id");
        if (cJSON_IsString(id))
          _strapp(&c->id, id->valuestring);
        cJSON *fn = cJSON_GetObjectItem(tc, "function");
        if (fn) {
          cJSON *name = cJSON_GetObjectItem(fn, "name");
          if (cJSON_IsString(name))
            _strapp(&c->name, name->valuestring);
          cJSON *args = cJSON_GetObjectItem(fn, "arguments");
          if (cJSON_IsString(args))
            _strapp(&c->arguments, args->valuestring);
        }
      }
    }

    cJSON *fr = cJSON_GetObjectItem(choice, "finish_reason");
    if (cJSON_IsString(fr) && fr->valuestring)
      snprintf(ctx->out->finish_reason, sizeof ctx->out->finish_reason, "%s",
               fr->valuestring);
  }

  cJSON_Delete(root);
}

static size_t _stream_callback_for_sse(char *data, size_t size, size_t nmemb,
                                       void *ud) {
  size_t realsize = size * nmemb;
  struct _sse_ctx *ctx = ud;

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
    _on_line(ctx, start, line_len);

    start = nl + 1;
  }

  size_t rest = _sse_len - (size_t)(start - _sse_buf);
  if (rest > 0 && start != _sse_buf)
    memmove(_sse_buf, start, rest);
  _sse_len = rest;
  _sse_buf[_sse_len] = '\0';

  return realsize;
}

int chat_complete_stream(const cJSON *messages, const cJSON *tools,
                         chat_delta_cb on_delta, chat_result_t *out) {
  if (!config()->api_key || !config()->base_url || !config()->model_id ||
      !messages || !out)
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
  cJSON_AddStringToObject(j, "model", config()->model_id);
  cJSON_AddBoolToObject(j, "stream", 1);
  cJSON_AddItemToObject(j, "messages", cJSON_Duplicate(messages, 1));
  if (tools)
    cJSON_AddItemToObject(j, "tools", cJSON_Duplicate(tools, 1));

  json = cJSON_PrintUnformatted(j);
  if (!json)
    goto cleanup;

  struct _sse_ctx ctx = {.out = out, .delta = on_delta};

  curl_easy_setopt(c, CURLOPT_URL, config()->base_url);
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, json);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, _stream_callback_for_sse);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &ctx);

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

void chat_result_free(chat_result_t *r) {
  if (!r)
    return;
  free(r->content);
  for (int i = 0; i < r->n_tool_calls; i++) {
    free(r->tool_calls[i].id);
    free(r->tool_calls[i].name);
    free(r->tool_calls[i].arguments);
  }
  memset(r, 0, sizeof *r);
}
