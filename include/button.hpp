#pragma once

#include <OneButton.h>

#define BUTTON_GPIO_PIN    0

namespace pixelbox
{
  namespace button
  {     
    void setup(callbackFunction click_callback);
    void loop();  
  }
}
