
```cpp
/*
ESP32-C3 XIAO NeoPixel Countdown Timer
Think Task Master style countdown.
BOOT → brightness adjust + pulsing 00:00 →press [set min] press→ [set hr] →press [start]
1 press → set minutes
2 press → set hours
3 press → countdown start

Color:
GREEN → YELLOW (first half)
YELLOW → RED (second half)
*/

#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>
#include <Adafruit_NeoPixel.h>

// ================= ENCODER ========================
Encoder myEnc(21, 9);
#define encoder_button 8

// ================= DISPLAY ========================
#define LED_PIN 20
#define NUM_DIGITS 4
#define LEDS_PER_DIGIT 29
#define LED_COUNT (1 + (NUM_DIGITS * LEDS_PER_DIGIT))

Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ================= STATE ==========================
int brightnessLevel = 100;

int setHour = 0;
int setMinute = 0;

unsigned long startMillis = 0;
unsigned long totalSeconds = 0;

bool countdownRunning = false;

// ================= MODE ===========================
enum Mode {
  MODE_BOOT,
  MODE_SET_MINUTES,
  MODE_SET_HOURS,
  MODE_RUN,
  MODE_FINISHED
};

Mode mode = MODE_BOOT;

// ================= ENCODER ========================
long lastEnc = 0;

// ================= BUTTON =========================
bool lastBtn = HIGH;
bool stableBtn = HIGH;
unsigned long debounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long pressTime = 0;
bool longHandled = false;

// ================= SEGMENT MAP ====================
const byte segmentMap[7][4] = {
  {0,1,2,3}, {4,5,6,7}, {8,9,10,11},
  {12,13,14,15}, {16,17,18,19}, {20,21,22,23},
  {24,25,26,27}
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

// ================= DISPLAY ========================
void clearDigit(byte pos) {
  int base = 1 + (pos * LEDS_PER_DIGIT);
  for (int i = 0; i < LEDS_PER_DIGIT; i++)
    pixels.setPixelColor(base + i, 0);
}

void showDigit(byte digit, byte pos, bool dp, uint32_t color) {
  int base = 1 + (pos * LEDS_PER_DIGIT);
  clearDigit(pos);

  for (int s = 0; s < 7; s++) {
    if (digitSegments[digit][s]) {
      for (int i = 0; i < 4; i++) {
        pixels.setPixelColor(base + segmentMap[s][i], color);
      }
    }
  }

  if (dp)
    pixels.setPixelColor(base + DP_OFFSET, pixels.Color(255, 0, 0));
}

void showNumber(int h, int m, bool colon, uint32_t color) {
  int d0 = h / 10;
  int d1 = h % 10;
  int d2 = m / 10;
  int d3 = m % 10;

  showDigit(d0, 0, false, color);
  showDigit(d1, 1, colon, color);
  showDigit(d2, 2, false, color);
  showDigit(d3, 3, false, color);

  pixels.show();
}

// ================= COLOR ENGINE ====================
uint32_t countdownColor(float progress) {
  // progress: 1.0 = start (green), 0.0 = end (red)

  uint16_t green  = 21845;
  uint16_t yellow = 10922;
  uint16_t red    = 0;

  uint16_t hue;

  if (progress > 0.5) {
    float t = (progress - 0.5) * 2.0;
    hue = yellow + (green - yellow) * t;
  } else {
    float t = progress * 2.0;
    hue = red + (yellow - red) * t;
  }

  return pixels.ColorHSV(hue);
}

// ================= TIME ===========================
unsigned long remainingSeconds() {
  if (!countdownRunning) return 0;

  unsigned long elapsed = (millis() - startMillis) / 1000;
  if (elapsed >= totalSeconds) return 0;

  return totalSeconds - elapsed;
}

// ================= BUTTON =========================
void updateButton() {
  bool reading = digitalRead(encoder_button);

  if (reading != lastBtn)
    debounceTime = millis();

  if (millis() - debounceTime > debounceDelay) {

    if (reading != stableBtn) {
      stableBtn = reading;

      if (stableBtn == LOW) {
        pressTime = millis();
        longHandled = false;
      }

      if (stableBtn == HIGH) {
        if (!longHandled) {

          if (mode == MODE_BOOT) {
            mode = MODE_SET_MINUTES;
            myEnc.write(setMinute * 4);
          }

          else if (mode == MODE_SET_MINUTES) {
            mode = MODE_SET_HOURS;
            myEnc.write(setHour * 4);
          }

          else if (mode == MODE_SET_HOURS) {
            mode = MODE_RUN;

            totalSeconds =
              ((unsigned long)setHour * 3600UL) +
              ((unsigned long)setMinute * 60UL);

            startMillis = millis();
            countdownRunning = true;
          }
        }
      }
    }
  }

  lastBtn = reading;

  // LONG PRESS RESET
  if (stableBtn == LOW &&
      !longHandled &&
      millis() - pressTime > 1200) {

    longHandled = true;

    setHour = 0;
    setMinute = 0;
    totalSeconds = 0;
    countdownRunning = false;
    mode = MODE_BOOT;

    myEnc.write(0);
  }
}

// ================= SETUP ==========================
void setup() {
  pinMode(encoder_button, INPUT_PULLUP);

  pixels.begin();
  pixels.setBrightness(brightnessLevel);
  pixels.clear();
  pixels.show();

  myEnc.write(0);
}

// ================= LOOP ===========================
void loop() {

  updateButton();

  // ================= BOOT (brightness + pulse) =================
  if (mode == MODE_BOOT) {

    long enc = myEnc.read() / 4;
    brightnessLevel = constrain(enc, 1, 255);
    pixels.setBrightness(brightnessLevel);

    float raw = (sin(millis() / 400.0) + 1.0) * 0.5;

    // bias curve toward brighter state (compress low end)
    float pulse = pow(raw, 0.4);
    uint8_t v = 40 + pulse * 200;

    showNumber(0, 0, true, pixels.Color(v, v, v));
    return;
  }

  // ================= SET MINUTES =================
  if (mode == MODE_SET_MINUTES) {

    long enc = myEnc.read() / 4;
    setMinute = constrain(enc, 0, 59);

    showNumber(0, setMinute, true, pixels.Color(0, 0, 255));
    return;
  }

  // ================= SET HOURS =================
  if (mode == MODE_SET_HOURS) {

    long enc = myEnc.read() / 4;
    setHour = constrain(enc, 0, 23);

    showNumber(setHour, setMinute, true, pixels.Color(0, 255, 0));
    return;
  }

  // ================= RUN =================
if (mode == MODE_RUN) {

    unsigned long rem = remainingSeconds();

    float progress =
      (totalSeconds == 0) ? 0.0 :
      (float)rem / (float)totalSeconds;

    uint32_t color = countdownColor(progress);

    bool colon = (millis() / 500) % 2;

    // ------------------------------------------------
    // UNDER 60 MINUTES = MM:SS
    // ------------------------------------------------
    if (rem <= 3600) {

      int displayMinutes = rem / 60;
      int displaySeconds = rem % 60;

      showNumber(
        displayMinutes,
        displaySeconds,
        colon,
        color
      );
    }

    // ------------------------------------------------
    // OVER 60 MINUTES = HH:MM
    // ------------------------------------------------
    else {

      int displayHours = rem / 3600;
      int displayMinutes = (rem % 3600) / 60;

      showNumber(
        displayHours,
        displayMinutes,
        colon,
        color
      );
    }

    if (rem == 0) {
  countdownRunning = false;

  bool blinkState = (millis() / 500) % 2;   // 1 Hz blink

  if (blinkState) {
    showNumber(0, 0, true, pixels.Color(255, 0, 0));
  } else {
    pixels.clear();
    pixels.show();
  }

  return;
    }}

// ================= FINISHED =================
if (mode == MODE_FINISHED) {

    bool blinkState = (millis() / 500) % 2;

    uint32_t color;

    if (blinkState) {
      // Bright red
      color = pixels.Color(255, 0, 0);
    } else {
      // Dim red (~15% brightness)
      color = pixels.Color(40, 0, 0);
    }

    showNumber(
      0,
      0,
      true,
      color
    );

    return;
}
}
```
