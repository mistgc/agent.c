#include "dotenv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s) {
  while (isspace((unsigned char)*s))
    s++;
  char *end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1]))
    end--;
  *end = '\0';
  return s;
}

int load_dotenv(const char *path) {
  FILE *fp = fopen(path, "r");
  if (!fp)
    return -1;

  char line[4096];
  while (fgets(line, sizeof line, fp)) {
    char *p = trim(line);
    if (*p == '\0' || *p == '#')
      continue;

    if (strncmp(p, "export ", 7) == 0)
      p += 7;

    char *eq = strchr(p, '=');
    if (!eq)
      continue;

    *eq = '\0';
    char *key = trim(p);
    char *val = trim(eq + 1);

    size_t len = strlen(val);
    if (len >= 2 && ((val[0] == '"' && val[len - 1] == '"') ||
                     (val[0] == '\'' && val[len - 1] == '\''))) {
      val[len - 1] = '\0';
      val++;
    }

    if (*key)
      setenv(key, val, 0);
  }

  fclose(fp);
  return 0;
}
