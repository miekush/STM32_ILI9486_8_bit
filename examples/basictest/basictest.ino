// A basic display test to display "Hello World"

#include <STM32_ILI9486_8_bit.h>

STM32_ILI9486_8_bit tft;

void setup(void) {
  tft.begin();
  tft.fillScreen(BLACK);
  tft.setRotation(3);
  tft.setCursor(10, 10);
  tft.setTextColor(WHITE);  tft.setTextSize(4);
  tft.println("Hello world!");
}
void loop(void) {

}
