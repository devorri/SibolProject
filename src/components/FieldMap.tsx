import { useEffect, useRef, useState, type PointerEvent } from 'react'
import type { Level } from '../types'
import { PinIcon } from './icons'

const TILE = 256
const TILE_URL = (z: number, x: number, y: number) =>
  `https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/${z}/${y}/${x}`

function project(lat: number, lng: number, z: number) {
  const s = TILE * 2 ** z
  const r = (lat * Math.PI) / 180
  return {
    x: ((lng + 180) / 360) * s,
    y: ((1 - Math.log(Math.tan(r) + 1 / Math.cos(r)) / Math.PI) / 2) * s,
  }
}

const HEAT: Record<Level, [string, string, string]> = {
  High: ['#e8221b', '#f47d1f', '#d8df2a'],
  Moderate: ['#f29a1f', '#f2d62a', '#7cc443'],
  Low: ['#7fd34a', '#46b53a', '#2f9e3b'],
}

export function FieldMap({
  lat,
  lng,
  radius,
  level,
  initialZoom = 17,
  showLegend = true,
}: {
  lat: number
  lng: number
  radius: number // meters
  level: Level
  initialZoom?: number
  showLegend?: boolean
}) {
  const ref = useRef<HTMLDivElement>(null)
  const [size, setSize] = useState({ w: 0, h: 0 })
  const [z, setZ] = useState(initialZoom)
  const [off, setOff] = useState({ x: 0, y: 0 })
  const drag = useRef<{ x: number; y: number; ox: number; oy: number } | null>(null)

  useEffect(() => {
    const el = ref.current
    if (!el) return
    const ro = new ResizeObserver(() => setSize({ w: el.clientWidth, h: el.clientHeight }))
    ro.observe(el)
    return () => ro.disconnect()
  }, [])

  // re-center when the field location changes
  const [lastPos, setLastPos] = useState(`${lat},${lng}`)
  if (lastPos !== `${lat},${lng}`) {
    setLastPos(`${lat},${lng}`)
    setOff({ x: 0, y: 0 })
  }

  const c = project(lat, lng, z)
  const left = c.x - off.x - size.w / 2
  const top = c.y - off.y - size.h / 2
  const n = 2 ** z

  const tiles: { key: string; src: string; x: number; y: number }[] = []
  if (size.w) {
    for (let tx = Math.floor(left / TILE); tx <= Math.floor((left + size.w) / TILE); tx++) {
      for (let ty = Math.floor(top / TILE); ty <= Math.floor((top + size.h) / TILE); ty++) {
        if (ty < 0 || ty >= n) continue
        const wx = ((tx % n) + n) % n
        tiles.push({ key: `${z}-${tx}-${ty}`, src: TILE_URL(z, wx, ty), x: tx * TILE - left, y: ty * TILE - top })
      }
    }
  }

  const fx = c.x - left
  const fy = c.y - top
  const mpp = (156543.03392 * Math.cos((lat * Math.PI) / 180)) / n
  const r = radius / mpp
  const [c1, c2, c3] = HEAT[level]

  const zoom = (d: 1 | -1) => {
    const nz = Math.max(3, Math.min(19, z + d))
    if (nz === z) return
    const f = 2 ** (nz - z)
    setOff({ x: off.x * f, y: off.y * f })
    setZ(nz)
  }

  const onDown = (e: PointerEvent) => {
    if ((e.target as HTMLElement).closest('button')) return
    ;(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId)
    drag.current = { x: e.clientX, y: e.clientY, ox: off.x, oy: off.y }
  }
  const onMove = (e: PointerEvent) => {
    if (!drag.current) return
    setOff({ x: drag.current.ox + e.clientX - drag.current.x, y: drag.current.oy + e.clientY - drag.current.y })
  }
  const onUp = () => (drag.current = null)

  return (
    <div className="map" ref={ref} onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerCancel={onUp}>
      {tiles.map((t) => (
        <img key={t.key} src={t.src} alt="" draggable={false} className="tile" onError={(e) => (e.currentTarget.style.visibility = 'hidden')} style={{ left: t.x, top: t.y }} />
      ))}

      {/* risk heat overlay over the field */}
      <svg className="heat" style={{ left: fx - r * 1.6, top: fy - r * 1.6, width: r * 3.2, height: r * 3.2 }} viewBox="-160 -160 320 320">
        <defs>
          <radialGradient id="hg">
            <stop offset="0%" stopColor={c1} stopOpacity="0.95" />
            <stop offset="45%" stopColor={c2} stopOpacity="0.85" />
            <stop offset="80%" stopColor={c3} stopOpacity="0.6" />
            <stop offset="100%" stopColor={c3} stopOpacity="0" />
          </radialGradient>
          <filter id="blur">
            <feGaussianBlur stdDeviation="6" />
          </filter>
        </defs>
        <g filter="url(#blur)">
          <path
            d="M-120,-60 L-20,-125 L90,-95 L135,-10 L105,85 L10,120 L-95,95 L-135,20 Z"
            fill={c3}
            opacity="0.45"
          />
          <ellipse cx="-30" cy="-10" rx="90" ry="75" fill="url(#hg)" />
          <ellipse cx="45" cy="25" rx="75" ry="65" fill="url(#hg)" />
          <ellipse cx="20" cy="-45" rx="55" ry="45" fill="url(#hg)" />
        </g>
        <path d="M-120,-60 L-20,-125 L90,-95 L135,-10 L105,85 L10,120 L-95,95 L-135,20 Z" fill="none" stroke="#fff" strokeWidth="2" opacity="0.6" />
      </svg>

      <div className="map-pin" style={{ left: fx, top: fy }}>
        <PinIcon size={30} color="#e8221b" />
      </div>

      {showLegend && (
        <div className="map-legend">
          <b>Risk Level</b>
          <div><i style={{ background: '#e8221b' }} /> High</div>
          <div><i style={{ background: '#f2b01f' }} /> Moderate</div>
          <div><i style={{ background: '#46b53a' }} /> Low</div>
        </div>
      )}

      <div className="north" aria-label="North">
        <svg width="18" height="26" viewBox="0 0 18 26">
          <path d="M9 1 17 25 9 19 1 25z" fill="#fff" stroke="#333" />
          <path d="M9 1v18L1 25z" fill="#333" />
        </svg>
      </div>

      <div className="zoom">
        <button onClick={() => zoom(1)} aria-label="Zoom in">+</button>
        <button onClick={() => zoom(-1)} aria-label="Zoom out">−</button>
      </div>
      <div className="attrib">Imagery © Esri</div>
    </div>
  )
}
