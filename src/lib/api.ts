import type { DailyPoint, ForecastDay, Reading } from '../types'
import { sbSelect } from './supabase'

interface Row {
  created_at: string
  device_id: string
  soil_moisture: number | null
  soil_raw: number | null
  temperature: number | null
  humidity: number | null
}

const toReading = (r: Row): Reading => ({
  ts: new Date(r.created_at).getTime(),
  device_id: r.device_id,
  soil_moisture: r.soil_moisture,
  soil_raw: r.soil_raw,
  temperature: r.temperature,
  humidity: r.humidity,
})

const COLS = 'created_at,device_id,soil_moisture,soil_raw,temperature,humidity'

export const api = {
  latest: async (): Promise<Reading | null> => {
    const { data } = await sbSelect<Row[]>(`sensor_readings?select=${COLS}&order=created_at.desc&limit=1`)
    return data[0] ? toReading(data[0]) : null
  },

  history: async (days: number): Promise<DailyPoint[]> => {
    const since = lastDays(days)[0]
    const { data } = await sbSelect<DailyPoint[]>(`sensor_daily?select=*&date=gte.${since}&order=date.asc`)
    return data
  },

  readings: async (limit = 500): Promise<{ readings: Reading[]; total: number }> => {
    const { data, total } = await sbSelect<Row[]>(`sensor_readings?select=${COLS}&order=created_at.desc&limit=${limit}`, {
      count: true,
    })
    return { readings: data.map(toReading), total: total ?? data.length }
  },
}

// Free weather forecast, no API key needed — https://open-meteo.com
export async function fetchForecast(lat: number, lng: number): Promise<ForecastDay[]> {
  const url =
    `https://api.open-meteo.com/v1/forecast?latitude=${lat}&longitude=${lng}` +
    `&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max` +
    `&timezone=Asia%2FManila&forecast_days=7`
  const res = await fetch(url)
  if (!res.ok) throw new Error(`${res.status}`)
  const d = (await res.json()) as {
    daily: {
      time: string[]
      weather_code: number[]
      temperature_2m_max: number[]
      temperature_2m_min: number[]
      precipitation_probability_max: (number | null)[]
    }
  }
  return d.daily.time.map((date, i) => ({
    date,
    code: d.daily.weather_code[i],
    tMax: Math.round(d.daily.temperature_2m_max[i]),
    tMin: Math.round(d.daily.temperature_2m_min[i]),
    rainProb: d.daily.precipitation_probability_max[i],
  }))
}

export const dayKey = (d: Date) => d.toLocaleDateString('en-CA', { timeZone: 'Asia/Manila' })

/** Last `n` calendar days (oldest first) as YYYY-MM-DD, aligned to Manila time. */
export function lastDays(n: number): string[] {
  const out: string[] = []
  for (let i = n - 1; i >= 0; i--) out.push(dayKey(new Date(Date.now() - i * 86400000)))
  return out
}

export const shortDate = (iso: string) =>
  new Date(iso + 'T00:00:00').toLocaleDateString('en-US', { month: 'short', day: 'numeric' })
