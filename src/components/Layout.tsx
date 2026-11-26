import type { ReactNode } from 'react'
import logoUrl from '../assets/sibol-wordmark.png'
import type { Page } from '../types'
import { useLocate } from '../hooks/useLocate'
import { useSettings } from '../settings'
import { CalendarIcon, ChartIcon, ChatIcon, DataIcon, GearIcon, HomeIcon, LeafIcon, MapIcon, PinIcon, SproutIcon, UserIcon } from './icons'

const NAV: { page: Page; label: string; icon: ReactNode }[] = [
  { page: 'home', label: 'Home', icon: <HomeIcon /> },
  { page: 'dashboard', label: 'Dashboard', icon: <ChartIcon /> },
  { page: 'chat', label: 'Sibol AI', icon: <ChatIcon /> },
  { page: 'maps', label: 'Maps', icon: <MapIcon /> },
  { page: 'data', label: 'Data', icon: <DataIcon /> },
  { page: 'settings', label: 'Settings', icon: <GearIcon /> },
]

export function Sidebar({ page, onNavigate, open }: { page: Page; onNavigate: (p: Page) => void; open: boolean }) {
  return (
    <aside className={`sidebar ${open ? 'open' : ''}`}>
      <div className="sidebar-logo">
        <LeafIcon size={44} />
      </div>
      <nav>
        {NAV.map((n) => (
          <button key={n.page} className={`nav-item ${page === n.page ? 'active' : ''}`} onClick={() => onNavigate(n.page)}>
            {n.icon}
            <span>{n.label}</span>
          </button>
        ))}
      </nav>
      <div className="sidebar-foot">
        <SproutIcon size={28} />
        <p>
          See the signs. Save
          <br />
          the harvest.
        </p>
      </div>
    </aside>
  )
}

export function Logo() {
  return <img className="logo" src={logoUrl} alt="SIBOL" />
}

export function Header({
  online,
  dbOk,
  onMenu,
}: {
  online: boolean
  dbOk: boolean
  onMenu: () => void
}) {
  const { settings } = useSettings()
  const loc = useLocate()
  const today = new Date().toLocaleDateString('en-US', { month: 'long', day: 'numeric', year: 'numeric', timeZone: 'Asia/Manila' })
  const status = !dbOk ? 'Database error' : online ? 'ESP32 online' : 'ESP32 offline'
  const cls = !dbOk ? 'bad' : online ? 'good' : 'warn'
  return (
    <header className="topbar">
      <button className="menu-btn" onClick={onMenu} aria-label="Menu">
        ☰
      </button>
      <Logo />
      <div className="topbar-right">
        <span className={`status-pill ${cls}`} title={status}>
          <i />
          {status}
        </span>
        <div className="topbar-item">
          <button
            className={`loc-btn ${loc.status}`}
            onClick={() => loc.locate()}
            disabled={loc.locating}
            title={loc.message || 'Click to use your current location for the forecast'}
          >
            <PinIcon size={20} />
            <span>{loc.locating ? 'Locating…' : settings.locationName}</span>
          </button>
        </div>
        <div className="divider" />
        <div className="topbar-item">
          <CalendarIcon size={20} />
          <span>{today}</span>
        </div>
        <div className="divider" />
        <UserIcon size={44} className="avatar" />
      </div>
    </header>
  )
}
