#include "network_service.h"
#include "app_config.h"
#include "config_service.h"
#include <WiFi.h>

#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
#include "esp32-hal-hosted.h"
#endif

namespace {
char g_status[96]="Wi-Fi starting";
bool g_was_connected=false;
uint32_t g_connect_started_ms=0,g_last_retry_ms=0;
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
extern "C" const uint8_t hosted_c6_firmware_start[];
extern "C" const uint8_t hosted_c6_firmware_end[];
constexpr uint32_t BUNDLED_HOSTED_MAJOR=2,BUNDLED_HOSTED_MINOR=12,BUNDLED_HOSTED_PATCH=3;
constexpr size_t HOSTED_UPDATE_CHUNK=2048;
uint32_t version_value(uint32_t a,uint32_t b,uint32_t c){return(a<<16)|(b<<8)|c;}
bool maybe_update_hosted_c6(){
#if HOSTED_C6_AUTO_UPDATE
    if(!hostedIsInitialized())return false;
    uint32_t hm=0,hmi=0,hp=0,sm=0,smi=0,sp=0;hostedGetHostVersion(&hm,&hmi,&hp);hostedGetSlaveVersion(&sm,&smi,&sp);
    const uint32_t hv=version_value(hm,hmi,hp),sv=version_value(sm,smi,sp),bv=version_value(BUNDLED_HOSTED_MAJOR,BUNDLED_HOSTED_MINOR,BUNDLED_HOSTED_PATCH);
    Serial0.printf("[Hosted] Host %lu.%lu.%lu | C6 %lu.%lu.%lu\n",(unsigned long)hm,(unsigned long)hmi,(unsigned long)hp,(unsigned long)sm,(unsigned long)smi,(unsigned long)sp);
    if(hv==sv)return false;if(hv!=bv||sv>hv){Serial0.println("[Hosted] Version mismatch not eligible for automatic update");return false;}
    const size_t image_size=static_cast<size_t>(hosted_c6_firmware_end-hosted_c6_firmware_start);if(image_size<64*1024||image_size>4*1024*1024||hosted_c6_firmware_start[0]!=0xE9)return false;
    if(!hostedBeginUpdate())return false;size_t offset=0;while(offset<image_size){size_t chunk=image_size-offset;if(chunk>HOSTED_UPDATE_CHUNK)chunk=HOSTED_UPDATE_CHUNK;uint8_t *ptr=const_cast<uint8_t *>(hosted_c6_firmware_start+offset);if(!hostedWriteUpdate(ptr,static_cast<uint32_t>(chunk)))return false;offset+=chunk;delay(1);}if(!hostedEndUpdate()||!hostedActivateUpdate())return false;Serial0.println("[Hosted] C6 update complete; restarting P4");Serial0.flush();delay(1500);ESP.restart();return true;
#else
    return false;
#endif
}
#endif
void begin_connection(){
#if defined(CONFIG_ESP_HOSTED_ENABLED) && CONFIG_ESP_HOSTED_ENABLED
    if(!WiFi.setPins(18,19,14,15,16,17,54))Serial0.println("[WiFi] WARNING: WiFi.setPins() reported failure");
#else
    Serial0.println("[WiFi] ERROR: framework lacks ESP-Hosted support");
#endif
    WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(true);
#if defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) && CONFIG_ESP_WIFI_REMOTE_ENABLED
    maybe_update_hosted_c6();
#endif
    WiFi.setHostname(config_service_get().device_id);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);g_connect_started_ms=millis();snprintf(g_status,sizeof(g_status),"Wi-Fi connecting to %s",WIFI_SSID);
}
}
bool network_service_configured(){return strcmp(WIFI_SSID,"YOUR_WIFI_SSID")!=0&&strcmp(WIFI_PASSWORD,"YOUR_WIFI_PASSWORD")!=0;}
void network_service_begin(){if(!network_service_configured()){snprintf(g_status,sizeof(g_status),"Wi-Fi not configured");Serial0.println("[WiFi] Copy app_secrets.example.h to app_secrets.h");return;}begin_connection();}
void network_service_loop(){if(!network_service_configured())return;const bool connected=WiFi.status()==WL_CONNECTED;if(connected&&!g_was_connected){g_was_connected=true;const String ip=WiFi.localIP().toString();snprintf(g_status,sizeof(g_status),"Wi-Fi connected | %s",ip.c_str());Serial0.printf("[WiFi] Connected. IP=%s RSSI=%d dBm\n",ip.c_str(),WiFi.RSSI());configTzTime(APP_TIMEZONE_POSIX,"pool.ntp.org","time.nist.gov");}else if(!connected&&g_was_connected){g_was_connected=false;snprintf(g_status,sizeof(g_status),"Wi-Fi disconnected");g_last_retry_ms=millis();}if(!connected){const uint32_t now=millis();if(now-g_connect_started_ms>20000UL&&now-g_last_retry_ms>15000UL){g_last_retry_ms=now;WiFi.disconnect();WiFi.begin(WIFI_SSID,WIFI_PASSWORD);g_connect_started_ms=now;snprintf(g_status,sizeof(g_status),"Wi-Fi reconnecting");}}}
bool network_service_connected(){return WiFi.status()==WL_CONNECTED;}const char *network_service_status(){return g_status;}String network_service_ip(){return network_service_connected()?WiFi.localIP().toString():String();}int network_service_rssi(){return network_service_connected()?WiFi.RSSI():0;}
