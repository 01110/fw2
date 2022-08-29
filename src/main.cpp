#include <Arduino.h>
#include <arduino-timer.h>
#include <FastLED.h>
#include <LittleFS.h>

#include "networking.hpp"
#include "ws2812b_8x8.hpp"
#include "button.hpp"
#include "web.hpp"
#include "png_parse.hpp"

using namespace am0r;

Timer hb_timer = Timer<2, millis>();

extern CRGB connecting_image[];

void click_cb()
{
}

void connection_cb(event_e event)
{
}

void image_updated()
{
  //open the image
  String filename;
  if(!web::get_displayed_image(filename)) return;
  File image_file = LittleFS.open("/images/" + filename, "r");
  if(!image_file) return;

  //currently only supports PNG
  if(!filename.endsWith(".png")) return;

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

  //copy input PNG data into the parsing context  
  png_parse::png_parse_context_s ctx;
  if(!png_parse::init(ctx, img_buf, img_size))
  {
    image_file.close();
    return;
  }
  free(img_buf);

  //parse it and extract RGB info
  CRGB image[WS_LED_NUM];
  if(!png_parse::parse(ctx))
  {
    image_file.close();
    return;
  }

  //don't support invalid sized PNG images
  if(ctx.hdr.height != 8 || ctx.hdr.width != 8) return;

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

  //deallocate resources
  image_file.close();
  png_parse::deinit(ctx);
 
  //display parsed image
  ws2812b_8x8::set(image);
}

void setup()
{
  networking::setup();
  web::setup();
  ws2812b_8x8::setup();
  ws2812b_8x8::set(connecting_image);

  networking::attach_conn_cb(connection_cb);
  button::attach_click_cb(click_cb);
  web::add_updated_cb(image_updated);

  image_updated();
}

void loop()
{
  hb_timer.tick();

  networking::loop();
  ws2812b_8x8::loop();
  button::loop();
}