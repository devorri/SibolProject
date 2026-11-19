import { computeRisk, currentStage, heatLevel, moistureStatus, recommendedActions } from '../lib/crop'
import { useSettings } from '../settings'
import type { SibolData } from './useSibolData'

/** Everything the cards need, derived from live sensor + forecast data. */
export function useCropModel(data: SibolData) {
  const { settings: s } = useSettings()
  const moisture = data.latest?.soil_moisture ?? null
  const next3 = data.forecast.slice(0, 3)
  const heat = next3.length ? Math.max(...next3.map((f) => f.tMax)) : null
  const rainSoon = next3.some((f) => (f.rainProb ?? 0) >= 60)
  const { stage, index, days } = currentStage(s.plantingDate)
  const sensitivity = stage?.sensitivity ?? 'Moderate'
  const risk = computeRisk(moisture, heat, sensitivity, s.optimalMin)
  const mStatus = moistureStatus(moisture, s.optimalMin, s.optimalMax)
  const hLevel = heatLevel(heat)
  const actions = recommendedActions({ moisture, heat, risk: risk.level, stage, min: s.optimalMin, rainSoon })
  return { s, moisture, heat, rainSoon, stage, index, days, sensitivity, risk, mStatus, hLevel, actions }
}
