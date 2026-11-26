import { useCallback, useEffect, useRef, useState } from 'react'
import { aiRecommendations, geminiConfigured, type FieldContext } from '../lib/gemini'

const CACHE_KEY = 'sibol-ai-advice'
const CACHE_MS = 30 * 60000 // reuse advice for 30 min so page reloads don't burn the Gemini quota

interface Cached {
  at: number
  actions: string[]
}

function readCache(): Cached | null {
  try {
    const c = JSON.parse(localStorage.getItem(CACHE_KEY) || 'null') as Cached | null
    return c && Date.now() - c.at < CACHE_MS ? c : null
  } catch {
    return null
  }
}

/** Gemini-written recommended actions. Generates once when the data is ready, then on demand. */
export function useAiAdvice(context: FieldContext, ready: boolean) {
  const [cached] = useState(readCache)
  const [actions, setActions] = useState<string[] | null>(cached?.actions ?? null)
  const [at, setAt] = useState<number | null>(cached?.at ?? null)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const ctxRef = useRef(context)
  useEffect(() => {
    ctxRef.current = context
  })
  const started = useRef(Boolean(cached))

  const generate = useCallback(async () => {
    if (!geminiConfigured) return
    setLoading(true)
    setError('')
    try {
      const out = await aiRecommendations(ctxRef.current)
      const now = Date.now()
      setActions(out)
      setAt(now)
      localStorage.setItem(CACHE_KEY, JSON.stringify({ at: now, actions: out }))
    } catch (e) {
      setError((e as Error).message)
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => {
    if (!ready || started.current || !geminiConfigured) return
    started.current = true
    generate()
  }, [ready, generate])

  return { enabled: geminiConfigured, actions, at, loading, error, generate }
}

export type AiAdvice = ReturnType<typeof useAiAdvice>
