#include "firmware_update.h"
#include "app_config.h"
#include <Arduino.h>
#include <Update.h>
#include <algorithm>
#include <assert.h>
#include <map>
#include <string>
#include <string.h>
#include <variant>
#include <stdio.h>
struct String : std::string {
    using std::string::string;
    String(const std::string &text) : std::string(text) {}
    void toLowerCase() { for (char &c : *this) c = static_cast<char>(tolower(c)); }
    bool endsWith(const char *suffix) const { return size() >= strlen(suffix) && compare(size()-strlen(suffix),strlen(suffix),suffix)==0; }
};
struct JsonDocument {
    struct Value { template<typename T> void operator=(const T &) {} };
    Value operator[](const char *) { return {}; }
};
enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status = UPLOAD_FILE_START; String filename = "firmware.bin"; uint8_t *buf = nullptr; size_t currentSize = 0; };
struct Server {
    bool authorized = true;
    std::map<std::string, String> args;
    HTTPUpload data;
    int contentLength = 2000, code = 0;
    std::string error;
    bool authenticate(const char *, const char *) { return authorized; }
    String arg(const char *name) { return args[name]; }
    HTTPUpload &upload() { return data; }
    int clientContentLength() { return contentLength; }
    void sendHeader(const char *,const char *) {}
} g_server;
uint32_t fake_now = 0, g_reboot_at_ms = 0;
unsigned fake_delays = 0;
FakeSerial Serial0;
FakeUpdate Update;
char g_boot_id[17] = "test-boot";
char g_ota_session[17] = {};
#undef WEB_MANAGER_USER
#undef WEB_MANAGER_PASSWORD
#define WEB_MANAGER_USER "admin"
#define WEB_MANAGER_PASSWORD "test"
uint32_t esp_random() { return 123456; }
void send_error(int code, const char *message) { g_server.code = code; g_server.error = message; }
void send_json(JsonDocument &, int code = 200) { g_server.code = code; }
bool ensure_auth() { if (g_server.authorized) return true; send_error(401,"unauthorized"); return false; }
bool firmware_busy() { if (!firmware_update_status().active && !firmware_update_status().verified) return false; send_error(409,"busy"); return true; }
