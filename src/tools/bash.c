#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>

#include <cjson/cJSON.h>

#include "tool_base.h"

/* 用单引号包裹 s, 内部单引号转义为 '\'', 使任意命令能安全传给 bash -c */
static char *shquote(const char *s) {
  size_t n = strlen(s) + 3;
  for (const char *p = s; *p; p++)
    if (*p == '\'')
      n += 3;

  char *out = malloc(n);
  char *w = out;
  *w++ = '\'';
  for (const char *p = s; *p; p++) {
    if (*p == '\'') {
      memcpy(w, "'\\''", 4);
      w += 4;
    } else {
      *w++ = *p;
    }
  }
  *w++ = '\'';
  *w = '\0';
  return out;
}

/* 执行 bash -c <command>, 合并 stderr, 返回 stdout+stderr */
static char *bash_run(const char *arguments) {
  cJSON *args = arguments ? cJSON_Parse(arguments) : NULL;
  cJSON *cmd = args ? cJSON_GetObjectItem(args, "command") : NULL;
  char *result = NULL;

  if (cJSON_IsString(cmd) && cmd->valuestring) {
    char *q = shquote(cmd->valuestring);
    size_t n = strlen(q) + 32;
    char *full = malloc(n);
    snprintf(full, n, "bash -c %s 2>&1", q);
    free(q);

    FILE *p = popen(full, "r");
    free(full);
    if (!p) {
      result = strdup("error: popen failed");
    } else {
      size_t len = 0;
      FILE *mem = open_memstream(&result, &len);
      char buf[8192];
      size_t r;
      while (mem && (r = fread(buf, 1, sizeof buf, p)) > 0)
        fwrite(buf, 1, r, mem);
      int status = pclose(p);
      if (mem) {
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
          char tail[32];
          snprintf(tail, sizeof tail, "\n[exit %d]", WEXITSTATUS(status));
          fputs(tail, mem);
        }
        fclose(mem);
      }
      if (!result)
        result = strdup("error: no memory");
    }
  } else {
    result = strdup("error: missing command argument");
  }

  if (args)
    cJSON_Delete(args);
  return result;
}

static cJSON *bash_schema(void) {
  cJSON *params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "type", "object");
  cJSON *props = cJSON_AddObjectToObject(params, "properties");
  cJSON *command = cJSON_AddObjectToObject(props, "command");
  cJSON_AddStringToObject(command, "type", "string");
  cJSON_AddStringToObject(command, "description",
                          "The shell command to run. Prefer non-interactive.");
  cJSON *required = cJSON_AddArrayToObject(params, "required");
  cJSON_AddItemToArray(required, cJSON_CreateString("command"));
  return params;
}

/* ponytail: 每次都开新 shell, cwd/环境变量不跨调用保留; 需要持久状态时再加长驻进程 */
const tool_t bash_tool = {
    .name = "bash",
    .description = "Run a shell command with bash and return its combined "
                   "stdout and stderr, plus the exit code on failure. Use for "
                   "files, git, building, and running programs.",
    .schema = bash_schema,
    .run = bash_run,
};
