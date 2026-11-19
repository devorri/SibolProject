// Tiny Supabase REST client (PostgREST) — no extra npm packages needed.
export const SUPABASE_URL = (import.meta.env.VITE_SUPABASE_URL as string | undefined)?.replace(/\/+$/, '') ?? ''
export const SUPABASE_KEY = (import.meta.env.VITE_SUPABASE_ANON_KEY as string | undefined) ?? ''
export const supabaseConfigured = Boolean(SUPABASE_URL && SUPABASE_KEY)

export async function sbSelect<T>(path: string, opts: { count?: boolean } = {}): Promise<{ data: T; total: number | null }> {
  if (!supabaseConfigured) throw new Error('Supabase is not configured — add VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY to .env')
  const res = await fetch(`${SUPABASE_URL}/rest/v1/${path}`, {
    headers: {
      apikey: SUPABASE_KEY,
      ...(opts.count ? { Prefer: 'count=exact' } : {}),
    },
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error(body.message || `${res.status} ${res.statusText}`)
  }
  const range = res.headers.get('content-range') // e.g. "0-499/1234"
  const total = range && range.includes('/') ? Number(range.split('/')[1]) : null
  return { data: (await res.json()) as T, total: Number.isFinite(total) ? total : null }
}
