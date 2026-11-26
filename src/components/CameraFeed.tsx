import { useEffect, useRef, useState } from 'react'
import type { DeviceState } from '../hooks/useDevice'
import { api, FRAME_URL } from '../lib/api'
import { aiCheckPhoto, geminiConfigured, type FieldContext, type PhotoCheck } from '../lib/gemini'
import type { Reading } from '../types'
import { DropIcon, ThermoIcon } from './icons'

/** A frame newer than this counts as live. */
const LIVE_MS = 10000
/** Gap between frame downloads while live / while the camera looks idle (saves bandwidth). */
const FAST_POLL_MS = 400
const SLOW_POLL_MS = 3000

function ago(ms: number) {
  const s = Math.max(0, Math.round(ms / 1000))
  if (s < 60) return `${s}s ago`
  if (s < 3600) return `${Math.round(s / 60)} min ago`
  if (s < 86400) return `${Math.round(s / 3600)} h ago`
  return `${Math.round(s / 86400)} d ago`
}

/* ===================== Camera feed (replaces the home map) ===================== */
export function CameraFeed({
  latest,
  device,
  context,
}: {
  latest: Reading | null
  device: DeviceState
  context: FieldContext
}) {
  const [src, setSrc] = useState<string | null>(null)
  const [frameTs, setFrameTs] = useState<number | null>(null)
  const [missing, setMissing] = useState(false)
  const [now, setNow] = useState(() => Date.now())
  const blobRef = useRef<Blob | null>(null)
  const frameTsRef = useRef<number | null>(null)

  const [check, setCheck] = useState<PhotoCheck | null>(null)
  const [checking, setChecking] = useState(false)
  const [checkError, setCheckError] = useState('')

  // Tell the ESP32-CAM someone is watching (it uploads frames only while stream_until is in the future)
  useEffect(() => {
    const ping = () => {
      if (!document.hidden) api.keepStreamAlive(30).catch(() => {})
    }
    ping()
    const t = setInterval(ping, 10000)
    document.addEventListener('visibilitychange', ping)
    return () => {
      clearInterval(t)
      document.removeEventListener('visibilitychange', ping)
    }
  }, [])

  // Download the latest frame over and over
  useEffect(() => {
    let alive = true
    let current: string | null = null
    let timer = 0

    const tick = async () => {
      if (!alive) return
      if (document.hidden) {
        timer = window.setTimeout(tick, 1000)
        return
      }
      try {
        const res = await fetch(`${FRAME_URL}?t=${Date.now()}`, { cache: 'no-store' })
        if (!res.ok) throw new Error(String(res.status))
        const lm = res.headers.get('last-modified')
        const ts = lm ? new Date(lm).getTime() : Date.now()
        const blob = await res.blob()
        if (!alive) return

        // only swap the image if it actually changed
        if (ts !== frameTsRef.current || !current) {
          const url = URL.createObjectURL(blob)
          setSrc(url)
          if (current) URL.revokeObjectURL(current)
          current = url
          blobRef.current = blob
          frameTsRef.current = ts
          setFrameTs(ts)
        }
        setMissing(false)
        const live = Date.now() - ts < LIVE_MS
        timer = window.setTimeout(tick, live ? FAST_POLL_MS : SLOW_POLL_MS)
      } catch {
        if (!alive) return
        setMissing(true)
        timer = window.setTimeout(tick, SLOW_POLL_MS)
      }
    }

    tick()
    return () => {
      alive = false
      clearTimeout(timer)
      if (current) URL.revokeObjectURL(current)
    }
  }, [])

  // clock for "x seconds ago"
  useEffect(() => {
    const t = setInterval(() => setNow(Date.now()), 1000)
    return () => clearInterval(t)
  }, [])

  const live = frameTs !== null && now - frameTs < LIVE_MS
  const badge = live
    ? 'LIVE'
    : frameTs
      ? `Last frame ${ago(now - frameTs)}`
      : missing
        ? 'No camera image yet'
        : 'Connecting…'

  const runCheck = async () => {
    if (!blobRef.current) return
    setChecking(true)
    setCheckError('')
    try {
      setCheck(await aiCheckPhoto(blobRef.current, context))
    } catch (e) {
      setCheckError((e as Error).message)
    } finally {
      setChecking(false)
    }
  }

  const temp = latest?.temperature
  const hum = latest?.humidity

  return (
    <div className="cam-wrap">
      <div className="cam">
        {src ? (
          <img src={src} alt="Live camera view of the field" draggable={false} />
        ) : (
          <div className="cam-empty">
            <CameraIcon />
            <p>{missing ? 'Waiting for the ESP32-CAM to upload a frame…' : 'Connecting to camera…'}</p>
            <p className="muted tiny">Make sure the ESP32-CAM is powered and online.</p>
          </div>
        )}

        <span className={`cam-badge ${live ? 'live' : 'idle'}`}>
          <i />
          {badge}
        </span>

        {geminiConfigured && (
          <button className="cam-ai" onClick={runCheck} disabled={!src || checking} title="Ask Gemini to check the plant in this photo">
            {checking ? 'Checking…' : '✦ AI check'}
          </button>
        )}

        <div className="cam-chips" title="Live readings from the DHT11 sensor at the field (not the forecast)">
          <span>
            <ThermoIcon size={14} color="#fff" /> {temp != null ? `${temp.toFixed(1)}°C` : '--'}
          </span>
          <span>
            <DropIcon size={14} color="#fff" /> {hum != null ? `${Math.round(hum)}%` : '--'}
          </span>
        </div>

        {(check || checkError) && (
          <div className={`cam-check ${check ? check.status.toLowerCase() : 'error'}`}>
            <button className="close" onClick={() => { setCheck(null); setCheckError('') }} aria-label="Close">
              ×
            </button>
            {check ? (
              <>
                <b>Plant check: {check.status}</b>
                <p>{check.summary}</p>
                {check.observations.length > 0 && (
                  <ul>
                    {check.observations.map((o) => (
                      <li key={o}>{o}</li>
                    ))}
                  </ul>
                )}
              </>
            ) : (
              <p>AI check failed: {checkError}</p>
            )}
          </div>
        )}
      </div>

      <PumpControls device={device} compact />
    </div>
  )
}

/* ===================== Water pump controls ===================== */
export function PumpControls({ device, compact = false }: { device: DeviceState; compact?: boolean }) {
  const { controls, busy, error, deviceOnline, lastSeen, now, setPump } = device
  const on = controls?.pump_state ?? false
  const mode = controls?.pump_mode ?? 'auto'
  const cmd = controls?.pump_command ?? false
  const pending = controls ? (mode === 'manual' && cmd !== on) : false

  return (
    <div className={`pump-bar ${compact ? 'compact' : ''}`}>
      <div className="pump-state">
        <span className={`pump-dot ${on ? 'on' : 'off'}`} />
        <div>
          <b>Water Pump: {controls ? (on ? 'ON' : 'OFF') : '--'}</b>
          <div className="muted tiny">
            {!controls
              ? error || 'Loading…'
              : !deviceOnline
                ? `ESP32 offline${lastSeen ? ` · last seen ${ago(now - lastSeen)}` : ''} — commands apply when it reconnects`
                : pending
                  ? 'Sending command to ESP32…'
                  : mode === 'auto'
                    ? 'Auto: waters when soil is dry'
                    : 'Manual control'}
          </div>
        </div>
      </div>
      <div className="seg" role="group" aria-label="Pump mode">
        <button className={mode === 'auto' ? 'active' : ''} disabled={busy || !controls} onClick={() => setPump('auto', false)}>
          Auto
        </button>
        <button className={mode === 'manual' && cmd ? 'active on' : ''} disabled={busy || !controls} onClick={() => setPump('manual', true)}>
          Turn ON
        </button>
        <button className={mode === 'manual' && !cmd ? 'active off' : ''} disabled={busy || !controls} onClick={() => setPump('manual', false)}>
          Turn OFF
        </button>
      </div>
      {controls && error && <div className="pump-error">{error}</div>}
    </div>
  )
}

const CameraIcon = () => (
  <svg width="42" height="42" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M4 8h3l2-3h6l2 3h3a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z" />
    <circle cx="12" cy="13.5" r="3.5" />
  </svg>
)
