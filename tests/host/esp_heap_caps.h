#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
inline void *heap_caps_malloc(size_t size, unsigned) { return malloc(size); }
inline void *heap_caps_calloc(size_t count, size_t size, unsigned) { return calloc(count, size); }
inline void *heap_caps_realloc(void *ptr, size_t size, unsigned) { return realloc(ptr, size); }
