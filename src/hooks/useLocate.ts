import { useState } from 'react'
import { getCurrentPosition, reverseGeocode } from '../lib/geo'
import { useSettings } from '../settings'
import type { Settings } from '../types'

export type LocateStatus = 'idle' | 'locating' | 'done' | 'error'

/** One-press "use my real location": saves lat/lng + place name, so the forecast and map follow. */
export function useLocate() {
  const { settings, save } = useSettings()
  const [status, setStatus] = useState<LocateStatus>('idle')
  const [message, setMessage] = useState('')

  const locate = async (base: Settings = settings): Promise<Settings | null> => {
    setStatus('locating')
    setMessage('Finding your location…')
    try {
      const pos = await getCurrentPosition()
      const lat = Math.round(pos.lat * 1e5) / 1e5
      const lng = Math.round(pos.lng * 1e5) / 1e5
      const name = await reverseGeocode(lat, lng).catch(() => `${lat.toFixed(4)}, ${lng.toFixed(4)}`)
      const next: Settings = { ...base, lat, lng, locationName: name }
      save(next)
      setStatus('done')
      setMessage(`Location set to ${name} (accurate to ±${Math.round(pos.accuracy)} m). Forecast updated.`)
      return next
    } catch (e) {
      setStatus('error')
      setMessage((e as Error).message)
      return null
    }
  }

  return { locate, status, message, locating: status === 'locating' }
}
