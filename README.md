# Sibol — Crop Risk Monitor

*See the signs. Save the harvest.*

A dashboard that combines **live sensor data from an ESP32-CAM** (stored in **Supabase**), a **live camera view**, **water pump control**, a **7-day weather forecast** (Open-Meteo, no API key needed), the **rice growth stage**, and **Gemini AI** advice to estimate crop stress risk.

```
ESP32-CAM ──insert readings──────────▶ Supabase sensor_readings  ◀── dashboard reads every 5 s
          ──upload latest.jpg────────▶ Storage files/cam/esp32/   ◀── home camera card (~1-2 fps)
          ◀─poll pump/stream cmds────  Supabase device_controls   ◀── Auto / Turn ON / Turn OFF buttons
Dashboard ──field data + photo───────▶ Gemini API (recommended actions, AI plant check)
```

**Forecast Heat vs Air Temperature:** *Forecast Heat* is the hottest forecast day in the next 3 days (Open-Meteo) and drives the risk score. *Air Temperature* is the live DHT11 reading at the field (camera overlay + Dashboard). They are different on purpose.

## Setup

1. **Supabase:** open SQL Editor → paste `supabase/schema.sql` → Run. This creates `sensor_readings`, `device_controls` (pump + camera), the views, RLS policies/grants, and the public `files` bucket policies. Safe to re-run.
2. **Keys:** copy `.env.example` to `.env`. Fill in the Project URL and the anon/publishable key from Project Settings → API, and `VITE_GEMINI_API_KEY` from Google AI Studio.
3. **Run:**
   ```bash
   npm run dev
   ```
   Then open http://localhost:5173.
4. **No ESP32 yet?** Run `npm run simulate -- --backfill`. It inserts a week of fake history, then adds a reading every 5 s.

## ESP32-CAM

Board: **AI Thinker ESP32-CAM** (Partition Scheme: Huge APP). Libraries: WiFiManager (tzapu), ArduinoJson v7, Adafruit ADS1X15, DHT sensor library.

| Part | Pin |
|---|---|
| ADS1115 SDA / SCL / ADDR | GPIO14 / GPIO15 / GND (VDD 3.3V) |
| Soil sensor AO | ADS1115 A0 (VCC 3.3V) |
| DHT11 DATA | GPIO2 |
| Relay IN (HIGH-trigger) | GPIO13 (VCC 5V) |

- `DEVICE_ID` in the sketch must be `esp32` (matches `src/lib/api.ts`).
- Calibrate `SOIL_DRY_V` (probe in air) and `SOIL_WET_V` (probe in water).
- The camera only uploads while someone has the Home page open (the page keeps `stream_until` 30 s ahead), to save Supabase bandwidth.
- Pump safety: max 2 min continuous run, then 1 min rest. If Supabase is unreachable for 30 s the ESP falls back to auto.

## How risk is calculated (`src/lib/crop.ts`)

- **Soil moisture:** below the optimal range (default 40–60 %) adds risk.
- **Forecast heat:** the hottest day in the next 3 days. ≥32 °C is moderate and ≥35 °C is high.
- **Growth stage:** estimated from the planting date. The reproductive (flowering) stage multiplies the risk ×1.5.

Change the crop, planting date, field coordinates, and optimal range on the **Settings** page.

> Security note: the anon key can read/insert readings, control the pump and upload to the `files` bucket, and `VITE_` variables (including the Gemini key) are visible to anyone who opens the site. That's fine for a prototype/demo. Before deploying publicly, restrict the Gemini key to your domain in Google Cloud Console (or move AI calls to a Supabase Edge Function) and add Supabase Auth.
