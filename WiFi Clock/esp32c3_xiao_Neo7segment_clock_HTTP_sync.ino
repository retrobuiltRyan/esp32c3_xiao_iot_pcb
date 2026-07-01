
```cpp
/*
ESP32-C3 XIAO NeoPixel Clock (HTTP Time Sync with Manual UTC + DST Handling)

Hardware:
Seeed XIAO ESP32-C3
Neo7Segment digits (29 LEDs per digit)

----------------------------------------------------
• HTTP time sync (no UDP / no NTP)
• No blocking waits
• Free-running fallback clock
• Smooth color transitions

DST fix implimented, but im not sure this code is good.
the esp8266 is to be avoided.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

// ================= NeoPixel Setup =================
#define LED_PIN    20
#define NUM_DIGITS 4
#define LEDS_PER_DIGIT 29

// LED #0 is the onboard RGB LED
#define LED_COUNT  (1 + (NUM_DIGITS * LEDS_PER_DIGIT))

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ================= Wi-Fi ==========================
const char* ssid = "CMU-DEVICE";
const char* password = "";

// ================= Clock Settings =================
bool use24HourFormat = false;

// ================= Time State =====================
time_t baseEpoch = 0;
unsigned long baseMillis = 0;
bool timeValid = false;

// ================= Segment Maps ===================
const byte segmentMap[7][4] = {
  { 0, 1, 2, 3 },     // A
  { 4, 5, 6, 7 },     // B
  { 8, 9, 10,11 },    // C
  { 12,13,14,15 },    // D
  { 16,17,18,19 },    // E
  { 20,21,22,23 },    // F
  { 24,25,26,27 }     // G
};

const byte DP_OFFSET = 28;

const byte digitSegments[10][7] = {
  {1,1,1,1,1,1,0},
  {0,1,1,0,0,0,0},
  {1,1,0,1,1,0,1},
  {1,1,1,1,0,0,1},
  {0,1,1,0,0,1,1},
  {1,0,1,1,0,1,1},
  {1,0,1,1,1,1,1},
  {1,1,1,0,0,0,0},
  {1,1,1,1,1,1,1},
  {1,1,1,1,0,1,1}
};

// ================= HTTP TIME SYNC =================
bool syncTimeHTTP() {
  WiFiClient client;

  Serial.println("🌐 Fetching time via HTTP header...");

  if (!client.connect("google.com", 80)) {
    Serial.println("❌ Connection failed");
    return false;
  }

  client.print(
    "GET / HTTP/1.1\r\n"
    "Host: google.com\r\n"
    "Connection: close\r\n\r\n"
  );

  unsigned long start = millis();
  while (!client.available()) {
    if (millis() - start > 5000) {
      Serial.println("❌ Timeout waiting for response");
      client.stop();
      return false;
    }
    delay(10);
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');

    if (line.startsWith("Date: ")) {
      Serial.print("📅 Raw date: ");
      Serial.println(line);

      struct tm t = {};

      if (strptime(
            line.c_str() + 6,
            "%a, %d %b %Y %H:%M:%S",
            &t)) {

#ifdef __USE_BSD
        time_t epoch = timegm(&t);
#else
        char *oldTZ = getenv("TZ");
        setenv("TZ", "UTC0", 1);
        tzset();
        time_t epoch = mktime(&t);

        if (oldTZ)
          setenv("TZ", oldTZ, 1);
        else
          unsetenv("TZ");

        tzset();
#endif

        baseEpoch = epoch;
        baseMillis = millis();
        timeValid = true;

        Serial.printf("✅ Time synced: %lu\n", (unsigned long)epoch);

        client.stop();
        return true;
      }
    }
  }

  Serial.println("❌ Date header not found");
  client.stop();
  return false;
}

// ================= Time Getter ====================
bool getCurrentTime(struct tm &timeinfo) {
  if (!timeValid) return false;

  // Current UTC time
  time_t now = baseEpoch + (millis() - baseMillis) / 1000;

  // Apply EST offset
  time_t local = now - (5 * 3600); //============================================EST OFFSET

  struct tm t;
  gmtime_r(&local, &t);

  // ---- DST detection (US rules) ----
  bool isDST = false;

  int month = t.tm_mon + 1;
  int day = t.tm_mday;
  int wday = t.tm_wday;

  if (month > 3 && month < 11) {
    isDST = true;
  }
  else if (month == 3) {
    if (day - wday >= 8) isDST = true;
  }
  else if (month == 11) {
    if (day - wday < 1) isDST = true;
  }

  if (isDST) {
    local += 3600;
    gmtime_r(&local, &t);
  }

  timeinfo = t;
  return true;
}

// ================= Color Helper ===================
uint32_t interpolateColor(uint32_t c1, uint32_t c2, float f) {
  uint8_t r1 = c1 >> 16, g1 = c1 >> 8, b1 = c1;
  uint8_t r2 = c2 >> 16, g2 = c2 >> 8, b2 = c2;

  return pixels.Color(
    r1 + (r2 - r1) * f,
    g1 + (g2 - g1) * f,
    b1 + (b2 - b1) * f
  );
}

// ================= Display ========================
void showDigitAt(byte digit, byte pos, bool dp, uint32_t color) {

  // LED #0 is onboard RGB LED on XIAO ESP32-C3
  int base = 1 + (pos * LEDS_PER_DIGIT);

  for (int i = 0; i < LEDS_PER_DIGIT; i++)
    pixels.setPixelColor(base + i, 0);

  for (int s = 0; s < 7; s++)
    if (digitSegments[digit][s])
      for (int i = 0; i < 4; i++)
        pixels.setPixelColor(base + segmentMap[s][i], color);

  if (dp)
    pixels.setPixelColor(base + DP_OFFSET, pixels.Color(255, 0, 0));
}

void showNumber(int num, bool dp, uint32_t color) {
  for (int i = NUM_DIGITS - 1; i >= 0; i--) {
    showDigitAt(num % 10, i, (dp && i == 1), color);
    num /= 10;
  }

  pixels.show();
}

// ================= SETUP ==========================
void setup() {
  Serial.begin(115200);
  delay(100);

  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();

  // onboard RGB LED (#0)
  pixels.setPixelColor(0, pixels.Color(0, 0, 32));
  pixels.show();

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    pixels.setPixelColor(0, pixels.Color(0, 32, 0));
    pixels.show();

    Serial.println("\n✅ Wi-Fi connected");
    Serial.println(WiFi.localIP());

    syncTimeHTTP();
  }
  else {

    pixels.setPixelColor(0, pixels.Color(32, 0, 0));
    pixels.show();

    Serial.println("\n❌ Wi-Fi failed");
  }
}

// ================= LOOP ===========================
void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastResync = 0;

  if (millis() - lastResync > 6UL * 3600UL * 1000UL) {
    lastResync = millis();
    syncTimeHTTP();
  }

  if (millis() - lastUpdate < 1000)
    return;

  lastUpdate = millis();

  struct tm t;

  if (!getCurrentTime(t))
    return;

  int hour = t.tm_hour;

  if (!use24HourFormat) {
    hour = hour % 12;

    if (hour == 0)
      hour = 12;
  }

  int display = hour * 100 + t.tm_min;

  bool blink = (t.tm_sec % 2 == 0);

  int mins = t.tm_hour * 60 + t.tm_min;

  uint32_t color;

  if (mins < 540)
    color = pixels.Color(128, 0, 128);

  else if (mins < 720)
    color = interpolateColor(
      pixels.Color(255,0,0),
      pixels.Color(0,255,0),
      (mins - 540) / 180.0
    );

  else if (mins < 1020)
    color = interpolateColor(
      pixels.Color(0,255,0),
      pixels.Color(0,0,255),
      (mins - 720) / 300.0
    );

  else
    color = interpolateColor(
      pixels.Color(0,0,255),
      pixels.Color(128,0,128),
      (mins - 1020) / 420.0
    );

  showNumber(display, blink, color);
}
```
