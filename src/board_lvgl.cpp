#include "board_lvgl.h"
#include <lvgl.h>
#include <chipguy_JC8012P4A1C_display.h>
#include <esp_lcd_panel_dpi_bb.h>
#include <esp_cache.h>
#include <cstring>

static chipguy_JC8012P4A1C_display g_display;
static uint8_t *g_draw_buf=nullptr;
static uint8_t g_target_fb=1;
static uint32_t g_last_diag_ms=0;
static volatile bool g_touch_activity=false,g_display_awake=true,g_touch_pressed=false,g_touch_position_valid=false;
static volatile int16_t g_touch_x=0,g_touch_y=0;
static bool g_swallow_touch_until_release=false;
static uint32_t g_last_underrun_count=0;
static constexpr int NATIVE_W=800,NATIVE_H=1280;
static constexpr size_t NATIVE_FB_BYTES=static_cast<size_t>(NATIVE_W)*NATIVE_H*sizeof(uint16_t);
static uint32_t tick_cb(){return millis();}

static void rotate_exact(const uint16_t *src,uint16_t *dst,uint8_t rotation){
    if(!src||!dst)return;
    switch(rotation){
        case 0: std::memcpy(dst,src,NATIVE_FB_BYTES); break;
        case 1:{constexpr int SW=1280,SH=800,TILE=16;for(int sy0=0;sy0<SH;sy0+=TILE){const int sye=(sy0+TILE<SH)?sy0+TILE:SH;for(int sx0=0;sx0<SW;sx0+=TILE){const int sxe=(sx0+TILE<SW)?sx0+TILE:SW;for(int sy=sy0;sy<sye;++sy){const uint16_t *s=src+static_cast<size_t>(sy)*SW+sx0;for(int sx=sx0;sx<sxe;++sx,++s){const int dx=sy,dy=(SW-1)-sx;dst[static_cast<size_t>(dy)*NATIVE_W+dx]=*s;}}}}break;}
        case 2:{const size_t n=static_cast<size_t>(NATIVE_W)*NATIVE_H;for(size_t i=0;i<n;++i)dst[n-1-i]=src[i];break;}
        case 3:{constexpr int SW=1280,SH=800,TILE=16;for(int sy0=0;sy0<SH;sy0+=TILE){const int sye=(sy0+TILE<SH)?sy0+TILE:SH;for(int sx0=0;sx0<SW;sx0+=TILE){const int sxe=(sx0+TILE<SW)?sx0+TILE:SW;for(int sy=sy0;sy<sye;++sy){const uint16_t *s=src+static_cast<size_t>(sy)*SW+sx0;for(int sx=sx0;sx<sxe;++sx,++s){const int dx=(SH-1)-sy,dy=sx;dst[static_cast<size_t>(dy)*NATIVE_W+dx]=*s;}}}}break;}
    }
}
static void flush_cb(lv_display_t *disp,const lv_area_t *,uint8_t *pixelmap){if(lv_display_flush_is_last(disp)){uint16_t *target=g_display.getFramebuffer(g_target_fb);if(target){rotate_exact(reinterpret_cast<const uint16_t *>(pixelmap),target,g_display.rotation());const esp_err_t r=esp_cache_msync(target,NATIVE_FB_BYTES,ESP_CACHE_MSYNC_FLAG_DIR_C2M);if(r!=ESP_OK)Serial0.printf("Framebuffer cache sync failed: %d\n",static_cast<int>(r));g_display.setActiveFramebuffer(g_target_fb,true);g_target_fb^=1;}}lv_display_flush_ready(disp);}
static void touch_cb(lv_indev_t *,lv_indev_data_t *data){uint16_t rx=0,ry=0;const bool touched=g_display.getTouchDriver().getTouch(&rx,&ry);if(!touched){g_touch_pressed=false;if(g_swallow_touch_until_release)g_swallow_touch_until_release=false;data->state=LV_INDEV_STATE_RELEASED;return;}g_touch_activity=true;if(!g_display_awake||g_swallow_touch_until_release){g_touch_pressed=false;g_swallow_touch_until_release=true;data->state=LV_INDEV_STATE_RELEASED;return;}int32_t x=0,y=0;if(!g_display.mapTouch(rx,ry,x,y)){g_touch_pressed=false;data->state=LV_INDEV_STATE_RELEASED;return;}g_touch_x=static_cast<int16_t>(x);g_touch_y=static_cast<int16_t>(y);g_touch_position_valid=true;g_touch_pressed=true;data->point.x=x;data->point.y=y;data->state=LV_INDEV_STATE_PRESSED;}

bool board_lvgl_begin(uint16_t rotation){if(!g_display.begin(rotation)){Serial0.println("ERROR: display/touch initialization failed");return false;}g_draw_buf=reinterpret_cast<uint8_t *>(g_display.getDrawBuffer(0));if(!g_draw_buf){Serial0.println("ERROR: draw buffer unavailable");return false;}lv_init();lv_tick_set_cb(tick_cb);lv_display_t *disp=lv_display_create(g_display.width(),g_display.height());lv_display_set_color_format(disp,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(disp,flush_cb);lv_display_set_buffers(disp,g_draw_buf,nullptr,g_display.framebufferSize(),LV_DISPLAY_RENDER_MODE_DIRECT);lv_indev_t *indev=lv_indev_create();lv_indev_set_type(indev,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(indev,touch_cb);g_last_underrun_count=chipguy_lcd_dpi_panel_get_underrun_count();Serial0.printf("Display ready: %dx%d rotation=%u\n",g_display.width(),g_display.height(),g_display.rotation());return true;}
void board_lvgl_loop(){lv_timer_handler();const uint32_t now=millis();if(now-g_last_diag_ms>=5000){g_last_diag_ms=now;const uint32_t u=chipguy_lcd_dpi_panel_get_underrun_count();if(u!=g_last_underrun_count){Serial0.printf("WARNING: LCD DMA underruns: %lu -> %lu\n",static_cast<unsigned long>(g_last_underrun_count),static_cast<unsigned long>(u));g_last_underrun_count=u;}}}
void board_set_backlight(uint8_t p){if(p>100)p=100;g_display.setBacklight(p);}void board_set_display_awake(bool a,uint8_t r){if(r>100)r=100;g_display_awake=a;g_display.setBacklight(a?r:0);}bool board_display_awake(){return g_display_awake;}bool board_take_touch_activity(){const bool a=g_touch_activity;g_touch_activity=false;return a;}bool board_get_touch_state(int16_t &x,int16_t &y,bool &pressed){if(!g_touch_position_valid){pressed=false;return false;}x=g_touch_x;y=g_touch_y;pressed=g_touch_pressed;return true;}int16_t board_width(){return g_display.width();}int16_t board_height(){return g_display.height();}
