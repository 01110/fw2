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
    bool render(void* data);
    void render_next_anim_frame();

    CRGB out[WS_LED_NUM]; //FastLED will display this
    bool on = true;
    anim::animation_s* anim = NULL;;

    Timer timer = Timer<1, millis>();

    void set(CRGB *in)
    {
      if(in == NULL) return;
      ws2812b_8x8::anim = NULL;
      timer.cancel();
      timer.every(33, render);
      memcpy(out, in, WS_LED_NUM * 3);
      FastLED.show();
    }

    void set(anim::animation_s* anim)
    {
      ws2812b_8x8::anim = anim;
      render_next_anim_frame();
      FastLED.show();
    }

    void set_color(CRGB color)
    {
      ws2812b_8x8::anim = NULL;
      timer.cancel();
      timer.every(33, render);
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
        ws2812b_8x8::anim = NULL;
        fill_solid(out, WS_LED_NUM, CRGB::Black);
        FastLED.show();
      }
    }

    void render_next_anim_frame()
    {
      if(!anim) return;

      if(anim->frame_index >= anim->frames_size) anim->frame_index = 0;

      //set the timer at the next frame transition
      timer.cancel();
      timer.every(anim->frames[anim->frame_index].delay_ms, render);
      
      //overcopy protection & copy to the frambuffer
      uint16_t pixels_to_copy = anim->frames[anim->frame_index].pixels_size;
      if(pixels_to_copy > WS_LED_NUM) pixels_to_copy = WS_LED_NUM;
      memcpy(out, anim->frames[anim->frame_index].pixels, pixels_to_copy * 3);

      //increment the index and make it loop
      anim->frame_index++;
    }

    bool render(void* data)
    {
      render_next_anim_frame(); //returns immediately if no animation is set
      FastLED.show();
      return true;
    }

    void setup()
    {
      FastLED.addLeds<WS2812B, WS_DATA_PIN, GRB>(out, WS_LED_NUM);
      FastLED.setBrightness(64);
      fill_solid(out, WS_LED_NUM, CHSV(0,0,0));
      FastLED.show();
      timer.every(33, render);
      anim = NULL;
    }

    void loop()
    {
      timer.tick();
    }
  };
};