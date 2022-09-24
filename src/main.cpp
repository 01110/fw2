#include <Arduino.h>
#include <arduino-timer.h>
#include <FastLED.h>
#include <LittleFS.h>

#include "gif_parse.hpp"
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
  }
  else
  {
    img_parse::gif_parse_context_s ctx;
    
    if(img_parse::init(ctx, img_buf, img_size) != img_parse::error_code_ok) return;
    free(img_buf);
    // ws2812b_8x8::set_color(CRGB::White);
    img_parse::error_code_e err = img_parse::parse(ctx);
    if(err != img_parse::error_code_ok)
    {
      // switch (err)
      // {
      // case img_parse::error_code_mem_alloc:
      //   ws2812b_8x8::set_color(CRGB::Orange);
      //   break;
      // case img_parse::error_code_out_of_bounds:
      //   ws2812b_8x8::set_color(CRGB::Purple);
      //   break;
      // case img_parse::error_code_not_supported:
      //   ws2812b_8x8::set_color(CRGB::Aqua);
      //   break;
      // case img_parse::error_code_inconsistence:
      //   ws2812b_8x8::set_color(CRGB::Red);
      //   break;
      // case img_parse::error_code_null_pt:
      //   ws2812b_8x8::set_color(CRGB::Green);
      //   break;
      // case img_parse::error_code_parsed:
      //   ws2812b_8x8::set_color(CRGB::Blue);
      //   break;
      // default:
      //   break;
      // }
      img_parse::deinit(ctx);
      return;
    }

    // ws2812b_8x8::set_color(CRGB::Blue);
    if(ctx.lsd.height != 8 || ctx.lsd.width != 8) return;
    memcpy(image, ctx.images[0].output, sizeof(image));
    // ws2812b_8x8::set_color(CRGB::Red);
    img_parse::deinit(ctx);
    // ws2812b_8x8::set_color(CRGB::Purple);
  }
 
  //display parsed image
  ws2812b_8x8::set(image);
}

bool displayed_image_check(void* data)
{
  image_updated();
  return true;
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
  // hb_timer.every(1000, displayed_image_check);

  delay(1000);
  image_updated();
}

void loop()
{
  hb_timer.tick();

  networking::loop();
  ws2812b_8x8::loop();
  button::loop();
}