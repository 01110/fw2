#pragma once

#include <FastLED.h>
#include <arduino-timer.h>
#include "networking.hpp"
#include "Hash.h"

#define WS_LED_WIDTH  8
#define WS_LED_HEIGHT 8
#define WS_LED_NUM    (WS_LED_WIDTH * WS_LED_HEIGHT)
#define WS_DATA_PIN   2

namespace am0r
{
  namespace ws2812b_8x8
  {
    CRGB out[WS_LED_NUM]; //FastLED will display this
    bool on = true;
    bool leds_cleared = false;

    Timer timer = Timer<1, millis>();

    void set(CRGB *in)
    {
      if(in == NULL) return;
      memcpy(out, in, WS_LED_NUM * 3);
      //fill_solid(out, WS_LED_NUM, CRGB::Orange);
      FastLED.show();
    }

    void set_color(CRGB color)
    {
      fill_solid(out, WS_LED_NUM, color);
      FastLED.show();
    }

    void set_brightness(uint8_t value)
    {
      FastLED.setBrightness(value);
    }

    void set_enable(bool on)
    {
      ws2812b_8x8::on = on;
      if(!on)
      {
        fill_solid(out, WS_LED_NUM, CRGB::Black);
        FastLED.show();
      }
    }

    bool render(void* data)
    {
      //return true;
      FastLED.show();
      return true;
    }

    void setup()
    {
      FastLED.addLeds<WS2812B, WS_DATA_PIN, GRB>(out, WS_LED_NUM);
      FastLED.setBrightness(32);
      fill_solid(out, WS_LED_NUM, CHSV(0,0,0));
      FastLED.show();
      timer.every(33, render);
    }

    void loop()
    {
      timer.tick();
    }
  };
};