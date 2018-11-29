#include "Adafruit_VL53L0X.h"
#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <FIR.h>

// WIFI
const char* ssid = "********";
const char* password = "********";;

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN    14      
// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS   33
uint8_t maxValue = 80;
uint8_t fadeOutValue = maxValue;
uint8_t currentValue = 0;
unsigned long lastTimeToggle = 0;
unsigned long lastFadeOutTick = 0;
#define FADEOUT_DURATION 3000
uint8_t state = 0; // 0 : OFF, 1 : ON, 2 : FADING OUT
#define DELAY_OFF_MS  5000
#define THRESHOLD 1000

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
FIR<uint32_t, 10> fir;

void setStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for(int i=0;i<NUMPIXELS;i++){

    // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(i, pixels.Color(r,g,b)); // Moderately bright green color.

    pixels.show(); // This sends the updated pixel color to the hardware.
  }
}

void fadeInSmooth(uint8_t from) {
	for(uint8_t i=from; i<=maxValue; i++) {
		setStripColor(i,i,i);
		delay(500/maxValue);
	}
	currentValue = maxValue;
}

bool presence(uint32_t _input) {
    if(_input < THRESHOLD) {
    	return true;
    }
	else {
		return false;
	}
}

void initFilter() {
	for(int i=0; i<20; i++) {
	    VL53L0X_RangingMeasurementData_t measure;
	    lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
	    if (measure.RangeStatus != 4) {  // phase failures have incorrect data
			fir.processReading(measure.RangeMilliMeter);
		}
	}
}

void setup() {
  Serial.begin(115200);

  // wait until serial port opens for native USB devices
  while (! Serial) {
    delay(1);
  }
  
  	WiFi.mode(WIFI_STA);
  	WiFi.begin(ssid, password);
  	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  		delay(3000);
  		ESP.restart();
  	}
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
  	ArduinoOTA.setHostname("FoodCloset"); // on donne une petit nom a notre module
  	ArduinoOTA.begin(); // initialisation de l'OTA
  
  	Serial.println("Adafruit VL53L0X test");
  	if (!lox.begin()) {
    	Serial.println(F("Failed to boot VL53L0X"));
    	while(1);
  	}
  	// This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  	if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  	// End of trinket special code

	pixels.begin(); // This initializes the NeoPixel library.
	setStripColor(0, 0, 0);
	// FIR coefs
	uint32_t coef[10] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	fir.setFilterCoeffs(coef);
	Serial.print("Gain set: ");
	Serial.println(fir.getGain());
	initFilter();
}

void loop() {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
    if (measure.RangeStatus != 4) {  // phase failures have incorrect data
		uint32_t output = fir.processReading(measure.RangeMilliMeter);
       	//Serial.println(String(measure.RangeMilliMeter) + ", " + String(output) + String(measure.RangeStatus));
        if(presence(measure.RangeMilliMeter)) {
			Serial.println("toggle presence");
        	lastTimeToggle = millis();
			if(state == 0) {
				Serial.println("\tfading in");
				fadeInSmooth(0);
				state = 1;
			}
			else if(state == 2) {
				fadeInSmooth(currentValue);
			}
        }
    }
	if(millis() - lastTimeToggle > DELAY_OFF_MS && state != 0) {
		// if ON, time to fade out
		if(state == 1) {
			Serial.println("Time to fade out");
			state = 2;
			lastFadeOutTick = millis();
		}
		else if(state == 2 && millis() - lastFadeOutTick > FADEOUT_DURATION/maxValue) {
			currentValue = currentValue - 1;
			// Serial.println("Fading out --> " + String(currentValue));
			setStripColor(currentValue, currentValue, currentValue);
			if(currentValue == 0) {
				state = 0;
				Serial.println("Turned OFF");
			}
			lastFadeOutTick = millis();
		}
	}
  	ArduinoOTA.handle();
}
