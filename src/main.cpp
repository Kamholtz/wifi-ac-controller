
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRCoolixAC ac(kIrLed);  // Set the GPIO to be used to sending the message.

void setup() {
  ac.begin();
  Serial.begin(115200);
}

void loop() {
  Serial.println("Sending...");

  // Set up what we want to send. See ir_Argo.cpp for all the options.
  ac.setPower(true);
  ac.setFan(kCoolixFanAuto0);
  ac.setMode(kCoolixCool);
  ac.setTemp(23);

#if SEND_ARGO
  // Now send the IR signal.
  ac.send();
#else  // SEND_ARGO
  Serial.println("Can't send because SEND_ARGO has been disabled.");
#endif  // SEND_ARGO

  delay(5000);
}