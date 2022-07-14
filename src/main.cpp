#include <Arduino.h>
#include <arduino-timer.h>
#include <FastLED.h>
#include <MsgPack.h>

#include "networking.hpp"
#include "ws2812b_8x8.hpp"
#include "button.hpp"
#include "cache.hpp"

using namespace am0r;

Timer hb_timer = Timer<1, millis>();
bitmap_cache cache;
uint8_t cache_index = 0;
state_e state = state_connecting;

extern CRGB selecting_image[];
extern CRGB connecting_image[];

bool hb_timer_cb(void* data)
{
  if(networking::get_connection_level() >= networking::connection_level_login)
    networking::send_cache_status(cache.calc_sha1());
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
        ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
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

void click_cb()
{
  if(state == state_idle)
  {
    networking::send_msg(msg_type_select_start);
    set_state(state_selecting, event_button_click);
  }
  if(state == state_selecting)
  {
    //show next pic from cache
    cache_index++;
    if(cache_index == cache.size()) cache_index = 0;
    ws2812b_8x8::set(cache.get_bitmap(cache_index)->data);
  }  
}

void longpress_start_cb()
{
  if(state == state_selecting)
  {
    set_state(state_idle, event_button_longclick_start);
    networking::send_select_end(cache_index);
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

void setup()
{
  networking::setup();
  ws2812b_8x8::setup();
  ws2812b_8x8::set(connecting_image);

  networking::attach_msg_cb(msg_cb);
  networking::attach_conn_cb(connection_cb);
  button::attach_click_cb(click_cb);
  button::attach_longpress_start_cb(longpress_start_cb);

  hb_timer.every(10000, hb_timer_cb);
}

void loop()
{
  hb_timer.tick();

  networking::loop();
  ws2812b_8x8::loop();
  button::loop();
}