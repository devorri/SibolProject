import { useCallback, useEffect, useState } from 'react'
import { api, fetchForecast } from '../lib/api'
import { useSettings } from '../settings'
import type { DailyPoint, ForecastDay, Reading } from '../types'

export interface SibolData {
  latest: Reading | null
  history: DailyPoint[]
  forecast: ForecastDay[]
  dbOk: boolean // Supabase reachable
  dbError: string
  online: boolean // ESP32 sent data recently
  weatherError: boolean
  refresh: () => void
}

export function useSibolData(historyDays = 8): SibolData {
  const { settings } = useSettings()
  const [latest, setLatest] = useState<Reading | null>(null)
  const [history, setHistory] = useState<DailyPoint[]>([])
  const [forecast, setForecast] = useState<ForecastDay[]>([])
  const [dbError, setDbError] = useState('')
  const [weatherError, setWeatherError] = useState(false)
  const [now, setNow] = useState(() => Date.now())

  const loadSensor = useCallback(async () => {
    try {
      setLatest(await api.latest())
      setDbError('')
    } catch (e) {
      setDbError((e as Error).message)
    }
    setNow(Date.now())
  }, [])

  const loadHistory = useCallback(async () => {
    try {
      setHistory(await api.history(historyDays))
    } catch {
      /* shown via dbError */
    }
  }, [historyDays])

  const loadWeather = useCallback(async () => {
    try {
      setForecast(await fetchForecast(settings.lat, settings.lng))
      setWeatherError(false)
    } catch {
      setWeatherError(true)
    }
  }, [settings.lat, settings.lng])

  useEffect(() => {
    loadSensor()
    const t = setInterval(loadSensor, 5000)
    return () => clearInterval(t)
  }, [loadSensor])

  useEffect(() => {
    loadHistory()
    const t = setInterval(loadHistory, 60000)
    return () => clearInterval(t)
  }, [loadHistory])

  useEffect(() => {
    loadWeather()
    const t = setInterval(loadWeather, 30 * 60000)
    return () => clearInterval(t)
  }, [loadWeather])

  const online = !!latest && now - latest.ts < settings.staleMinutes * 60000

  return {
    latest,
    history,
    forecast,
    dbOk: !dbError,
    dbError,
    online,
    weatherError,
    refresh: () => {
      loadSensor()
      loadHistory()
      loadWeather()
    },
  }
}
