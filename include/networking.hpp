#pragma once

#include <ESP8266WiFi.h>
#include <arduino-timer.h>
// #include <ArduinoOTA.h>
#include <FastLED.h>

#include "am0r.hpp"

#define WIFI_SSID   "Telekom-eMPY3P"
#define WIFI_PASS   "643tfcmcfu6c"

namespace am0r
{
  namespace networking
  {
    const char* ssid     = WIFI_SSID;
    const char* password = WIFI_PASS;

    typedef enum
    {
      connection_level_wifi  = 0,
      connection_level_ready = 1
    } connection_level_e;

    connection_level_e connection_level = connection_level_wifi;
    connection_level_e last_connection_level = connection_level_wifi;

    typedef void (*eventCallbackFunction)(event_e);

    eventCallbackFunction conn_cb = NULL;

    void attach_conn_cb(eventCallbackFunction cb)
    {
      conn_cb = cb;
    }

    void update_connection_level()
    {
      connection_level = connection_level_wifi;
      if(WiFi.status() == WL_CONNECTED)
      {
        connection_level = connection_level_ready;
      }
    }

    connection_level_e get_connection_level()
    {
      return connection_level;
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

    void setup()
    {
      //Wi-Fi handler
      WiFi.setAutoReconnect(true);
      WiFi.setSleep(false);
      WiFi.begin(ssid, password);

      //ota
      // ArduinoOTA.begin();
    }

    unsigned long counter = 0;

    void loop()
    {
      //for ota update
      // ArduinoOTA.handle();
      //update state variables
      update_connection_level();

      //react to state changes
      check_conn_events();

      //update the remaining state variables
      last_connection_level = connection_level;
    }
  }
}