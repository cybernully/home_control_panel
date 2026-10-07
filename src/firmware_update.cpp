#include "firmware_update.h"
#include "app_config.h"
#include <Arduino.h>
#include <Update.h>
#include <stdio.h>

namespace {
FirmwareUpdateStatus status = {};

bool fail(const char *message, bool abort = true) {
    if (abort && status.active) Update.abort();
    status.active = false;
    snprintf(status.error, sizeof(status.error), "%s", message);
    return false;
}

bool begin(size_t size, bool legacy) {
    if (status.active || status.verified) return false;
    status = {};
    if ((!legacy && !size) || size > WEB_OTA_MAX_BYTES)
        return fail("Firmware must be non-empty and no larger than the 6 MB application partition.", false);
    if (!Update.begin(legacy ? UPDATE_SIZE_UNKNOWN : size, U_FLASH)) {
        char error[160];
        snprintf(error, sizeof(error), "Could not open the inactive firmware partition: %s (error %u).",
                 Update.errorString(), static_cast<unsigned>(Update.getError()));
        return fail(error, false);
    }
    status.active = true;
    status.expected = legacy ? 0 : size;
    status.last_activity_ms = millis();
    return true;
}
}

const FirmwareUpdateStatus &firmware_update_status() { return status; }
bool firmware_update_begin(size_t size) { return begin(size, false); }
bool firmware_update_begin_legacy() { return begin(0, true); }

bool firmware_update_write(uint8_t *data, size_t size) {
    // yield() alone need not schedule lower-priority tasks. A real RTOS delay
    // gives hosted Wi-Fi and idle tasks time even with a continuously full TCP buffer.
    delay(1);
    if (!status.active || status.verified) return false;
    const size_t limit = status.expected ? status.expected : WEB_OTA_MAX_BYTES;
    if (!size || !data || size > limit - status.written)
        return fail("Firmware data exceeds the declared image size or application partition.");
    if (!status.written && data[0] != 0xE9)
        return fail("This file is not an ESP application firmware image.");
    const size_t written = Update.write(data, size);
    delay(1);
    if (written != size) {
        char error[160];
        snprintf(error, sizeof(error), "Firmware write failed after %u bytes: %s (error %u).",
                 static_cast<unsigned>(status.written), Update.errorString(),
                 static_cast<unsigned>(Update.getError()));
        return fail(error);
    }
    status.written += written;
    status.last_activity_ms = millis();
    return true;
}

bool firmware_update_finish() {
    if (!status.active || status.verified) return false;
    if (!status.written || (status.expected && status.written != status.expected))
        return fail("Firmware upload is incomplete; the image has not been activated.");
    delay(1);
    // Only the compatibility endpoint permits an initially unknown size.
    if (!Update.end(status.expected == 0)) {
        char error[160];
        snprintf(error, sizeof(error), "Firmware validation failed: %s (error %u).",
                 Update.errorString(), static_cast<unsigned>(Update.getError()));
        return fail(error);
    }
    status.active = false;
    status.verified = true;
    status.last_activity_ms = millis();
    Serial0.printf("[Web] OTA image verified: %u bytes\n", static_cast<unsigned>(status.written));
    return true;
}

void firmware_update_abort(const char *reason) {
    if (status.active && !status.verified) fail(reason);
}

void firmware_update_expire(uint32_t now) {
    if (status.active && now - status.last_activity_ms >= WEB_OTA_IDLE_TIMEOUT_MS)
        firmware_update_abort("Firmware upload timed out between chunks; the image has not been activated.");
}
