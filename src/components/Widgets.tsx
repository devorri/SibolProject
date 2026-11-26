import type { ReactNode } from 'react'
import type { AiAdvice } from '../hooks/useAiAdvice'
import type { DailyPoint, ForecastDay, Level } from '../types'
import { LEVEL_COLOR, STAGES, type Stage } from '../lib/crop'
import { lastDays, shortDate } from '../lib/api'
import { DropIcon, LeafIcon, SproutIcon, SunIcon, ThermoIcon, ToolsIcon, WarnIcon, WeatherIcon, weatherLabel } from './icons'

export const Info = ({ tip }: { tip: string }) => (
  <span className="info" data-tip={tip} tabIndex={0} role="note" aria-label={tip}>
    i
  </span>
)

export function Card({ children, className = '' }: { children: ReactNode; className?: string }) {
  return <section className={`card ${className}`}>{children}</section>
}

const statusColor = (l: Level | 'Ok' | null) => (l && l !== 'Ok' ? LEVEL_COLOR[l] : '#777')

/* ---------- Soil moisture / Forecast heat ---------- */
export function StatCard(props: {
  title: string
  tip: string
  icon: ReactNode
  iconBg: string
  value: string
  sub: string
  progress: number // 0..1
  barColor: string
  status: string
  statusLevel: Level | 'Ok' | null
}) {
  return (
    <Card className="stat-card">
      <Info tip={props.tip} />
      <div className="stat-head">
        <div className="icon-circle" style={{ background: props.iconBg }}>
          {props.icon}
        </div>
        <div>
          <h3>{props.title}</h3>
          <div className="big-value">{props.value}</div>
          <div className="muted small">{props.sub}</div>
        </div>
      </div>
      <div className="bar">
        <div style={{ width: `${Math.max(0, Math.min(1, props.progress)) * 100}%`, background: props.barColor }} />
      </div>
      <div className="status">
        Status: <b style={{ color: statusColor(props.statusLevel) }}>{props.status}</b>
      </div>
    </Card>
  )
}

export const SoilIcon = () => <DropIcon size={30} color="#fff" />
export const HeatIcon = () => <ThermoIcon size={30} color="#fff" />

/* ---------- Growth stage ---------- */
export function StageCard({ stage, index, days }: { stage: Stage | null; index: number; days: number }) {
  const label = stage ? `${stage.phase} (${stage.name})` : days < 0 ? 'Not yet planted' : 'Set planting date'
  const status = !stage ? '—' : stage.sensitivity === 'High' ? 'Sensitive' : stage.sensitivity === 'Moderate' ? 'Moderate' : 'Stable'
  const color = !stage ? '#777' : stage.sensitivity === 'High' ? '#e8a317' : stage.sensitivity === 'Moderate' ? '#c9a227' : '#3aa335'
  return (
    <Card className="stat-card">
      <Info tip="Estimated from the planting date (typical 110–120 day rice variety)." />
      <div className="stat-head">
        <div className="icon-circle" style={{ background: '#3f8a2e' }}>
          <SproutIcon size={30} color="#fff" />
        </div>
        <div>
          <h3>Crop Growth Stage</h3>
          <div className="stage-label">{label}</div>
          {days >= 0 && <div className="muted small">Day {days} after planting</div>}
        </div>
      </div>
      <div className="stage-track">
        {STAGES.map((s, i) => (
          <div key={s.key} className={`stage-step ${i === index ? 'current' : i < index ? 'done' : ''}`} title={s.name}>
            <StagePlant i={i} />
            <span className="dot" />
          </div>
        ))}
      </div>
      <div className="status">
        Status: <b style={{ color }}>{status}</b>
      </div>
    </Card>
  )
}

function StagePlant({ i }: { i: number }) {
  const h = 8 + i * 3
  return (
    <svg width="22" height="26" viewBox="0 0 22 26">
      <path d={`M11 25V${25 - h}`} stroke="currentColor" strokeWidth="2" />
      <path d={`M11 ${25 - h * 0.5}c-4 0-6-2-7-5 3 0 6 1 7 5zM11 ${25 - h * 0.7}c4 0 6-2 7-5-3 0-6 1-7 5z`} fill="currentColor" />
      {i >= 2 && <ellipse cx="11" cy={25 - h - 2} rx="2" ry="3.5" fill={i >= 3 ? '#d6a623' : 'currentColor'} />}
    </svg>
  )
}

/* ---------- Crop risk ---------- */
export function RiskCard({ level, score, hasData }: { level: Level; score: number; hasData: boolean }) {
  const segs = ['#3aa335', '#8cc63f', '#e8d22a', '#f0a020', '#d93b2b']
  const text =
    level === 'High'
      ? 'High risk of crop stress due to low soil moisture and/or high temperature during a sensitive stage.'
      : level === 'Moderate'
        ? 'Some stress factors detected. Keep an eye on moisture and heat.'
        : 'Crop conditions are within a safe range.'
  return (
    <Card className="stat-card">
      <Info tip="Combines soil moisture, forecast heat and growth-stage sensitivity." />
      <div className="stat-head">
        <div className="icon-circle" style={{ background: LEVEL_COLOR[level] }}>
          <WarnIcon size={30} color="#fff" />
        </div>
        <div>
          <h3>Crop Risk Level</h3>
          <div className={`big-value ${level === 'Moderate' ? 'long' : ''}`} style={{ color: LEVEL_COLOR[level] }}>
            {level.toUpperCase()}
          </div>
        </div>
      </div>
      <div className="gauge">
        {segs.map((c) => (
          <span key={c} style={{ background: c }} />
        ))}
        <i style={{ left: `${Math.max(3, Math.min(97, score * 100))}%` }} />
      </div>
      <p className="muted tiny">{hasData ? text : 'Waiting for sensor data — risk based on forecast only.'}</p>
    </Card>
  )
}

/* ---------- Soil moisture trend chart ---------- */
export function MoistureTrend({
  history,
  min,
  max,
  days = 8,
  title = 'Soil Moisture Trend',
}: {
  history: DailyPoint[]
  min: number
  max: number
  days?: number
  title?: string
}) {
  const dates = lastDays(days)
  const map = new Map(history.map((h) => [h.date, h.soil_moisture]))
  const pts = dates.map((d) => ({ d, v: map.get(d) ?? null }))
  const values = pts.map((p) => p.v).filter((v): v is number => v !== null)
  const yMax = Math.max(60, Math.ceil((Math.max(0, ...values) + 5) / 20) * 20)
  const W = 440,
    H = 180,
    L = 32,
    R = 24,
    T = 12,
    B = 26
  const x = (i: number) => L + (i * (W - L - R)) / Math.max(1, days - 1)
  const y = (v: number) => T + (1 - v / yMax) * (H - T - B)
  const valid = pts.map((p, i) => ({ ...p, i })).filter((p) => p.v !== null) as { d: string; v: number; i: number }[]
  const path = valid.map((p, k) => `${k ? 'L' : 'M'}${x(p.i)},${y(p.v)}`).join(' ')
  const labelIdx = new Set([valid[0]?.i, valid[Math.floor(valid.length / 2)]?.i, valid.at(-1)?.i])
  const ticks = Array.from({ length: yMax / 20 + 1 }, (_, i) => i * 20)
  const step = days > 10 ? Math.ceil(days / 8) : 1

  return (
    <Card className="trend-card tinted">
      <h3 className="card-title">
        <span className="title-icon" style={{ background: '#2f8fd8' }}>
          <DropIcon size={14} color="#fff" />
        </span>
        {title}
      </h3>
      <div className="trend-body">
        <div className="legend-side">
          <div>
            <span className="legend-dot" style={{ background: '#3b5fa8' }} /> Soil Moisture
          </div>
          <div className="optimal">
            Optimal Range
            <br />({min}–{max}%)
          </div>
        </div>
        <svg viewBox={`0 0 ${W} ${H}`} className="chart">
          <rect x={L} y={y(max)} width={W - L - R} height={y(min) - y(max)} fill="#3aa335" opacity="0.1" />
          {ticks.map((t) => (
            <g key={t}>
              <line x1={L} x2={W - R} y1={y(t)} y2={y(t)} stroke="#e3e3e3" />
              <text x={L - 6} y={y(t) + 3} textAnchor="end" className="axis">
                {t}
              </text>
            </g>
          ))}
          {pts.map((p, i) =>
            i % step === 0 || i === days - 1 ? (
              <text key={p.d} x={x(i)} y={H - 6} textAnchor="middle" className="axis">
                {shortDate(p.d)}
              </text>
            ) : null,
          )}
          {path && <path d={path} fill="none" stroke="#3b5fa8" strokeWidth="2" />}
          {valid.map((p) => (
            <g key={p.d}>
              <circle cx={x(p.i)} cy={y(p.v)} r="3.5" fill="#3b5fa8" />
              {labelIdx.has(p.i) && (
                <text x={x(p.i)} y={y(p.v) - 8} textAnchor="middle" className="point-label">
                  {Math.round(p.v)}%
                </text>
              )}
            </g>
          ))}
          {!valid.length && (
            <text x={W / 2} y={H / 2} textAnchor="middle" className="axis">
              No sensor history yet
            </text>
          )}
        </svg>
      </div>
    </Card>
  )
}

/* ---------- 7-day forecast ---------- */
export function ForecastCard({ forecast, error }: { forecast: ForecastDay[]; error: boolean }) {
  const W = 420,
    H = 90,
    L = 22,
    R = 22,
    T = 10,
    B = 18
  const all = forecast.flatMap((f) => [f.tMax, f.tMin])
  const lo = Math.floor((Math.min(...all, 20) - 2) / 5) * 5
  const hi = Math.ceil((Math.max(...all, 35) + 2) / 5) * 5
  const x = (i: number) => L + (i * (W - L - R)) / Math.max(1, forecast.length - 1)
  const y = (v: number) => T + (1 - (v - lo) / (hi - lo)) * (H - T - B)
  const line = (k: 'tMax' | 'tMin') => forecast.map((f, i) => `${i ? 'L' : 'M'}${x(i)},${y(f[k])}`).join(' ')

  return (
    <Card className="forecast-card">
      <h3 className="card-title">
        <SunIcon size={22} />
        7-Day Forecast
      </h3>
      {error && !forecast.length ? (
        <p className="muted small">Weather unavailable — check the internet connection.</p>
      ) : !forecast.length ? (
        <p className="muted small">Loading forecast…</p>
      ) : (
        <>
          <div className="forecast-days">
            {forecast.map((f, i) => {
              const d = new Date(f.date + 'T00:00:00')
              return (
                <div key={f.date} className="fday" title={weatherLabel(f.code) + (f.rainProb !== null ? ` · ${f.rainProb}% rain` : '')}>
                  <div className="fdate">{d.toLocaleDateString('en-US', { month: 'short', day: 'numeric' })}</div>
                  <div className="fname">{i === 0 ? 'Today' : d.toLocaleDateString('en-US', { weekday: 'short' })}</div>
                  <WeatherIcon code={f.code} />
                  <div className="fmax" style={{ color: f.tMax >= 35 ? '#d93b2b' : undefined }}>
                    {f.tMax}°C
                  </div>
                  <div className="fmin">{f.tMin}°C</div>
                </div>
              )
            })}
          </div>
          <svg viewBox={`0 0 ${W} ${H}`} className="chart mini">
            {[lo, Math.round((lo + hi) / 2), hi].map((t) => (
              <g key={t}>
                <line x1={L} x2={W - R} y1={y(t)} y2={y(t)} stroke="#eee" />
                <text x={L - 4} y={y(t) + 3} textAnchor="end" className="axis">
                  {t}
                </text>
              </g>
            ))}
            <path d={line('tMax')} fill="none" stroke="#f0a020" strokeWidth="1.8" />
            <path d={line('tMin')} fill="none" stroke="#3fb8d6" strokeWidth="1.8" />
            {forecast.map((f, i) => (
              <g key={f.date}>
                <circle cx={x(i)} cy={y(f.tMax)} r="2.5" fill="#f0a020" />
                <circle cx={x(i)} cy={y(f.tMin)} r="2.5" fill="#3fb8d6" />
                <text x={x(i)} y={H - 4} textAnchor="middle" className="axis">
                  {shortDate(f.date)}
                </text>
              </g>
            ))}
          </svg>
          <div className="mini-legend">
            <span><i style={{ background: '#f0a020' }} /> High temperature (°C)</span>
            <span><i style={{ background: '#3fb8d6' }} /> Low temperature (°C)</span>
          </div>
        </>
      )}
    </Card>
  )
}

/* ---------- Crop information ---------- */
export function CropInfoCard({
  crop,
  plantingDate,
  stage,
  sensitivity,
}: {
  crop: string
  plantingDate: string
  stage: string
  sensitivity: Level | '—'
}) {
  const pd = plantingDate
    ? new Date(plantingDate + 'T00:00:00').toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' })
    : '—'
  return (
    <Card className="crop-card">
      <Info tip="Change these in Settings." />
      <h3 className="card-title green">
        <LeafIcon size={20} />
        Crop Information
      </h3>
      <div className="crop-img" role="img" aria-label="Rice field illustration" />
      <dl>
        <dt>Crop</dt>
        <dd>{crop}</dd>
        <dt>Planting Date</dt>
        <dd>{pd}</dd>
        <dt>Growth Stage</dt>
        <dd>{stage}</dd>
        <dt>Sensitivity Level</dt>
        <dd>{sensitivity}</dd>
      </dl>
    </Card>
  )
}

/* ---------- Recommended actions ---------- */
export function ActionsCard({ actions, ai }: { actions: string[]; ai?: AiAdvice }) {
  const useAi = Boolean(ai?.enabled && ai.actions?.length)
  const list = useAi ? ai!.actions! : actions
  return (
    <Card className="actions-card">
      <h3 className="card-title">
        <span className="title-icon dark">
          <ToolsIcon size={14} color="#fff" />
        </span>
        Recommended Actions
        {useAi && <span className="ai-tag">✦ Gemini</span>}
      </h3>
      <ol>
        {list.map((a) => (
          <li key={a}>{a}</li>
        ))}
      </ol>
      {ai?.enabled && (
        <div className="ai-row">
          <span className="muted tiny">
            {ai.loading
              ? 'Gemini is analyzing your field…'
              : ai.error
                ? `AI unavailable (${ai.error}) — showing built-in tips.`
                : ai.at
                  ? `AI advice from ${new Date(ai.at).toLocaleTimeString('en-US', { hour: 'numeric', minute: '2-digit' })}`
                  : 'Built-in tips'}
          </span>
          <button className="btn ghost small" onClick={ai.generate} disabled={ai.loading}>
            {ai.loading ? '…' : '✦ Refresh'}
          </button>
        </div>
      )}
      <div className="cta">Early action can help reduce crop loss!</div>
    </Card>
  )
}
