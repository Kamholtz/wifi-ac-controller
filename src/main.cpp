
#include <IRremoteESP8266.h>
#include <IRsend.h>


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <math.h>
#include <NTPClient.h>
#include <IRac.h>
#include <IRUtils.h>

#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_SOCKET_TIMEOUT 1

#include "main.h"

decode_type_t protocols[] = {COOLIX, MITSUBISHI};

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Global Variable
const int OLED_ckl = D3;
const int OLED_sdin = D2;
const int OLED_rst = D1;
const int OLED_cs = D0;
WiFiClient espClient;
WiFiManager wifiManager;
PubSubClient client(espClient);
IRac ac(IR_pin);
IRsend irsend(IR_pin);

// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

//Declare prototype functions
void increment();
void decrement();
void update();
void updateServerValue();

// void btnSwingInt()
// {
//   isSwing = !isSwing;
//   update();
//   updateServerValue();
// }

// void btnSpeedInt()
// {
//   fanSpeed = ++fanSpeed % 4;
//   update();
//   updateServerValue();
// }

// function for up-down interrupt
void ICACHE_RAM_ATTR btnUpInt()
{
  currentAction = ACTION_UP;
}

void ICACHE_RAM_ATTR btnDnInt()
{
  currentAction = ACTION_DOWN;
}

// void changeACmode()
// {
//   isCool = !isCool;
//   update();
//   updateServerValue();
// }
//
void sendAllProtocols(IRac ac)
{
  uint8_t numProtocols = sizeof(protocols) / sizeof(protocols[0]);
  for (uint8_t i = 0; i < numProtocols; i++)
  {
    ac.next.protocol = protocols[i];
    ac.sendAc();
  }
}

void update()
{
  static bool isLastOn = false;
  static int lastTemp = 0;
  static bool lastIsSwing = isSwing;
  static int lastFanSpeed = fanSpeed;
  static int lastIsCool = isCool;

  if (setTemp == lastTemp && isSwing == lastIsSwing && lastFanSpeed == fanSpeed && lastIsCool == isCool && isLastOn == isOn)
    return;

  if (isOn)
  {
    ac.next.power = true;
    if (isCool)
    {
      Serial.println("Send IR : temp = " + String(setTemp) + " swing = " + String(isSwing) + " Fan Speed : " + String(fanSpeed) + " IsLastOn : " + isLastOn);
      isLastOn = true;
      lastTemp = setTemp;
      lastIsSwing = isSwing;
      lastFanSpeed = fanSpeed;
      lastIsCool = true;

      //Set and send commands
      ac.next.mode = stdAc::opmode_t::kCool;
      switch (fanSpeed)
      {
      case (0):
        Serial.println("Setting to coolix fan auto");
        ac.next.fanspeed = stdAc::fanspeed_t::kAuto;
        break;
      case (1):
        ac.next.fanspeed = stdAc::fanspeed_t::kMin;
        break;
      case (2):
        ac.next.fanspeed = stdAc::fanspeed_t::kMedium;
        break;
      case (3):
        ac.next.fanspeed = stdAc::fanspeed_t::kMax;
        break;
      }

      // ac.setSwing();
      ac.next.degrees = setTemp;
    }
    else
    {
      ac.next.power = true;
      ac.next.mode = stdAc::opmode_t::kFan;
      lastIsCool = false;
    }

    ac.sendAc();
  }
  else if (isLastOn)
  {
    Serial.println("Send off");
    isLastOn = false;

    //set and send command
    ac.next.power = false;
    ac.sendAc();
  }

  Serial.println("Sent command to AC");
  previousMillis = millis();
  currentContrast = 255;
}

void updateServerValue()
{

  /*!!-- Need to redefine MQTT_MAX_PACKET_SIZE 256 --!! */

  String value;
  String message;
  char data[200];

  //Primary
  // message = "{\"name\" : \"" + device_name + "\", \"service_name\" : \"" + service_name + "\", \"characteristic\" : \"CurrentTemperature\", \"value\" : " + String(setTemp) + "}";
  // message.toCharArray(data, (message.length() + 1));
  message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"CurrentTemperature\",\"value\":" + String(setTemp) + "}";
  client.publish(mqtt_device_value_to_set_topic, message.c_str());

  message = "{\"name\" : \"" + device_name + "\", \"service_name\" : \"" + service_name + "\", \"characteristic\" : \"Active\", \"value\" : " + String(isOn) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);

  message = "{\"name\" : \"" + device_name + "\", \"service_name\" : \"" + service_name + "\", \"characteristic\" : \"SwingMode\", \"value\" : " + String(isSwing) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);

  message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"CoolingThresholdTemperature\",\"value\":" + String(setTemp) + " }";
  client.publish(mqtt_device_value_to_set_topic, message.c_str());

  message = "{\"name\" : \"" + device_name + "\", \"service_name\" : \"" + service_name + "\", \"characteristic\" : \"RotationSpeed\", \"value\" : " + String(fanSpeed) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);

  if (isCool)
  {
    message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"CurrentHeaterCoolerState\",\"value\":1}";
    client.publish(mqtt_device_value_to_set_topic, message.c_str());
    message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"TargetHeaterCoolerState\",\"value\":2}";
    client.publish(mqtt_device_value_to_set_topic, message.c_str());
  }
  else
  {
    message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"CurrentHeaterCoolerState\",\"value\":2}";
    client.publish(mqtt_device_value_to_set_topic, message.c_str());
    message = "{\"name\": \"Smart AC\",\"service_name\": \"smart_ac\",\"characteristic\": \"TargetHeaterCoolerState\",\"value\":1}";
    client.publish(mqtt_device_value_to_set_topic, message.c_str());
  }

  //Secondary
  message = "{\"name\" : \"" + device_name_secondary + "\", \"service_name\" : \"" + service_name_secondary + "\", \"characteristic\" : \"On\", \"value\" : " + String(isOn) + "}";
  message.toCharArray(data, (message.length() + 1));
  client.publish(mqtt_device_value_to_set_topic, data);
}

void blink()
{
  //Blink on received MQTT message
  digitalWrite(2, LOW);
  delay(20);
  digitalWrite(2, HIGH);
}

void setup_ota()
{
  // Set OTA Password, and change it in platformio.ini
  ArduinoOTA.setHostname("ESP8266-AC");
  // ArduinoOTA.setPassword("12345678");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR)
      ; // Auth failed
    else if (error == OTA_BEGIN_ERROR)
      ; // Begin failed
    else if (error == OTA_CONNECT_ERROR)
      ; // Connect failed
    else if (error == OTA_RECEIVE_ERROR)
      ; // Receive failed
    else if (error == OTA_END_ERROR)
      ; // End failed
  });
  ArduinoOTA.begin();
}

void ACOnOff()
{
  if (!isOn)
  {
    Serial.println("AC On");
    isOn = true;
  }
  else
  {
    Serial.println("AC Off");
    isOn = false;
  }
  update();
}

void reconnect()
{
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect

  if (client.connect(clientId.c_str()))
  {
    // Once connected, resubscribe.
    client.subscribe(mqtt_device_value_from_set_topic);
  }
}

void increment()
{
  if (!isCool)
    return;
  setTemp++;
  update();
  updateServerValue();
}

void decrement()
{
  if (!isCool)
    return;
  setTemp--;
  update();
  updateServerValue();
}

void power()
{
  ACOnOff();
  updateServerValue();
}

void ICACHE_RAM_ATTR btnPowerPressed()
{
  // int press_millis = millis();
  // while (digitalRead(btnPower) == LOW)
  // {
  //   delay(0);
  //   if (millis() - press_millis > 1000)   //if long press, change AC Mode
  //   {
  //     isCool = !isCool;
  //     update();
  //     return;
  //   }
  // };
  currentAction = ACTION_POWER;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);

  Serial.println(s_payload + "\0");

  StaticJsonBuffer<200> jsonBuffer;

  JsonObject &root = jsonBuffer.parseObject(s_payload);

  const char *name = root["name"];

  // Serial.println(name);
  if (strcmp(name, device_name.c_str()) != 0 && strcmp(name, device_name_secondary.c_str()) != 0)
  {
    return;
  }

  // blink();
  const char *characteristic = root["characteristic"];

  if (strcmp(characteristic, "CoolingThresholdTemperature") == 0)
  {
    int value = root["value"];
    if (value < minTemp || value > maxTemp)
    {
      return;
    }
    setTemp = value;
    update();
  }
  if (strcmp(characteristic, "Active") == 0)
  {
    int value = root["value"];
    if (value != 1 && value != 0)
    {
      return;
    }
    isOn = value;
    update();

    //set secondary device status (For primitive service as regular fan.)
    char data[150];
    String message = "{\"name\" : \"" + device_name_secondary + "\", \"service_name\" : \"" + service_name_secondary + "\", \"characteristic\" : \"On\", \"value\" : " + String(isOn) + "}";
    message.toCharArray(data, (message.length() + 1));
    client.publish(mqtt_device_value_to_set_topic, data);
  }
  if (strcmp(characteristic, "On") == 0)
  {
    int value = root["value"];
    if (value != 1 && value != 0)
    {
      return;
    }
    isOn = value;
    update();

    //set primary device status (For Homekit HeaterCooler service)
    char data[150];
    String message = "{\"name\" : \"" + device_name + "\", \"service_name\" : \"" + service_name + "\", \"characteristic\" : \"Active\", \"value\" : " + String(isOn) + "}";
    message.toCharArray(data, (message.length() + 1));
    client.publish(mqtt_device_value_to_set_topic, data);
  }

  if (strcmp(characteristic, "SwingMode") == 0)
  {
    int value = root["value"];
    if (value != 1 && value != 0)
    {
      return;
    }
    isSwing = value;
    update();
  }
  if (strcmp(characteristic, "RotationSpeed") == 0)
  {
    int value = root["value"];
    if (value < 0 || value > 3)
    {
      return;
    }
    fanSpeed = value;
    update();
  }
  /*
    // The value property of TargetHeaterCoolerState must be one of the following:
  Characteristic.TargetHeaterCoolerState.AUTO = 0;
  Characteristic.TargetHeaterCoolerState.HEAT = 1;
  Characteristic.TargetHeaterCoolerState.COOL = 2;
  */
  if (strcmp(characteristic, "TargetHeaterCoolerState") == 0)
  {
    int value = root["value"];
    if (value < 0 || value > 3)
    {
      return;
    }
    if (value == 0 || value == 2)
    {
      isCool = true;
    }
    else
    {
      isCool = false;
    }
    update();
  }

  updateServerValue();
}

void setAcNextDefaults()
{
  Serial.println("Setting AC next defaults");

  // Set up what we want to send.
  // See state_t, opmode_t, fanspeed_t, swingv_t, & swingh_t in IRsend.h for
  // all the various options.
  ac.next.protocol = decode_type_t::COOLIX;      // Set a protocol to use.
  ac.next.model = 1;                             // Some A/Cs have different models. Try just the first.
  ac.next.mode = stdAc::opmode_t::kCool;         // Run in cool mode initially.
  ac.next.celsius = true;                        // Use Celsius for temp units. False = Fahrenheit
  ac.next.degrees = 25;                          // 25 degrees.
  ac.next.fanspeed = stdAc::fanspeed_t::kMedium; // Start the fan at medium.
  ac.next.swingv = stdAc::swingv_t::kOff;        // Don't swing the fan up or down.
  ac.next.swingh = stdAc::swingh_t::kOff;        // Don't swing the fan left or right.
  ac.next.light = false;                         // Turn off any LED/Lights/Display that we can.
  ac.next.beep = false;                          // Turn off any beep from the A/C if we can.
  ac.next.econo = false;                         // Turn off any economy modes if we can.
  ac.next.filter = false;                        // Turn off any Ion/Mold/Health filters if we can.
  ac.next.turbo = false;                         // Don't use any turbo/powerful/etc modes.
  ac.next.quiet = false;                         // Don't use any quiet/silent/etc modes.
  ac.next.sleep = -1;                            // Don't set any sleep time or modes.
  ac.next.clean = false;                         // Turn off any Cleaning options if we can.
  ac.next.clock = -1;                            // Don't set any current time if we can avoid it.
  ac.next.power = false;                         // Initially start with the unit off.
}

void setup()
{
  Serial.begin(115200);

  // Setup buttons
  Serial.println("Setting up buttons");
  pinMode(btnPower, INPUT_PULLUP);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDn, INPUT_PULLUP);

  pinMode(2, OUTPUT);

  // Setup networking
  Serial.println("Setting up network");
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.autoConnect(autoconf_ssid, autoconf_pwd);
  setup_ota();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //Attach interrupt for manual button controls
  Serial.println("Attaching Interupt");
  attachInterrupt(digitalPinToInterrupt(btnPower), btnPowerPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(btnUp), btnUpInt, FALLING);
  attachInterrupt(digitalPinToInterrupt(btnDn), btnDnInt, FALLING);

  digitalWrite(2, HIGH);
  updateServerValue();

  //Setup IR Lib
  Serial.println("Setting up IR Lib");
  ac.next.protocol = protocols[0];
  setAcNextDefaults();
  irsend.begin();

  //Start the NTP UDP client
  Serial.println("Setting up NTP Client");
  timeClient.begin();

  Serial.println("Setup done!");
}

void handleCurrentAction()
{

  switch (currentAction)
  {
  case ACTION_UP:
  {
    increment();
    break;
  }
  case ACTION_DOWN:
  {
    decrement();
    break;
  }
  case ACTION_POWER:
  {
    power();
    break;
  }
  default:
  {
    break;
  }
  }

  currentAction = 0;
}

void loop()
{
  if (!client.connected() && (millis() - lastConnectingTime > 60000))
  {
    Serial.println("reconnect");
    reconnect();
    lastConnectingTime = millis();
  }
  client.loop();
  ArduinoOTA.handle();
  handleCurrentAction();
}