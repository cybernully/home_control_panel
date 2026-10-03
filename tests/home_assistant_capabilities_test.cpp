#include "home_assistant_capabilities.h"

#include <cassert>
#include <cstdio>

int main() {
    assert(!home_assistant_light_mode_supports_brightness(nullptr));
    assert(!home_assistant_light_mode_supports_brightness(""));
    assert(!home_assistant_light_mode_supports_brightness("unknown"));
    assert(!home_assistant_light_mode_supports_brightness("onoff"));
    assert(home_assistant_light_mode_supports_brightness("brightness"));
    assert(home_assistant_light_mode_supports_brightness("color_temp"));
    assert(home_assistant_light_mode_supports_brightness("hs"));
    assert(home_assistant_light_mode_supports_brightness("rgb"));
    assert(home_assistant_light_mode_supports_brightness("rgbw"));
    assert(home_assistant_light_mode_supports_brightness("rgbww"));
    assert(home_assistant_light_mode_supports_brightness("white"));
    assert(home_assistant_light_mode_supports_brightness("xy"));
    std::puts("Home Assistant light capability tests passed.");
}
