export interface Reading {
  ts: number
  device_id?: string
  soil_moisture: number | null
  soil_raw?: number | null
  soil_voltage?: number | null
  temperature?: number | null
  humidity?: number | null
  pump_on?: boolean | null
}

export interface DailyPoint {
  date: string // YYYY-MM-DD
  soil_moisture: number | null
  temperature: number | null
  humidity: number | null
  count: number
}

export interface ForecastDay {
  date: string
  tMax: number
  tMin: number
  code: number
  rainProb: number | null
}

export type PumpMode = 'auto' | 'manual'

export interface DeviceControls {
  device_id: string
  pump_mode: PumpMode
  pump_command: boolean
  pump_state: boolean
  last_seen: string | null
}

export interface Settings {
  locationName: string
  lat: number
  lng: number
  fieldRadius: number
  cropName: string
  plantingDate: string
  optimalMin: number
  optimalMax: number
  staleMinutes: number
}

export type Level = 'Low' | 'Moderate' | 'High'
export type Page = 'home' | 'dashboard' | 'chat' | 'maps' | 'data' | 'settings'
