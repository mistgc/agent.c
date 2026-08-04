#include <stdio.h>
#include <stdlib.h>

#include "call_llm.h"
#include "config.h"
#include "dotenv.h"


int main() {
    if (load_dotenv(".env") != 0) {
        fprintf(stderr, "error: could not load .env\n");
        return -1;
    }

    if (config_init() != 0) {
        fprintf(stderr, "error: could not init config\n");
        return -1;
    }

    config_free();

    return 0;
}
