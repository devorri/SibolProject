import type { SVGProps } from 'react'

type P = SVGProps<SVGSVGElement> & { size?: number }
const base = (size = 20) => ({
  width: size,
  height: size,
  viewBox: '0 0 24 24',
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 2,
  strokeLinecap: 'round' as const,
  strokeLinejoin: 'round' as const,
})

export const HomeIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M12 3 2 11.5h3V21h5.5v-6h3v6H19v-9.5h3z" />
  </svg>
)
export const ChartIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <rect x="3" y="11" width="4" height="10" rx="1" />
    <rect x="10" y="6" width="4" height="15" rx="1" />
    <rect x="17" y="3" width="4" height="18" rx="1" />
  </svg>
)
export const MapIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M3 6.5 9 4l6 2.5L21 4v13.5L15 20l-6-2.5L3 20z" />
    <path d="M9 4v13.5M15 6.5V20" />
  </svg>
)
export const DataIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <ellipse cx="12" cy="5" rx="8" ry="3" fill="currentColor" />
    <path d="M4 5v6c0 1.7 3.6 3 8 3s8-1.3 8-3V5" />
    <path d="M4 11v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6" />
  </svg>
)
export const GearIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <circle cx="12" cy="12" r="3" />
    <path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z" />
  </svg>
)
export const PinIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M12 2a7 7 0 0 0-7 7c0 5.2 7 13 7 13s7-7.8 7-13a7 7 0 0 0-7-7zm0 9.5A2.5 2.5 0 1 1 12 6.5a2.5 2.5 0 0 1 0 5z" />
  </svg>
)
export const CalendarIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <rect x="3" y="5" width="18" height="16" rx="2" fill="currentColor" />
    <path d="M8 3v4M16 3v4" />
    <path d="M6 11h2M11 11h2M16 11h2M6 15h2M11 15h2M16 15h2" stroke="#fff" />
  </svg>
)
export const UserIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <circle cx="12" cy="12" r="10" />
    <circle cx="12" cy="10" r="3.5" />
    <path d="M5.5 19c1.5-3 4-4 6.5-4s5 1 6.5 4" />
  </svg>
)
export const DropIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M12 2.5S5 10.3 5 15a7 7 0 0 0 14 0c0-4.7-7-12.5-7-12.5z" />
  </svg>
)
export const ThermoIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M14 14.8V4a2 2 0 0 0-4 0v10.8a4 4 0 1 0 4 0z" />
    <circle cx="12" cy="17.5" r="1.5" fill="currentColor" />
  </svg>
)
export const SproutIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M11 21v-7.5C6.5 13.5 4 10.5 4 6c4.5 0 7 2.5 7.5 6 .5-4.5 3.5-8 8.5-8 0 5.5-3 8.8-7.5 9.3V21z" />
  </svg>
)
export const WarnIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M12 2 1 21h22zm-1 7h2v6h-2zm0 8h2v2h-2z" />
  </svg>
)
export const LeafIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <path d="M20 3C9 3 4 8.5 4 15.5c0 1.6.3 3 .8 4.2C6.5 13 11 9.5 16 8c-4.6 2.4-8 6.4-9.6 12 1.4.7 3 1 4.6 1C18 21 20.6 13 20 3z" />
  </svg>
)
export const SunIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <circle cx="12" cy="12" r="4.5" fill="currentColor" />
    <path d="M12 1.5v2.5M12 20v2.5M1.5 12H4M20 12h2.5M4.6 4.6l1.8 1.8M17.6 17.6l1.8 1.8M4.6 19.4l1.8-1.8M17.6 6.4l1.8-1.8" />
  </svg>
)
export const ToolsIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.6 2.6-2.4-.6-.6-2.4z" />
  </svg>
)
export const RefreshIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M21 12a9 9 0 1 1-2.6-6.4M21 3v6h-6" />
  </svg>
)

export const ChatIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M4 5h16a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H9l-5 4v-4H4a1 1 0 0 1-1-1V6a1 1 0 0 1 1-1z" />
    <path d="M8 10h8M8 13h5" />
  </svg>
)
export const MicIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <rect x="9" y="3" width="6" height="11" rx="3" />
    <path d="M5 11a7 7 0 0 0 14 0M12 18v3" />
  </svg>
)
export const SendIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M4 12 20 4l-6 16-3-7z" />
  </svg>
)
export const StopIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p} fill="currentColor" stroke="none">
    <rect x="6" y="6" width="12" height="12" rx="2" />
  </svg>
)
export const SpeakerIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M4 9h4l5-4v14l-5-4H4z" />
    <path d="M17 9a4 4 0 0 1 0 6M19.5 6.5a8 8 0 0 1 0 11" />
  </svg>
)
export const CameraIcon = ({ size, ...p }: P) => (
  <svg {...base(size)} {...p}>
    <path d="M4 8h3l2-3h6l2 3h3a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z" />
    <circle cx="12" cy="13.5" r="3.5" />
  </svg>
)

/** Weather icon from Open-Meteo WMO code */
export function WeatherIcon({ code, size = 28 }: { code: number; size?: number }) {
  const sun = <circle cx="10" cy="10" r="5" fill="#f5b81b" />
  const cloud = (x = 0, fill = '#9fc3e6') => (
    <path
      d={`M${7 + x} 22h12a4.5 4.5 0 0 0 .5-9 6 6 0 0 0-11.4 1.6A3.8 3.8 0 0 0 ${7 + x} 22z`}
      fill={fill}
      stroke="#6f9cc8"
      strokeWidth="1"
    />
  )
  let content
  if (code === 0) content = <circle cx="16" cy="16" r="7" fill="#f5b81b" />
  else if (code <= 2) content = <>{sun}{cloud(2)}</>
  else if (code <= 48) content = cloud(1, '#c9d6e3')
  else if (code <= 67 || (code >= 80 && code <= 82))
    content = (
      <>
        {cloud(1)}
        <path d="M11 25l-1.5 4M16 25l-1.5 4M21 25l-1.5 4" stroke="#3b82c4" strokeWidth="2" strokeLinecap="round" />
      </>
    )
  else if (code >= 95)
    content = (
      <>
        {cloud(1, '#8aa2bb')}
        <path d="M16 23l-3 5h3l-2 4" stroke="#f5b81b" strokeWidth="2" fill="none" strokeLinecap="round" />
      </>
    )
  else content = <>{sun}{cloud(2)}</>
  return (
    <svg width={size} height={size} viewBox="0 0 32 32">
      {content}
    </svg>
  )
}

export function weatherLabel(code: number) {
  if (code === 0) return 'Clear'
  if (code <= 2) return 'Partly cloudy'
  if (code === 3) return 'Overcast'
  if (code <= 48) return 'Fog'
  if (code <= 57) return 'Drizzle'
  if (code <= 67) return 'Rain'
  if (code <= 77) return 'Snow'
  if (code <= 82) return 'Showers'
  return 'Thunderstorm'
}
