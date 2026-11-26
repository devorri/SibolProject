# SIBOL Display — Waveshare ESP32-S3-Touch-LCD-4.3

Touchscreen companion for the Sibol system. Home screen has three buttons:

| Button | What it does |
|---|---|
| **Dashboard** | Live ESP32-CAM view, soil moisture, air temp/humidity (DHT11), crop risk, water pump Auto / ON / OFF, 8-day soil trend, 7-day forecast, Gemini "AI Tips" |
| **Sibol Chat** | Gemini chatbot that knows your live field data and how Sibol works; also answers general questions (English / Tagalog / Taglish). "Check my plant photo" sends the latest camera picture to the AI. |
| **Palay Defense** | Mini lane-defense game: plant Sili (shoots), Palay (earns seeds), Bakod (blocks) to stop Kuhol and Daga. Best score is saved. |

It reads/writes the **same Supabase project** as the website and the ESP32-CAM, so all three stay in sync.

## 1. Arduino IDE setup

1. **Board package:** esp32 by Espressif **3.x** (Boards Manager).
2. **Libraries** (Library Manager):
   - **lvgl** 9.x (by LVGL)
   - **ArduinoJson** 7.x (by Benoit Blanchon)
   - **JPEGDEC** (by Larry Bank)
   - **WiFiManager** (by tzapu)
3. **lv_conf.h:** copy `lv_conf.h` from this folder to `Documents/Arduino/libraries/lv_conf.h`
   (next to the `lvgl` folder, not inside it). Without it you get "lv_font_montserrat_40 not declared".
4. **Tools menu:**
   - Board: **ESP32S3 Dev Module**
   - PSRAM: **OPI PSRAM**  ← required (if it boot-loops with a PSRAM error, try **QSPI PSRAM**)
   - Flash Size + Partition Scheme: **must match your board's flash chip**
     - **8MB** board → Flash Size **8MB (64Mb)**, Partition **8M with spiffs (3MB APP/1.5MB SPIFFS)**
     - **16MB** board → Flash Size **16MB (128Mb)**, Partition **16M Flash (3MB APP/9.9MB FATFS)**
     - Not sure? If Serial shows `exceeds flash chip size 0x800000`, your board is **8MB**.
   - CPU Frequency: 240MHz
   - USB CDC On Boot: Enabled (if you use the native USB port for Serial)

## 2. Settings

Edit `config.h`: Supabase URL/key, Gemini key, field location, crop, planting date, optimal moisture range.
`DEVICE_ID` must be the same as in the ESP32-CAM sketch (`esp32`).

## 3. First boot

1. Upload. The screen shows the Home page.
2. If there is no saved WiFi, Home shows: connect your phone to **SIBOL-Display** (pass `12345678`) and open **192.168.4.1** → pick a **2.4 GHz** network.
3. Gear button (top right) → info + **Reset WiFi**.

## How it works

- **Core 0:** WiFi, Supabase polling (controls 2 s, readings 5 s, trend 60 s), Open-Meteo forecast (30 min), camera frames, and a separate AI task for Gemini (so the dashboard keeps updating while the AI thinks).
- **Core 1:** LVGL UI + touch.
- Camera: while the Dashboard "Live" tab is open, the display keeps `stream_until` refreshed (like the website), downloads `files/cam/esp32/latest.jpg`, and decodes it with JPEGDEC.
- Pump buttons update `device_controls` — the ESP32-CAM applies them within ~1.5 s (same safety limits: max 2 min run, 1 min rest).
- Gemini models are tried in order (`GEMINI_MODELS` in config.h); busy/unavailable models are skipped automatically.

## Troubleshooting

| Symptom | Fix |
|---|---|
| Boot loop: `partition ... exceeds flash chip size 0x800000` | Board has 8MB flash: set Flash Size 8MB + partition "8M with spiffs (3MB APP/1.5MB SPIFFS)". |
| Black screen, Serial OK | PSRAM not set to **OPI PSRAM**, or wrong board. Check Serial for `[board]` lines. |
| Screen flickers/shifts while loading | Normal brief drift on RGB panels under heavy WiFi; bounce buffers are already on. Try `pclk_hz` 14 MHz in `board.cpp`. |
| Touch doesn't respond | Serial should say `GT911 touch at 0x5D`. If "NOT found", reseat the touch ribbon. |
| Touch is mirrored | In `board.cpp` `touchRead()`, use `LCD_W - 1 - x` / `LCD_H - 1 - y`. |
| Camera says "Asking camera for frames..." forever | ESP32-CAM offline, or `schema.sql` not run (stream_until). |
| Chat: "All Gemini models are busy" | Google overload — try again in a minute. |

Voice chat needs extra hardware on this board (INMP441 I2S mic + MAX98357A amp) — planned as the next stage.
