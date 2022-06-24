#pragma once

#include "Arduino.h"
#include "OneButton.h"

#define BUTTON_GPIO_PIN          0

namespace am0r
{
  namespace button
  {
    OneButton btn = OneButton(
      BUTTON_GPIO_PIN,  // Input pin for the button
      true,             // Button is active LOW
      false             // Enable internal pull-up resistor
    );

    void attach_click_cb(callbackFunction cb)
    {
      btn.attachClick(cb);
    }

    void attach_longpress_stop_cb(callbackFunction cb)
    {
      btn.attachLongPressStop(cb);
    }

    void attach_longpress_start_cb(callbackFunction cb)
    {
      btn.attachLongPressStart(cb);
    }

    void attach_doubleclick_cb(callbackFunction cb)
    {
      btn.attachDoubleClick(cb);
    }

    void setup()
    {
    }

    void loop()
    {
      btn.tick();
    }
  };
};