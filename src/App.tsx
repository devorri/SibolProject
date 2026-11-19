import { useEffect, useState } from 'react'
import './App.css'
import { Header, Sidebar } from './components/Layout'
import { useSibolData } from './hooks/useSibolData'
import { Home } from './pages/Home'
import { Dashboard, DataPage, Maps, SettingsPage } from './pages/Other'
import { SettingsProvider } from './settings'
import type { Page } from './types'

const PAGES: Page[] = ['home', 'dashboard', 'maps', 'data', 'settings']
const fromHash = (): Page => {
  const h = window.location.hash.replace('#/', '') as Page
  return PAGES.includes(h) ? h : 'home'
}

function Shell() {
  const [page, setPage] = useState<Page>(fromHash)
  const [menuOpen, setMenuOpen] = useState(false)
  const data = useSibolData(14)

  useEffect(() => {
    const onHash = () => setPage(fromHash())
    window.addEventListener('hashchange', onHash)
    return () => window.removeEventListener('hashchange', onHash)
  }, [])

  const navigate = (p: Page) => {
    window.location.hash = `/${p}`
    setPage(p)
    setMenuOpen(false)
  }

  return (
    <div className="app">
      <Sidebar page={page} onNavigate={navigate} open={menuOpen} />
      {menuOpen && <div className="backdrop" onClick={() => setMenuOpen(false)} />}
      <div className="main">
        <Header online={data.online} dbOk={data.dbOk} onMenu={() => setMenuOpen(!menuOpen)} />
        {page === 'home' && <Home data={data} />}
        {page === 'dashboard' && <Dashboard data={data} />}
        {page === 'maps' && <Maps data={data} />}
        {page === 'data' && <DataPage />}
        {page === 'settings' && <SettingsPage />}
      </div>
    </div>
  )
}

export default function App() {
  return (
    <SettingsProvider>
      <Shell />
    </SettingsProvider>
  )
}
