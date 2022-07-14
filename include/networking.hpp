#pragma once

#include <ESP8266WiFi.h>
#include <arduino-timer.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <MsgPack.h>

#include "am0r.hpp"
#include "cache.hpp"

#define WIFI_SSID   "Telekom-eMPY3P"
#define WIFI_PASS   "643tfcmcfu6c"
#define SERVER_IP   "192.168.1.66"
#define SERVER_PORT 1234

namespace am0r
{
  namespace networking
  {
    const char* ssid     = WIFI_SSID;
    const char* password = WIFI_PASS;
    const char* server_ip = SERVER_IP;
    int         server_port = SERVER_PORT;

    WiFiClient  client;
    Timer       connection_timer = Timer<1, millis>();

    uint64_t    tx_size = 0;
    char        tx_buffer[256];

    uint64_t    rx_size = 0;
    uint32_t    rx_offset = 0;
    uint32_t    rx_remaining = 8;
    char        rx_buffer[1024];
    bool        rx_reading_len = true;

    bool        login_successful = false;

    char        log_buf[1024];

    unsigned long get_start = 0;
    unsigned long get_end = 0;

    String      cache_hash = "";

    typedef enum
    {
      connection_level_wifi  = 0,
      connection_level_tcp   = 1,
      connection_level_login = 2,
      connection_level_ready = 3
    } connection_level_e;

    typedef enum
    {
      sending_level_none = 0,
      sending_level_sent = 1,
      sending_level_received = 2
    } sending_level_e;

    connection_level_e connection_level = connection_level_wifi;
    connection_level_e last_connection_level = connection_level_wifi;

    sending_level_e sending_level = sending_level_none;

    typedef void (*eventCallbackFunction)(event_e);
    typedef void (*msgCallbackFunction)(msg_type_e, const char*, uint64_t);

    msgCallbackFunction msg_cb = NULL;
    eventCallbackFunction conn_cb = NULL;

    void attach_msg_cb(msgCallbackFunction cb)
    {
      msg_cb = cb;
    }

    void attach_conn_cb(eventCallbackFunction cb)
    {
      conn_cb = cb;
    }

    void update_connection_level()
    {
      connection_level = connection_level_wifi;
      if(WiFi.status() == WL_CONNECTED)
      {
        connection_level = connection_level_tcp;
        if(client.connected())
        {
          connection_level = connection_level_login;
          if(login_successful)
            connection_level = connection_level_ready;
        }
      }
    }

    connection_level_e get_connection_level()
    {
      return connection_level;
    }

    sending_level_e get_sending_level()
    {
      return sending_level;
    }

    void check_conn_events()
    {
      if(connection_level == connection_level_ready && last_connection_level != connection_level_ready)
      {
        if(conn_cb) conn_cb(event_connected);
      }
      else if(connection_level != connection_level_ready && last_connection_level == connection_level_ready)
      {
        if(conn_cb) conn_cb(event_disconnected);
      }
    }

    void send(const MsgPack::Packer& packer)
    {
      if(client.connected() == 0) return;

      tx_size = packer.size();
      client.write((const char*)&tx_size, 8);
      client.write(packer.data(), tx_size);
      //client.flush(); //not necessary, because the sync mode
    }

    void send_status(state_e starting, state_e ending, event_e event)
    {
      MsgPack::Packer packer;
      msg_type_e type = msg_type_status;
      packer.packInt8(type);
      packer.packUInt32(ESP.getChipId());
      packer.packInt8(starting);
      packer.packInt8(ending);
      packer.packInt8(event);
      send(packer);
    }

    void send_log(const char* fmt, ...)
    {
      va_list arglist;
      va_start(arglist, fmt);
      vsnprintf(log_buf, sizeof(log_buf), fmt, arglist);
      va_end(arglist);

      MsgPack::Packer packer;
      msg_type_e type = msg_type_log;
      packer.packInt8(type);
      packer.packUInt32(ESP.getChipId());
      packer.packString(log_buf);
      send(packer);
    }

    void send_cache_status(const String& hash)
    {
      MsgPack::Packer packer;
      packer.packInt8(msg_type_cache_status);
      packer.packUInt32(ESP.getChipId());
      packer.packString(hash);
      send(packer);
    }

    void send_select_end(uint8_t index)
    {
      MsgPack::Packer packer;
      packer.packInt8(msg_type_select_end);
      packer.packUInt32(ESP.getChipId());
      packer.packUInt8(index);
      send(packer);
    }

    void send_msg(msg_type_e type)
    {
      get_start = millis();
      MsgPack::Packer packer;
      packer.packInt8(type);
      packer.packUInt32(ESP.getChipId());
      send(packer);
    }

    bool reconnect(void* data)
    {
      if(WiFi.status() == WL_CONNECTED && client.connected() == 0)
        client.connect(server_ip, server_port);
      
      return true;
    }

    void rx_buffer_init()
    {
      rx_reading_len = true;
      rx_remaining = 8;
      rx_size = 0;
      rx_offset = 0;
      memset(rx_buffer, 0, sizeof(rx_buffer));
    }

    void rx_frame()
    {
      MsgPack::Unpacker unpacker;
      unpacker.feed((const uint8_t*)rx_buffer, rx_size);

      int type;
      unpacker.unpack(type);
      //send_log("RX: %d", type);

      switch(type)
      {
        case am0r::msg_type_cache_ok:
        {
          login_successful = true;
          if(msg_cb) msg_cb(am0r::msg_type_cache_ok, rx_buffer, rx_size);
          break;
        }
        case am0r::msg_type_cache_part:
        {
          login_successful = false;
          if(msg_cb) msg_cb(am0r::msg_type_cache_part, rx_buffer, rx_size);
          break;
        }
        case am0r::msg_type_select_start:
        case am0r::msg_type_select_end:
        {
          if(msg_cb) msg_cb((am0r::msg_type_e)type, rx_buffer, rx_size);
          break;
        }
      }
    }

    void read()
    {
      int32_t rx_available = client.available();
      size_t rx_cnt = 0;
      if(rx_available > 0)
      {
        uint32_t rx_now = rx_remaining;
        if(rx_available < (int32_t)rx_remaining) rx_now = rx_available;
        if(rx_reading_len) rx_cnt = client.readBytes((((char*)&rx_size) + rx_offset), rx_now);
        else rx_cnt = client.readBytes(rx_buffer + rx_offset, rx_now);

        if(rx_cnt == 0)
        {
          client.stop();
          login_successful = false;
          rx_buffer_init();
        }
        else
        {
          rx_remaining -= rx_cnt;
          rx_offset += rx_cnt;        
          if(rx_remaining == 0)
          {
            if(rx_reading_len)
            { 
              rx_reading_len = false;
              rx_offset = 0;
              rx_remaining = rx_size;      
            }
            else
            {
              rx_frame();
              rx_buffer_init();
            }
          }
        }        
        
      }
    }

    void setup()
    {
      //TCP client
      client.setTimeout(100);
      client.setNoDelay(true);
      client.setSync(true);
      client.setDefaultNoDelay(true);
      client.setDefaultSync(true);

      //Wi-Fi handler
      WiFi.setAutoReconnect(true);
      WiFi.setSleep(false);
      WiFi.begin(ssid, password);

      //timers
      connection_timer.every(1000, reconnect);

      //ota
      ArduinoOTA.begin();
    }

    unsigned long counter = 0;

    void loop()
    {
      //for ota update
      ArduinoOTA.handle();

      //tick the timers
      connection_timer.tick();

      //update state variables
      update_connection_level();
      if(connection_level < connection_level_login) login_successful = false;

      //react to state changes
      if(last_connection_level == connection_level_tcp && 
         connection_level == connection_level_login && 
         !login_successful)
      {
        send_cache_status(cache_hash);
        send_status(state_connecting, state_connecting, event_none);
      }
      check_conn_events();

      //read TCP data
      read();

      //update the remaining state variables
      last_connection_level = connection_level;
    }
  }
}