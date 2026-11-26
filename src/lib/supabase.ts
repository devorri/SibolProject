// Tiny Supabase REST client (PostgREST) — no extra npm packages needed.
export const SUPABASE_URL = (import.meta.env.VITE_SUPABASE_URL as string | undefined)?.replace(/\/+$/, '') ?? ''
export const SUPABASE_KEY = (import.meta.env.VITE_SUPABASE_ANON_KEY as string | undefined) ?? ''
export const supabaseConfigured = Boolean(SUPABASE_URL && SUPABASE_KEY)

const NOT_CONFIGURED = 'Supabase is not configured — add VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY to .env'

function headers(extra: Record<string, string> = {}): Record<string, string> {
  const h: Record<string, string> = { apikey: SUPABASE_KEY, ...extra }
  // Legacy JWT anon keys also go in Authorization; new sb_publishable_ keys must not.
  if (SUPABASE_KEY.startsWith('eyJ')) h.Authorization = `Bearer ${SUPABASE_KEY}`
  return h
}

async function fail(res: Response): Promise<never> {
  const body = await res.json().catch(() => ({}))
  throw new Error(body.message || `${res.status} ${res.statusText}`)
}

export async function sbSelect<T>(path: string, opts: { count?: boolean } = {}): Promise<{ data: T; total: number | null }> {
  if (!supabaseConfigured) throw new Error(NOT_CONFIGURED)
  const res = await fetch(`${SUPABASE_URL}/rest/v1/${path}`, {
    headers: headers(opts.count ? { Prefer: 'count=exact' } : {}),
  })
  if (!res.ok) await fail(res)
  const range = res.headers.get('content-range') // e.g. "0-499/1234"
  const total = range && range.includes('/') ? Number(range.split('/')[1]) : null
  return { data: (await res.json()) as T, total: Number.isFinite(total) ? total : null }
}

export async function sbUpdate(path: string, body: Record<string, unknown>): Promise<void> {
  if (!supabaseConfigured) throw new Error(NOT_CONFIGURED)
  const res = await fetch(`${SUPABASE_URL}/rest/v1/${path}`, {
    method: 'PATCH',
    headers: headers({ 'Content-Type': 'application/json', Prefer: 'return=minimal' }),
    body: JSON.stringify(body),
  })
  if (!res.ok) await fail(res)
}
