-- =====================================================================
-- Baristina V1 — schéma Supabase
-- À exécuter une fois dans Supabase > SQL Editor (puis seed.sql).
-- Le script peut être relancé sans casser l'existant.
-- =====================================================================

create extension if not exists citext;

-- ---------------------------------------------------------------------
-- Profils : un pseudo unique par compte (insensible à la casse)
-- ---------------------------------------------------------------------
create table if not exists public.profiles (
  id          uuid primary key references auth.users (id) on delete cascade,
  username    citext unique
              check (username ~ '^[A-Za-z0-9_.-]{3,20}$'),
  created_at  timestamptz not null default now()
);
-- Profil public : biographie et photo (stockée dans le bucket « avatars », dossier = id du compte).
alter table public.profiles add column if not exists bio text check (char_length(bio) <= 300);
alter table public.profiles add column if not exists avatar_path text;
alter table public.profiles drop constraint if exists profiles_avatar_path_check;
alter table public.profiles add constraint profiles_avatar_path_check
  check (avatar_path is null or (avatar_path like id::text || '/%' and avatar_path ~ '^[0-9a-f-]{36}/[A-Za-z0-9_.-]{1,60}$'));
alter table public.profiles enable row level security;

-- Changement de pseudo : une fois tous les 7 jours (le premier choix du pseudo n'est pas limité).
alter table public.profiles add column if not exists username_changed_at timestamptz;
create or replace function public.limit_username_change()
returns trigger
language plpgsql set search_path = public
as $$
begin
  if new.username is distinct from old.username and old.username is not null then
    if old.username_changed_at is not null and old.username_changed_at > now() - interval '7 days' then
      raise exception 'username_cooldown: un seul changement de pseudo tous les 7 jours';
    end if;
    new.username_changed_at := now();
  else
    new.username_changed_at := old.username_changed_at; -- non modifiable directement
  end if;
  return new;
end;
$$;
drop trigger if exists profiles_username_cooldown on public.profiles;
create trigger profiles_username_cooldown
  before update on public.profiles
  for each row execute function public.limit_username_change();

drop policy if exists "Pseudos visibles" on public.profiles;
create policy "Pseudos visibles" on public.profiles
  for select using (true);

drop policy if exists "Profil modifiable par son propriétaire" on public.profiles;
create policy "Profil modifiable par son propriétaire" on public.profiles
  for update to authenticated using (auth.uid() = id) with check (auth.uid() = id);

-- Vérification de disponibilité d'un pseudo (appelée pendant la saisie).
create or replace function public.username_available(p_username text)
returns boolean
language sql stable security definer set search_path = public
as $$
  select p_username ~ '^[A-Za-z0-9_.-]{3,20}$'
     and not exists (select 1 from public.profiles where username = p_username::citext);
$$;
grant execute on function public.username_available(text) to anon, authenticated;

-- Création automatique du profil à l'inscription.
-- Inscription par e-mail : le pseudo arrive dans les métadonnées et la contrainte
-- UNIQUE bloque l'inscription s'il est déjà pris (même en cas de course).
-- Connexion Google / Apple : pas de pseudo, l'app le demande ensuite.
create or replace function public.handle_new_user()
returns trigger
language plpgsql security definer set search_path = public
as $$
begin
  insert into public.profiles (id, username)
  values (new.id, nullif(trim(new.raw_user_meta_data ->> 'username'), ''));
  return new;
end;
$$;
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- Suppression de compte par l'utilisateur lui-même (RGPD, exigé par Apple).
create or replace function public.delete_my_account()
returns void
language sql security definer set search_path = public
as $$
  delete from auth.users where id = auth.uid();
$$;
revoke execute on function public.delete_my_account() from anon, public;
grant execute on function public.delete_my_account() to authenticated;

-- ---------------------------------------------------------------------
-- Catalogue (lecture publique, écriture réservée au tableau de bord)
-- ---------------------------------------------------------------------
create table if not exists public.brands (
  id        text primary key,
  name      text not null,
  website   text,
  sort      smallint not null default 100
);

create table if not exists public.products (
  id              text primary key,
  brand_id        text not null references public.brands (id),
  name            text not null,
  range           text,                       -- gamme / collection de la marque
  intensity       smallint check (intensity between 1 and 13),
  cup_sizes       text[] not null default '{}',
  aromas          text[] not null default '{}',
  origin          text,
  roast           text check (roast in ('blonde', 'medium', 'dark')),
  highlight       text,                       -- particularité vérifiée (extra caféiné, mi-caféiné…)
  decaf           boolean not null default false,
  flavoured       boolean not null default false,
  organic         boolean not null default false,
  limited         boolean not null default false,
  color           text check (color ~ '^#[0-9a-fA-F]{6}$'),
  photo_url       text,
  shop_url        text not null,
  system          text not null default 'original' check (system in ('original', 'vertuo')), -- format de capsule Nespresso
  verified_on     date,
  active          boolean not null default true
);
create index if not exists products_brand_idx on public.products (brand_id);
alter table public.products add column if not exists system text not null default 'original' check (system in ('original', 'vertuo'));

alter table public.brands   enable row level security;
alter table public.products enable row level security;

drop policy if exists "Marques visibles" on public.brands;
create policy "Marques visibles" on public.brands for select using (true);

drop policy if exists "Produits actifs visibles" on public.products;
create policy "Produits actifs visibles" on public.products for select using (active);

-- ---------------------------------------------------------------------
-- Collection : publique (visible sur le profil), modifiable seulement par son propriétaire
-- ---------------------------------------------------------------------
create table if not exists public.collection_items (
  user_id     uuid not null default auth.uid() references auth.users (id) on delete cascade,
  product_id  text not null references public.products (id) on delete cascade,
  quantity    integer not null default 1 check (quantity between 1 and 9999),
  rating      smallint check (rating between 1 and 5),
  note        text check (char_length(note) <= 500),
  updated_at  timestamptz not null default now(),
  primary key (user_id, product_id)
);
alter table public.collection_items enable row level security;

drop policy if exists "Ma collection" on public.collection_items;
drop policy if exists "Collections publiques" on public.collection_items;
create policy "Collections publiques" on public.collection_items for select using (true);
drop policy if exists "Ajouter à ma collection" on public.collection_items;
create policy "Ajouter à ma collection" on public.collection_items
  for insert to authenticated with check (auth.uid() = user_id);
drop policy if exists "Modifier ma collection" on public.collection_items;
create policy "Modifier ma collection" on public.collection_items
  for update to authenticated using (auth.uid() = user_id) with check (auth.uid() = user_id);
drop policy if exists "Retirer de ma collection" on public.collection_items;
create policy "Retirer de ma collection" on public.collection_items
  for delete to authenticated using (auth.uid() = user_id);

-- ---------------------------------------------------------------------
-- Photos proposées par la communauté (validées à la main)
-- ---------------------------------------------------------------------
create table if not exists public.photo_submissions (
  id            bigint generated always as identity primary key,
  user_id       uuid not null default auth.uid() references auth.users (id) on delete cascade,
  product_id    text not null references public.products (id) on delete cascade,
  storage_path  text not null,
  status        text not null default 'pending' check (status in ('pending', 'approved', 'rejected')),
  created_at    timestamptz not null default now()
);
alter table public.photo_submissions enable row level security;

drop policy if exists "Envoyer une photo" on public.photo_submissions;
create policy "Envoyer une photo" on public.photo_submissions
  for insert with check (auth.uid() = user_id and status = 'pending');

drop policy if exists "Voir mes envois" on public.photo_submissions;
create policy "Voir mes envois" on public.photo_submissions
  for select using (auth.uid() = user_id);

-- ---------------------------------------------------------------------
-- Stockage : envois privés de la communauté + photos publiques validées
-- ---------------------------------------------------------------------
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values
  ('submissions', 'submissions', false, 3145728, array['image/jpeg', 'image/png', 'image/webp']),
  ('capsules',    'capsules',    true,  3145728, array['image/jpeg', 'image/png', 'image/webp']),
  ('avatars',     'avatars',     true,  1048576, array['image/jpeg'])
on conflict (id) do nothing;

drop policy if exists "Envoi dans mon dossier" on storage.objects;
create policy "Envoi dans mon dossier" on storage.objects
  for insert to authenticated
  with check (bucket_id = 'submissions' and (storage.foldername(name))[1] = auth.uid()::text);

-- Photos de profil : publiques en lecture, chacun ne gère que son dossier.
drop policy if exists "Avatar : voir le mien" on storage.objects;
create policy "Avatar : voir le mien" on storage.objects
  for select to authenticated
  using (bucket_id = 'avatars' and (storage.foldername(name))[1] = auth.uid()::text);
drop policy if exists "Avatar : envoyer" on storage.objects;
create policy "Avatar : envoyer" on storage.objects
  for insert to authenticated
  with check (bucket_id = 'avatars' and (storage.foldername(name))[1] = auth.uid()::text);
drop policy if exists "Avatar : supprimer" on storage.objects;
create policy "Avatar : supprimer" on storage.objects
  for delete to authenticated
  using (bucket_id = 'avatars' and (storage.foldername(name))[1] = auth.uid()::text);
