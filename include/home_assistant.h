#pragma once
#include <Arduino.h>
#include <stdint.h>

struct HomeAssistantStatus {
    bool configured;
    bool connected;
    bool authenticated;
    bool request_in_progress;
    int http_code;
    uint32_t latency_ms;
    uint32_t last_success_ms;
    char message[96];
};

void home_assistant_begin();
void home_assistant_loop();
bool home_assistant_request_health_check();
void home_assistant_get_status(HomeAssistantStatus &out);
bool home_assistant_set_credentials(const char *base_url, const char *token);
String home_assistant_base_url();
bool home_assistant_token_configured();
