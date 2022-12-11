#include "state_machine.hpp"

#include <FastLED.h>

#include "web.hpp"
#include "gif_parse.hpp"
#include "png_parse.hpp"

namespace pixelbox
{
  namespace state_machine
  {
    extern CRGB connecting_image[];
    pixelbox::anim::animation_s animation;

    void click_cb()
    { 
      String act;
      pixelbox::web::get_displayed_image(act);
      pixelbox::web::select_next_image(act);
    }

    void image_updated()
    {
      //open the image
      String filename;
      if(!pixelbox::web::get_displayed_image(filename)) return;
      File image_file = LittleFS.open("/images/" + filename, "r");
      if(!image_file) return;

      //currently only supports PNG and GIF
      bool png = true;
      if(filename.endsWith(".png")) png = true;
      else if(filename.endsWith(".gif")) png = false;
      else return;

      //read image file into RAM
      uint32_t img_size = image_file.size();
      uint8_t* img_buf = (uint8_t*) malloc(img_size);
      if(img_buf == NULL) return;
      if((size_t)image_file.read((uint8_t*)img_buf, img_size) != img_size)
      {
        free(img_buf);
        image_file.close();
        return;
      }
      image_file.close();

      //frame buffer
      CRGB image[WS_LED_NUM];

      if(png)
      {
        img_parse::png_parse_context_s ctx;
        if(!img_parse::init(ctx, img_buf, img_size)) return;
        free(img_buf);
        if(!img_parse::parse(ctx)) return;

        //don't support invalid sized PNG images
        if(ctx.hdr.height != 8 || ctx.hdr.width != 8) return;

        //export output
        if(ctx.pixel_size == 3)
          memcpy(image, ctx.unfiltered_data, sizeof(image));
        else if(ctx.pixel_size == 4)
        {
          for(u8 i = 0; i < WS_LED_NUM; i++)
          {
            image[i].r = ctx.unfiltered_data[4*i+0];
            image[i].g = ctx.unfiltered_data[4*i+1];
            image[i].b = ctx.unfiltered_data[4*i+2];
          }
        }
        img_parse::deinit(ctx);
        pixelbox::ws2812b_8x8::set(image);
      }
      else
      {
        img_parse::gif_parse_context_s ctx;
        
        if(img_parse::init(ctx, img_buf, img_size) != img_parse::error_code_ok) return;
        free(img_buf);
        img_parse::error_code_e err = img_parse::parse(ctx);
        if(err != img_parse::error_code_ok)
        {
          img_parse::deinit(ctx);
          return;
        }
        if(ctx.lsd.height != 8 || ctx.lsd.width != 8) return;

        if(ctx.images_size == 1)
        {
          memcpy(image, ctx.images[0].output, sizeof(image));
          pixelbox::ws2812b_8x8::set(image);
        }
        else
        {
          pixelbox::anim::animation_init(&animation); //dealloc if necessary and zero everything
          for(uint32_t i = 0; i < ctx.images_size; i++)
          {
            if(!ctx.images[i].gce.valid) continue; //skip frames without gce
            pixelbox::anim::add_frame(&animation, 
                            ctx.images[i].gce.delay_time_10ms * 10, 
                            ctx.images[i].id.left_position, 
                            ctx.images[i].id.top_position,
                            (CRGB*)ctx.images[i].output,
                            ctx.images[i].output_size);
          }
          pixelbox::ws2812b_8x8::set(&animation);
        }
        img_parse::deinit(ctx);
      }
    }

    void setup()
    {
      pixelbox::ws2812b_8x8::set(connecting_image);
      image_updated();
    }
  }  
}
