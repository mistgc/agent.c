/*
* MIT License
 *
 * Copyright (c) 2025 LunaStev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "openai.h"
#include "openai_internal.h"

char* api_key = NULL;
char* base_url = NULL;

void openai_init(const char* key, const char* url) {
    if (api_key) free(api_key);
    api_key = key ? strdup(key) : NULL;

    if (base_url) free(base_url);
    if (!url || strlen(url) == 0) {
        base_url = strdup("https://api.openai.com/v1/chat/completions");
    } else {
        base_url = strdup(url);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void openai_cleanup() {
    if (api_key) free(api_key);
    api_key = NULL;
    if (base_url) free(base_url);
    base_url = NULL;
    curl_global_cleanup();
}

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct memory* mem = (struct memory*)userp;

    char* ptr = realloc(mem->response, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}
