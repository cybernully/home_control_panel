#include <esp_heap_caps.h>
#include <stdlib.h>

namespace {

void *artwork_stbi_malloc(size_t size) {
    void *memory = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) memory = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    return memory;
}

void *artwork_stbi_realloc(void *memory, size_t size) {
    void *resized = heap_caps_realloc(memory, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!resized) resized = heap_caps_realloc(memory, size, MALLOC_CAP_8BIT);
    return resized;
}

}  // namespace

#define STBI_MALLOC(size) artwork_stbi_malloc(size)
#define STBI_REALLOC(memory, size) artwork_stbi_realloc(memory, size)
#define STBI_FREE(memory) free(memory)
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
