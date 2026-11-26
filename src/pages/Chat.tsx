import { useEffect, useRef, useState } from 'react'
import { CameraIcon, MicIcon, SendIcon, SpeakerIcon, StopIcon } from '../components/icons'
import { useCropModel } from '../hooks/useCropModel'
import { useDevice } from '../hooks/useDevice'
import type { SibolData } from '../hooks/useSibolData'
import { api, FRAME_URL } from '../lib/api'
import { toFieldContext } from '../lib/context'
import { aiChat, geminiConfigured, type ChatTurn } from '../lib/gemini'
import {
  secureContext,
  speak,
  speechInputSupported,
  speechOutputSupported,
  startListening,
  stopSpeaking,
  type VoiceLang,
} from '../lib/speech'

interface Msg {
  id: number
  role: 'user' | 'model'
  text: string
  imageUrl?: string // preview only (not saved)
  image?: Blob
  error?: boolean
}

type Mode = 'idle' | 'listening' | 'thinking' | 'speaking'

const STORE = 'sibol-chat'
const LANG_STORE = 'sibol-chat-lang'
const SPEAK_STORE = 'sibol-chat-speak'
const MAX_HISTORY = 16

const WELCOME: Msg = {
  id: 0,
  role: 'model',
  text: "Hi! I'm Sibol AI. Ask me about your crop, the sensors and the water pump — or anything else. You can type or tap the mic to talk.",
}

const CHIPS = ['How is my crop today?', 'Should I water now?', 'Check my plant photo', 'Ano ang forecast bukas?']

const cacheBust = () => Date.now()

function loadMsgs(): Msg[] {
  try {
    const saved = JSON.parse(localStorage.getItem(STORE) || '[]') as Msg[]
    return saved.length ? saved : [WELCOME]
  } catch {
    return [WELCOME]
  }
}

export function ChatPage({ data }: { data: SibolData }) {
  const m = useCropModel(data)
  const device = useDevice()
  const ctx = toFieldContext(m, data, device.controls)
  const ctxRef = useRef(ctx)
  useEffect(() => {
    ctxRef.current = ctx
  })

  const [msgs, setMsgs] = useState<Msg[]>(loadMsgs)
  const msgsRef = useRef(msgs)
  const [input, setInput] = useState('')
  const [mode, setMode] = useState<Mode>('idle')
  const [note, setNote] = useState('')
  const [conversation, setConversation] = useState(false) // hands-free voice chat
  const convRef = useRef(false)
  const [lang, setLang] = useState<VoiceLang>(() => (localStorage.getItem(LANG_STORE) as VoiceLang) || 'en-PH')
  const [speakReplies, setSpeakReplies] = useState(() => localStorage.getItem(SPEAK_STORE) === '1')
  const stopListenRef = useRef<() => void>(() => {})
  const listRef = useRef<HTMLDivElement>(null)
  const scrolledOnce = useRef(false)

  // keep refs + storage in sync
  useEffect(() => {
    msgsRef.current = msgs
    const toSave = msgs.filter((x) => !x.error).slice(-MAX_HISTORY * 2).map(({ id, role, text }) => ({ id, role, text }))
    localStorage.setItem(STORE, JSON.stringify(toSave))
    const el = listRef.current
    if (el) {
      const first = !scrolledOnce.current
      scrolledOnce.current = true
      setTimeout(() => el.scrollTo({ top: el.scrollHeight, behavior: first ? 'auto' : 'smooth' }), 60)
    }
  }, [msgs])
  useEffect(() => {
    const el = listRef.current
    if (el && mode === 'thinking') setTimeout(() => el.scrollTo({ top: el.scrollHeight, behavior: 'smooth' }), 60)
  }, [mode])
  useEffect(() => localStorage.setItem(LANG_STORE, lang), [lang])
  useEffect(() => localStorage.setItem(SPEAK_STORE, speakReplies ? '1' : '0'), [speakReplies])

  // stop mic + speech when leaving the page
  useEffect(
    () => () => {
      convRef.current = false
      stopListenRef.current()
      stopSpeaking()
    },
    [],
  )

  const push = (msg: Omit<Msg, 'id'>) => {
    const full = { ...msg, id: (msgsRef.current.at(-1)?.id ?? 0) + 1 }
    msgsRef.current = [...msgsRef.current, full]
    setMsgs(msgsRef.current)
  }

  async function send(text: string, opts: { voice?: boolean; image?: Blob } = {}) {
    const t = text.trim()
    if ((!t && !opts.image) || mode === 'thinking') return
    setNote('')
    setInput('')
    push({ role: 'user', text: t, image: opts.image, imageUrl: opts.image ? URL.createObjectURL(opts.image) : undefined })
    setMode('thinking')

    const history: ChatTurn[] = msgsRef.current
      .filter((x) => !x.error && x.id !== 0)
      .slice(-MAX_HISTORY)
      .map((x) => ({ role: x.role, text: x.text, image: x.image }))

    let reply: string
    try {
      reply = await aiChat(history, ctxRef.current, Boolean(opts.voice))
      push({ role: 'model', text: reply })
    } catch (e) {
      push({ role: 'model', text: `Sorry, I couldn't answer: ${(e as Error).message}`, error: true })
      convRef.current = false
      setConversation(false)
      setMode('idle')
      return
    }

    if ((opts.voice || speakReplies) && speechOutputSupported) {
      setMode('speaking')
      await speak(reply, lang)
    }
    setMode('idle')
    if (convRef.current) listen(true)
  }

  function listen(voice = true) {
    stopSpeaking()
    setNote('')
    setMode('listening')
    let gotFinal = false
    stopListenRef.current = startListening(lang, {
      onInterim: (t) => setInput(t),
      onFinal: (t) => {
        gotFinal = true
        setInput(t)
        send(t, { voice })
      },
      onError: (msg) => {
        setNote(msg)
        convRef.current = false
        setConversation(false)
      },
      onEnd: () => {
        if (!gotFinal) setMode((cur) => (cur === 'listening' ? 'idle' : cur))
      },
    })
  }

  function stopAll() {
    convRef.current = false
    setConversation(false)
    stopListenRef.current()
    stopSpeaking()
    setMode((cur) => (cur === 'thinking' ? cur : 'idle'))
  }

  function toggleConversation() {
    if (conversation) return stopAll()
    convRef.current = true
    setConversation(true)
    listen(true)
  }

  async function checkPhoto() {
    if (mode === 'thinking') return
    setNote('Getting the latest camera photo…')
    try {
      api.keepStreamAlive(30).catch(() => {})
      const res = await fetch(`${FRAME_URL}?t=${cacheBust()}`, { cache: 'no-store' })
      if (!res.ok) throw new Error(String(res.status))
      const blob = await res.blob()
      setNote('')
      await send(lang === 'fil-PH' ? 'Tingnan mo ang litrato ng tanim ko. Ayos ba siya?' : 'Check my plant photo. How does it look?', {
        image: blob,
      })
    } catch {
      setNote('No camera photo yet — open the Home page so the ESP32-CAM starts sending pictures.')
    }
  }

  function clearChat() {
    stopAll()
    msgsRef.current = [WELCOME]
    setMsgs([WELCOME])
  }

  const voiceAvailable = speechInputSupported && secureContext
  const status =
    mode === 'listening' ? 'Listening… speak now' : mode === 'thinking' ? 'Sibol AI is thinking…' : mode === 'speaking' ? 'Speaking…' : ''

  return (
    <div className="page chat-page">
      <div className="chat-head">
        <div>
          <h1 className="welcome">Sibol AI</h1>
          <p className="lead">Chat or talk about your crop, sensors and pump — or ask anything.</p>
        </div>
        <div className="chat-tools">
          <select value={lang} onChange={(e) => setLang(e.target.value as VoiceLang)} aria-label="Voice language">
            <option value="en-PH">English</option>
            <option value="fil-PH">Filipino</option>
          </select>
          {speechOutputSupported && (
            <button
              className={`icon-btn ${speakReplies ? 'on' : ''}`}
              onClick={() => {
                if (speakReplies) stopSpeaking()
                setSpeakReplies(!speakReplies)
              }}
              title={speakReplies ? 'Reading replies aloud' : 'Read replies aloud'}
              aria-label="Read replies aloud"
            >
              <SpeakerIcon size={18} />
            </button>
          )}
          <button className="btn ghost small" onClick={clearChat}>
            New chat
          </button>
        </div>
      </div>

      {!geminiConfigured && <div className="alert">Gemini is not configured — add VITE_GEMINI_API_KEY to .env and restart.</div>}

      <div className="chat-list" ref={listRef}>
        {msgs.map((x) => (
          <div key={x.id} className={`bubble-row ${x.role}`}>
            <div className={`bubble ${x.role} ${x.error ? 'error' : ''}`}>
              {x.imageUrl && <img src={x.imageUrl} alt="Camera photo sent to Sibol AI" />}
              {x.text}
              {x.role === 'model' && x.id !== 0 && !x.error && speechOutputSupported && (
                <button className="bubble-speak" onClick={() => speak(x.text, lang)} aria-label="Read aloud">
                  <SpeakerIcon size={14} />
                </button>
              )}
            </div>
          </div>
        ))}
        {mode === 'thinking' && (
          <div className="bubble-row model">
            <div className="bubble model typing">
              <i />
              <i />
              <i />
            </div>
          </div>
        )}
      </div>

      {(status || note) && (
        <div className={`chat-status ${mode}`}>
          {status && (
            <span>
              {mode === 'listening' && <span className="rec-dot" />}
              {status}
            </span>
          )}
          {note && <span className="chat-note">{note}</span>}
          {(mode === 'listening' || mode === 'speaking' || conversation) && (
            <button className="btn small danger" onClick={stopAll}>
              <StopIcon size={14} /> Stop
            </button>
          )}
        </div>
      )}

      <div className="chips">
        {CHIPS.map((c) => (
          <button key={c} className="chip" disabled={mode === 'thinking'} onClick={() => (c.includes('photo') ? checkPhoto() : send(c))}>
            {c.includes('photo') && <CameraIcon size={14} />} {c}
          </button>
        ))}
      </div>

      <form
        className="chat-input"
        onSubmit={(e) => {
          e.preventDefault()
          send(input)
        }}
      >
        <input
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder={mode === 'listening' ? 'Listening…' : 'Type a message (English or Tagalog)…'}
          enterKeyHint="send"
          maxLength={600}
        />
        {voiceAvailable && (
          <button
            type="button"
            className={`icon-btn mic ${mode === 'listening' ? 'on' : ''}`}
            onClick={() => (mode === 'listening' ? stopAll() : listen(true))}
            disabled={mode === 'thinking'}
            aria-label="Speak a message"
            title="Tap and speak"
          >
            <MicIcon size={20} />
          </button>
        )}
        <button type="submit" className="icon-btn send" disabled={!input.trim() || mode === 'thinking'} aria-label="Send">
          <SendIcon size={20} />
        </button>
      </form>

      {voiceAvailable ? (
        <button className={`voice-chat-btn ${conversation ? 'on' : ''}`} onClick={toggleConversation} disabled={mode === 'thinking' && !conversation}>
          <MicIcon size={20} /> {conversation ? 'End voice chat' : 'Start voice chat (hands-free)'}
        </button>
      ) : (
        <p className="muted small center">
          {!secureContext
            ? 'Voice chat needs a secure link (https:// or localhost). Open the deployed site on your phone to talk.'
            : 'Voice input is not supported in this browser — use Chrome on Android/PC or Safari on iPhone.'}
        </p>
      )}
    </div>
  )
}
