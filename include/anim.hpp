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
    
    bool push_back_frame(animation_s* anim, frame_s& frame)
    {
      if(!anim) return false;
    
      if(anim->frames == NULL)
      {
        anim->frames = (frame_s*)calloc(FRAME_ALLOCATION_SIZE, sizeof(frame_s));
        if(anim->frames == NULL) return false;
        anim->frames_size = 0;
        anim->frames_allocated = FRAME_ALLOCATION_SIZE;
      }
      if(anim->frames_size == anim->frames_allocated)
      {
        anim->frames = (frame_s*)realloc(anim->frames, anim->frames_allocated + FRAME_ALLOCATION_SIZE * sizeof(frame_s));
        if(anim->frames == NULL) return false;
        memset(anim->frames + anim->frames_size, 0, FRAME_ALLOCATION_SIZE * sizeof(frame_s));
        anim->frames_allocated += FRAME_ALLOCATION_SIZE;
      }

      memcpy(&anim->frames[anim->frames_size], &frame, sizeof(frame_s));
      anim->frames_size++;
      return true;
    }

    bool add_frame(animation_s* anim, uint32_t delay_ms, uint32_t x, uint32_t y, CRGB* pixels, uint32_t pixels_size)
    {
      if(!anim) return false;
      frame_s frame;
      frame.delay_ms = delay_ms;
      frame.x = x;
      frame.y = y;
      frame.pixels = (CRGB*) calloc(pixels_size, sizeof(CRGB));
      if(frame.pixels == NULL) return false;
      memcpy(frame.pixels, pixels, pixels_size * sizeof(CRGB));
      frame.pixels_size = pixels_size;
      return push_back_frame(anim, frame);
    }

    bool animation_init(animation_s* anim)
    {
      if(!anim) return false;
      if(anim->frames_size == 0) return true;
      for(uint32_t i = 0; i < anim->frames_size; i++)
        if(anim->frames[i].pixels != NULL) free(anim->frames[i].pixels);
      free(anim->frames);
      memset(anim, 0, sizeof(animation_s));
      return true;
    }
    
  } // namespace anim  
} // namespace pixelbox