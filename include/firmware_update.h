#pragma once
#include <stddef.h>
#include <stdint.h>

struct FirmwareUpdateStatus {
    bool active;
    bool verified;
    size_t expected;
    size_t written;
    uint32_t last_activity_ms;
    char error[160];
};

const FirmwareUpdateStatus &firmware_update_status();
bool firmware_update_begin(size_t size);
bool firmware_update_begin_legacy();
bool firmware_update_write(uint8_t *data, size_t size);
bool firmware_update_finish();
void firmware_update_abort(const char *reason);
void firmware_update_expire(uint32_t now);
