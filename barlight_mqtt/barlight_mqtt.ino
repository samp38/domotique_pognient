#include <CapacitiveSensor.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "SmoothieWiFi";
const char* password = "lalalou84";

#define LIGHT_PIN D2

unsigned int threshold = 0;
unsigned int resolution = 1;
unsigned int lightState = 1;
unsigned int counter = 0;


CapacitiveSensor cs_14_12 = CapacitiveSensor(14, 12);       // 10M resistor between pins 14 (D5 on Wemos D1 mini) & 12 (D6), pin 12 is sensor pin, add a wire and or foil if desired


bool CSread(unsigned int * _counter) {
  long cs = cs_14_12.capacitiveSensor(2);
  Serial.print(cs);
  if (cs != -2) {
    *_counter += 1;
    Serial.print("\t");
    Serial.println(*_counter);
    if (*_counter >= 50) {
      *_counter = 0;
      return true;
    }
    else {
      //Serial.println(*_counter);
      return false;
    }
  }
  else {
    *_counter = 0;
    return false;
  }
}


void toggleLight(unsigned int * _state) {
  unsigned int newState;
  int i = 0;
  switch (*_state) {
    case 0:
      while (i < 1024) {
        analogWrite(LIGHT_PIN, i);
        i++;
        delayMicroseconds(500);
      }
      newState = 1;
      break;

    case 1:
      i = 1023;
      while (i >= 0) {
        analogWrite(LIGHT_PIN, i);
        i--;
        delayMicroseconds(500);
      }
      analogWrite(LIGHT_PIN, 0);
      newState = 0;
      break;
  }
  //Serial.print("From " + String(lightState) + " to " + String(newState));
  *_state = newState;
  //cs_14_12.reset_CS_AutoCal();
}

void setup()
{
  Serial.begin(115200);
  toggleLight(&lightState);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(3000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("kitchenLeds"); // on donne une petit nom a notre module
  ArduinoOTA.begin(); // initialisation de l'OTA
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  cs_14_12.set_CS_AutocaL_Millis(0xFFFFFFFF);
  cs_14_12.set_CS_Timeout_Millis(80);
  pinMode(LIGHT_PIN, OUTPUT);
}

void loop()
{
  ArduinoOTA.handle();
  if (CSread(&counter)) {
    toggleLight(&lightState);
  }
}
