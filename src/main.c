#include <stdio.h>

#include "agent_loop/agent_loop.h"
#include "config.h"
#include "dotenv.h"

int main(int argc, char **argv) {
  if (load_dotenv(".env") != 0) {
    fprintf(stderr, "error: could not load .env\n");
    return 1;
  }

  if (config_init() != 0) {
    fprintf(stderr, "error: could not init config\n");
    return 1;
  }

  /* 命令行首条 prompt (可选): ./agent "question" */
  agent_loop_run(argc > 1 ? argv[1] : NULL);

  config_free();
  return 0;
}
