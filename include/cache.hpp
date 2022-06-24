#pragma once

#include "FastLED.h"
#include "Hash.h"

namespace am0r
{
  struct bitmap_s
  {
    CRGB data[64];
  };

  class bitmap_cache
  {
    std::vector<bitmap_s> bitmaps;

  public:
    bitmap_cache() {}

    bitmap_cache(std::vector<bitmap_s> bitmaps): bitmaps(bitmaps)
    {}

    void add_bitmap(const bitmap_s& bitmap)
    {
      bitmaps.push_back(bitmap);
    }

    bitmap_s* get_bitmap(uint64_t index)
    {
      return &bitmaps[index];
    }

    size_t size() { return bitmaps.size(); }
    
    String calc_sha1() const
    {
      return sha1((const char*)bitmaps.data(), bitmaps.size()*sizeof(bitmap_s));
    }

    void clear()
    {
      bitmaps.clear();
    }
  };
  
} // namespace am0r
