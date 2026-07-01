
```cpp
/*
ESP32-C3 XIAO NeoPixel Clock
DRIFT-FREE + SETTABLE 24H COLOR CURVE + BRIGHTNESS MODE
No IoT, just a basic clock.
----------------------------------------------------
UI Flow:
BOOT → SET HOURS → SET MINUTES → SET COLOR → RUN
RUN → (press) BRIGHTNESS → RUN
long press resets clock to 12:00
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

Adafruit_NeoPixel pixels(
  LED_COUNT,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);

// ================= STATE ==========================
int brightnessLevel = 40;

int setHour24 = 0;
int setMinute = 0;

unsigned long startMillis = 0;
bool clockRunning = false;

// ================= COLOR ==========================
int colorOffset = 0;

// ================= MODE ===========================
enum ClockMode {
  MODE_BOOT,
  MODE_SET_HOURS,
  MODE_SET_MINUTES,
  MODE_SET_COLOR,
  MODE_RUN,
  MODE_BRIGHTNESS
};

ClockMode mode = MODE_BOOT;

// ================= ENCODER ========================
long lastEncoderValue = 0;

// ================= FADE ===========================
float fadePhase = 0;

// ================= BUTTON =========================
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long buttonPressTime = 0;
bool longPressHandled = false;
const unsigned long longPressTime = 1200;

// ================= SEGMENTS =======================
const byte segmentMap[7][4] = {
  { 0, 1, 2, 3 },
  { 4, 5, 6, 7 },
  { 8, 9,10,11 },
  {12,13,14,15 },
  {16,17,18,19 },
  {20,21,22,23 },
  {24,25,26,27 }
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

void showDigitAt(byte digit, byte pos, bool dp, uint32_t color) {

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
    pixels.setPixelColor(base + DP_OFFSET, pixels.Color(255,0,0));
}

void showClockDisplay(
  int displayHour,
  int displayMinute,
  bool colon,
  bool showHours,
  bool showMinutes,
  uint32_t color
) {

  int d0 = displayHour / 10;
  int d1 = displayHour % 10;
  int d2 = displayMinute / 10;
  int d3 = displayMinute % 10;

  if (showHours) {
    showDigitAt(d0, 0, false, color);
    showDigitAt(d1, 1, colon, color);
  } else {
    clearDigit(0);
    clearDigit(1);
  }

  if (showMinutes) {
    showDigitAt(d2, 2, false, color);
    showDigitAt(d3, 3, false, color);
  } else {
    clearDigit(2);
    clearDigit(3);
  }

  pixels.show();
}

// ================= CLOCK ==========================
void getRunningTime(int &hour, int &minute) {

  if (!clockRunning) {
    hour = setHour24;
    minute = setMinute;
    return;
  }

  unsigned long elapsedSeconds =
    (millis() - startMillis) / 1000;

  int totalMinutes =
    (setHour24 * 60) +
    setMinute +
    (elapsedSeconds / 60);

  totalMinutes %= 1440;

  hour = totalMinutes / 60;
  minute = totalMinutes % 60;
}

// ================= COLOR ENGINE ===================
uint32_t getTimeColor(int minutesOfDay) {

  uint16_t baseHue =
    map(minutesOfDay, 0, 1439, 0, 65535);

  baseHue += (colorOffset * 200);

  return pixels.ColorHSV(baseHue);
}

// ================= BUTTON =========================
void updateButton() {

  bool reading = digitalRead(encoder_button);

  if (reading != lastButtonReading)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (reading != stableButtonState) {

      stableButtonState = reading;

      if (stableButtonState == LOW) {
        buttonPressTime = millis();
        longPressHandled = false;
      }

      if (stableButtonState == HIGH) {

        unsigned long duration = millis() - buttonPressTime;

        if (!longPressHandled && duration < longPressTime) {

          // ===== SETUP FLOW =====
          if (mode == MODE_BOOT) {
            mode = MODE_SET_HOURS;
            myEnc.write(setHour24 * 4);
          }
          else if (mode == MODE_SET_HOURS) {
            mode = MODE_SET_MINUTES;
            myEnc.write(setMinute * 4);
          }
          else if (mode == MODE_SET_MINUTES) {
            mode = MODE_SET_COLOR;
            myEnc.write(colorOffset * 4);
          }
          else if (mode == MODE_SET_COLOR) {
            mode = MODE_RUN;
            startMillis = millis();
            clockRunning = true;
          }

          // ===== RUN → BRIGHTNESS =====
          else if (mode == MODE_RUN) {
            mode = MODE_BRIGHTNESS;
            myEnc.write(brightnessLevel * 4);
            lastEncoderValue = brightnessLevel;
          }

          // ===== BRIGHTNESS → RUN =====
          else if (mode == MODE_BRIGHTNESS) {
            mode = MODE_RUN;
          }
        }
      }
    }
  }

  lastButtonReading = reading;

  // LONG PRESS RESET
  if (stableButtonState == LOW &&
      !longPressHandled &&
      (millis() - buttonPressTime > longPressTime)) {

    longPressHandled = true;

    setHour24 = 0;
    setMinute = 0;
    colorOffset = 0;
    brightnessLevel = 40;

    myEnc.write(0);
    lastEncoderValue = 0;

    clockRunning = false;
    mode = MODE_BOOT;
  }
}

// ================= SETUP ==========================
void setup() {

  Serial.begin(115200);

  pinMode(encoder_button, INPUT_PULLUP);

  pixels.begin();
  pixels.setBrightness(brightnessLevel);
  pixels.clear();

  pixels.setPixelColor(0, pixels.Color(0,0,20));
  pixels.show();

  myEnc.write(0);

  Serial.println("Clock Ready");
}

// ================= LOOP ===========================
void loop() {

  bool fadeEnabled =
    (mode != MODE_RUN &&
     mode != MODE_BRIGHTNESS);

  float fade = 1.0;

  if (fadeEnabled) {

    fadePhase += 0.05;
    if (fadePhase > 6.28318) fadePhase = 0;

    float v = (sin(fadePhase) + 1.0) * 0.5;
    fade = 0.35 + (v * 0.65);
  }

  updateButton();

  int hour, minute;
  getRunningTime(hour, minute);

  int displayHour = hour % 12;
  if (displayHour == 0) displayHour = 12;

  int mins = hour * 60 + minute;

  uint32_t baseColor = getTimeColor(mins);

  uint32_t color = pixels.Color(
    (uint8_t)(((baseColor >> 16) & 0xFF) * fade),
    (uint8_t)(((baseColor >> 8)  & 0xFF) * fade),
    (uint8_t)(( baseColor        & 0xFF) * fade)
  );

  // ONBOARD LED INDICATOR
  if (mode == MODE_BRIGHTNESS)
    pixels.setPixelColor(0, pixels.Color(20,20,0));
  else
    pixels.setPixelColor(0, pixels.Color(0,0,20));

  // ================= MODES ========================

  if (mode == MODE_BOOT) {
    showClockDisplay(12, 0, true, true, true, color);
    return;
  }

  if (mode == MODE_SET_HOURS) {
    long enc = myEnc.read() / 4;
    enc = constrain(enc, 0, 23);

    if (enc != lastEncoderValue) {
      setHour24 = enc;
      lastEncoderValue = enc;
    }

    showClockDisplay(displayHour, setMinute, true, true, true, color);
    return;
  }

  if (mode == MODE_SET_MINUTES) {
    long enc = myEnc.read() / 4;
    enc = constrain(enc, 0, 59);

    if (enc != lastEncoderValue) {
      setMinute = enc;
      lastEncoderValue = enc;
    }

    showClockDisplay(displayHour, setMinute, true, true, true, color);
    return;
  }

  if (mode == MODE_SET_COLOR) {
    long enc = myEnc.read() / 4;
    colorOffset = constrain(enc, -50, 50);

    showClockDisplay(displayHour, minute, true, true, true, color);
    return;
  }

  if (mode == MODE_BRIGHTNESS) {

    long enc = myEnc.read() / 4;
    enc = constrain(enc, 1, 255);

    if (enc != lastEncoderValue) {
      brightnessLevel = enc;
      lastEncoderValue = enc;
      pixels.setBrightness(brightnessLevel);
      Serial.println(brightnessLevel);
    }

    bool colon = (millis() / 250) % 2;

    showClockDisplay(displayHour, minute, colon, true, true, color);
    return;
  }

  if (mode == MODE_RUN) {

    bool colon = (millis() / 1000) % 2;

    showClockDisplay(displayHour, minute, colon, true, true, color);
  }
}
```
