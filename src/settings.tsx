/* eslint-disable react-refresh/only-export-components */
import { createContext, useContext, useState, type ReactNode } from 'react'
import type { Settings } from './types'

export const DEFAULT_SETTINGS: Settings = {
  locationName: 'San Miguel, Taguig City',
  lat: 14.5266,
  lng: 121.0645,
  fieldRadius: 60,
  cropName: 'Rice (Palay)',
  plantingDate: '2026-08-30',
  optimalMin: 40,
  optimalMax: 60,
  staleMinutes: 2,
}

const KEY = 'sibol-settings'

function load(): Settings {
  try {
    return { ...DEFAULT_SETTINGS, ...JSON.parse(localStorage.getItem(KEY) || '{}') }
  } catch {
    return DEFAULT_SETTINGS
  }
}

interface Ctx {
  settings: Settings
  save: (s: Settings) => void
}

const SettingsContext = createContext<Ctx>({ settings: DEFAULT_SETTINGS, save: () => {} })

export function SettingsProvider({ children }: { children: ReactNode }) {
  const [settings, setSettings] = useState<Settings>(load)
  const save = (s: Settings) => {
    setSettings(s)
    localStorage.setItem(KEY, JSON.stringify(s))
  }
  return <SettingsContext.Provider value={{ settings, save }}>{children}</SettingsContext.Provider>
}

export const useSettings = () => useContext(SettingsContext)
