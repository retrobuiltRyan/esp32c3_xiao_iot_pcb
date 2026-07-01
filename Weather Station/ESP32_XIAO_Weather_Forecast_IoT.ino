
```cpp
//Weather forcast dashboard json data edge device
//Hardware: ESP32-C3 XIAO + Custom PCB.
//Board Manager ID: XIAO_ESP32C3 (if com fails 'error status 2' you need to press on XIAO Boot+resert then release Reset>boot))
// Flash Memory 4MB 
// SRAM 400KB

/* calls API "https://api.weather.gov/points/40.4406,-79.9959"
Then grabs weather forecast data from https://api.weather.gov/gridpoints/PBZ/78,66/forecast
parses only the first five 'periods' to save SRAM and prints over serial and OLED.
Forecast updates every hour. 
Also grabs the time over NTP (time.h) ****need to fix this, it will get booted off enterprise networks
RGB_LEDs turn on for extra razzle-dazzle
Code:  rev: Feb  7, 2026
 + hardware
--------------------------------------------------------------------------
API things to know
day-day forcast summary: 
https://api.weather.gov/gridpoints/PBZ/78,66/forecast

detailed plottable temp, wind, percip%:
https://api.weather.gov/gridpoints/PBZ/78,66

--------------------------------------------------------------------------
i2C Addresses and hardware details:
OLED                         0x3C
---------------------------------------------------------------------------
Authors: Ryan,
Hardware: 
*/

// ======================= To DO ==========================
// remove the NTP sync. is blockable on entreprise networks
// =======================================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <GyverOLED.h>
#include <time.h>  // <-- Include time functions

//============NeoPixel Setup============================================
// Which pin on the Arduino is connected to the NeoPixels?
#define LED_PIN    20   // ESP32-C3 CHANGE (safe GPIO on XIAO)
// How many NeoPixels are attached to the Arduino?
#define LED_COUNT 1
// Declare our NeoPixel strip object:
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Built-in LED
#define LEDpin 21   // ESP32-C3 CHANGE (XIAO onboard LED)

// OLED setup
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
//GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled; //the big screen

// =========================== WiFi Config =============================
const char* ssid = "";   // your SSID
const char* password = ""; // your password

// =========================== Weather API Config =============================
const char* host = "api.weather.gov";
const int httpsPort = 443;
WiFiClientSecure client;

// =========================== Globals =============================
unsigned long lastFetch = 0;
const unsigned long interval = 3600000; // 1 hour

// ===================== Forecast Display Config (NEW) =====================
#define MAX_PERIODS 5           //how many days or chunks of forecast data to collect (biggest culprit of DRAM usage)
#define FORECAST_DELAY_MS 6000   // <-- adjust per-forecast screen delay

#define OLED_COLS 21   // approx chars per line @ scale 1
#define OLED_ROWS 8

struct ForecastPeriod {
  String name;
  String text;
  int precip;      // probability %
  int temperature; // optional, useful later
  int windSpeed;     // mph (numeric)
};

ForecastPeriod forecasts[MAX_PERIODS];
int forecastCount = 0;
int currentPeriod = 0;

// Timezone offset (e.g. UTC-5 hours for EST)
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 3600;  // 1 hour daylight saving

const static uint8_t icons_rain_8x8[][8] PROGMEM = {
  {0x10, 0x38, 0x7C, 0x7C, 0x38, 0x10, 0x28, 0x10}  // rain drop 🌧️
  // Explanation:
  // 0x10 = 00010000    -> top point of drop
  // 0x38 = 00111000    -> upper curve
  // 0x7C = 01111100    -> middle wide
  // 0x7C = 01111100
  // 0x38 = 00111000
  // 0x10 = 00010000    -> bottom point
  // 0x28 = 00101000    -> tiny rain trail
  // 0x10 = 00010000    -> final point of trail
};


void setup() {
  // ESP32-C3 explicit I2C pins (XIAO defaults)
  Wire.begin(); 

  Serial.begin(115200);
  pinMode(LEDpin, OUTPUT);
  delay(100);

  String mac = WiFi.macAddress();
  Serial.print("ESP32 MAC Address: ");
  Serial.println(mac);

  pixels.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear(); // Set all pixel colors to 'off'

  pixels.setPixelColor(0, pixels.Color(10, 0, 0));
  pixels.setPixelColor(1, pixels.Color(0, 10, 0));
  pixels.setPixelColor(2, pixels.Color(5, 5, 0));
  pixels.setPixelColor(3, pixels.Color(0, 0, 10));
  pixels.show(); 

  oled.init();
  oled.setContrast(255);
  oled.setScale(1);
  oled.clear();
  oled.setCursor(0, 0);
  oled.print("Connecting...");
  oled.update();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ Connected to WiFi!");
  oled.clear();
  Serial.println("\nWiFi connected. IP address: " + WiFi.localIP().toString());
  oled.setCursorXY(0,16);
  oled.print("WiFi@ " + WiFi.localIP().toString() );
  oled.update();

  oled.setCursorXY(0,28);
  oled.print("MAC "); oled.print(mac); 
  oled.rect(0, 9, 127, 10);
  oled.setCursorXY(0,40); oled.print("Fetching forecast..");
  oled.update();

  client.setInsecure(); // Disable certificate validation

  // ====== Setup NTP time sync =======
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  Serial.println("Waiting for time synchronization...");
  oled.setCursor(0, 26);
  oled.print("Fetching NTP Time...");
  oled.update();

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
    oled.print(".");
    oled.update();
  }
  Serial.println("\nTime synchronized!");

  fetchForecast();  // Initial forecast
}

// =============== New function to show current time on OLED ===============
void showCurrentTime() {
  struct tm timeinfo;
  oled.setScale(1);
  if (getLocalTime(&timeinfo)) {
    digitalWrite(LEDpin, HIGH);
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Time:");
    oled.setCursor(0, 2);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d", &timeinfo);
    oled.print(timeStr);
    oled.setCursor(0, 4);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    oled.print(timeStr);
    oled.update();
  } else {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Time not");
    oled.setCursor(0, 2);
    oled.print("available");
    oled.update();
  }
  delay(350);
  digitalWrite(LEDpin, LOW);
}

// ===================== OLED Word Wrap (NEW) =====================
void drawWrappedText(const String& text, int startRow) {
  int row = startRow;
  int idx = 0;

  while (idx < text.length() && row < OLED_ROWS) {
    int lineEnd = idx + OLED_COLS;
    if (lineEnd >= text.length()) {
      oled.setCursor(0, row);
      oled.print(text.substring(idx));
      break;
    }

    int space = text.lastIndexOf(' ', lineEnd);
    if (space <= idx) space = lineEnd;

    oled.setCursor(0, row);
    oled.print(text.substring(idx, space));
    idx = space + 1;
    row++;
  }
}

// ===================== Static Forecast Display (NEW) =====================
void showForecastPeriod(int index) {
  if (index >= forecastCount) return;

  oled.clear();
  oled.setCursor(0, 0);
  oled.print(forecasts[index].name);
  oled.line(0, 9, 127, 9);
  drawWrappedText(forecasts[index].text, 2);
    // Page indicator (bottom-right)
  drawTimelineDots(index, forecastCount);
  drawPrecipBar(forecasts[index].precip);
  drawWindBar(forecasts[index].windSpeed);
  oled.update();
}

// =========================== Main Loop ===============================================================================
void loop() {
  unsigned long now = millis();

  if (now - lastFetch >= interval || lastFetch == 0) {
    Serial.println("\n🔄 Fetching updated forecast...");
    fetchForecast();
    lastFetch = now;
  }

  //showCurrentTime();
  delay(2500);

  showForecastPeriod(currentPeriod);
  delay(FORECAST_DELAY_MS);

  currentPeriod++;
  if (currentPeriod >= forecastCount) currentPeriod = 0;
}

// =========================== Format Forecast =============================
void printFormattedForecast(JsonArray periods) {
  forecastCount = min((int)periods.size(), MAX_PERIODS);
  Serial.println("\n========== 🌦️ Weather Forecast ==========\n");

  for (int i = 0; i < forecastCount; i++) {
    JsonObject p = periods[i];

    forecasts[i].name = p["name"] | "N/A";

    int temp = p["temperature"] | 0;
    String unit = p["temperatureUnit"] | "F";
    int precip = p["probabilityOfPrecipitation"]["value"] | 0;
    //String wind = p["windSpeed"] | "";  //just string value, not using because dping bar graph visual instead
    String windDir = p["windDirection"] | "";
    String shortForecast = p["shortForecast"] | "";
    forecasts[i].precip = precip;
    forecasts[i].temperature = temp;

String windStr = p["windSpeed"] | "";
int windSpeed = 0;

// Extract first number from "5 mph" or "5 to 10 mph"
for (int j = 0; j < windStr.length(); j++) {
  if (isDigit(windStr[j])) {
    windSpeed = windStr.substring(j).toInt();
    break;
  }
}

forecasts[i].windSpeed = windSpeed;

if (windStr.length())
  forecasts[i].text += " Wind " + windDir + " " + windStr + ".";



    forecasts[i].text = String(temp) + "" + unit; //oled has issue displaying the degree sign °
    if (precip > 0) forecasts[i].text += ", " + String(precip) + "% precip.";
    if (windStr.length()) forecasts[i].text += " Wind " + windDir + " " + windStr + ".";
    forecasts[i].text += " " + shortForecast;

    Serial.println("--------------------------------------------------");
    Serial.println(forecasts[i].name);
    Serial.println(forecasts[i].text);
  }

  Serial.println("\n==========================================");
}

// =========================== Fetch Forecast =============================
void fetchForecast() {
  if (!client.connect(host, httpsPort)) {
    Serial.println("❌ Connection failed (step 1)");
    return;
  }

  String url = "/points/40.4406,-79.9959";
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, client)) {
    client.stop();
    return;
  }

  const char* forecastUrl = doc["properties"]["forecast"];
  client.stop();
  delay(500);

  if (!client.connect(host, httpsPort)) return;

  String path = String(forecastUrl);
  path.replace("https://api.weather.gov", "");
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP32\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  StaticJsonDocument<1024> filter; // bytes per forecast period if you bump to 10 forecast periods, change to 2048 
  for (int i = 0; i < MAX_PERIODS; i++) {
    filter["properties"]["periods"][i]["name"] = true;
    filter["properties"]["periods"][i]["temperature"] = true;
    filter["properties"]["periods"][i]["temperatureUnit"] = true;
    filter["properties"]["periods"][i]["shortForecast"] = true;
    filter["properties"]["periods"][i]["windSpeed"] = true;
    filter["properties"]["periods"][i]["windDirection"] = true;
    filter["properties"]["periods"][i]["probabilityOfPrecipitation"]["value"] = true;
  }

  DynamicJsonDocument forecastDoc(2048);
  if (!deserializeJson(forecastDoc, client, DeserializationOption::Filter(filter))) {
    JsonArray periods = forecastDoc["properties"]["periods"];
    if (!periods.isNull()) printFormattedForecast(periods);
  }

  client.stop();
}

// //=====================page number ==================
// void drawPageIndicator(int current, int total) {
//   oled.setScale(1);

//   String page = String(current) + "/" + String(total);

//   // Right-aligned bottom corner
//   int col = 21 - page.length();  // 21 chars wide @ scale 1
//   int row = OLED_ROWS - 1;       // bottom row (7)

//   oled.setCursor(0, 54); //was (col, row); still deciding on style
//   oled.print(page);
// }

//===================weather icon graphics==============================
// Draw icon from array
void drawRainIcon(int x, int y) {
  size_t s = sizeof(icons_rain_8x8[0]);
  oled.setCursorXY(x, y);
  for (unsigned int i = 0; i < s; i++) {
    oled.drawByte(pgm_read_byte(&(icons_rain_8x8[0][i])));
  }
}




//==========precipitation bar garph visual==================
void drawPrecipBar(int percent) {
  int barWidth = map(percent, 0, 100, 0, 120);
  oled.rect(34, 60, 124, 63, OLED_STROKE); // outline, which is kinda big 
  oled.rect(34, 60, 34 + barWidth, 63, OLED_FILL);
    //drawRainIcon(0, 55);
  oled.setCursorXY(6, 56);  oled.print("p"); //bar graph is a bit small to get text next to it
}
//=========wind bar graph==================================
void drawWindBar(int mph) {
  mph = constrain(mph, 0, 30); // map a percentant with a range 0-30mph
  int barWidth = map(mph, 0, 30, 0, 120);
  oled.rect(34, 55, 124, 58, OLED_STROKE);  // Outline
  oled.rect(34, 55, 34 + barWidth, 58, OLED_FILL); // Fill
  oled.setCursorXY(0, 52);  // Label
  oled.print("w");
  //oled.print(mph);
  //oled.print("mph");
}


//============page number but as dots========================
void drawTimelineDots(int currentIndex, int totalPeriods) {
  const int dotRadius = 2;      // radius of each dot
  const int spacing = 6;        // space between dots
  const int y = 4;              // 4 pixels from top
  int xStart = 128 - (totalPeriods * spacing) - 2; // right-aligned with 2px padding

  for (int i = 0; i < totalPeriods; i++) {
    int x = xStart + i * spacing;

    if (i == currentIndex)
      oled.circle(x, y, dotRadius, OLED_FILL);   // filled = current period
    else
      oled.circle(x, y, dotRadius, OLED_STROKE); // outline = future periods
  }
}




```
