#include "battery_service.h"
#include "app_config.h"
#include <Arduino.h>
#include <math.h>

namespace {
struct ChargePoint { uint16_t mv; uint8_t percent; };
constexpr ChargePoint CURVE[] = {{3500,0},{3550,10},{3650,20},{3700,30},{3740,40},{3790,50},{3850,60},{3920,70},{4000,80},{4100,90},{4200,100}};
BatteryStatus g_status = {};
float g_filtered_adc_mv = NAN;
uint32_t g_last_sample_ms = 0;

uint8_t voltage_percent(uint16_t battery_mv) {
    constexpr size_t count = sizeof(CURVE)/sizeof(CURVE[0]);
    if (battery_mv <= CURVE[0].mv) return 0;
    if (battery_mv >= CURVE[count-1].mv) return 100;
    for (size_t i=1;i<count;++i) {
        if (battery_mv > CURVE[i].mv) continue;
        const uint32_t span_mv = CURVE[i].mv - CURVE[i-1].mv;
        const uint32_t into = battery_mv - CURVE[i-1].mv;
        const uint32_t span_pct = CURVE[i].percent - CURVE[i-1].percent;
        return static_cast<uint8_t>(CURVE[i-1].percent + (into*span_pct + span_mv/2U)/span_mv);
    }
    return 0;
}

uint16_t read_trimmed_mv() {
    constexpr size_t count = BATTERY_ADC_SAMPLE_COUNT;
    uint32_t s[count] = {};
    (void)analogReadMilliVolts(BATTERY_ADC_PIN); delayMicroseconds(250);
    for (size_t i=0;i<count;++i) { s[i]=analogReadMilliVolts(BATTERY_ADC_PIN); delayMicroseconds(250); }
    for (size_t i=1;i<count;++i) { uint32_t v=s[i]; size_t j=i; while(j>0 && s[j-1]>v){s[j]=s[j-1];--j;} s[j]=v; }
    constexpr size_t trim=2; uint64_t total=0;
    for (size_t i=trim;i<count-trim;++i) total+=s[i];
    const size_t kept=count-trim*2;
    return static_cast<uint16_t>((total+kept/2U)/kept);
}

void sample(bool initial) {
    const uint16_t raw=read_trimmed_mv();
    if (initial || !isfinite(g_filtered_adc_mv)) g_filtered_adc_mv=raw;
    else g_filtered_adc_mv=g_filtered_adc_mv*0.75f+raw*0.25f;
    const uint16_t adc=static_cast<uint16_t>(lroundf(g_filtered_adc_mv));
    const float v=(adc/1000.0f)*BATTERY_VOLTAGE_MULTIPLIER;
    const uint16_t mv=static_cast<uint16_t>(lroundf(v*1000.0f));
    g_status.valid=mv>=BATTERY_VALID_MIN_MV && mv<=BATTERY_VALID_MAX_MV;
    g_status.adc_mv=adc; g_status.voltage_v=v; g_status.percent=g_status.valid?voltage_percent(mv):0; g_status.sampled_ms=millis(); g_last_sample_ms=g_status.sampled_ms;
}
}

void battery_service_begin(){ pinMode(BATTERY_ADC_PIN,INPUT); analogReadResolution(12); analogSetPinAttenuation(BATTERY_ADC_PIN,ADC_11db); sample(true); }
void battery_service_loop(){ const uint32_t now=millis(); if(g_last_sample_ms && now-g_last_sample_ms<BATTERY_SAMPLE_INTERVAL_MS)return; sample(false); }
bool battery_service_get_status(BatteryStatus &out){ out=g_status; return g_status.valid; }
