#pragma once

#include <lvgl.h>

// A deliberately small, embedded Material Design Icons subset.  These are
// the same familiar pictograms used by Home Assistant, without the size cost
// of the complete MDI web font.
#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(ha_icons_font);
#ifdef __cplusplus
}
#endif

