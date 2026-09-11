#include <stdlib.h>
#include <string.h>

#include "tool_base.h"

/* 各工具导出的基类实例 */
extern const tool_t web_search_tool;

/* 注册表: 新增工具只需要在这里加一行 */
static const tool_t *const _registry[] = {
    &web_search_tool,
    NULL,
};

cJSON *tools_schema(void) {
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; _registry[i]; i++) {
    cJSON *fn = cJSON_CreateObject();
    cJSON_AddStringToObject(fn, "name", _registry[i]->name);
    cJSON_AddStringToObject(fn, "description", _registry[i]->description);
    cJSON_AddItemToObject(fn, "parameters", _registry[i]->schema());

    cJSON *tool = cJSON_CreateObject();
    cJSON_AddStringToObject(tool, "type", "function");
    cJSON_AddItemToObject(tool, "function", fn);
    cJSON_AddItemToArray(arr, tool);
  }
  return arr;
}

char *tool_invoke(const char *name, const char *arguments) {
  if (!name)
    return strdup("error: missing tool name");
  for (int i = 0; _registry[i]; i++)
    if (strcmp(_registry[i]->name, name) == 0)
      return _registry[i]->run(arguments);
  return strdup("error: unknown tool");
}
