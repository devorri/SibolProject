import { FieldMap } from '../components/FieldMap'
import {
  ActionsCard,
  CropInfoCard,
  ForecastCard,
  HeatIcon,
  MoistureTrend,
  RiskCard,
  SoilIcon,
  StageCard,
  StatCard,
} from '../components/Widgets'
import { useCropModel } from '../hooks/useCropModel'
import type { SibolData } from '../hooks/useSibolData'

export function SoilCard({ m }: { m: ReturnType<typeof useCropModel> }) {
  const { moisture, s, mStatus } = m
  const sub =
    moisture === null ? 'Waiting for ESP32' : moisture < s.optimalMin ? '(Below reference level)' : moisture > s.optimalMax ? '(Above reference level)' : '(Within optimal range)'
  return (
    <StatCard
      title="Soil Moisture"
      tip={`Live reading from the ESP32 soil sensor. Optimal: ${s.optimalMin}–${s.optimalMax}%.`}
      icon={<SoilIcon />}
      iconBg="#2f8fd8"
      value={moisture === null ? '--%' : `${Math.round(moisture)}%`}
      sub={sub}
      progress={(moisture ?? 0) / 100}
      barColor="#2f8fd8"
      status={mStatus.label}
      statusLevel={mStatus.level}
    />
  )
}

export function HeatCard({ m }: { m: ReturnType<typeof useCropModel> }) {
  const { heat, hLevel } = m
  return (
    <StatCard
      title="Forecast Heat"
      tip="Highest forecast temperature over the next 3 days (Open-Meteo)."
      icon={<HeatIcon />}
      iconBg="#f0a020"
      value={heat === null ? '--°C' : `${heat}°C`}
      sub={hLevel === 'High' ? '(High temperature expected)' : hLevel === 'Moderate' ? '(Warm temperature expected)' : heat === null ? 'Loading forecast' : '(Normal temperature)'}
      progress={heat === null ? 0 : (heat - 20) / 25}
      barColor="#f0a020"
      status={hLevel ?? '—'}
      statusLevel={hLevel}
    />
  )
}

export function Home({ data }: { data: SibolData }) {
  const m = useCropModel(data)
  return (
    <div className="page">
      <h1 className="welcome">Welcome!</h1>
      <p className="lead">Here's the current crop condition and risk forecast for your area.</p>
      {!data.dbOk && <div className="alert">{data.dbError}</div>}

      <div className="home-grid">
        <div className="g-soil"><SoilCard m={m} /></div>
        <div className="g-heat"><HeatCard m={m} /></div>
        <div className="g-stage"><StageCard stage={m.stage} index={m.index} days={m.days} /></div>
        <div className="g-risk"><RiskCard level={m.risk.level} score={m.risk.score} hasData={m.moisture !== null} /></div>
        <div className="g-map card map-card">
          <FieldMap lat={m.s.lat} lng={m.s.lng} radius={m.s.fieldRadius} level={m.risk.level} />
        </div>
        <div className="g-crop">
          <CropInfoCard
            crop={m.s.cropName}
            plantingDate={m.s.plantingDate}
            stage={m.stage?.name ?? '—'}
            sensitivity={m.stage?.sensitivity ?? '—'}
          />
        </div>
        <div className="g-trend"><MoistureTrend history={data.history} min={m.s.optimalMin} max={m.s.optimalMax} /></div>
        <div className="g-forecast"><ForecastCard forecast={data.forecast} error={data.weatherError} /></div>
        <div className="g-actions"><ActionsCard actions={m.actions} /></div>
      </div>
    </div>
  )
}
