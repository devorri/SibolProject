import type { useCropModel } from '../hooks/useCropModel'
import type { SibolData } from '../hooks/useSibolData'
import type { DeviceControls } from '../types'
import type { FieldContext } from './gemini'

/** Snapshot of the field for the AI prompts. */
export function toFieldContext(
  m: ReturnType<typeof useCropModel>,
  data: SibolData,
  controls: DeviceControls | null,
): FieldContext {
  return {
    crop: m.s.cropName,
    location: m.s.locationName,
    stage: m.stage ? `${m.stage.phase} (${m.stage.name})` : null,
    sensitivity: m.sensitivity,
    daysAfterPlanting: m.days,
    soilMoisture: m.moisture,
    optimalMin: m.s.optimalMin,
    optimalMax: m.s.optimalMax,
    sensorTemp: data.latest?.temperature ?? null,
    sensorHumidity: data.latest?.humidity ?? null,
    forecast: data.forecast.slice(0, 3).map((f) => ({ date: f.date, tMax: f.tMax, tMin: f.tMin, rainProb: f.rainProb })),
    riskLevel: m.risk.level,
    pumpOn: controls ? controls.pump_state : (data.latest?.pump_on ?? null),
    pumpMode: controls?.pump_mode ?? null,
  }
}
