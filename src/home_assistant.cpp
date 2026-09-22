#include "home_assistant.h"
#include "app_config.h"
#include "network_service.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

namespace {
TaskHandle_t g_worker=nullptr;volatile bool g_health_requested=false,g_in_progress=false;portMUX_TYPE g_mux=portMUX_INITIALIZER_UNLOCKED;
char g_base_url[160]={},g_token[256]={};HomeAssistantStatus g_status={};uint32_t g_connected_since_ms=0,g_last_health_request_ms=0;
String normalized_base(){String b(g_base_url);b.trim();while(b.endsWith("/"))b.remove(b.length()-1);return b;}
int http_get_api(String &payload){payload="";if(!g_base_url[0]||!g_token[0])return-100;const String url=normalized_base()+"/api/";HTTPClient http;http.setConnectTimeout(HA_HTTP_CONNECT_TIMEOUT_MS);http.setTimeout(HA_HTTP_TIMEOUT_MS);int code=-1;if(url.startsWith("https://")){WiFiClientSecure client;
#if HA_TLS_ALLOW_INSECURE
client.setInsecure();
#endif
if(!http.begin(client,url))return-101;http.addHeader("Authorization",String("Bearer ")+g_token);code=http.GET();if(code>0)payload=http.getString();http.end();}else{WiFiClient client;if(!http.begin(client,url))return-101;http.addHeader("Authorization",String("Bearer ")+g_token);code=http.GET();if(code>0)payload=http.getString();http.end();}return code;}
void load_preferences(){Preferences p;if(!p.begin("panel_ha",true))return;const String u=p.getString("url","");const String t=p.getString("token","");p.end();snprintf(g_base_url,sizeof(g_base_url),"%s",u.c_str());snprintf(g_token,sizeof(g_token),"%s",t.c_str());}
void worker_task(void *){for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);bool do_health=false;portENTER_CRITICAL(&g_mux);if(g_health_requested&&!g_in_progress){g_health_requested=false;g_in_progress=true;g_status.request_in_progress=true;do_health=true;}portEXIT_CRITICAL(&g_mux);if(!do_health)continue;HomeAssistantStatus r={};r.configured=g_base_url[0]&&g_token[0];r.connected=network_service_connected();const uint32_t started=millis();String payload;r.http_code=r.connected?http_get_api(payload):-102;r.latency_ms=millis()-started;r.authenticated=r.http_code>=200&&r.http_code<300;if(r.authenticated){r.last_success_ms=millis();snprintf(r.message,sizeof(r.message),"connected | HTTP %d | %lums",r.http_code,(unsigned long)r.latency_ms);}else if(!r.configured)snprintf(r.message,sizeof(r.message),"not configured");else if(!r.connected)snprintf(r.message,sizeof(r.message),"Wi-Fi offline");else if(r.http_code==401)snprintf(r.message,sizeof(r.message),"token rejected (401)");else snprintf(r.message,sizeof(r.message),"connection error %d",r.http_code);portENTER_CRITICAL(&g_mux);const uint32_t prev=g_status.last_success_ms;g_status=r;if(!g_status.last_success_ms)g_status.last_success_ms=prev;g_in_progress=false;g_status.request_in_progress=false;portEXIT_CRITICAL(&g_mux);delay(HA_HTTP_INTER_REQUEST_GAP_MS);}}
}
void home_assistant_begin(){load_preferences();memset(&g_status,0,sizeof(g_status));g_status.configured=g_base_url[0]&&g_token[0];snprintf(g_status.message,sizeof(g_status.message),"%s",g_status.configured?"configured; waiting for network":"not configured");xTaskCreate(worker_task,"ha_worker",HA_WORKER_STACK_BYTES,nullptr,HA_WORKER_PRIORITY,&g_worker);}
void home_assistant_loop(){const uint32_t now=millis();if(!network_service_connected()){g_connected_since_ms=0;return;}if(!g_connected_since_ms)g_connected_since_ms=now;if(now-g_connected_since_ms<HA_BOOT_NETWORK_STABLE_MS)return;if(g_base_url[0]&&g_token[0]&&(!g_last_health_request_ms||now-g_last_health_request_ms>=HA_HEALTH_INTERVAL_MS)){if(home_assistant_request_health_check())g_last_health_request_ms=now;}}
bool home_assistant_request_health_check(){if(!g_worker||!network_service_connected()||!g_base_url[0]||!g_token[0])return false;bool queued=false;portENTER_CRITICAL(&g_mux);if(!g_health_requested&&!g_in_progress){g_health_requested=true;queued=true;}portEXIT_CRITICAL(&g_mux);if(queued)xTaskNotifyGive(g_worker);return queued;}
void home_assistant_get_status(HomeAssistantStatus &out){portENTER_CRITICAL(&g_mux);out=g_status;out.request_in_progress=g_in_progress||g_health_requested;portEXIT_CRITICAL(&g_mux);}
bool home_assistant_set_credentials(const char *base_url,const char *token){String b=base_url?base_url:"",t=token?token:"";b.trim();t.trim();if(!b.isEmpty()&&!b.startsWith("http://")&&!b.startsWith("https://"))return false;Preferences p;if(!p.begin("panel_ha",false))return false;p.putString("url",b);if(!t.isEmpty())p.putString("token",t);p.end();snprintf(g_base_url,sizeof(g_base_url),"%s",b.c_str());if(!t.isEmpty())snprintf(g_token,sizeof(g_token),"%s",t.c_str());portENTER_CRITICAL(&g_mux);g_status.configured=g_base_url[0]&&g_token[0];portEXIT_CRITICAL(&g_mux);return true;}
String home_assistant_base_url(){return String(g_base_url);}bool home_assistant_token_configured(){return g_token[0]!='\0';}
