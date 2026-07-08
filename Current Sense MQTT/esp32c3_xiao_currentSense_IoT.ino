/* temp and humidity sensor, current probe (0-10A 120vAC) over MQTT, publishes a JSON formatted string to broker
prints wifi strength to json /telemtry
NOTE:A heavy trimmed down version of the ESP32-wroom iot playground board. Trimmed for a XIAO IoT board which is: Only temp sense, MqTT IoT comm, no motor drivers hardware 
The old LED and Relay functions are still present in code, but that hardware is not present, however the GPIO will still toggle via mqtt published value.

Uptime handling:
60s watchdog timer (will reboot ESP if hangs). For Wifi and MQTT connect issues. prints the reset reason over serial (not that helpful)
12hr hard reset (REBOOT_INTERVAL )

Code rev: July 7, 2026 
Hardware rev: June 4, 2025

Hardware: ESP32-C3 XIAO       digikey p/n= 1597-113991054-ND
Board: ESP32-C3 XIAO IoT
--------------------------------------------------------------------------
i2C Addresses and hardware details:
OLED                         0x3C
buck converter = 4.8V
SHT45 (temp humidity sensor) 0x44
NeoPixels 1 
---------------------------------------------------------------------------
Authors: Adafruit, Ryan Bates
Hardware: PCB=Ryan, 

*/
//Libraries used in this 
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ArduinoJson.h>
#include "Adafruit_SHT4x.h"  // <--- SHT45 Sensor Library
#include <GyverOLED.h>
GyverOLED<SSD1306_128x64> oled; //the small 0.97" screen
//GyverOLED<SSH1106_128x64> oled; //the med 1.3" screen
#include <esp_task_wdt.h>   // <--- Added for watchdog functionality

//Current sense 
const int ACPin1 = A1;
const int ACPin2 = A2;

#define ACTECTION_RANGE 10.0
#define VREF 3.29

const float CAL_FACTOR_1 = 0.97865; 
const float CAL_FACTOR_2 = 0.97610;

float probe1 = 0.0;
float probe2 = 0.0;

// -----------------------------------------------------
// Read AC current from a given analog pin
// -----------------------------------------------------
float readACCurrentValue(int analogPin)
{
  float adcSum = 0;

  // Average ADC samples
  for (int i = 0; i < 100; i++)
  {
    adcSum += analogRead(analogPin);
    delay(1);
  }

  float adcAvg = adcSum / 100.0;

  // Convert ADC -> "virtual RMS-ish voltage"
  float voltageVirtualValue = adcAvg * 0.707;

  // Convert ADC counts to volts, then account for divider/amplifier
  voltageVirtualValue = (voltageVirtualValue / 4096.0 * VREF) / 2.0;

  // Convert voltage to current
  float current = voltageVirtualValue * ACTECTION_RANGE;

  return current;
}

// Which pin on the Arduino is connected to the NeoPixels?
#define LED_PIN    20

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 1

// Declare our NeoPixel strip object:
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#define LEDpin 2 
#define RELAY1pin 3
String LEDpin_status = "OFF";

//************************12hr reboot**********************************/
#define REBOOT_INTERVAL 43200000UL  // 12 hours in milliseconds
unsigned long lastRebootCheck = 0;

/************************* WiFi Access Point **************************/
#define WLAN_SSID       "" //your wifi IF
#define WLAN_PASS       ""// your password

/************************* MQTT Broker Setup **************************/
#define AIO_SERVER      ""
#define AIO_SERVERPORT  
#define AIO_USERNAME    ""
#define AIO_KEY         ""

//************************MQTT feeds and topics setup*******************/
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

//direct publish data (this is mostly a redundant section when you see the json telemetry string).
Adafruit_MQTT_Publish Temperature = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/serverroom/temperature");
Adafruit_MQTT_Publish Humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/serverroom/humidity");
Adafruit_MQTT_Publish Voltage = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/kitchen/deviceBattery"); //this hardware is not present
Adafruit_MQTT_Publish Amp_Probe_1 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/serverroom/currentprobe1"); 
Adafruit_MQTT_Publish Amp_Probe_2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/serverroom/currentprobe2"); 

//you can publish to theses topics via MQTTX... see code that controls hardware
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/kitchen/LEDpin");
Adafruit_MQTT_Subscribe RELAY1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/kitchen/RELAY1pin");


#define CLIENT_TELEMETRY_TOPIC AIO_USERNAME "/feeds/serverroom/telemetry"


//*************************Tempurature sensor setup***********************/
Adafruit_SHT4x sht4 = Adafruit_SHT4x();  // <--- SHT45 object
float tempC = 0.0;
float humid = 0.0;


void setup() {
  delay(100);
  pixels.setPixelColor(0, pixels.Color(50, 0, 0)); //Sets RGB values from 0 (off) to 255 (max) brightness
  pixels.show(); 
  delay(1000); //short pause to show a red status and initalizing

  oled.init(); oled.clear(); oled.setScale(1); //1 is small, 4 is huge 
  oled.setCursorXY(0, 0); 
  oled.print(" ESP32-C3 XIAO IoT");
  oled.update();

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear(); // Set all pixel colors to 'off'

  Serial.begin(115200);
  // Wait until serial port is opened
  //while (!Serial) { delay(10); }


  pinMode(LEDpin, OUTPUT);
  pinMode(RELAY1pin, OUTPUT);

  // Get MAC address and print it
  String mac = WiFi.macAddress();
  Serial.print("ESP32 MAC Address: ");
  Serial.println(mac);
  oled.setCursorXY(0,26);
  oled.print("MAC "); oled.print(mac); 
  oled.rect(0, 9, 127, 10);  // a 1-pixel tall rectangle
  oled.update();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  oled.setCursorXY(0,14);
  oled.print("WiFi@ " + WiFi.localIP().toString() );
  oled.update();
  pixels.setPixelColor(0, pixels.Color(10, 10, 0)); //Sets RGB values from 0 (off) to 255 (max) brightness
  pixels.show();  //go yellow light after wifi is connected

  mqtt.subscribe(&onoffbutton);
  mqtt.subscribe(&RELAY1);

  // Initialize SHT45 sensor
  Serial.println("Initializing SHT4x...");
  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(10);
  }
//temp and humidity accuracy/ power/ read time. pick only one
  //sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setPrecision(SHT4X_MED_PRECISION);
  //sht4.setPrecision(SHT4X_LOW_PRECISION);

  sht4.setHeater(SHT4X_NO_HEATER);
  // a built-in resistive heater can turn on temporarily to warm the sensor, Evaporate condensation....
  Serial.println("SHT4x sensor ready.");



  // ===== Watchdog setup (added) =====
  esp_task_wdt_init(60, true);    // 60 sec timeout, auto-reset if not fed
  esp_task_wdt_add(NULL);         // Add main loop task to watchdog
  Serial.println("Watchdog initialized (60s timeout)");
}

uint32_t x = 0, y = 0; //counters to track how many times 
//temperature (x) and humidity (y) values are published to the MQTT broker 

void loop() {
  esp_task_wdt_reset();  // feed watchdog at start of loop

  // ===== WiFi Reconnect Check =====
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    unsigned long startAttemptTime = millis();

    // try for 10s max
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
      esp_task_wdt_reset();   // feed watchdog during reconnect loop
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi, IP: " + WiFi.localIP().toString());
    } else {
      Serial.println("\nWiFi reconnect failed.");
    }
  }

    currentSenseRead();  //get the current probe values


  // ===== MQTT Reconnect Check =====
  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected, reconnecting...");
    MQTT_connect();   // your existing connection handler
  }

  // ===== Keep MQTT alive =====
  mqtt.processPackets(10);  // non-blocking subscription handling
  mqtt.ping();              // keeps broker connection alive

  get_SHT45_data();         // Read SHT45 data
  json_telemetry();         // Package and send telemetry
  esp_task_wdt_reset();     // feed after telemetry work

//=====================Hardware Toggle Capable topics==================================================================================
//if published topics match these checks, do something with the hardware
  Adafruit_MQTT_Subscribe *subscription; //check for telemetry subscription data (5sec) sent to the ESP
  while ((subscription = mqtt.readSubscription(5000))) {
    esp_task_wdt_reset(); // feed watchdog while waiting for subscriptions
    if (subscription == &onoffbutton) {
      if (strcmp((char *)onoffbutton.lastread, "1") == 0) {
        digitalWrite(LEDpin, HIGH);
        Serial.println("LED ON");
      } else {
        digitalWrite(LEDpin, LOW);
        Serial.println("LED OFF");
      }
    }

    if (subscription == &RELAY1) {
      if (strcmp((char *)RELAY1.lastread, "1") == 0) {
        digitalWrite(RELAY1pin, HIGH);
        Serial.println("Relay ON");
      } else {
        digitalWrite(RELAY1pin, LOW);
        Serial.println("Relay OFF");
      }
    }
  }
//==============block that publishes the solo temp data=========================
  Serial.print("\nPublishing temperature... ");
  if (!Temperature.publish(tempC)) {
    Serial.println("Failed");
  } else {
    Serial.println("OK");
    x++;
  }
//==============block that publishes the solo humidity data=========================
  Serial.print("Publishing humidity... ");
  if (!Humidity.publish(humid)) {
    Serial.println("Failed");
  } else {
    Serial.println("OK");
    y++;
  }




  esp_task_wdt_reset();  // final feed before next loop iteration





// ================================== 12-Hour Auto Reboot ================================
if (millis() - lastRebootCheck > REBOOT_INTERVAL) {
  Serial.println("\n=== 12-HOUR AUTO REBOOT INITIATED ===");
  delay(1000);
  ESP.restart();   // perform a clean system reboot
}


}

void currentSenseRead(){
  probe1 = readACCurrentValue(ACPin1) * CAL_FACTOR_1;
  probe2 = readACCurrentValue(ACPin2) * CAL_FACTOR_2;

  Serial.print("P1: ");
  Serial.print(probe1, 2);
  Serial.print(" A   P2: ");
  Serial.print(probe2, 2);
  Serial.println(" A");

  //screenUpdate(probe1, probe2);
}

// === SHT45 Sensor Reading ===
void get_SHT45_data() {
  sensors_event_t humidity_event, temp_event;
  uint32_t timestamp = millis();

  sht4.getEvent(&humidity_event, &temp_event);
  timestamp = millis() - timestamp;

  tempC = temp_event.temperature;
  humid = humidity_event.relative_humidity;

  Serial.print("Temperature: "); Serial.print(tempC); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(humid); Serial.println(" %");
  Serial.print("Read duration (ms): "); Serial.println(timestamp);

  oled.setCursorXY(0,40);
  oled.print(tempC); oled.print(" C  ");
  oled.print(humid); oled.print(" % H");
  oled.setCursorXY(0,50); 
  oled.print("P1:"); oled.print(probe1,2); oled.print("A  ");
  oled.print("P2:"); oled.print(probe1,2); oled.print("A");
  oled.update();
}

// === Telemetry JSON Pack + Publish ===
void json_telemetry() {
  LEDpin_status = digitalRead(LEDpin) ? "ON" : "OFF";

  DynamicJsonDocument doc(1024);
  //doc["sensorType"] = "SHT45";
  doc["Location"] = "Kitchen";
  doc["temperature"] = tempC;
  doc["humidity"] = humid;
  doc["currentprobe1"] = probe1;
  doc["currentprobe2"] = probe2;
  doc["uptime"] = (millis() / 1000);
  //doc["LEDpinStatus"] = LEDpin_status;
  //doc["RelayPinStatus"] = digitalRead(RELAY1pin);
  

  // === WiFi Signal Information ===
  int rssi = WiFi.RSSI();  // dBm value
  int wifiStrength = constrain(map(rssi, -90, -30, 0, 100), 0, 100);
  doc["AV WiFi RSSI (dBm)"] = rssi;          // e.g. -67 [-45 = very strong, -80 = weak]
  doc["AV WiFi Signal (%)"] = wifiStrength;  // e.g. 75%

  // add more data strings here

  String telemetry;
  serializeJson(doc, telemetry);
  Serial.print("Sending telemetry: ");
  Serial.println(telemetry);

  if (!mqtt.publish(CLIENT_TELEMETRY_TOPIC, telemetry.c_str())) {
    Serial.println("Failed to send telemetry");
  } else {
    Serial.println("Telemetry sent successfully");
  }
}


// === MQTT Connection Handler ===
void MQTT_connect() {
  int8_t ret;

  if (mqtt.connected()) return;

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.print("Failed, code: ");
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(5000);
    esp_task_wdt_reset();   // feed watchdog during retry delays
    if (--retries == 0) {
      Serial.println("Giving up on MQTT.");
      while (1);
    }
  }
  Serial.println("MQTT connected!");

  pixels.setPixelColor(0, pixels.Color(0, 20, 0)); //Sets RGB values from 0 (off) to 255 (max) brightness
  pixels.show(); //go green led status if MQTT is connected

}


//built from the Adafruit MQTT library example
/***********************************************************************
  Adafruit MQTT Library ESP32 Adafruit IO SSL/TLS example

  Use the latest version of the ESP32 Arduino Core:
    https://github.com/espressif/arduino-esp32

  Works great with Adafruit Huzzah32 Feather and Breakout Board:
    https://www.adafruit.com/product/3405
    https://www.adafruit.com/products/4172

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  Modified by Brent Rubell for Adafruit Industries
  MIT license, all text above must be included in any redistribution
 **********************************************************************/
