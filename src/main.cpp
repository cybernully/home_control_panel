#include <Arduino.h>
#include <esp_chip_info.h>
#include "app_config.h"
#include "battery_service.h"
#include "board_lvgl.h"
#include "config_service.h"
#include "home_assistant.h"
#include "module_registry.h"
#include "network_service.h"
#include "ui_shell.h"
#include "web_manager.h"

void setup() {
    Serial0.begin(115200);
    delay(250);
    Serial0.printf("\n%s v%s starting...\n", APP_NAME, APP_VERSION);

    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);
    Serial0.printf("[Hardware] ESP32-P4 silicon revision %u.%02u (raw=%u)\n",
                   static_cast<unsigned>(chip_info.revision / 100U),
                   static_cast<unsigned>(chip_info.revision % 100U),
                   static_cast<unsigned>(chip_info.revision));

    if (!config_service_begin()) Serial0.println("[Config] WARNING: using defaults");
    const PanelConfig &config = config_service_get();

    if (!board_lvgl_begin(APP_DISPLAY_ROTATION)) {
        Serial0.println("Fatal display initialization error.");
        while (true) delay(1000);
    }

    board_set_backlight(config.backlight);
    battery_service_begin();
    module_registry_begin();
    ui_shell_begin();

    /* Proven policy: render the UI before bringing up network/TLS work. */
    network_service_begin();
    home_assistant_begin();
    web_manager_begin();

    Serial0.printf("%s ready. profile=%s area=%s\n", APP_NAME, config.profile, config.area_id);
}

void loop() {
    network_service_loop();
    home_assistant_loop();
    web_manager_loop();
    battery_service_loop();
    ui_shell_loop();
    board_lvgl_loop();
    delay(5);
}
