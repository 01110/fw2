#include <Arduino.h>
#include <arduino-timer.h>
#include <FastLED.h>
#include <MsgPack.h>
#include <LittleFS.h>

#include "networking.hpp"
#include "ws2812b_8x8.hpp"
#include "button.hpp"
#include "cache.hpp"
#include "web.hpp"

using namespace am0r;

Timer hb_timer = Timer<2, millis>();
Timer send_timer = Timer<1, millis>();
bitmap_cache cache;
uint8_t cache_index = 0;
state_e state = state_connecting;

extern CRGB selecting_image[];
extern CRGB connecting_image[];

CRGB test_image[WS_LED_NUM];
uint8_t active_pixel = 0;

uint8_t png_buffer[512];

bool hb_timer_cb(void* data)
{
  if(networking::get_connection_level() >= networking::connection_level_login)
    networking::send_cache_status(cache.calc_sha1());
  return true;
}

bool test_cb(void* data)
{
  ws2812b_8x8::set_brightness(32);
  fill_solid(test_image, WS_LED_NUM, CRGB::White);
  test_image[active_pixel] = CRGB::White;
  active_pixel++;
  if(active_pixel >= WS_LED_NUM) active_pixel = 0;
  ws2812b_8x8::set(test_image);
  
  return true;
}

void set_state(state_e new_state, event_e event)
{
  networking::send_status(state, new_state, event);
  state = new_state;
}   

void msg_cb(msg_type_e type, const char* msg, uint64_t len)
{
  MsgPack::Unpacker unpacker;
  unpacker.feed((const uint8_t*)msg, len);
  int msg_type; unpacker.unpack(msg_type);

  switch (type)
  {
    case msg_type_cache_part:
    {
      bool first_part; 
      unpacker.unpack(first_part);
      if(first_part) cache.clear();

      bitmap_s bitmap;
      memcpy(bitmap.data, msg + unpacker.index(), sizeof(bitmap.data));
      cache.add_bitmap(bitmap);
      break;
    }
    case msg_type_cache_ok:
    {
      if(state == state_idle || state == state_connecting)
      {
        unpacker.unpack(cache_index);
        //ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
      }
      break;
    }
    case msg_type_select_start:
    {
      //display selecting
      ws2812b_8x8::set(selecting_image);
      set_state(state_bond_is_selecting, event_rx_select_start);
      break;
    }
    case msg_type_select_end:
    {
      unpacker.unpack(cache_index);
      if(cache_index >= cache.size()) cache_index = cache.size() -1;
      ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
      set_state(state_idle, event_rx_select_end);
    }
    default:
      networking::send_log("unkown msg type: %d", type);
      break;
  }
}

bool selected_cb(void* data)
{
  if(state == state_selecting)
  {
    set_state(state_idle, event_button_longclick_start);
    networking::send_select_end(cache_index);
  }
  return true;
}

void click_cb()
{
  if(state == state_idle)
  {
    networking::send_msg(msg_type_select_start);
    set_state(state_selecting, event_button_click);
    send_timer.cancel();
    send_timer.in(1000, selected_cb);
  }
  if(state == state_selecting)
  {
    //show next pic from cache
    cache_index++;
    if(cache_index == cache.size()) cache_index = 0;
    ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
    send_timer.cancel();
    send_timer.in(1000, selected_cb);
  }  
}

void connection_cb(event_e event)
{
  if(event == event_disconnected)
  {
    set_state(state_connecting, event);
    ws2812b_8x8::set(connecting_image);
  }
  else if(event == event_connected)
  {
    if(state == state_connecting) set_state(state_idle, event);
    ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
    networking::cache_hash = cache.calc_sha1();
  }
}

void updated_cb()
{
  File image = LittleFS.open("image.raw", "r");
  if(!image)
  {
    networking::send_log("Failed to open PNG file.");
    return;
  }
  size_t bytes = image.read(png_buffer, 512);

  if(bytes != image.size())
  {
    networking::send_log("Failed to read the entire PNG file into RAM.");
    image.close();
    return;
  }

  networking::send_log("Image loaded: %u bytes.", bytes);

  for(uint8_t i = 0; i < 64; i++)
  {
    CRGB px;
    px.r = png_buffer[i*3 + 0];
    px.g = png_buffer[i*3 + 1];
    px.b = png_buffer[i*3 + 2];
    test_image[i] = px;
  }

  image.close();
  ws2812b_8x8::set(test_image);
}

void setup()
{
  networking::setup();
  // networking::set_client_enable(false);
  web::setup();
  ws2812b_8x8::setup();
  ws2812b_8x8::set(connecting_image);

  networking::attach_msg_cb(msg_cb);
  networking::attach_conn_cb(connection_cb);
  button::attach_click_cb(click_cb);
  web::add_updated_cb(updated_cb);
  hb_timer.every(10000, hb_timer_cb);
  // hb_timer.every(1000, test_cb);
}

void loop()
{
  hb_timer.tick();
  send_timer.tick();

  networking::loop();
  ws2812b_8x8::loop();
  button::loop();
}