// "Where am I?" — browser GPS/network location + a free reverse geocoder (no API key).

export interface Position {
  lat: number
  lng: number
  accuracy: number // meters
}

export function getCurrentPosition(): Promise<Position> {
  return new Promise((resolve, reject) => {
    if (!('geolocation' in navigator)) {
      reject(new Error('This browser cannot share its location.'))
      return
    }
    if (!window.isSecureContext) {
      reject(new Error('Location only works on https:// or localhost.'))
      return
    }
    navigator.geolocation.getCurrentPosition(
      (p) => resolve({ lat: p.coords.latitude, lng: p.coords.longitude, accuracy: p.coords.accuracy }),
      (err) =>
        reject(
          new Error(
            err.code === err.PERMISSION_DENIED
              ? 'Location permission was denied — allow it in the browser (lock icon in the address bar) and try again.'
              : err.code === err.TIMEOUT
                ? 'Getting your location took too long — try again (outdoors or near a window helps).'
                : 'Your location is unavailable right now — check that GPS/location services are on.',
          ),
        ),
      { enableHighAccuracy: true, timeout: 15000, maximumAge: 0 },
    )
  })
}

/** Turns coordinates into a readable place name, e.g. "San Miguel, Taguig, Metro Manila". */
export async function reverseGeocode(lat: number, lng: number): Promise<string> {
  const url = `https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=${lat}&longitude=${lng}&localityLanguage=en`
  const res = await fetch(url)
  if (!res.ok) throw new Error(`${res.status}`)
  const d = (await res.json()) as { locality?: string; city?: string; principalSubdivision?: string; countryName?: string }
  const parts = [d.locality, d.city, d.principalSubdivision].map((p) => p?.trim()).filter((p): p is string => Boolean(p))
  const unique = parts.filter((p, i) => parts.findIndex((q) => q.toLowerCase() === p.toLowerCase()) === i)
  if (!unique.length) throw new Error('No place name found')
  return unique.join(', ')
}
