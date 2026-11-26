import { useCallback, useEffect, useState } from 'react'
import { api } from '../lib/api'
import type { DeviceControls, PumpMode } from '../types'

/** ESP32 sends a heartbeat every 15 s; after this long without one we call it offline. */
const OFFLINE_AFTER_MS = 45000

export interface DeviceState {
  controls: DeviceControls | null
  error: string
  busy: boolean
  deviceOnline: boolean
  lastSeen: number | null
  now: number
  setPump: (mode: PumpMode, command: boolean) => Promise<void>
}

/** Pump/camera control row for the ESP32 (polled every 2 s). */
export function useDevice(): DeviceState {
  const [controls, setControls] = useState<DeviceControls | null>(null)
  const [error, setError] = useState('')
  const [busy, setBusy] = useState(false)
  const [now, setNow] = useState(() => Date.now())

  const load = useCallback(async () => {
    try {
      const c = await api.controls()
      setControls(c)
      setError(c ? '' : 'No device_controls row for this ESP32 — run supabase/schema.sql')
    } catch (e) {
      setError((e as Error).message)
    }
    setNow(Date.now())
  }, [])

  useEffect(() => {
    const first = setTimeout(load, 0)
    const t = setInterval(load, 2000)
    return () => {
      clearTimeout(first)
      clearInterval(t)
    }
  }, [load])

  const setPump = async (mode: PumpMode, command: boolean) => {
    setBusy(true)
    // optimistic update so the buttons react instantly
    setControls((c) => (c ? { ...c, pump_mode: mode, pump_command: command } : c))
    try {
      await api.setPump(mode, command)
      setError('')
    } catch (e) {
      setError((e as Error).message)
    } finally {
      setBusy(false)
      load()
    }
  }

  const lastSeen = controls?.last_seen ? new Date(controls.last_seen).getTime() : null
  const deviceOnline = lastSeen !== null && now - lastSeen < OFFLINE_AFTER_MS

  return { controls, error, busy, deviceOnline, lastSeen, now, setPump }
}
