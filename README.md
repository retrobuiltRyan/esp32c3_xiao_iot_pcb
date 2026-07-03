# esp32c3_xiao_iot_pcb
PCB for MQTT projects. Sensor, I2C screen, rotary encoder input, and buck converter breakout.

| Project Difficulty | Rating |
|----------|--------|
| **Hardware Difficulty** | ●● Easy |
| **Software Complexity** | ●● Easy? | 

<img width="1876" height="705" alt="PCB" src="https://github.com/user-attachments/assets/1a4ffd75-a3f0-4bc4-aa0d-190ac4781f29" /> <br>
This is the ~$5 [ESP32c3 XIAO microcontroller](https://www.digikey.com/en/products/detail/seeed-technology-co-ltd/113991054/16652880) (with its antenna on the left) variant.

<img width="1919" height="570" alt="P1130036" src="https://github.com/user-attachments/assets/a89cdde8-1319-409d-ba4b-bf4b4098ede6" />

### Example Projects

Three-ish example projects are shown using this PCB:

- 1.1 – WiFi Clock
- 1.2 – Basic "Dumb" Clock
- 1.3 – Countdown Timer
- 2.0 – Weather Station with Terminal-Style Display
- 3.0 – Current (120V AC) Probe Monitor over MQTT

## 1.1 WiFi Clock
<img width="1024" height="576" alt="P1130023" src="https://github.com/user-attachments/assets/5e21adeb-c497-4adb-8a76-c49935571382" /> <br>
<img width="1492" height="629" alt="P1130020" src="https://github.com/user-attachments/assets/4cb25e22-8332-466c-b476-2b30f0743302" /> <br>

Large digit (70mm height) 7-segment clock. Connects to 2.4 GHz WiFi and grabs time from HTTP "google.com" TCP port 80. Previously grabbed time from NTP, but I have found some enterprise networks do not like this traffic and block the device.  

Files for the [RGB LED 7-segment digit PCB](https://github.com/retrobuiltRyan/Neo7Segment-Design-Files). You'll need to make four (each digit needs qty 29 RGB LEDs).  

A Daylight Saving Time function is implemented; however, every 6 months I find this doesn't work as expected, and 6 months between bug testing is annoying, so YMMV. DST compensation works better on Network Time Protocol grabs, but again, some networks do not like this.  

The body is 3D printed. The digit diffusion layer can be experimented with in material color and thickness. (about 0.2)

## 1.2 Dumb Clock (no WiFi)
<img width="1919" height="1080" alt="P1130025" src="https://github.com/user-attachments/assets/87d5c010-d2e1-48c5-bf73-2e45cd356431" /> <br>

Same hardware, different code for a regular clock that keeps time without WiFi. Hours and minutes are set using the rotary encoder & push button.  

UI Flow:  
BOOT → SET HOURS → SET MINUTES → SET COLOR → RUN  
While in RUN → (press) BRIGHTNESS → Return  
Long press resets clock to 12:00  

## 1.3 Countdown Timer
Task Master style countdown timer. 
UI Flow:
BOOT → brightness adjust + pulsing 00:00 →press [set min] press→ [set hr] →press [start]
Time starts out green color and gradually changes to red as 00:00 time approaches.

## 2.0 Weather Station
<img width="1637" height="855" alt="P1130031" src="https://github.com/user-attachments/assets/17bc18bb-709e-4e4e-b68e-8e1ce07c39ce" /> <br>

Calls API: https://api.weather.gov/points/40.4406,-79.9959. Then grabs weather forecast data from https://api.weather.gov/gridpoints/PBZ/78,66/forecast. A bunch of forecast data is returned in JSON text format.  The lat and long point to Pittsburgh, PA, which can be changed. This weather API is a free government service available in the USA only, one benefit is no need for API key. Code then parses only the first five forecast 'periods' to save SRAM and prints over serial (for debug) and to OLED in a terminal-style display using this [2.4 inch OLED screen](https://www.aliexpress.us/item/3256805914521312.html).  
Forecast updates every hour. 3D printed body is stylized as a classic Mac if it was extra wide.  

## 3.0 Current Probe Monitor over MQTT
<img width="1919" height="1080" alt="P1130028" src="https://github.com/user-attachments/assets/0e6ad40c-bcc4-4613-89ae-34365eac3b49" /> <br>
(add a dashboard pic when I get that done) <br>

It's lunchtime and you're starving, but there might be a line for the microwave and toaster! Three minutes for someone's Factor meal to cook?! Wouldn't you like to know if these appliances are available before walking to the office kitchen? What if you could check a dashboard that monitors microwave and toaster oven usage?  

Two contactless probes (current clamp) monitor current draw (0–10A) of both appliances and report their status over MQTT. A dashboard logs usage history and can provide a bunch of interesting data on office lunchtime habits. Could also be deployed as an online monitor for critical infrastructure like deep freezers or coolant pumps—useful things. A temperature and humidity sensor (SHT45) is populated on this PCB to also report ambient conditions as a baseline data point. The SHT45 is a very tiny SMD part; adding this is advanced soldering (you need that stencil ordered).

## PCB BoM
| Reference | Value | Qty | DigiKey P/N |
|-----------|-------|----:|--------------|
| BZ1 | Buzzer_5V | 1 | |
| C1, C3, C4, C5, C6, C8, C10 | 0.1uF 50V | 7 | 1276-1068-1-ND |
| C2, C7, C9, C11 | 0.22uF 50V CER | 4 | 445-2283-1-ND |
| D1 | WS2812B | 1 | [aliexpress](https://www.aliexpress.us/item/3256802466699315.html) |
| D5, D6, D7 | LED | 3 | 1080-1419-1-ND |
| D8 | D | 1 | S5AC-FDICT-ND |
| D9 | SMF15A | 1 | SMF15A-E3-08CT-ND |
| J1 | Barrel_Jack 2.1 × 5.5mm | 1 | EJ508A-ND |
| J2, J5, J6, J7, J10 | Screw_Terminal_2_P3.50mm | 5 | 732-2747-ND |
| Q1 | MOSFET P-CH 30V 25A TO252 | 1 | 785-1106-1-ND |
| Q2 | MMBT2222A | 1 | MMBT2222ATPMSCT-ND |
| Q3 | AO3400A | 1 | 785-1000-1-ND |
| R1 | 220 | 1 | 311-220FRCT-ND |
| R2, R3, R5, R6 | 1K | 4 | 311-1.00KFRCT-ND |
| R4 | 3.3K | 1 | 311-3.30KFRCT-ND |
| R7 | 100k | 1 | 311-100KFRCT-ND |
| R8, R9 | 10K | 2 | 311-10.0KFRCT-ND |
| RN1 | 10k array | 1 | CAY16-103J4LFCT-ND |
| RN4 | 200 array | 1 | CAY16-201J4LFCT-ND |
| SW1 | RotaryEncoder_Switch_MP | 1 | PEC11R-4220F-S0024-ND |
| U1 | MAX40200AUK | 1 | 175-MAX40203AUK+TCT-ND |
| U2 | MP1584_Buck | 1 |[aliexpress](https://www.aliexpress.us/item/3256805684077964.html) |
| U3 | L7805 | 1 | 497-7255-1-ND |
| U7 | ESP32C3 | 1 | 1597-113991054-ND |
| U8 | UCC27511ADBV | 1 | 296-49474-1-ND |
| U9 | SHT4x | 1 | 1649-SHT45-AD1B-R2CT-ND |
