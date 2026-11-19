import { useEffect, useState, type ReactNode } from 'react'
import { FieldMap } from '../components/FieldMap'
import { Card, ForecastCard, MoistureTrend, RiskCard, StageCard } from '../components/Widgets'
import { DropIcon, RefreshIcon, ThermoIcon } from '../components/icons'
import { useCropModel } from '../hooks/useCropModel'
import type { SibolData } from '../hooks/useSibolData'
import { api } from '../lib/api'
import { LEVEL_COLOR } from '../lib/crop'
import { supabaseConfigured, SUPABASE_URL } from '../lib/supabase'
import { DEFAULT_SETTINGS, useSettings } from '../settings'
import type { Reading, Settings } from '../types'
import { HeatCard, SoilCard } from './Home'

const fmtTime = (ts: number) =>
  new Date(ts).toLocaleString('en-US', { month: 'short', day: 'numeric', hour: 'numeric', minute: '2-digit', second: '2-digit' })

function ago(ts: number) {
  const s = Math.round((Date.now() - ts) / 1000)
  if (s < 60) return `${s}s ago`
  if (s < 3600) return `${Math.round(s / 60)} min ago`
  if (s < 86400) return `${Math.round(s / 3600)} h ago`
  return `${Math.round(s / 86400)} d ago`
}

/* ===================== Dashboard ===================== */
export function Dashboard({ data }: { data: SibolData }) {
  const m = useCropModel(data)
  const r = data.latest
  return (
    <div className="page">
      {!data.dbOk && <div className="alert">{data.dbError}</div>}
      <div className="page-head">
        <h1 className="welcome">Dashboard</h1>
        <button className="btn ghost" onClick={data.refresh}>
          <RefreshIcon size={16} /> Refresh
        </button>
      </div>

      <div className="tiles">
        <Tile label="Soil Moisture" value={r?.soil_moisture != null ? `${r.soil_moisture}%` : '--'} icon={<DropIcon size={22} color="#2f8fd8" />} />
        <Tile label="Air Temperature" value={r?.temperature != null ? `${r.temperature}°C` : '--'} icon={<ThermoIcon size={22} color="#f0a020" />} />
        <Tile label="Humidity" value={r?.humidity != null ? `${r.humidity}%` : '--'} icon={<DropIcon size={22} color="#3fb8d6" />} />
        <Tile label="Soil Sensor Raw" value={r?.soil_raw != null ? String(r.soil_raw) : '--'} />
        <Tile
          label="Device"
          value={r?.device_id ?? '—'}
          sub={r ? `Last update ${ago(r.ts)}` : 'No data yet'}
          dot={data.online ? '#3aa335' : '#d93b2b'}
        />
      </div>

      <div className="dash-grid">
        <SoilCard m={m} />
        <HeatCard m={m} />
        <StageCard stage={m.stage} index={m.index} days={m.days} />
        <RiskCard level={m.risk.level} score={m.risk.score} hasData={m.moisture !== null} />
      </div>

      <div className="two-col">
        <MoistureTrend history={data.history} min={m.s.optimalMin} max={m.s.optimalMax} days={14} title="Soil Moisture — Last 14 Days" />
        <ForecastCard forecast={data.forecast} error={data.weatherError} />
      </div>
    </div>
  )
}

function Tile({ label, value, sub, icon, dot }: { label: string; value: string; sub?: string; icon?: ReactNode; dot?: string }) {
  return (
    <Card className="tile-card">
      <div className="tile-label">
        {icon}
        {dot && <span className="legend-dot" style={{ background: dot }} />}
        {label}
      </div>
      <div className="tile-value">{value}</div>
      {sub && <div className="muted small">{sub}</div>}
    </Card>
  )
}

/* ===================== Maps ===================== */
export function Maps({ data }: { data: SibolData }) {
  const m = useCropModel(data)
  return (
    <div className="page">
      <h1 className="welcome">Field Map</h1>
      <p className="lead">
        {m.s.locationName} · {m.s.lat.toFixed(5)}, {m.s.lng.toFixed(5)} · Current risk:{' '}
        <b style={{ color: LEVEL_COLOR[m.risk.level] }}>{m.risk.level}</b>
      </p>
      <div className="card map-card big">
        <FieldMap lat={m.s.lat} lng={m.s.lng} radius={m.s.fieldRadius} level={m.risk.level} initialZoom={17} />
      </div>
      <p className="muted small">Drag to move · use +/− to zoom · change the field location in Settings.</p>
    </div>
  )
}

/* ===================== Data ===================== */
export function DataPage() {
  const [rows, setRows] = useState<Reading[]>([])
  const [total, setTotal] = useState(0)
  const [error, setError] = useState('')
  const [tick, setTick] = useState(0)

  useEffect(() => {
    let alive = true
    const load = () =>
      api
        .readings(500)
        .then((r) => {
          if (!alive) return
          setRows(r.readings)
          setTotal(r.total)
          setError('')
        })
        .catch((e: Error) => alive && setError(`Cannot load readings from Supabase: ${e.message}`))
    load()
    const t = setInterval(load, 10000)
    return () => {
      alive = false
      clearInterval(t)
    }
  }, [tick])

  const exportCsv = () => {
    const head = 'timestamp,device_id,soil_moisture,soil_raw,temperature,humidity'
    const lines = [...rows]
      .reverse()
      .map((r) => [new Date(r.ts).toISOString(), r.device_id ?? '', r.soil_moisture ?? '', r.soil_raw ?? '', r.temperature ?? '', r.humidity ?? ''].join(','))
    const blob = new Blob([[head, ...lines].join('\n')], { type: 'text/csv' })
    const a = document.createElement('a')
    a.href = URL.createObjectURL(blob)
    a.download = `sibol-readings-${new Date().toISOString().slice(0, 10)}.csv`
    a.click()
    URL.revokeObjectURL(a.href)
  }

  return (
    <div className="page">
      <div className="page-head">
        <h1 className="welcome">Sensor Data</h1>
        <div className="btn-row">
          <button className="btn ghost" onClick={() => setTick(tick + 1)}>
            <RefreshIcon size={16} /> Refresh
          </button>
          <button className="btn" onClick={exportCsv} disabled={!rows.length}>
            Export CSV
          </button>
        </div>
      </div>
      <p className="lead">
        Showing the latest {rows.length} of {total} readings from the ESP32.
      </p>
      {error && <div className="alert">{error}</div>}
      <Card className="table-card">
        <table>
          <thead>
            <tr>
              <th>Time</th>
              <th>Device</th>
              <th>Soil Moisture</th>
              <th>Raw</th>
              <th>Temp</th>
              <th>Humidity</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((r) => (
              <tr key={`${r.ts}-${r.device_id}`}>
                <td data-label="Time">{fmtTime(r.ts)}</td>
                <td data-label="Device">{r.device_id}</td>
                <td data-label="Soil Moisture">{r.soil_moisture != null ? `${r.soil_moisture}%` : '—'}</td>
                <td data-label="Raw">{r.soil_raw ?? '—'}</td>
                <td data-label="Temp">{r.temperature != null ? `${r.temperature}°C` : '—'}</td>
                <td data-label="Humidity">{r.humidity != null ? `${r.humidity}%` : '—'}</td>
              </tr>
            ))}
            {!rows.length && (
              <tr>
                <td colSpan={6} className="muted center">
                  No readings yet. Power up the ESP32 or run <code>npm run simulate</code>.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </Card>
    </div>
  )
}

/* ===================== Settings ===================== */
export function SettingsPage() {
  const { settings, save } = useSettings()
  const [f, setF] = useState<Settings>(settings)
  const [saved, setSaved] = useState(false)
  const set = <K extends keyof Settings>(k: K, v: Settings[K]) => {
    setF({ ...f, [k]: v })
    setSaved(false)
  }
  const numField = (k: keyof Settings, label: string, step = 'any') => (
    <label>
      {label}
      <input type="number" step={step} value={f[k] as number} onChange={(e) => set(k, Number(e.target.value) as never)} />
    </label>
  )

  return (
    <div className="page">
      <h1 className="welcome">Settings</h1>
      <p className="lead">Saved in this browser.</p>
      <form
        className="settings"
        onSubmit={(e) => {
          e.preventDefault()
          save(f)
          setSaved(true)
        }}
      >
        <Card>
          <h3 className="card-title">Crop</h3>
          <label>
            Crop name
            <input value={f.cropName} onChange={(e) => set('cropName', e.target.value)} />
          </label>
          <label>
            Planting date
            <input type="date" value={f.plantingDate} onChange={(e) => set('plantingDate', e.target.value)} />
          </label>
          <div className="row2">
            {numField('optimalMin', 'Optimal moisture min (%)')}
            {numField('optimalMax', 'Optimal moisture max (%)')}
          </div>
        </Card>

        <Card>
          <h3 className="card-title">Field location</h3>
          <label>
            Location name
            <input value={f.locationName} onChange={(e) => set('locationName', e.target.value)} />
          </label>
          <div className="row2">
            {numField('lat', 'Latitude')}
            {numField('lng', 'Longitude')}
          </div>
          {numField('fieldRadius', 'Field radius on map (meters)', '1')}
          <p className="muted small">Tip: in Google Maps, right-click your field and click the coordinates to copy them.</p>
        </Card>

        <Card>
          <h3 className="card-title">ESP32 / Supabase</h3>
          <p className="small">
            Database:{' '}
            {supabaseConfigured ? (
              <b style={{ color: '#2e8a2a' }}>{SUPABASE_URL.replace('https://', '')}</b>
            ) : (
              <b style={{ color: '#c62f20' }}>not configured — fill in .env and restart npm run dev</b>
            )}
          </p>
          {numField('staleMinutes', 'Mark ESP32 offline after (minutes)', '1')}
          <p className="muted small">
            The ESP32 inserts rows into the <code>sensor_readings</code> table. Old data can be deleted from the Supabase Table Editor.
          </p>
        </Card>

        <div className="btn-row">
          <button className="btn" type="submit">
            Save settings
          </button>
          <button
            className="btn ghost"
            type="button"
            onClick={() => {
              setF(DEFAULT_SETTINGS)
              setSaved(false)
            }}
          >
            Reset to defaults
          </button>
          {saved && <span className="saved">✓ Saved</span>}
        </div>
      </form>
    </div>
  )
}
