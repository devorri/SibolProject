-- Sibol — run this once in Supabase: Dashboard > SQL Editor > New query > Run

-- 1) Readings table (the ESP32 inserts here)
create table if not exists public.sensor_readings (
  id            bigint generated always as identity primary key,
  created_at    timestamptz not null default now(),
  device_id     text not null default 'esp32',
  soil_moisture real check (soil_moisture between 0 and 100),
  soil_raw      integer,
  temperature   real,
  humidity      real
);

create index if not exists sensor_readings_created_at_idx
  on public.sensor_readings (created_at desc);

-- 2) Row Level Security: the anon/publishable key may read and insert, nothing else
alter table public.sensor_readings enable row level security;

drop policy if exists "sibol read" on public.sensor_readings;
create policy "sibol read" on public.sensor_readings
  for select to anon, authenticated using (true);

drop policy if exists "sibol insert" on public.sensor_readings;
create policy "sibol insert" on public.sensor_readings
  for insert to anon, authenticated with check (true);

-- 3) Daily averages for the trend chart (Philippine time)
create or replace view public.sensor_daily
with (security_invoker = true) as
select
  (created_at at time zone 'Asia/Manila')::date as date,
  round(avg(soil_moisture)::numeric, 1)           as soil_moisture,
  round(avg(temperature)::numeric, 1)             as temperature,
  round(avg(humidity)::numeric, 1)                as humidity,
  count(*)::int                                   as count
from public.sensor_readings
group by 1;

grant select on public.sensor_daily to anon, authenticated;
