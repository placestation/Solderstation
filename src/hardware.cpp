#include <Arduino.h>  // Add this at the top
#include "hardware.h"

//TCA9554 expander(IO_EXPANDER_ADDR, &Wire);

void initI2C() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);
    delay(50);
}

/*void initExpander() {
    if (!expander.begin()) {
        Serial.println("ERROR: IO Expander failed to initialize!");
        while (true) {
            delay(1000);
        }
    }
    Serial.println("IO Expander initialized");
    
    expander.pinMode1(EXPANDER_PIN_LCD_RST, OUTPUT);
    expander.write1(EXPANDER_PIN_LCD_RST, HIGH);
    delay(20);
}
*/
void initBacklight() {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight enabled");
}

void customLcdReset() {
    Serial.println("Performing LCD reset...");
    pinMode(LCD_RST, OUTPUT);  // make sure it's set as output
    digitalWrite(LCD_RST, HIGH);
    delay(10);
    digitalWrite(LCD_RST, LOW);
    delay(120);
    digitalWrite(LCD_RST, HIGH);
    delay(150);
    Serial.println("LCD reset complete");
}