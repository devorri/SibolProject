# Sibol — Crop Risk Monitor

*See the signs. Save the harvest.*

A dashboard that combines **live soil moisture from an ESP32** (stored in **Supabase**), a **7-day weather forecast** (Open-Meteo, no API key needed), and the **rice growth stage** to estimate crop stress risk.

```
ESP32 + soil sensor ──HTTPS insert──▶  Supabase (sensor_readings)  ◀──reads every 5 s──  React dashboard
```

## Setup

1. **Supabase:** open SQL Editor → paste `supabase/schema.sql` → Run. This creates the `sensor_readings` table, its RLS policies, and the `sensor_daily` view.
2. **Keys:** copy `.env.example` to `.env`. Fill in the Project URL and the anon/publishable key from Project Settings → API.
3. **Run:**
   ```bash
   npm run dev
   ```
   Then open http://localhost:5173.
4. **No ESP32 yet?** Run `npm run simulate -- --backfill`. It inserts a week of fake history, then adds a reading every 5 s.

## ESP32

1. Open `esp32/sibol_esp32/sibol_esp32.ino` in the Arduino IDE (board: **ESP32 Dev Module**).
2. Install **ArduinoJson** v7, plus the Adafruit **DHT sensor library** if you use a DHT22.
3. Set the Wi-Fi name and password, `SUPABASE_URL`, and `SUPABASE_KEY` (the same values as in `.env`).
4. Wiring: soil sensor AOUT → **GPIO34**. DHT22 DATA → **GPIO4** (optional).
5. Calibrate: in Serial Monitor note the raw value **in air** → `SOIL_DRY` and **in water** → `SOIL_WET`.

When an insert succeeds, Serial Monitor prints `Saved to Supabase`.

## How risk is calculated (`src/lib/crop.ts`)

- **Soil moisture:** below the optimal range (default 40–60 %) adds risk.
- **Forecast heat:** the hottest day in the next 3 days. ≥32 °C is moderate and ≥35 °C is high.
- **Growth stage:** estimated from the planting date. The reproductive (flowering) stage multiplies the risk ×1.5.

Change the crop, planting date, field coordinates, and optimal range on the **Settings** page.

> Security note: the anon key can read and insert readings. That's fine for a prototype; don't put other sensitive data in this project.
