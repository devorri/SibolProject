// Fake ESP32 — inserts random readings into Supabase so you can test without hardware.
// Usage: npm run simulate                 (inserts every 5 s)
//        npm run simulate -- --backfill   (first adds 7 days of past readings for the trend chart)
// Reads VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY from .env

const URL_BASE = (process.env.VITE_SUPABASE_URL || '').replace(/\/+$/, '')
const KEY = process.env.VITE_SUPABASE_ANON_KEY || ''

if (!URL_BASE || !KEY) {
  console.error('Missing VITE_SUPABASE_URL / VITE_SUPABASE_ANON_KEY in .env')
  process.exit(1)
}

let moisture = 45

async function insert(rows) {
  const res = await fetch(`${URL_BASE}/rest/v1/sensor_readings`, {
    method: 'POST',
    headers: { apikey: KEY, 'Content-Type': 'application/json', Prefer: 'return=minimal' },
    body: JSON.stringify(rows),
  })
  if (!res.ok) throw new Error(`${res.status} ${await res.text()}`)
}

function next() {
  moisture = Math.max(5, Math.min(90, moisture + (Math.random() - 0.55) * 2))
  return {
    device_id: 'esp32-sim',
    soil_moisture: Math.round(moisture * 10) / 10,
    temperature: Math.round((30 + Math.random() * 5) * 10) / 10,
    humidity: Math.round((60 + Math.random() * 20) * 10) / 10,
  }
}

if (process.argv.includes('--backfill')) {
  const now = Date.now()
  const rows = []
  for (let h = 7 * 24; h > 0; h -= 3) {
    const sm = 18 + (h / (7 * 24)) * 26 + (Math.random() - 0.5) * 3
    rows.push({ ...next(), soil_moisture: Math.round(sm * 10) / 10, created_at: new Date(now - h * 3600000).toISOString() })
  }
  await insert(rows)
  moisture = 18
  console.log(`Backfilled ${rows.length} readings over 7 days`)
}

console.log(`Simulating ESP32 -> ${URL_BASE}  (Ctrl+C to stop)`)
setInterval(async () => {
  const r = next()
  try {
    await insert([r])
    console.log(`inserted soil=${r.soil_moisture}%  temp=${r.temperature}°C`)
  } catch (e) {
    console.error('Insert failed:', e.message)
  }
}, 5000)
