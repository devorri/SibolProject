import { useEffect, useState } from 'react'
import './App.css'
import { Header, Sidebar } from './components/Layout'
import { useSibolData } from './hooks/useSibolData'
import { ChatIcon } from './components/icons'
import { ChatPage } from './pages/Chat'
import { Home } from './pages/Home'
import { Dashboard, DataPage, Maps, SettingsPage } from './pages/Other'
import { SettingsProvider } from './settings'
import type { Page } from './types'

const PAGES: Page[] = ['home', 'dashboard', 'chat', 'maps', 'data', 'settings']
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

  useEffect(() => {
    document.body.style.overflow = menuOpen ? 'hidden' : ''
    const onKey = (e: KeyboardEvent) => e.key === 'Escape' && setMenuOpen(false)
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [menuOpen])

  useEffect(() => {
    const mq = window.matchMedia('(min-width: 901px)')
    const onChange = () => mq.matches && setMenuOpen(false)
    mq.addEventListener('change', onChange)
    return () => mq.removeEventListener('change', onChange)
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
        {page === 'chat' && <ChatPage data={data} />}
        {page === 'maps' && <Maps data={data} />}
        {page === 'data' && <DataPage />}
        {page === 'settings' && <SettingsPage />}
      </div>
      {page !== 'chat' && (
        <button className="chat-fab" onClick={() => navigate('chat')} aria-label="Open Sibol AI chat">
          <ChatIcon size={26} />
          <span>Ask Sibol AI</span>
        </button>
      )}
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
