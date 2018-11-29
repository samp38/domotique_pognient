#include <CapacitiveSensor.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>


// WIFI & MQTT
WiFiClient espClient;
const char* mqtt_server = "192.168.1.102";
PubSubClient client(espClient);

// WIFI
const char* ssid = "**********";
const char* password = "*********";


// LIGHT PIN
#define LIGHT_PIN D2
unsigned int lightState = 1;
unsigned int counter = 0;

// CAPACITIVE SENSOR
CapacitiveSensor cs_14_12 = CapacitiveSensor(14, 12);       // 10M resistor between pins 14 (D5 on Wemos D1 mini) & 12 (D6), pin 12 is sensor pin, add a wire and or foil if desired

// DALLAS TEMP SENSOR
#define ONE_WIRE_BUS D7
#define TEMP_SCHEDULE_DELAY_MS 1000*60*2
#define TEMP_CONV_DELAY_MS 1500
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;
unsigned long lastTempScheduled = 0;
unsigned long lastTempRequest = 0;
float temperature = 0.0;
volatile bool tempPending = false;



bool CSread(unsigned int * _counter) {
  long cs = cs_14_12.capacitiveSensor(2);
//  Serial.print(cs);
  if (cs != -2) {
    *_counter += 1;
//    Serial.print("\t");
//    Serial.println(*_counter);
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



void turnLightOff(unsigned int * _state) {
	int i = 1024;
  	while (i >= 0) {
  	 	analogWrite(LIGHT_PIN, i);
  	  	i--;
  	  	delayMicroseconds(500);
  	}
  	analogWrite(LIGHT_PIN, 0);
  	*_state = 0;
}

void turnLightOn(unsigned int * _state) {
	int i = 0;
  	while (i < 1024) {
    	analogWrite(LIGHT_PIN, i);
    	i++;
    	delayMicroseconds(500);
  	}
  	*_state = 1;
}


void toggleLight(unsigned int * _state) {
  unsigned int newState;
  int i = 0;
  switch (*_state) {
    case 0:
      turnLightOn(&lightState);
      newState = 1;
      break;

    case 1:
      turnLightOff(&lightState);
      newState = 0;
      break;
  }
  client.publish("home/lights/barlight/state", *_state ? "ON" : "OFF");
  //Serial.print("From " + String(lightState) + " to " + String(newState));
  *_state = newState;
  //cs_14_12.reset_CS_AutoCal();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String payload_str = "";
  for (int i=0;i<length;i++) {
	payload_str += (char)payload[i];
  }
  if (payload_str == "ON") {
    turnLightOn(&lightState);
  } else if (payload_str == "OFF") {
    turnLightOff(&lightState);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), "openhabian", "lalalou84")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("home/lights/barlight/command");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
	Serial.begin(115200);
	pinMode(LIGHT_PIN, OUTPUT);
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
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
	cs_14_12.set_CS_AutocaL_Millis(0xFFFFFFFF);
	cs_14_12.set_CS_Timeout_Millis(80);
	sensors.begin();
	sensors.getAddress(tempDeviceAddress, 0);
	sensors.setResolution(tempDeviceAddress, 12);
	sensors.setWaitForConversion(false);
	lastTempScheduled = millis();
}

void loop()
{
	if (!client.connected()) {
    	reconnect();
  	}
	if (millis() - lastTempScheduled >= TEMP_SCHEDULE_DELAY_MS) {
		if(!tempPending) {
			sensors.requestTemperatures();
			lastTempRequest = millis();
			tempPending = true;
		}
		else {
			if(millis() - lastTempRequest >= TEMP_CONV_DELAY_MS) {
				temperature = sensors.getTempCByIndex(0);
				char payload[10];
				String payload_str = String(temperature);
				payload_str.toCharArray(payload, 10);
				client.publish("home/temperature/kitchen", payload);
				lastTempScheduled = millis();
				tempPending = false;
			}	
		}
	}
  	client.loop();
  	ArduinoOTA.handle();
 	if (CSread(&counter)) {
    	toggleLight(&lightState);
  	}
}
