#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <algorithm>
#include <cstring>

namespace {
constexpr const char *TAG="HomePanel";
constexpr uint32_t PSRAM_CAPS=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
constexpr uint32_t INTERNAL_CAPS=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
void log_failure(const char *op,size_t size){
    ESP_LOGE(TAG,"[LVGL memory] %s failed for %u bytes; PSRAM free=%u largest=%u, internal free=%u largest=%u",op,static_cast<unsigned>(size),static_cast<unsigned>(heap_caps_get_free_size(PSRAM_CAPS)),static_cast<unsigned>(heap_caps_get_largest_free_block(PSRAM_CAPS)),static_cast<unsigned>(heap_caps_get_free_size(INTERNAL_CAPS)),static_cast<unsigned>(heap_caps_get_largest_free_block(INTERNAL_CAPS)));
}
}
extern "C" {
void lv_mem_init(void){}
void lv_mem_deinit(void){}
void *lv_malloc_core(size_t size){void *p=heap_caps_malloc(size,PSRAM_CAPS);if(!p)p=heap_caps_malloc(size,MALLOC_CAP_8BIT);if(!p)log_failure("malloc",size);return p;}
void lv_free_core(void *p){if(p)heap_caps_free(p);}
void *lv_realloc_core(void *p,size_t size){if(!p)return lv_malloc_core(size);void *r=heap_caps_realloc(p,size,PSRAM_CAPS);if(!r)r=heap_caps_realloc(p,size,MALLOC_CAP_8BIT);if(!r)log_failure("realloc",size);return r;}
void lv_mem_monitor_core(lv_mem_monitor_t *m){if(!m)return;std::memset(m,0,sizeof(*m));const size_t pt=heap_caps_get_total_size(PSRAM_CAPS),pf=heap_caps_get_free_size(PSRAM_CAPS),pl=heap_caps_get_largest_free_block(PSRAM_CAPS),it=heap_caps_get_total_size(INTERNAL_CAPS),inf=heap_caps_get_free_size(INTERNAL_CAPS),il=heap_caps_get_largest_free_block(INTERNAL_CAPS);m->total_size=pt+it;m->free_size=pf+inf;m->free_biggest_size=std::max(pl,il);if(m->total_size)m->used_pct=static_cast<uint8_t>(((m->total_size-m->free_size)*100U)/m->total_size);if(m->free_size){const size_t bp=(m->free_biggest_size*100U)/m->free_size;m->frag_pct=static_cast<uint8_t>(bp>=100U?0U:100U-bp);}}
lv_result_t lv_mem_test_core(void){return LV_RESULT_OK;}
}
