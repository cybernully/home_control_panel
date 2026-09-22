#pragma once
#include <stdint.h>
struct BatteryStatus {
    bool valid;
    uint16_t adc_mv;
    float voltage_v;
    uint8_t percent;
    uint32_t sampled_ms;
};
void battery_service_begin();
void battery_service_loop();
bool battery_service_get_status(BatteryStatus &out);
