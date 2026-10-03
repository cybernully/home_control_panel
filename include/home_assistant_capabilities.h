#pragma once

#include <string.h>

// Home Assistant exposes a light's dimming capability through
// supported_color_modes even while the light is off and has no brightness
// state attribute. ONOFF and UNKNOWN are the only non-dimmable light modes.
inline bool home_assistant_light_mode_supports_brightness(const char *mode) {
    return mode && mode[0] && strcmp(mode, "onoff") != 0 && strcmp(mode, "unknown") != 0;
}
