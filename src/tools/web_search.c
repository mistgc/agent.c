#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "../config.h"
#include "tool_base.h"

struct dynbuf {
  char *p;
  size_t len;
};

static void db_append(struct dynbuf *b, const char *s, size_t n) {
  char *np = realloc(b->p, b->len + n + 1);
  if (!np)
    return;
  memcpy(np + b->len, s, n);
  b->len += n;
  np[b->len] = '\0';
  b->p = np;
}

static void db_puts(struct dynbuf *b, const char *s) {
  db_append(b, s, strlen(s));
}

static size_t _write_cb(char *data, size_t size, size_t nmemb, void *ud) {
  size_t n = size * nmemb;
  db_append(ud, data, n);
  return n;
}

static char *_format(cJSON *root) {
  struct dynbuf b = {0};
  cJSON *answer = cJSON_GetObjectItem(root, "answer");
  if (cJSON_IsString(answer) && answer->valuestring && *answer->valuestring) {
    db_puts(&b, "answer: ");
    db_puts(&b, answer->valuestring);
    db_puts(&b, "\n\n");
  }

  cJSON *results = cJSON_GetObjectItem(root, "results");
  if (cJSON_IsArray(results)) {
    int i = 0;
    cJSON *r;
    cJSON_ArrayForEach(r, results) {
      cJSON *title = cJSON_GetObjectItem(r, "title");
      cJSON *url = cJSON_GetObjectItem(r, "url");
      cJSON *content = cJSON_GetObjectItem(r, "content");
      char head[16];
      snprintf(head, sizeof head, "\n[%d] ", ++i);
      db_puts(&b, head);
      if (cJSON_IsString(title) && title->valuestring)
        db_puts(&b, title->valuestring);
      db_puts(&b, "\n");
      if (cJSON_IsString(url) && url->valuestring) {
        db_puts(&b, url->valuestring);
        db_puts(&b, "\n");
      }
      if (cJSON_IsString(content) && content->valuestring)
        db_puts(&b, content->valuestring);
      db_puts(&b, "\n");
    }
  }

  if (!b.p)
    b.p = strdup("(no results)");
  return b.p;
}

static char *_search(const char *query) {
  const char *key = config()->tavily_api_key;
  if (!key)
    return strdup("error: TAVILY_API_KEY not set");

  char *result = strdup("error: search failed");
  CURL *c = NULL;
  struct curl_slist *headers = NULL;
  cJSON *req = NULL;
  char *body = NULL, *auth = NULL;
  struct dynbuf resp = {0};

  c = curl_easy_init();
  if (!c)
    goto done;

  req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "query", query);
  cJSON_AddBoolToObject(req, "include_answer", 1);
  cJSON_AddNumberToObject(req, "max_results", 5);
  body = cJSON_PrintUnformatted(req);
  if (!body)
    goto done;

  size_t auth_len = strlen(key) + 32;
  auth = malloc(auth_len);
  snprintf(auth, auth_len, "Authorization: Bearer %s", key);

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, auth);

  curl_easy_setopt(c, CURLOPT_URL, "https://api.tavily.com/search");
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, _write_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);

  CURLcode res = curl_easy_perform(c);
  long http = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);

  if (res != CURLE_OK) {
    free(result);
    size_t n = strlen(curl_easy_strerror(res)) + 16;
    result = malloc(n);
    snprintf(result, n, "error: %s", curl_easy_strerror(res));
    goto done;
  }

  cJSON *root = resp.p ? cJSON_Parse(resp.p) : NULL;
  free(result);
  if (http >= 400 || !root) {
    cJSON *detail = root ? cJSON_GetObjectItem(root, "detail") : NULL;
    cJSON *err = detail ? cJSON_GetObjectItem(detail, "error") : NULL;
    const char *msg = cJSON_IsString(err) ? err->valuestring : "request failed";
    size_t n = strlen(msg) + 32;
    result = malloc(n);
    snprintf(result, n, "error: tavily http %ld: %s", http, msg);
    if (root)
      cJSON_Delete(root);
    goto done;
  }

  result = _format(root);
  cJSON_Delete(root);

done:
  free(resp.p);
  free(auth);
  if (body)
    free(body);
  if (req)
    cJSON_Delete(req);
  if (headers)
    curl_slist_free_all(headers);
  if (c)
    curl_easy_cleanup(c);
  return result;
}

static char *web_search_run(const char *arguments) {
  cJSON *args = arguments ? cJSON_Parse(arguments) : NULL;
  cJSON *q = args ? cJSON_GetObjectItem(args, "query") : NULL;
  char *result;
  if (cJSON_IsString(q) && q->valuestring)
    result = _search(q->valuestring);
  else
    result = strdup("error: missing query argument");
  if (args)
    cJSON_Delete(args);
  return result;
}

static cJSON *web_search_schema(void) {
  cJSON *params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "type", "object");
  cJSON *props = cJSON_AddObjectToObject(params, "properties");
  cJSON *query = cJSON_AddObjectToObject(props, "query");
  cJSON_AddStringToObject(query, "type", "string");
  cJSON_AddStringToObject(query, "description", "The search query.");
  cJSON *required = cJSON_AddArrayToObject(params, "required");
  cJSON_AddItemToArray(required, cJSON_CreateString("query"));
  return params;
}

const tool_t web_search_tool = {
    .name = "web_search",
    .description = "Search the web for up-to-date information. Use when the "
                   "answer needs current or external facts.",
    .schema = web_search_schema,
    .run = web_search_run,
};
