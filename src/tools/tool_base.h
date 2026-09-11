#ifndef TOOL_BASE_H
#define TOOL_BASE_H

#include <cjson/cJSON.h>

typedef struct tool {
  const char *name;
  const char *description;
  cJSON *(*schema)(void);
  char *(*run)(const char *arguments);
} tool_t;

cJSON *tools_schema(void);

char *tool_invoke(const char *name, const char *arguments);

#endif /* TOOL_BASE_H */
