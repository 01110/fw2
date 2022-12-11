#pragma once

#include <FastLED.h>

#define FRAME_ALLOCATION_SIZE 4

namespace pixelbox
{
  namespace anim
  {
    typedef struct frame_s
    {
      uint32_t delay_ms;
      uint32_t x;
      uint32_t y;
      CRGB* pixels;
      uint32_t pixels_size;      
    }frame_s;

    typedef struct animation_s
    {
      frame_s* frames;
      uint32_t frames_size;
      uint32_t frames_allocated;
      uint32_t frame_index;
    }animation_s;
    
    bool push_back_frame(animation_s* anim, frame_s& frame);
    bool add_frame(animation_s* anim, uint32_t delay_ms, uint32_t x, uint32_t y, CRGB* pixels, uint32_t pixels_size);
    bool animation_init(animation_s* anim);    
  } 
}
