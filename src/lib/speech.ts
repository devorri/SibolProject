// Voice in/out with the browser's built-in Web Speech API (no extra keys).
// Speech recognition: Chrome (Android/desktop), Edge, Safari (iOS 14.5+). Needs https:// or localhost.

interface SRAlternative { transcript: string }
interface SRResult { isFinal: boolean; length: number; [index: number]: SRAlternative }
interface SREvent { resultIndex: number; results: { length: number; [index: number]: SRResult } }
interface SR {
  lang: string
  interimResults: boolean
  continuous: boolean
  maxAlternatives: number
  onresult: ((e: SREvent) => void) | null
  onerror: ((e: { error: string }) => void) | null
  onend: (() => void) | null
  start(): void
  stop(): void
  abort(): void
}
type SRCtor = new () => SR

const w = window as unknown as { SpeechRecognition?: SRCtor; webkitSpeechRecognition?: SRCtor }
const SRClass: SRCtor | undefined = w.SpeechRecognition || w.webkitSpeechRecognition

export const speechInputSupported = Boolean(SRClass)
export const speechOutputSupported = typeof window !== 'undefined' && 'speechSynthesis' in window
export const secureContext = typeof window !== 'undefined' && window.isSecureContext

export type VoiceLang = 'en-PH' | 'fil-PH'

const ERRORS: Record<string, string> = {
  'not-allowed': 'Microphone permission was denied — allow it in the browser settings (lock icon) and try again.',
  'service-not-allowed': 'Voice input is blocked in this browser. Try Chrome.',
  'no-speech': "I didn't hear anything — tap the mic and speak again.",
  'audio-capture': 'No microphone found.',
  network: 'Voice recognition needs an internet connection.',
  aborted: '',
}

/** Listen once. Returns a stop() function. */
export function startListening(
  lang: VoiceLang,
  h: { onInterim: (text: string) => void; onFinal: (text: string) => void; onError: (msg: string) => void; onEnd: () => void },
): () => void {
  if (!SRClass) {
    h.onError('Voice input is not supported in this browser. Use Chrome on Android/PC or Safari on iPhone.')
    h.onEnd()
    return () => {}
  }
  const rec = new SRClass()
  rec.lang = lang
  rec.interimResults = true
  rec.continuous = false
  rec.maxAlternatives = 1
  let finalSent = false

  rec.onresult = (e) => {
    let text = ''
    let isFinal = false
    for (let i = 0; i < e.results.length; i++) {
      text += e.results[i][0].transcript
      if (e.results[i].isFinal) isFinal = true
    }
    text = text.trim()
    if (isFinal && text && !finalSent) {
      finalSent = true
      h.onFinal(text)
    } else {
      h.onInterim(text)
    }
  }
  rec.onerror = (e) => {
    const msg = ERRORS[e.error] ?? `Voice error: ${e.error}`
    if (msg) h.onError(msg)
  }
  rec.onend = () => h.onEnd()

  try {
    rec.start()
  } catch {
    h.onError('Could not start the microphone.')
    h.onEnd()
  }
  return () => {
    try {
      rec.abort()
    } catch {
      /* already stopped */
    }
  }
}

function pickVoice(lang: VoiceLang): SpeechSynthesisVoice | undefined {
  const voices = window.speechSynthesis.getVoices()
  const want = lang === 'fil-PH' ? ['fil', 'tl'] : ['en-PH', 'en-US', 'en-GB', 'en']
  for (const w of want) {
    const v = voices.find((v) => v.lang.toLowerCase().replace('_', '-').startsWith(w.toLowerCase()))
    if (v) return v
  }
  return undefined
}

/** Read text aloud. Resolves when finished (or stopped). */
export function speak(text: string, lang: VoiceLang): Promise<void> {
  return new Promise((resolve) => {
    if (!speechOutputSupported || !text) return resolve()
    window.speechSynthesis.cancel()
    const u = new SpeechSynthesisUtterance(text)
    u.lang = lang
    const v = pickVoice(lang)
    if (v) u.voice = v
    u.rate = 1
    u.onend = () => resolve()
    u.onerror = () => resolve()
    window.speechSynthesis.speak(u)
  })
}

export function stopSpeaking() {
  if (speechOutputSupported) window.speechSynthesis.cancel()
}
