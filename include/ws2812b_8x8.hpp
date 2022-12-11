#pragma once

#include <FastLED.h>
#include <arduino-timer.h>
#include "anim.hpp"
#include "Hash.h"

#define WS_LED_WIDTH  8
#define WS_LED_HEIGHT 8
#define WS_LED_NUM    (WS_LED_WIDTH * WS_LED_HEIGHT)
#define WS_DATA_PIN   2

namespace pixelbox
{
  namespace ws2812b_8x8
  {
    void set(CRGB *in);
    void set(anim::animation_s* anim);
    void set_color(CRGB color);

    void set_brightness(uint8_t value);
    void set_enable(bool on);

    void render_next_anim_frame();
    bool render(void* data);

    void setup();
    void loop();
  };
};