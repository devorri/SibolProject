import type { Level } from '../types'

export interface Stage {
  key: string
  name: string
  phase: string
  from: number // days after planting
  to: number
  sensitivity: Level
}

// Typical 110–120 day lowland rice variety
export const STAGES: Stage[] = [
  { key: 'seedling', name: 'Seedling', phase: 'Vegetative Stage', from: 0, to: 20, sensitivity: 'Moderate' },
  { key: 'tillering', name: 'Tillering', phase: 'Vegetative Stage', from: 21, to: 45, sensitivity: 'Low' },
  { key: 'flowering', name: 'Flowering', phase: 'Reproductive Stage', from: 46, to: 85, sensitivity: 'High' },
  { key: 'grain', name: 'Grain Filling', phase: 'Ripening Stage', from: 86, to: 105, sensitivity: 'Moderate' },
  { key: 'maturity', name: 'Maturity', phase: 'Harvest Ready', from: 106, to: Infinity, sensitivity: 'Low' },
]

export function daysSince(dateStr: string): number {
  const start = new Date(dateStr + 'T00:00:00')
  return Math.floor((Date.now() - start.getTime()) / 86400000)
}

export function currentStage(plantingDate: string): { stage: Stage | null; index: number; days: number } {
  const days = daysSince(plantingDate)
  if (Number.isNaN(days) || days < 0) return { stage: null, index: -1, days }
  const index = STAGES.findIndex((s) => days >= s.from && days <= s.to)
  return { stage: STAGES[index], index, days }
}

export function moistureStatus(m: number | null, min: number, max: number): { label: string; level: Level | 'Ok' } {
  if (m === null) return { label: 'No data', level: 'Ok' }
  if (m < min * 0.6) return { label: 'Low', level: 'High' }
  if (m < min) return { label: 'Slightly Low', level: 'Moderate' }
  if (m > max + 20) return { label: 'Saturated', level: 'Moderate' }
  return { label: 'Optimal', level: 'Low' }
}

export function heatLevel(t: number | null): Level | null {
  if (t === null) return null
  if (t >= 35) return 'High'
  if (t >= 32) return 'Moderate'
  return 'Low'
}

export interface Risk {
  score: number // 0..1
  level: Level
}

export function computeRisk(moisture: number | null, heat: number | null, sensitivity: Level, min: number): Risk {
  let m = 0
  if (moisture !== null) {
    if (moisture < min * 0.6) m = 3
    else if (moisture < min) m = 2
    else if (moisture < min + 5) m = 1
  }
  let h = 0
  if (heat !== null) {
    if (heat >= 35) h = 3
    else if (heat >= 32) h = 2
    else if (heat >= 30) h = 1
  }
  const mult = sensitivity === 'High' ? 1.5 : sensitivity === 'Moderate' ? 1 : 0.7
  const raw = (m + h) * mult // max 9
  const score = Math.min(1, raw / 9)
  const level: Level = raw >= 5 ? 'High' : raw >= 2.5 ? 'Moderate' : 'Low'
  return { score, level }
}

export function recommendedActions(opts: {
  moisture: number | null
  heat: number | null
  risk: Level
  stage: Stage | null
  min: number
  rainSoon: boolean
}): string[] {
  const { moisture, heat, risk, stage, min, rainSoon } = opts
  const a: string[] = []
  if (moisture !== null && moisture < min) {
    a.push(
      rainSoon
        ? 'Rain is forecast — check if it will be enough before adding irrigation.'
        : 'Check and, if possible, supplement irrigation.',
    )
  }
  if (heat !== null && heat >= 32) a.push('Consider heat mitigation (e.g., early-morning irrigation, keep standing water).')
  if (stage?.sensitivity === 'High') a.push(`Crop is at the ${stage.name.toLowerCase()} stage — monitor daily, especially during peak heat hours.`)
  if (risk === 'High') a.push('Prepare for possible stress (e.g., adjust fertilizer or water management).')
  if (moisture !== null && moisture > min + 40) a.push('Soil is very wet — check drainage to avoid waterlogging.')
  if (moisture === null) a.push('No sensor data yet — check that the ESP32 is powered and connected to Wi-Fi.')
  if (a.length === 0) a.push('Conditions look good. Keep monitoring soil moisture and the forecast.')
  return a
}

export const LEVEL_COLOR: Record<Level, string> = {
  Low: '#3aa335',
  Moderate: '#e8a317',
  High: '#d93b2b',
}
