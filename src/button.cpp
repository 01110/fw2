#include "button.hpp"

#include <OneButton.h>

namespace pixelbox
{
  namespace button
  {       
    OneButton btn = OneButton(BUTTON_GPIO_PIN, true, false); //pin, active low, pull-up resistor

    void setup(callbackFunction click_callback)
    {
      btn.attachClick(click_callback);
    }

    void loop()
    {
      btn.tick();
    }    
  }
}
