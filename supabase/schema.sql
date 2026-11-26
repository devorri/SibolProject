-- Sibol — run this in Supabase: Dashboard > SQL Editor > New query > Run
-- Safe to run more than once.

-- =====================================================
-- 1) Sensor readings (the ESP32-CAM inserts here every 15 s)
-- =====================================================
create table if not exists public.sensor_readings (
  id            bigint generated always as identity primary key,
  created_at    timestamptz not null default now(),
  device_id     text not null default 'esp32',
  soil_moisture real check (soil_moisture between 0 and 100),
  soil_raw      integer,
  temperature   real,
  humidity      real
);

alter table public.sensor_readings
  add column if not exists soil_voltage real,
  add column if not exists pump_on boolean not null default false;

create index if not exists sensor_readings_created_at_idx
  on public.sensor_readings (created_at desc);
create index if not exists sensor_readings_device_time_idx
  on public.sensor_readings (device_id, created_at desc);

alter table public.sensor_readings enable row level security;

drop policy if exists "sibol read" on public.sensor_readings;
drop policy if exists "sibol insert" on public.sensor_readings;
drop policy if exists "anon read readings" on public.sensor_readings;
drop policy if exists "anon insert readings" on public.sensor_readings;

create policy "sibol read" on public.sensor_readings
  for select to anon, authenticated using (true);
create policy "sibol insert" on public.sensor_readings
  for insert to anon, authenticated with check (true);

grant select, insert on public.sensor_readings to anon, authenticated;

-- =====================================================
-- 2) Daily averages for the trend chart (Philippine time)
-- =====================================================
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

-- =====================================================
-- 3) Water pump + camera control (one row per device)
-- =====================================================
create table if not exists public.device_controls (
  device_id    text primary key,
  pump_mode    text not null default 'auto' check (pump_mode in ('auto', 'manual')),
  pump_command boolean not null default false,  -- manual ON/OFF from the web
  pump_state   boolean not null default false,  -- actual relay state, reported by the ESP32
  stream_until timestamptz,                     -- camera uploads frames while now() < this
  last_seen    timestamptz                      -- ESP32 heartbeat
);

insert into public.device_controls (device_id)
values ('esp32')
on conflict (device_id) do nothing;

-- The ESP32 reads this view (stream_active is computed on the server, so the ESP needs no clock)
create or replace view public.device_controls_view
with (security_invoker = true) as
select device_id, pump_mode, pump_command, pump_state, last_seen,
       coalesce(stream_until > now(), false) as stream_active
from public.device_controls;

alter table public.device_controls enable row level security;

drop policy if exists "anon read controls" on public.device_controls;
drop policy if exists "anon update controls" on public.device_controls;
drop policy if exists "sibol read controls" on public.device_controls;
drop policy if exists "sibol update controls" on public.device_controls;

create policy "sibol read controls" on public.device_controls
  for select to anon, authenticated using (true);
create policy "sibol update controls" on public.device_controls
  for update to anon, authenticated using (true) with check (true);

grant select, update on public.device_controls to anon, authenticated;
grant select on public.device_controls_view to anon, authenticated;

-- =====================================================
-- 4) Camera frames: public bucket "files", ESP32 overwrites cam/esp32/latest.jpg
-- =====================================================
insert into storage.buckets (id, name, public)
values ('files', 'files', true)
on conflict (id) do update set public = true;

drop policy if exists "anon upload files" on storage.objects;
drop policy if exists "anon update files" on storage.objects;
drop policy if exists "anon read files" on storage.objects;

create policy "anon upload files" on storage.objects for insert to anon
  with check (bucket_id = 'files');
create policy "anon update files" on storage.objects for update to anon
  using (bucket_id = 'files') with check (bucket_id = 'files');
create policy "anon read files" on storage.objects for select to anon
  using (bucket_id = 'files');
