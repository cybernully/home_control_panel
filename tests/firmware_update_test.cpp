#include "firmware_update.h"
#include "app_config.h"
#include <Arduino.h>
#include <Update.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <vector>
uint32_t fake_now = 0;
unsigned fake_delays = 0;
FakeSerial Serial0;
FakeUpdate Update;

int main(int argc, char **argv) {
    assert(argc == 2);
    const char *test = argv[1];
    uint8_t data[1436] = {0xE9};
    if (!strcmp(test, "size")) {
        assert(!firmware_update_begin(0));
        assert(!firmware_update_begin(WEB_OTA_MAX_BYTES + 1));
        assert(!Update.running && !Update.activated);
    } else if (!strcmp(test, "begin_failure")) {
        Update.begin_ok = false;
        assert(!firmware_update_begin(4096));
        assert(strstr(firmware_update_status().error, "inactive"));
    } else if (!strcmp(test, "busy")) {
        assert(firmware_update_begin(4096));
        assert(!firmware_update_begin(8192));
        assert(firmware_update_status().expected == 4096);
    } else if (!strcmp(test, "truncated")) {
        assert(firmware_update_begin(4096));
        assert(firmware_update_write(data, sizeof(data)));
        assert(!firmware_update_finish());
        assert(Update.aborts == 1 && Update.ends == 0 && !Update.activated);
    } else if (!strcmp(test, "overflow") || !strcmp(test, "magic") || !strcmp(test, "write_failure")) {
        assert(firmware_update_begin(1000));
        size_t size = 1000;
        if (!strcmp(test, "overflow")) size = 1001;
        if (!strcmp(test, "magic")) data[0] = 0;
        if (!strcmp(test, "write_failure")) Update.write_ok = false;
        assert(!firmware_update_write(data, size));
        assert(!firmware_update_status().active && Update.aborts == 1 && !Update.activated);
    } else if (!strcmp(test, "verification_failure")) {
        assert(firmware_update_begin(1000));
        assert(firmware_update_write(data, 1000));
        Update.end_ok = false;
        assert(!firmware_update_finish());
        assert(!firmware_update_status().verified && !Update.activated && Update.aborts == 1);
    } else if (!strcmp(test, "abort")) {
        assert(firmware_update_begin(1000));
        assert(firmware_update_write(data, 1000));
        firmware_update_abort("connection closed");
        assert(!firmware_update_finish() && Update.ends == 0 && Update.aborts == 1);
        assert(firmware_update_begin(1000)); // Failed sessions release the inactive partition.
    } else if (!strcmp(test, "timeout")) {
        fake_now = UINT32_MAX - 100;
        assert(firmware_update_begin(1000));
        const uint32_t start = fake_now;
        firmware_update_expire(start + WEB_OTA_IDLE_TIMEOUT_MS - 1);
        assert(firmware_update_status().active);
        firmware_update_expire(start + WEB_OTA_IDLE_TIMEOUT_MS);
        assert(!firmware_update_status().active && !Update.activated);
    } else if (!strcmp(test, "success") || !strcmp(test, "legacy")) {
        const size_t total = 3625600; // Full release-sized image, not a tiny fixture.
        const bool legacy = !strcmp(test, "legacy");
        assert(legacy ? firmware_update_begin_legacy() : firmware_update_begin(total));
        for (size_t offset = 0; offset < total;) {
            const size_t count = total - offset < sizeof(data) ? total - offset : sizeof(data);
            const unsigned delays = fake_delays;
            assert(firmware_update_write(data, count));
            assert(fake_delays >= delays + 2);
            offset += count;
            assert(firmware_update_status().written == offset && !Update.activated);
        }
        assert(firmware_update_finish() && Update.activated && Update.allow_remaining == legacy);
        assert(firmware_update_status().verified);
        firmware_update_abort("late connection loss");
        firmware_update_expire(fake_now + WEB_OTA_IDLE_TIMEOUT_MS);
        assert(Update.aborts == 0 && firmware_update_status().verified);
        assert(!firmware_update_begin(1000));
    } else assert(false);
    printf("OTA lifecycle passed: %s\n", test);
}
