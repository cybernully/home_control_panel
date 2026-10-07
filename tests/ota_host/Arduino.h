#pragma once
#include <stdint.h>
#include <stddef.h>
extern uint32_t fake_now;
extern unsigned fake_delays;
inline uint32_t millis() { return fake_now; }
inline void delay(uint32_t ms) { fake_now += ms; ++fake_delays; }
struct FakeSerial { template <typename... T> void printf(const char *, T...) {} };
extern FakeSerial Serial0;
