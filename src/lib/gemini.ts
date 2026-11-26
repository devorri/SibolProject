// Gemini API (Google AI Studio) — called straight from the browser with fetch.
// Docs: https://ai.google.dev/api
export const GEMINI_KEY = (import.meta.env.VITE_GEMINI_API_KEY as string | undefined) ?? ''
export const GEMINI_MODEL = (import.meta.env.VITE_GEMINI_MODEL as string | undefined) || 'gemini-3.5-flash'
export const geminiConfigured = Boolean(GEMINI_KEY)

/** Tried in order when a model is overloaded, rate-limited, slow or unavailable. */
const FALLBACK_MODELS = ['gemini-3.5-flash', 'gemini-3.1-flash-lite', 'gemini-3-flash-preview']
const MODELS = [GEMINI_MODEL, ...FALLBACK_MODELS.filter((m) => m !== GEMINI_MODEL)]
const TIMEOUT_MS = 25000

/** Remembers which model answered last, so the next call starts with it. */
let preferred = 0
export const lastModelUsed = () => MODELS[preferred]

type Part = { text: string } | { inline_data: { mime_type: string; data: string } }
type Content = { role: 'user' | 'model'; parts: Part[] }

interface Request {
  contents: Content[]
  systemInstruction?: { parts: { text: string }[] }
  generationConfig?: Record<string, unknown>
}

class RetryableError extends Error {}

async function callModel(model: string, req: Request): Promise<string> {
  const ctrl = new AbortController()
  const timer = setTimeout(() => ctrl.abort(), TIMEOUT_MS)
  try {
    const res = await fetch(`https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent`, {
      method: 'POST',
      signal: ctrl.signal,
      headers: { 'x-goog-api-key': GEMINI_KEY, 'Content-Type': 'application/json' },
      body: JSON.stringify(req),
    })
    if (!res.ok) {
      const body = await res.json().catch(() => ({}))
      const msg: string = body?.error?.message || `Gemini error ${res.status}`
      // busy / rate limited / server error / model not available -> try the next model
      if ([404, 429, 500, 502, 503, 504].includes(res.status)) throw new RetryableError(`${model}: ${msg}`)
      throw new Error(msg)
    }
    const data = await res.json()
    const text: string = (data?.candidates?.[0]?.content?.parts ?? []).map((p: { text?: string }) => p.text ?? '').join('')
    if (!text) throw new RetryableError(`${model}: empty answer`)
    return text
  } catch (e) {
    if ((e as Error).name === 'AbortError') throw new RetryableError(`${model}: timed out`)
    throw e
  } finally {
    clearTimeout(timer)
  }
}

/** Runs a request on the first model that answers. `parse` may throw SyntaxError to try the next model. */
async function run<T>(req: Request, parse: (text: string) => T): Promise<T> {
  if (!geminiConfigured) throw new Error('Gemini is not configured — add VITE_GEMINI_API_KEY to .env')
  const order = [...MODELS.slice(preferred), ...MODELS.slice(0, preferred)]
  let lastError: Error | null = null
  for (const model of order) {
    try {
      const out = parse(await callModel(model, req))
      preferred = MODELS.indexOf(model)
      return out
    } catch (e) {
      lastError = e as Error
      if (!(e instanceof RetryableError) && !(e instanceof SyntaxError)) throw e
      console.warn('[gemini] falling back:', lastError.message)
    }
  }
  throw new Error('All Gemini models are busy right now — try again in a minute. ' + (lastError?.message ?? ''))
}

function generateJson<T>(parts: Part[], schema: object): Promise<T> {
  return run(
    {
      contents: [{ role: 'user', parts }],
      generationConfig: { temperature: 0.4, responseMimeType: 'application/json', responseSchema: schema },
    },
    (text) => JSON.parse(text) as T,
  )
}

/** Everything the AI needs to know about the field right now. */
export interface FieldContext {
  crop: string
  location: string
  stage: string | null
  sensitivity: string
  daysAfterPlanting: number
  soilMoisture: number | null
  optimalMin: number
  optimalMax: number
  sensorTemp: number | null
  sensorHumidity: number | null
  forecast: { date: string; tMax: number; tMin: number; rainProb: number | null }[]
  riskLevel: string
  pumpOn: boolean | null
  pumpMode: string | null
}

const describe = (c: FieldContext) =>
  [
    `Crop: ${c.crop} in ${c.location}, Philippines.`,
    `Growth stage: ${c.stage ?? 'unknown'} (day ${c.daysAfterPlanting} after planting, sensitivity ${c.sensitivity}).`,
    `Soil moisture (sensor): ${c.soilMoisture ?? 'no data'}% — optimal range ${c.optimalMin}-${c.optimalMax}%.`,
    `Air temperature right now (DHT11 sensor at the field): ${c.sensorTemp ?? 'no data'}°C, humidity ${c.sensorHumidity ?? 'no data'}%.`,
    `Weather forecast (Open-Meteo, next days): ${c.forecast
      .map((f) => `${f.date}: ${f.tMin}-${f.tMax}°C, rain ${f.rainProb ?? '?'}%`)
      .join('; ')}.`,
    `Computed crop risk level: ${c.riskLevel}.`,
    `Water pump: ${c.pumpOn === null ? 'unknown' : c.pumpOn ? 'ON' : 'OFF'} (mode: ${c.pumpMode ?? 'unknown'}). The pump waters automatically in auto mode.`,
  ].join('\n')

/** 3–5 short, practical actions for the farmer. */
export async function aiRecommendations(c: FieldContext): Promise<string[]> {
  const prompt =
    `You are an agronomist helping a smallholder farmer in the Philippines.\n` +
    `Field data:\n${describe(c)}\n\n` +
    `Give 3 to 5 short, practical recommended actions for the next 1-3 days. ` +
    `Each item must be one plain sentence (max 25 words), specific to this data, no markdown. ` +
    `If sensor data is missing, say what to check. Do not repeat the numbers back unless needed.`
  const out = await generateJson<{ actions: string[] }>([{ text: prompt }], {
    type: 'OBJECT',
    properties: { actions: { type: 'ARRAY', items: { type: 'STRING' } } },
    required: ['actions'],
  })
  return out.actions.filter(Boolean).slice(0, 5)
}

export interface PhotoCheck {
  status: 'Healthy' | 'Watch' | 'Problem' | 'Unclear'
  summary: string
  observations: string[]
}

/** Look at the latest camera frame and judge the plant's condition. */
export async function aiCheckPhoto(image: Blob, c: FieldContext): Promise<PhotoCheck> {
  const data = await blobToBase64(image)
  const prompt =
    `This photo is from a fixed ESP32-CAM monitoring a ${c.crop} plant/field.\n` +
    `Field data for context:\n${describe(c)}\n\n` +
    `Assess the visible plant health (leaf color, wilting, spots, pests, dryness, waterlogging). ` +
    `If the image is too dark, blurry or shows no plant, use status "Unclear" and say how to fix the camera view. ` +
    `Summary: 1-2 plain sentences. Observations: 2-4 short items. No markdown.`
  return generateJson<PhotoCheck>([{ inline_data: { mime_type: image.type || 'image/jpeg', data } }, { text: prompt }], {
    type: 'OBJECT',
    properties: {
      status: { type: 'STRING', enum: ['Healthy', 'Watch', 'Problem', 'Unclear'] },
      summary: { type: 'STRING' },
      observations: { type: 'ARRAY', items: { type: 'STRING' } },
    },
    required: ['status', 'summary', 'observations'],
  })
}

/* ===================== Sibol AI chat ===================== */

export interface ChatTurn {
  role: 'user' | 'model'
  text: string
  image?: Blob // only sent for the newest turn that has one
}

const SYSTEM_ABOUT =
  'You are Sibol AI, the assistant of SIBOL, a crop monitoring system for Filipino farmers ' +
  '(slogan: "See the signs. Save the harvest.").\n' +
  'How SIBOL works: an ESP32-CAM unit at the field reads soil moisture (probe through an ADS1115), air temperature ' +
  'and humidity (DHT11), takes photos of the crop, and controls a water pump relay. In AUTO mode the pump waters ' +
  'when soil is below 35% and stops above 50%; it never runs more than 2 minutes at a time, then rests 1 minute. ' +
  'Users can force the pump ON or OFF from the website (Home camera card or Dashboard) or the 4.3 inch touchscreen. ' +
  'Data is stored in Supabase. The weather forecast comes from Open-Meteo. Crop risk combines soil moisture, the ' +
  'hottest forecast day and how sensitive the growth stage is. "Forecast Heat" is the forecast, not the sensor ' +
  'temperature.'

function systemText(c: FieldContext, voice: boolean) {
  return (
    `${SYSTEM_ABOUT}\n\nLIVE DATA RIGHT NOW:\n${describe(c)}\n\nRULES: Answer in the same language the user uses ` +
    `(English, Tagalog or Taglish). Plain text only: no markdown, no asterisks, no bullet symbols, no emojis. ` +
    (voice
      ? 'This is a VOICE conversation: answer in 1-3 short spoken sentences, no lists, no numbers with many decimals. '
      : 'Keep answers short (under 120 words) and practical. ') +
    'You cannot press buttons or control the pump yourself; tell the user which button to press. ' +
    'For questions outside farming or SIBOL, answer normally like a helpful general assistant.'
  )
}

const clean = (t: string) =>
  t
    .replace(/\*\*/g, '')
    .replace(/^#+\s*/gm, '')
    .replace(/^\s*[*•]\s+/gm, '- ')
    .trim()

/** Multi-turn chat that knows the live field data. Set `voice` for short spoken answers. */
export async function aiChat(history: ChatTurn[], c: FieldContext, voice = false): Promise<string> {
  const lastImageIdx = history.map((t) => Boolean(t.image)).lastIndexOf(true)
  const contents: Content[] = []
  for (let i = 0; i < history.length; i++) {
    const t = history[i]
    const parts: Part[] = []
    if (i === lastImageIdx && t.image) {
      parts.push({ inline_data: { mime_type: t.image.type || 'image/jpeg', data: await blobToBase64(t.image) } })
    }
    parts.push({ text: t.text || '(photo)' })
    contents.push({ role: t.role, parts })
  }
  return run(
    {
      systemInstruction: { parts: [{ text: systemText(c, voice) }] },
      contents,
      generationConfig: { temperature: 0.6 },
    },
    clean,
  )
}

function blobToBase64(blob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    const r = new FileReader()
    r.onload = () => resolve(String(r.result).split(',')[1] ?? '')
    r.onerror = () => reject(r.error)
    r.readAsDataURL(blob)
  })
}
