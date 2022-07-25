#include <Arduino.h>
#include <arduino-timer.h>
#include <FastLED.h>
#include <LittleFS.h>

#include "networking.hpp"
#include "ws2812b_8x8.hpp"
#include "button.hpp"
#include "web.hpp"

using namespace am0r;

Timer hb_timer = Timer<2, millis>();

extern CRGB connecting_image[];

CRGB temp_image[WS_LED_NUM];

void click_cb()
{
}

void connection_cb(event_e event)
{
}

void image_updated()
{
  File image_file = LittleFS.open("image.raw", "r");
  if(!image_file) return;
  if((size_t)image_file.read((uint8_t*)temp_image, sizeof(temp_image)) != image_file.size())
  {
    image_file.close();
    return;
  }
  image_file.close();
  ws2812b_8x8::set(temp_image);
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