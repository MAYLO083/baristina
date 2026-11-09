// Baristina V1 — application (catalogue, fiches, scanner, collection, compte).
// Toutes les données passent par Supabase : rien n'est stocké sur l'appareil,
// à part la session de connexion gérée par Supabase.
import { createClient } from 'https://cdn.jsdelivr.net/npm/@supabase/supabase-js@2.45.4/+esm';
import { SUPABASE_URL, SUPABASE_ANON_KEY } from './config.js';
import { identify } from './scan.js';

const OAUTH_ENABLED = false; // connexions Google / Apple : à activer plus tard
const CONFIGURED =!SUPABASE_URL.includes('VOTRE-PROJET') && !SUPABASE_ANON_KEY.includes('VOTRE');
const sb = CONFIGURED
  ? createClient(SUPABASE_URL, SUPABASE_ANON_KEY, { auth: { persistSession: true, detectSessionInUrl: true, flowType: 'pkce' } })
  : null;

// ---------------------------------------------------------------- pas de zoom
// Safari (iPhone) ignore « user-scalable=no » : on bloque le pincement à deux doigts nous-mêmes.
for (const ev of ['gesturestart', 'gesturechange', 'gestureend']) document.addEventListener(ev, (e) => e.preventDefault(), { passive: false });
document.addEventListener('touchmove', (e) => { if (e.touches.length > 1) e.preventDefault(); }, { passive: false });

// ---------------------------------------------------------------- état
const S = {
  brands: new Map(), products: [], byId: new Map(), loaded: false, loadError: null,
  user: null, profile: null, collection: new Map(), recovery: false,
  screen: 'home', authTab: 'login', catalogScroll: 0,
  filters: null,
};
const defaultFilters = () => ({ q: '', brand: 'all', range: 'all', min: 1, max: 13, sort: 'name',
  decaf: false, flavoured: false, organic: false, limited: false, owned: false });
S.filters = defaultFilters();

// ---------------------------------------------------------------- utilitaires
const $ = (id) => document.getElementById(id);
const esc = (v) => String(v ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const norm = (t) => String(t ?? '').toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9 ]/g, ' ').replace(/\s+/g, ' ').trim();
const joinFr = (a) => a.length <= 1 ? (a[0] ?? '') : a.slice(0, -1).join(', ') + ' et ' + a[a.length - 1];
const brandName = (p) => S.brands.get(p.brand_id)?.name ?? '';
const redirectURL = () => location.origin + location.pathname;
let toastTimer;
function toast(msg) {
  const t = $('toast'); t.textContent = msg; t.classList.add('show');
  clearTimeout(toastTimer); toastTimer = setTimeout(() => t.classList.remove('show'), 2200);
}
const AUTH_ERRORS = [
  [/invalid login credentials/i, 'E-mail ou mot de passe incorrect.'],
  [/email not confirmed/i, 'Confirme d’abord ton adresse e-mail (lien reçu par mail).'],
  [/already registered|already been registered/i, 'Un compte existe déjà avec cet e-mail.'],
  [/password should be at least|weak password/i, 'Mot de passe trop faible (8 caractères minimum).'],
  [/rate limit|too many/i, 'Trop de tentatives, réessaie dans quelques minutes.'],
  [/database error saving new user/i, 'Ce pseudo vient d’être pris, choisis-en un autre.'],
];
const authMessage = (e) => (AUTH_ERRORS.find(([re]) => re.test(e?.message ?? '')) ?? [0, 'Une erreur est survenue, réessaie.'])[1];

// ---------------------------------------------------------------- visuels
const BRAND_COLORS = { nespresso: '#5b4636', lor: '#8a6a2c', starbucks: '#1e5b45', 'carte-noire': '#2a2a2a', lavazza: '#1d3557', 'cafe-royal': '#7a1f2b' };
const hexToRgb = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
const rgbToHex = (rgb) => '#' + rgb.map((v) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0')).join('');
const mix = (h, target, p) => rgbToHex(hexToRgb(h).map((v) => v + (target - v) * p));
function capColor(p) {
  if (p.color) return p.color;
  const base = BRAND_COLORS[p.brand_id] ?? '#6b5a45';
  if (!p.intensity) return base;
  const k = (p.intensity - 7) / 13; // plus intense = plus foncé (illustration seulement)
  return k > 0 ? mix(base, 0, k) : mix(base, 255, -k);
}
function artHTML(p, big) {
  const c = capColor(p);
  const tag = p.color ? '' : `<span class="brand-tag">${esc(brandName(p))}</span>`;
  return `<span class="capsule${big ? ' big' : ''}" title="${esc(p.name)}"><span class="shell" style="--cap:${c};--cap-light:${mix(c, 255, .25)};--cap-dark:${mix(c, 0, .28)}"></span>${tag}</span>`;
}
function capsuleHTML(p, big = false) {
  if (!p.photo_url) return artHTML(p, big);
  // Photo locale, ou photo officielle affichée depuis le site de la marque (pas de copie).
  const external = /^https?:/.test(p.photo_url);
  return `<span class="capsule${big ? ' big' : ''}"><img src="${esc(p.photo_url)}" alt="${esc(p.name)}" loading="lazy" data-fallback="${esc(p.id)}"${external ? ' referrerpolicy="no-referrer"' : ''}></span>${big && external ? `<p class="photo-credit">Photo : ${esc(brandName(p))}</p>` : ''}`;
}
// Photo introuvable : on affiche l'illustration à la place (les gestionnaires inline sont bloqués par la CSP).
document.addEventListener('error', (e) => {
  const img = e.target;
  if (img.tagName !== 'IMG' || !img.dataset.fallback) return;
  const p = S.byId.get(img.dataset.fallback);
  const wrap = img.closest('.capsule');
  if (p && wrap) wrap.outerHTML = artHTML(p, wrap.classList.contains('big'));
}, true);

// ---------------------------------------------------------------- textes des fiches
// Rédigés uniquement à partir des faits enregistrés : rien n'est inventé.
const ROAST = { blonde: 'blonde (légère)', medium: 'ambrée (moyenne)', dark: 'brune (foncée)' };
function intensityWord(i) { return i <= 4 ? 'doux' : i <= 7 ? 'équilibré' : i <= 10 ? 'intense' : 'très intense'; }
function describe(p) {
  const out = [];
  const range = p.range && !/^capsules/i.test(p.range) ? ` de la gamme ${p.range}` : '';
  out.push(`${p.name} est une capsule ${brandName(p)}${range}, au format Nespresso Original.`);
  if (p.intensity) out.push(`Son intensité est de ${p.intensity} sur 13 selon la marque : c’est un café ${intensityWord(p.intensity)}.`);
  if (p.roast) out.push(`Sa torréfaction est ${ROAST[p.roast]}.`);
  if (p.aromas?.length) out.push(`${p.flavoured ? 'Saveur annoncée' : 'Profil aromatique'} : ${joinFr(p.aromas)}.`);
  if (p.origin) out.push(`Origine : ${p.origin}.`);
  if (p.cup_sizes?.length) out.push(`Préparation conseillée : ${joinFr(p.cup_sizes.map((c) => c.toLowerCase()))}.`);
  if (p.decaf) out.push('C’est une version décaféinée.');
  if (p.organic) out.push('Café issu de l’agriculture biologique.');
  if (p.limited) out.push('Édition limitée : disponible pour une durée limitée.');
  if (p.highlight) out.push(p.highlight + '.');
  if (!p.intensity && !p.aromas?.length) out.push('Les autres caractéristiques n’ont pas encore été vérifiées : consulte la page officielle du produit.');
  return out.join(' ');
}

// ---------------------------------------------------------------- profils publics : photo
const PROFILE_FIELDS = 'id, username, bio, avatar_path, created_at, username_changed_at';
const USERNAME_COOLDOWN_DAYS = 7;
const nextUsernameChange = (pr) => pr?.username_changed_at ? new Date(new Date(pr.username_changed_at).getTime() + USERNAME_COOLDOWN_DAYS * 864e5) : null;
const avatarURL = (pr) => pr?.avatar_path ? sb.storage.from('avatars').getPublicUrl(pr.avatar_path).data.publicUrl : null;
function avatarHTML(pr, big = false) {
  const url = avatarURL(pr);
  return `<span class="avatar${big ? ' big' : ''}">${url ? `<img src="${esc(url)}" alt="" loading="lazy">` : esc(String(pr?.username ?? '?').charAt(0))}</span>`;
}
const profileLink = (username) => '#/profil/' + encodeURIComponent(username);

// ---------------------------------------------------------------- données
async function loadCatalog() {
  const [b, p] = await Promise.all([
    sb.from('brands').select('*').order('sort'),
    sb.from('products').select('*').eq('active', true),
  ]);
  if (b.error || p.error) { S.loadError = 'Impossible de charger le catalogue. Vérifie ta connexion.'; return; }
  // Seules les marques qui ont des capsules au catalogue (une marque retirée garde sa ligne en base).
  S.brands = new Map(b.data.filter((x) => p.data.some((y) => y.brand_id === x.id)).map((x) => [x.id, x]));
  S.products = p.data;
  S.byId = new Map(p.data.map((x) => [x.id, x]));
  S.loaded = true;
}
async function loadUserData() {
  if (!S.user) { S.profile = null; S.collection = new Map(); return; }
  const [pr, col] = await Promise.all([
    sb.from('profiles').select(PROFILE_FIELDS).eq('id', S.user.id).maybeSingle(),
    sb.from('collection_items').select('*').eq('user_id', S.user.id), // les collections sont publiques : ne charger que la mienne
  ]);
  S.profile = pr.data ?? null;
  S.collection = new Map((col.data ?? []).map((x) => [x.product_id, x]));
}

// ---------------------------------------------------------------- navigation
const ROUTES = { '': 'home', catalogue: 'catalog', scanner: 'scan', collection: 'collection', compte: 'account', membres: 'members' };
const SCREENS = { home: 'screen-home', catalog: 'screen-catalog', detail: 'screen-detail', scan: 'screen-scan', collection: 'screen-collection', account: 'screen-account', members: 'screen-members', profile: 'screen-profile' };
let detailId = null;
function route() {
  const [a, b] = location.hash.replace(/^#\/?/, '').split('/');
  if (a === 'capsule' && b) { detailId = decodeURIComponent(b); R.delete(detailId); show('detail'); return; } // avis rechargés à chaque ouverture
  if (a === 'profil' && b) { openProfile(decodeURIComponent(b)); show('profile'); return; }
  // « Compte » : mon profil public (modification via #/compte/modifier) ; sinon connexion / choix du pseudo.
  if (a === 'compte') {
    if (b !== 'modifier' && S.user && S.profile?.username && !S.recovery) { openProfile(S.profile.username); show('profile'); return; }
    show('account'); return;
  }
  show(ROUTES[a] ?? 'home');
}
function show(name) {
  if (S.screen === 'catalog') S.catalogScroll = window.scrollY;
  if (S.screen === 'scan' && name !== 'scan') stopCamera();
  S.screen = name;
  for (const [k, id] of Object.entries(SCREENS)) $(id).classList.toggle('active', k === name);
  document.body.classList.toggle('on-home', name === 'home');
  const navKey = location.hash.startsWith('#/compte') ? 'account' : name;
  document.querySelectorAll('.nav a').forEach((a) => a.classList.toggle('active', a.dataset.nav === navKey));
  render();
  window.scrollTo(0, name === 'catalog' ? S.catalogScroll : 0);
  if (name === 'scan') resetScan();
}
function render() {
  // Bulle de profil en haut à droite de l'accueil : ma photo (ou initiale), sinon une silhouette (connexion).
  $('home-avatar').innerHTML = S.profile?.username ? avatarHTML(S.profile)
    : '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="8" r="4"/><path d="M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6"/></svg>';
  $('home-avatar').setAttribute('aria-label', S.profile?.username ? `Mon profil (${S.profile.username})` : 'Se connecter');
  ({ catalog: renderCatalog, detail: renderDetail, collection: renderCollection, account: renderAccount, members: renderMembers, profile: renderProfile })[S.screen]?.();
}
window.addEventListener('hashchange', route);

// ---------------------------------------------------------------- catalogue
function filtered() {
  const f = S.filters, words = norm(f.q).split(' ').filter(Boolean);
  const out = S.products.filter((p) => {
    if (f.brand !== 'all' && p.brand_id !== f.brand) return false;
    if (f.range !== 'all' && p.range !== f.range) return false;
    if ((f.min > 1 || f.max < 13) && (!p.intensity || p.intensity < f.min || p.intensity > f.max)) return false;
    for (const k of ['decaf', 'flavoured', 'organic', 'limited']) if (f[k] && !p[k]) return false;
    if (f.owned && !S.collection.has(p.id)) return false;
    if (!words.length) return true;
    const hay = norm([p.name, brandName(p), p.range, p.origin, p.highlight, ...(p.aromas ?? [])].join(' '));
    return words.every((w) => hay.includes(w));
  });
  const byName = (a, b) => a.name.localeCompare(b.name, 'fr', { sensitivity: 'base' });
  if (f.sort === 'name') out.sort(byName);
  else out.sort((a, b) => ((f.sort === 'intensity-desc' ? -1 : 1) * ((a.intensity ?? (f.sort === 'intensity-desc' ? -1 : 99)) - (b.intensity ?? (f.sort === 'intensity-desc' ? -1 : 99)))) || byName(a, b));
  return out;
}
function cardHTML(p) {
  const item = S.collection.get(p.id);
  const tags = [p.intensity ? `Intensité ${p.intensity}` : null, p.decaf ? 'Déca' : null, p.flavoured ? 'Aromatisé' : null, p.organic ? 'Bio' : null, p.limited ? 'Édition limitée' : null].filter(Boolean);
  return `<a class="card" href="#/capsule/${encodeURIComponent(p.id)}">${capsuleHTML(p)}<div class="meta"><strong>${esc(p.name)}</strong><span class="sub">${esc(brandName(p))}${tags.length ? ' · ' + esc(tags.join(' · ')) : ''}</span></div>${item ? `<span class="qty">×${item.quantity}</span>` : ''}</a>`;
}
function renderFilterControls() {
  const f = S.filters;
  $('brand-chips').innerHTML = [['all', 'Toutes les marques'], ...[...S.brands.values()].map((b) => [b.id, b.name])]
    .map(([id, n]) => `<button class="chip${f.brand === id ? ' on' : ''}" data-brand="${esc(id)}">${esc(n)}</button>`).join('');
  const ranges = [...new Set(S.products.filter((p) => f.brand === 'all' || p.brand_id === f.brand).map((p) => p.range).filter(Boolean))].sort((a, b) => a.localeCompare(b, 'fr'));
  if (f.range !== 'all' && !ranges.includes(f.range)) f.range = 'all';
  $('f-range').innerHTML = `<option value="all">Toutes</option>` + ranges.map((r) => `<option${r === f.range ? ' selected' : ''}>${esc(r)}</option>`).join('');
  const opts = (sel) => Array.from({ length: 13 }, (_, i) => `<option value="${i + 1}"${i + 1 === sel ? ' selected' : ''}>${i + 1}</option>`).join('');
  $('f-min').innerHTML = opts(f.min); $('f-max').innerHTML = opts(f.max);
  $('f-sort').value = f.sort;
  document.querySelectorAll('[data-toggle]').forEach((b) => { b.classList.toggle('on', !!f[b.dataset.toggle]); b.hidden = b.dataset.toggle === 'owned' && !S.user; });
}
function renderCatalog() {
  if (!CONFIGURED) { $('catalog-list').innerHTML = setupNotice(); return; }
  if (S.loadError) { $('catalog-list').innerHTML = `<div class="notice">${esc(S.loadError)}</div>`; return; }
  if (!S.loaded) return;
  renderFilterControls();
  const list = filtered();
  $('result-count').textContent = `${list.length} capsule${list.length > 1 ? 's' : ''}`;
  if (!list.length) { $('catalog-list').innerHTML = '<div class="empty">Aucune capsule ne correspond à ces critères.</div>'; return; }
  let html = '';
  for (const brand of S.brands.values()) {
    const items = list.filter((p) => p.brand_id === brand.id);
    if (!items.length) continue;
    html += `<h3 class="group-title">${esc(brand.name)} <small>· ${items.length}</small></h3>`;
    if (S.filters.sort === 'name') {
      const ranges = [...new Set(items.map((p) => p.range ?? 'Autres'))].sort((a, b) => a.localeCompare(b, 'fr'));
      for (const r of ranges) {
        if (ranges.length > 1) html += `<div class="range-title">${esc(r)}</div>`;
        html += items.filter((p) => (p.range ?? 'Autres') === r).map(cardHTML).join('');
      }
    } else html += items.map(cardHTML).join('');
  }
  $('catalog-list').innerHTML = html;
}
$('q').addEventListener('input', (e) => { S.filters.q = e.target.value; renderCatalog(); });
for (const [id, key] of [['f-range', 'range'], ['f-sort', 'sort'], ['f-min', 'min'], ['f-max', 'max']]) {
  $(id).addEventListener('change', (e) => {
    const v = e.target.value;
    S.filters[key] = key === 'min' || key === 'max' ? Number(v) : v;
    if (S.filters.min > S.filters.max) [S.filters.min, S.filters.max] = [S.filters.max, S.filters.min];
    renderCatalog();
  });
}

// ---------------------------------------------------------------- fiche capsule
function renderDetail() {
  const box = $('detail-content');
  if (!S.loaded) { box.innerHTML = CONFIGURED ? '<div class="loading">Chargement…</div>' : `<div class="content">${setupNotice()}</div>`; return; }
  const p = S.byId.get(detailId);
  if (!p) { box.innerHTML = '<div class="empty">Cette capsule est introuvable.</div>'; return; }
  let dots = '';
  if (p.intensity) for (let i = 1; i <= 13; i++) dots += `<span class="dot${i > p.intensity ? ' off' : ''}"></span>`;
  const badges = [p.intensity ? `<span class="badge purple">Intensité ${p.intensity}/13</span>` : '',
    p.decaf ? '<span class="badge">Décaféiné</span>' : '', p.flavoured ? '<span class="badge">Aromatisé</span>' : '',
    p.organic ? '<span class="badge">Bio</span>' : '', p.limited ? '<span class="badge">Édition limitée</span>' : ''].join('');
  const facts = [
    ['Marque', brandName(p)], ['Gamme', p.range], ['Intensité', p.intensity ? `${p.intensity} / 13` : null],
    ['Torréfaction', p.roast ? ROAST[p.roast] : null], ['Arômes', p.aromas?.length ? joinFr(p.aromas) : null],
    ['Origine', p.origin], ['Tasse', p.cup_sizes?.length ? p.cup_sizes.join(' · ') : null], ['Particularité', p.highlight],
  ].filter(([, v]) => v).map(([k, v]) => `<div class="fact"><small>${esc(k)}</small><span>${esc(v)}</span></div>`).join('');
  const verified = p.verified_on ? new Date(p.verified_on).toLocaleDateString('fr-FR') : null;
  box.innerHTML = `
    <div class="hero">${capsuleHTML(p, true)}
      <div class="brand">${esc(brandName(p))}</div><h1>${esc(p.name)}</h1>
      ${p.range ? `<div class="range">${esc(p.range)}</div>` : ''}
      <div class="badges">${badges}</div>${dots ? `<div class="dots" aria-hidden="true">${dots}</div>` : ''}
    </div>
    <p class="description">${esc(describe(p))}</p>
    <div class="facts">${facts}</div>
    ${collectionBoxHTML(p)}
    <div id="reviews">${reviewsHTML(p)}</div>
    <a class="shop-link" href="${esc(p.shop_url)}" target="_blank" rel="noopener noreferrer">Voir sur la boutique officielle ↗</a>
    <p class="source">${verified ? `Informations relevées sur le site de la marque le ${esc(verified)}.` : ''} Prix et disponibilité sur la boutique officielle.</p>
    ${S.user && !p.photo_url ? `<div class="box"><h3>Proposer une photo</h3><p class="hint">Photographie la capsule de côté, bien centrée, sans flash. Elle sera visible par tous après validation.</p><label class="btn secondary file-btn">Choisir une photo<input type="file" accept="image/*" hidden data-upload="${esc(p.id)}"></label></div>` : ''}
    <div class="content"></div>`;
}
function collectionBoxHTML(p) {
  if (!CONFIGURED) return '';
  if (!S.user) return `<div class="box"><h3>Ma collection</h3><p class="hint">Connecte-toi pour ajouter cette capsule à ta collection.</p><a class="btn secondary" href="#/compte">Se connecter</a></div>`;
  const item = S.collection.get(p.id);
  if (!item) return `<div class="box"><button class="btn" data-action="add" data-id="${esc(p.id)}">♡ Ajouter à ma collection</button></div>`;
  const stars = [1, 2, 3, 4, 5].map((n) => `<button class="${n <= (item.rating ?? 0) ? 'on' : ''}" data-action="rate" data-id="${esc(p.id)}" data-value="${n}" aria-label="${n} sur 5">★</button>`).join('');
  return `<div class="box"><h3>Dans ma collection</h3>
    <div class="stepper"><button data-action="qty" data-id="${esc(p.id)}" data-delta="-1" aria-label="Retirer une capsule">−</button><output>${item.quantity}</output><button data-action="qty" data-id="${esc(p.id)}" data-delta="1" aria-label="Ajouter une capsule">+</button></div>
    <div class="stars">${stars}</div>
    <label class="lbl" for="note-${esc(p.id)}">Ma note de dégustation</label>
    <textarea id="note-${esc(p.id)}" class="field" maxlength="500" placeholder="Ce que j’en pense…">${esc(item.note ?? '')}</textarea>
    <button class="btn secondary" data-action="save-note" data-id="${esc(p.id)}">Enregistrer la note</button>
    <button class="link-btn" data-action="remove" data-id="${esc(p.id)}">Retirer de ma collection</button></div>`;
}

// ---------------------------------------------------------------- avis de la communauté (fiche capsule)
// Les collections sont publiques : on agrège celles qui contiennent la capsule.
const R = new Map(); // id capsule → { items, profiles } | 'loading' | 'error'
async function loadReviews(id) {
  R.set(id, 'loading');
  const { data: items, error } = await sb.from('collection_items').select('user_id, quantity, rating, note, updated_at').eq('product_id', id).order('updated_at', { ascending: false }).limit(500);
  let profiles = [];
  const ids = [...new Set((items ?? []).filter((i) => i.rating || i.note).slice(0, 20).map((i) => i.user_id))];
  if (!error && ids.length) profiles = (await sb.from('profiles').select(PROFILE_FIELDS).in('id', ids)).data ?? [];
  R.set(id, error ? 'error' : { items, profiles: new Map(profiles.map((x) => [x.id, x])) });
  if (S.screen === 'detail' && detailId === id && $('reviews')) $('reviews').innerHTML = reviewsHTML(S.byId.get(id));
}
function refreshReviews(id) { if (R.has(id)) loadReviews(id); }
function reviewsHTML(p) {
  if (!CONFIGURED || !p) return '';
  const r = R.get(p.id);
  if (!r) { loadReviews(p.id); return ''; }
  if (r === 'loading' || r === 'error') return '';
  const rated = r.items.filter((i) => i.rating);
  const owners = r.items.length;
  if (!owners) return `<div class="box"><h3>Avis de la communauté</h3><p class="hint">Aucun membre n’a encore cette capsule dans sa collection.</p></div>`;
  const avg = rated.length ? rated.reduce((s, i) => s + i.rating, 0) / rated.length : 0;
  const summary = `${rated.length ? `<div class="avg"><b>${avg.toFixed(1).replace('.', ',')}</b>${miniStars(Math.round(avg))}<span>${rated.length} note${rated.length > 1 ? 's' : ''}</span></div>` : ''}
    <p class="hint">${owners} membre${owners > 1 ? 's l’ont' : ' l’a'} dans sa collection.</p>`;
  const list = r.items.filter((i) => (i.rating || i.note) && r.profiles.get(i.user_id)?.username).slice(0, 20).map((i) => {
    const pr = r.profiles.get(i.user_id), me = i.user_id === S.user?.id;
    return `<a class="review" href="${profileLink(pr.username)}">${avatarHTML(pr)}<div class="meta"><strong>${esc(pr.username)}${me ? ' <small>(toi)</small>' : ''}</strong>${i.rating ? miniStars(i.rating) : ''}${i.note ? `<p>« ${esc(i.note)} »</p>` : ''}<span class="date">${esc(new Date(i.updated_at).toLocaleDateString('fr-FR'))}</span></div></a>`;
  }).join('');
  return `<div class="box"><h3>Avis de la communauté</h3>${summary}${list}</div>`;
}

// ---------------------------------------------------------------- collection (Supabase)
async function saveItem(id, patch) {
  const prev = S.collection.get(id);
  const next = { quantity: 1, rating: null, note: null, ...prev, ...patch, product_id: id, updated_at: new Date().toISOString() };
  S.collection.set(id, next); render();
  const { error } = await sb.from('collection_items').upsert({ user_id: S.user.id, product_id: id, quantity: next.quantity, rating: next.rating, note: next.note, updated_at: next.updated_at });
  if (error) { prev ? S.collection.set(id, prev) : S.collection.delete(id); render(); toast('Enregistrement impossible, réessaie.'); return false; }
  refreshReviews(id);
  return true;
}
async function removeItem(id) {
  const prev = S.collection.get(id); if (!prev) return;
  S.collection.delete(id); render();
  const { error } = await sb.from('collection_items').delete().eq('user_id', S.user.id).eq('product_id', id);
  if (error) { S.collection.set(id, prev); render(); toast('Suppression impossible, réessaie.'); } else { toast('Capsule retirée de ta collection'); refreshReviews(id); }
}
function renderCollection() {
  const box = $('collection-content');
  if (!CONFIGURED) { box.innerHTML = setupNotice(); return; }
  if (!S.user) { box.innerHTML = `<div class="empty">Ta collection est enregistrée dans ton compte : retrouve-la sur tous tes appareils.</div><a class="btn" href="#/compte">Se connecter ou créer un compte</a>`; return; }
  if (!S.loaded) { box.innerHTML = '<div class="loading">Chargement…</div>'; return; }
  const items = [...S.collection.values()].map((i) => ({ i, p: S.byId.get(i.product_id) })).filter((x) => x.p);
  if (!items.length) { box.innerHTML = '<div class="empty">Ta collection est vide.<br>Ajoute des capsules depuis le catalogue ou le scanner.</div><a class="btn" href="#/catalogue">Parcourir le catalogue</a>'; return; }
  const total = items.reduce((s, x) => s + x.i.quantity, 0);
  const brands = new Set(items.map((x) => x.p.brand_id)).size;
  let html = (S.profile?.username ? `<a class="btn secondary" href="${profileLink(S.profile.username)}">Voir mon profil public</a>` : '') + `<div class="stats"><div class="stat"><b>${items.length}</b><span>références</span></div><div class="stat"><b>${total}</b><span>capsules</span></div><div class="stat"><b>${brands}</b><span>marque${brands > 1 ? 's' : ''}</span></div></div>`;
  for (const brand of S.brands.values()) {
    const list = items.filter((x) => x.p.brand_id === brand.id).sort((a, b) => a.p.name.localeCompare(b.p.name, 'fr'));
    if (list.length) html += `<h3 class="group-title">${esc(brand.name)}</h3>` + list.map((x) => cardHTML(x.p)).join('');
  }
  box.innerHTML = html;
}

// ---------------------------------------------------------------- membres (recherche de profils)
const M = { q: null, list: null, seq: 0 };
let memberTimer;
async function searchMembers(q) {
  const seq = ++M.seq;
  let req = sb.from('profiles').select(PROFILE_FIELDS).not('username', 'is', null).limit(30);
  // Sans recherche : les derniers inscrits. Les caractères spéciaux de LIKE sont neutralisés.
  req = q ? req.ilike('username', `%${q.replace(/[\\%_]/g, (c) => '\\' + c)}%`).order('username') : req.order('created_at', { ascending: false });
  const { data, error } = await req;
  if (seq !== M.seq) return; // une recherche plus récente est en cours
  M.q = q; M.list = error ? 'error' : data;
  if (S.screen === 'members') renderMembers();
}
function renderMembers() {
  const box = $('members-list');
  if (!CONFIGURED) { box.innerHTML = setupNotice(); return; }
  const q = $('member-q').value.trim();
  if (M.q !== q) { if (M.list === null) box.innerHTML = '<div class="loading">Chargement…</div>'; searchMembers(q); return; }
  if (M.list === 'error') { box.innerHTML = '<div class="notice">Recherche impossible pour le moment. Vérifie ta connexion.</div>'; return; }
  if (!M.list.length) { box.innerHTML = `<div class="empty">Aucun membre ne correspond à « ${esc(q)} ».</div>`; return; }
  box.innerHTML = (q ? '' : '<p class="result-count">Derniers inscrits</p>') + M.list.map((pr) =>
    `<a class="member" href="${profileLink(pr.username)}">${avatarHTML(pr)}<div class="meta"><strong>${esc(pr.username)}</strong><span class="sub">${esc(pr.bio || 'Membre depuis ' + monthYear(pr.created_at))}</span></div><span class="arr">›</span></a>`).join('');
}
$('member-q').addEventListener('input', () => { clearTimeout(memberTimer); memberTimer = setTimeout(renderMembers, 300); });
const monthYear = (d) => new Date(d).toLocaleDateString('fr-FR', { month: 'long', year: 'numeric' });

// ---------------------------------------------------------------- profil public
const P = { username: null, profile: null, items: null, error: false, seq: 0 };
async function openProfile(username) {
  if (P.username?.toLowerCase() === username.toLowerCase() && P.profile && !P.error) { loadProfileItems(); return; }
  Object.assign(P, { username, profile: null, items: null, error: false });
  const seq = ++P.seq;
  if (!CONFIGURED) return;
  const { data, error } = await sb.from('profiles').select(PROFILE_FIELDS).eq('username', username).maybeSingle();
  if (seq !== P.seq) return;
  P.profile = data ?? null; P.error = !!error || !data;
  if (data) await loadProfileItems(seq); else if (S.screen === 'profile') renderProfile();
}
async function loadProfileItems(seq = P.seq) {
  const { data, error } = await sb.from('collection_items').select('product_id, quantity, rating, note, updated_at').eq('user_id', P.profile.id);
  if (seq !== P.seq) return;
  P.items = error ? [] : data;
  if (S.screen === 'profile') renderProfile();
}
const miniStars = (r) => `<span class="mini-stars" aria-label="${r} sur 5">${'★'.repeat(r)}<span class="off">${'★'.repeat(5 - r)}</span></span>`;
function ratedCardHTML(i, p) {
  return `<a class="card rated" href="#/capsule/${encodeURIComponent(p.id)}">${capsuleHTML(p)}<div class="meta"><strong>${esc(p.name)}</strong><span class="sub">${esc(brandName(p))}${p.intensity ? ' · Intensité ' + p.intensity : ''}</span>${i.rating ? miniStars(i.rating) : ''}</div><span class="qty">×${i.quantity}</span>${i.note ? `<p class="tasting">« ${esc(i.note)} »</p>` : ''}</a>`;
}
function renderProfile() {
  const box = $('profile-content');
  if (!CONFIGURED) { box.innerHTML = setupNotice(); return; }
  if (P.error) { box.innerHTML = `<div class="empty">Le membre « ${esc(P.username)} » est introuvable.</div><a class="btn secondary" href="#/membres">Rechercher un membre</a>`; return; }
  if (!P.profile || !P.items || !S.loaded) { box.innerHTML = '<div class="loading">Chargement du profil…</div>'; return; }
  const pr = P.profile, mine = S.user?.id === pr.id;
  const items = P.items.map((i) => ({ i, p: S.byId.get(i.product_id) })).filter((x) => x.p);
  const rated = items.filter((x) => x.i.rating);
  const avg = rated.length ? (rated.reduce((s, x) => s + x.i.rating, 0) / rated.length).toFixed(1).replace('.', ',') : '–';
  const brands = new Set(items.map((x) => x.p.brand_id)).size;
  let html = `<div class="profile-head">${mine ? '<a class="edit-btn" href="#/compte/modifier">✎ Modifier</a>' : ''}${avatarHTML(pr, true)}<h1>${esc(pr.username)}</h1><div class="since">Membre depuis ${esc(monthYear(pr.created_at))}</div>
    ${pr.bio ? `<p class="bio">${esc(pr.bio)}</p>` : ''}</div>
    <div class="stats four"><div class="stat"><b>${items.length}</b><span>références</span></div><div class="stat"><b>${items.reduce((s, x) => s + x.i.quantity, 0)}</b><span>capsules</span></div><div class="stat"><b>${brands}</b><span>marque${brands > 1 ? 's' : ''}</span></div><div class="stat"><b>${avg}</b><span>note moy.</span></div></div>`;
  if (!items.length) { box.innerHTML = html + `<div class="empty">${mine ? 'Ta collection est vide pour l’instant.' : 'Sa collection est vide pour l’instant.'}</div>`; return; }
  const favs = rated.filter((x) => x.i.rating >= 4).sort((a, b) => b.i.rating - a.i.rating || a.p.name.localeCompare(b.p.name, 'fr')).slice(0, 5);
  if (favs.length && items.length > 5) html += `<h3 class="group-title">Coups de cœur</h3>` + favs.map((x) => ratedCardHTML(x.i, x.p)).join('');
  for (const brand of S.brands.values()) {
    const list = items.filter((x) => x.p.brand_id === brand.id).sort((a, b) => a.p.name.localeCompare(b.p.name, 'fr'));
    if (list.length) html += `<h3 class="group-title">${esc(brand.name)} <small>· ${list.length}</small></h3>` + list.map((x) => ratedCardHTML(x.i, x.p)).join('');
  }
  box.innerHTML = html;
}

// ---------------------------------------------------------------- compte
function setupNotice() {
  return '<div class="notice"><b>Configuration nécessaire.</b><br>Renseigne l’adresse et la clé de ton projet Supabase dans <code>js/config.js</code> (voir le README).</div>';
}
function renderAccount() {
  const box = $('account-content');
  if (!CONFIGURED) { box.innerHTML = setupNotice(); return; }
  if (S.recovery) {
    box.innerHTML = `<h3>Nouveau mot de passe</h3><form data-form="new-password"><label class="lbl" for="np">Mot de passe (8 caractères minimum)</label><input id="np" class="field" type="password" minlength="8" required autocomplete="new-password"><button class="btn">Enregistrer</button></form>`;
    return;
  }
  if (!S.user) {
    const login = S.authTab === 'login';
    box.innerHTML = `
      <button class="btn oauth" data-action="oauth" data-provider="google"><b>G</b> Continuer avec Google${OAUTH_ENABLED ? '' : ' <small>(bientôt)</small>'}</button>
      <button class="btn oauth apple" data-action="oauth" data-provider="apple"> Continuer avec Apple${OAUTH_ENABLED ? '' : ' <small>(bientôt)</small>'}</button>
      <div class="sep">ou avec ton e-mail</div>
      <div class="tabs"><button class="${login ? 'on' : ''}" data-action="tab" data-tab="login">Connexion</button><button class="${login ? '' : 'on'}" data-action="tab" data-tab="signup">Créer un compte</button></div>
      ${login ? `
      <form data-form="login">
        <label class="lbl" for="le">E-mail</label><input id="le" class="field" type="email" required autocomplete="email">
        <label class="lbl" for="lp">Mot de passe</label><input id="lp" class="field" type="password" required autocomplete="current-password">
        <button class="btn">Se connecter</button>
      </form>
      <button class="link-btn" data-action="forgot">Mot de passe oublié ?</button>` : `
      <form data-form="signup">
        <label class="lbl" for="su">Pseudo (3 à 20 caractères : lettres, chiffres, . _ -)</label>
        <input id="su" class="field" required minlength="3" maxlength="20" pattern="[A-Za-z0-9_.\\-]{3,20}" autocomplete="username" data-username-check>
        <div class="hint" id="su-hint"></div>
        <label class="lbl" for="se">E-mail</label><input id="se" class="field" type="email" required autocomplete="email">
        <label class="lbl" for="sp">Mot de passe (8 caractères minimum)</label><input id="sp" class="field" type="password" required minlength="8" autocomplete="new-password">
        <button class="btn">Créer mon compte</button>
      </form>`}
      <p class="disclaimer">Les profils sont publics : ton pseudo, ta photo, ta biographie et ta collection (avec tes notes) seront visibles par tous. Ton e-mail reste privé. Tu peux supprimer ton compte à tout moment.</p>`;
    return;
  }
  if (!S.profile?.username) {
    box.innerHTML = `<h3>Choisis ton pseudo</h3><p class="hint">Il sera unique et visible des autres utilisateurs.</p>
      <form data-form="username"><input id="un" class="field" required minlength="3" maxlength="20" pattern="[A-Za-z0-9_.\\-]{3,20}" data-username-check autocomplete="username">
      <div class="hint" id="un-hint"></div><button class="btn">Valider</button></form>
      <button class="link-btn" data-action="logout">Se déconnecter</button>`;
    return;
  }
  box.innerHTML = `
    <div class="box flush avatar-edit">${avatarHTML(S.profile, true)}<div class="actions"><h3>${esc(S.profile.username)}</h3><p class="hint">${esc(S.user.email ?? '')}</p>
      <label class="btn secondary file-btn">${S.profile.avatar_path ? 'Changer de photo' : 'Ajouter une photo'}<input type="file" accept="image/*" hidden data-avatar></label>
      ${S.profile.avatar_path ? '<button class="link-btn" data-action="remove-avatar">Retirer la photo</button>' : ''}</div></div>
    <form data-form="bio"><label class="lbl" for="bio">Biographie</label>
      <textarea id="bio" class="field" maxlength="300" placeholder="Tes cafés préférés, ta machine, ta façon de déguster…">${esc(S.profile.bio ?? '')}</textarea>
      <div class="counter" id="bio-count">${(S.profile.bio ?? '').length} / 300</div>
      <button class="btn secondary">Enregistrer la biographie</button></form>
    <p class="hint">Ton profil est public : pseudo, photo, biographie et collection (avec tes notes et étoiles) sont visibles par tous.</p>
    ${usernameFormHTML()}
    <button class="btn" data-action="logout">Se déconnecter</button>
    <button class="btn danger" data-action="delete-account">Supprimer mon compte</button>
    <p class="disclaimer">Baristina V1 — application indépendante, non affiliée aux marques citées. Les informations produits proviennent des sites officiels des marques.</p>`;
}
// Changement de pseudo : une fois tous les 7 jours (règle vérifiée aussi par la base de données).
function usernameFormHTML() {
  const next = nextUsernameChange(S.profile), locked = next && next > new Date();
  return `<form data-form="username"><label class="lbl" for="un">Changer de pseudo</label>
      <input id="un" class="field" required minlength="3" maxlength="20" pattern="[A-Za-z0-9_.\\-]{3,20}" value="${esc(S.profile.username)}" data-username-check autocomplete="username"${locked ? ' disabled' : ''}>
      <div class="hint" id="un-hint">${locked ? `Tu pourras changer de pseudo à partir du ${esc(next.toLocaleDateString('fr-FR', { day: 'numeric', month: 'long' }))} à ${esc(next.toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit' }))}.` : `Possible une fois tous les ${USERNAME_COOLDOWN_DAYS} jours.`}</div>
      <button class="btn secondary"${locked ? ' disabled' : ''}>Modifier le pseudo</button></form>`;
}
let usernameTimer;
async function checkUsername(input) {
  const hint = $(input.id + '-hint'); const v = input.value.trim();
  clearTimeout(usernameTimer);
  if (S.profile?.username && v.toLowerCase() === String(S.profile.username).toLowerCase()) { hint.textContent = ''; return true; }
  if (!/^[A-Za-z0-9_.-]{3,20}$/.test(v)) { hint.className = 'hint bad'; hint.textContent = '3 à 20 caractères : lettres, chiffres, point, tiret, tiret bas.'; return false; }
  hint.className = 'hint'; hint.textContent = 'Vérification…';
  const { data, error } = await sb.rpc('username_available', { p_username: v });
  if (input.value.trim() !== v) return false; // la saisie a changé entre-temps
  if (error) { hint.textContent = ''; return true; } // le serveur tranchera à l'enregistrement
  hint.className = data ? 'hint ok' : 'hint bad'; hint.textContent = data ? '✓ Pseudo disponible' : '✗ Ce pseudo est déjà utilisé';
  return !!data;
}
document.addEventListener('input', (e) => {
  if (e.target.matches?.('[data-username-check]')) { clearTimeout(usernameTimer); usernameTimer = setTimeout(() => checkUsername(e.target), 400); }
  if (e.target.id === 'bio') $('bio-count').textContent = `${e.target.value.length} / 300`;
});
async function updateProfile(patch) {
  const { error } = await sb.from('profiles').update(patch).eq('id', S.user.id);
  if (error) return false;
  S.profile = { ...S.profile, ...patch };
  if (P.profile?.id === S.user.id) P.profile = { ...P.profile, ...patch };
  M.q = null; // la liste des membres sera rechargée
  return true;
}
// Photo de profil : recadrée en carré 320 px, envoyée dans avatars/<id>/, l'ancienne est supprimée.
async function uploadAvatar(file) {
  toast('Envoi de la photo…');
  try {
    const src = await fileToCanvas(file, 1024), side = Math.min(src.width, src.height);
    const c = document.createElement('canvas'); c.width = c.height = Math.min(320, side);
    c.getContext('2d').drawImage(src, (src.width - side) / 2, (src.height - side) / 2, side, side, 0, 0, c.width, c.height);
    const path = `${S.user.id}/${Date.now()}.jpg`, old = S.profile.avatar_path;
    const up = await sb.storage.from('avatars').upload(path, await canvasToBlob(c), { contentType: 'image/jpeg', cacheControl: '31536000' });
    if (up.error) throw up.error;
    if (!(await updateProfile({ avatar_path: path }))) { await sb.storage.from('avatars').remove([path]); throw new Error('profil'); }
    if (old) sb.storage.from('avatars').remove([old]);
    toast('Photo de profil mise à jour ✓'); render();
  } catch { toast('Envoi impossible, réessaie avec une autre photo.'); }
}
document.addEventListener('submit', async (e) => {
  const form = e.target.closest('[data-form]'); if (!form) return;
  e.preventDefault();
  const btn = form.querySelector('button'); btn.disabled = true;
  try {
    const kind = form.dataset.form;
    if (kind === 'login') {
      const { error } = await sb.auth.signInWithPassword({ email: $('le').value.trim(), password: $('lp').value });
      if (error) toast(authMessage(error));
    } else if (kind === 'signup') {
      const username = $('su').value.trim();
      if (!(await checkUsername($('su')))) return;
      const { data, error } = await sb.auth.signUp({ email: $('se').value.trim(), password: $('sp').value, options: { data: { username }, emailRedirectTo: redirectURL() } });
      if (error) { toast(authMessage(error)); if (/database error/i.test(error.message)) checkUsername($('su')); }
      // E-mail déjà utilisé : Supabase renvoie un compte sans identité (un seul compte par adresse).
      else if (data.user && !data.user.identities?.length) toast('Un compte existe déjà avec cet e-mail. Connecte-toi ou utilise « Mot de passe oublié ».');
      else if (!data.session) $('account-content').innerHTML = '<div class="notice">Compte créé ! Ouvre le lien reçu par e-mail pour l’activer, puis reviens te connecter.</div>';
    } else if (kind === 'username') {
      const v = $('un').value.trim();
      if (!(await checkUsername($('un')))) return;
      if (v === S.profile?.username) return;
      const { data, error } = await sb.from('profiles').update({ username: v }).eq('id', S.user.id).select(PROFILE_FIELDS).single();
      if (error) { toast(error.code === '23505' ? 'Ce pseudo est déjà utilisé.' : /username_cooldown/.test(error.message) ? `Un seul changement de pseudo tous les ${USERNAME_COOLDOWN_DAYS} jours.` : 'Pseudo invalide.'); return; }
      const first = !S.profile?.username;
      S.profile = data; M.q = null; P.username = null; toast('Pseudo enregistré ✓');
      if (first) route(); else render();
    } else if (kind === 'bio') {
      const bio = $('bio').value.trim().slice(0, 300) || null;
      if (await updateProfile({ bio })) { toast('Biographie enregistrée ✓'); render(); } else toast('Enregistrement impossible, réessaie.');
    } else if (kind === 'new-password') {
      const { error } = await sb.auth.updateUser({ password: $('np').value });
      if (error) toast(authMessage(error)); else { S.recovery = false; toast('Mot de passe modifié ✓'); render(); }
    }
  } finally { btn.disabled = false; }
});

// ---------------------------------------------------------------- actions (délégation : pas de JS inline)
document.addEventListener('click', async (e) => {
  const brandChip = e.target.closest('[data-brand]');
  if (brandChip) { S.filters.brand = brandChip.dataset.brand; renderCatalog(); return; }
  const toggle = e.target.closest('[data-toggle]');
  if (toggle) { S.filters[toggle.dataset.toggle] = !S.filters[toggle.dataset.toggle]; renderCatalog(); return; }
  const el = e.target.closest('[data-action]'); if (!el) return;
  const id = el.dataset.id;
  switch (el.dataset.action) {
    case 'toggle-filters': $('filters-panel').hidden = !$('filters-panel').hidden; break;
    case 'reset-filters': S.filters = { ...defaultFilters(), q: $('q').value }; renderCatalog(); break;
    case 'back': history.length > 1 ? history.back() : (location.hash = '#/catalogue'); break;
    case 'add': if (await saveItem(id, { quantity: 1 })) toast('Ajoutée à ta collection ✓'); break;
    case 'qty': {
      const q = (S.collection.get(id)?.quantity ?? 0) + Number(el.dataset.delta);
      if (q <= 0) removeItem(id); else saveItem(id, { quantity: q });
      break;
    }
    case 'rate': {
      const v = Number(el.dataset.value);
      saveItem(id, { rating: S.collection.get(id)?.rating === v ? null : v });
      break;
    }
    case 'save-note': if (await saveItem(id, { note: $('note-' + id).value.trim() || null })) toast('Note enregistrée ✓'); break;
    case 'remove': removeItem(id); break;
    case 'tab': S.authTab = el.dataset.tab; renderAccount(); break;
    case 'oauth': {
      // Google / Apple pas encore configurés : boutons affichés mais inactifs.
      // Pour les activer : passer OAUTH_ENABLED à true (après configuration dans Supabase, voir README).
      if (!OAUTH_ENABLED) { toast(`Connexion avec ${el.dataset.provider === 'apple' ? 'Apple' : 'Google'} bientôt disponible`); break; }
      const { error } = await sb.auth.signInWithOAuth({ provider: el.dataset.provider, options: { redirectTo: redirectURL() } });
      if (error) toast('Connexion impossible avec ce service pour le moment.');
      break;
    }
    case 'forgot': {
      const email = $('le')?.value.trim();
      if (!email) { toast('Indique ton e-mail ci-dessus.'); break; }
      const { error } = await sb.auth.resetPasswordForEmail(email, { redirectTo: redirectURL() });
      toast(error ? authMessage(error) : 'Si un compte existe, un e-mail de réinitialisation t’a été envoyé.');
      break;
    }
    case 'logout': await sb.auth.signOut(); toast('Déconnecté'); break;
    case 'delete-account': {
      if (!confirm('Supprimer définitivement ton compte, ton profil, ta photo et ta collection ? Cette action est irréversible.')) break;
      const files = (await sb.storage.from('avatars').list(S.user.id)).data ?? [];
      if (files.length) await sb.storage.from('avatars').remove(files.map((f) => `${S.user.id}/${f.name}`));
      const { error } = await sb.rpc('delete_my_account');
      if (error) { toast('Suppression impossible, réessaie.'); break; }
      await sb.auth.signOut(); toast('Compte supprimé'); location.hash = '#/';
      break;
    }
    case 'remove-avatar': {
      const old = S.profile.avatar_path;
      if (await updateProfile({ avatar_path: null })) { sb.storage.from('avatars').remove([old]); toast('Photo retirée'); render(); } else toast('Impossible pour le moment, réessaie.');
      break;
    }
    case 'capture': capture(); break;
    case 'live-camera': liveWanted = true; startCamera(); break;
    case 'retake': resetScan(); break;
  }
});

// ---------------------------------------------------------------- envoi de photos communautaires
async function fileToCanvas(file, max = 1024) {
  const url = URL.createObjectURL(file);
  try {
    const img = await new Promise((res, rej) => { const i = new Image(); i.onload = () => res(i); i.onerror = rej; i.src = url; });
    const k = Math.min(1, max / Math.max(img.naturalWidth, img.naturalHeight));
    const c = document.createElement('canvas'); c.width = Math.round(img.naturalWidth * k); c.height = Math.round(img.naturalHeight * k);
    c.getContext('2d').drawImage(img, 0, 0, c.width, c.height);
    return c;
  } finally { URL.revokeObjectURL(url); }
}
const canvasToBlob = (c) => new Promise((res) => c.toBlob(res, 'image/jpeg', 0.85));
document.addEventListener('change', async (e) => {
  const input = e.target;
  if (input.dataset && 'avatar' in input.dataset) {
    const file = input.files?.[0]; input.value = ''; if (file) uploadAvatar(file);
  } else if (input.dataset?.upload) {
    const file = input.files?.[0]; input.value = ''; if (!file) return;
    toast('Envoi de la photo…');
    try {
      const blob = await canvasToBlob(await fileToCanvas(file));
      const path = `${S.user.id}/${input.dataset.upload}-${Date.now()}.jpg`;
      const up = await sb.storage.from('submissions').upload(path, blob, { contentType: 'image/jpeg' });
      if (up.error) throw up.error;
      const ins = await sb.from('photo_submissions').insert({ product_id: input.dataset.upload, storage_path: path });
      if (ins.error) throw ins.error;
      toast('Merci ! Photo envoyée pour validation.');
    } catch { toast('Envoi impossible, réessaie.'); }
  } else if (input.id === 'scan-file' || input.id === 'scan-native') {
    const file = input.files?.[0]; input.value = ''; if (!file) return;
    analyze(await fileToCanvas(file));
  }
});

// ---------------------------------------------------------------- scanner
let stream = null, starting = null, scanRun = 0;
// Caméra : le navigateur (surtout sur iPhone) redemande l'autorisation à chaque lancement de l'app.
// On n'ouvre donc la vue en direct automatiquement que si l'autorisation est déjà accordée
// (ou si elle a été acceptée pendant cette session) ; sinon on propose l'appareil photo du
// téléphone, qui ne demande aucune autorisation.
let liveWanted = false;
async function cameraGranted() {
  try { return (await navigator.permissions?.query({ name: 'camera' }))?.state === 'granted'; } catch { return false; }
}
function showCameraChoice(show) {
  $('camera-choice').hidden = !show;
  $('scan-frame').hidden = show; $('video').hidden = show;
}
const scanStatus = (html) => { $('scan-status').innerHTML = html; };
async function startCamera() {
  if (stream || starting) return starting;
  if (!navigator.mediaDevices?.getUserMedia) { scanStatus('Caméra indisponible sur cet appareil : utilise le bouton 🖼 pour choisir une photo.'); return; }
  starting = (async () => {
    try {
      const s = await navigator.mediaDevices.getUserMedia({ video: { facingMode: { ideal: 'environment' }, width: { ideal: 1920 }, height: { ideal: 1080 } }, audio: false });
      if (S.screen !== 'scan' || !$('photo-preview').hidden) { s.getTracks().forEach((t) => t.stop()); return; } // écran quitté entre-temps
      stream = s; liveWanted = true; showCameraChoice(false); $('video').srcObject = s; await $('video').play().catch(() => {});
    } catch { showCameraChoice(true); scanStatus('Caméra en direct non autorisée : utilise « Prendre une photo » ou le bouton 🖼.'); }
  })().finally(() => { starting = null; });
  return starting;
}
function stopCamera() { stream?.getTracks().forEach((t) => t.stop()); stream = null; $('video').srcObject = null; }
function resetScan() {
  scanRun++;
  $('photo-preview').hidden = true; $('photo-preview').removeAttribute('src');
  $('video').hidden = false; $('scan-frame').hidden = false; $('scan-results').innerHTML = '';
  scanStatus('Place la capsule dans le cadre, nom visible, sans flash.');
  if (S.screen !== 'scan') return;
  showCameraChoice(!liveWanted);
  if (liveWanted) startCamera();
  else cameraGranted().then((ok) => { if (ok && S.screen === 'scan' && $('photo-preview').hidden) startCamera(); });
}
function capture() {
  const v = $('video');
  if (!stream) { $('scan-native').click(); return; } // pas de vue en direct : appareil photo du téléphone
  if (!v.videoWidth) { scanStatus('Caméra pas encore prête : patiente une seconde ou utilise le bouton 🖼.'); return; }
  const side = Math.min(v.videoWidth, v.videoHeight) * 0.76; // zone du cadre
  const sx = (v.videoWidth - side) / 2, sy = (v.videoHeight - side) / 2, out = Math.min(1024, side);
  const c = document.createElement('canvas'); c.width = c.height = Math.round(out);
  c.getContext('2d').drawImage(v, sx, sy, side, side, 0, 0, c.width, c.height);
  analyze(c);
}
async function analyze(canvas) {
  const run = ++scanRun;
  stopCamera();
  const preview = $('photo-preview'); preview.src = canvas.toDataURL('image/jpeg', 0.85); preview.hidden = false;
  $('video').hidden = true; $('scan-frame').hidden = true; $('camera-choice').hidden = true; $('scan-results').innerHTML = '';
  if (!S.loaded) { scanStatus('Le catalogue n’est pas encore chargé, réessaie dans un instant.'); return; }
  scanStatus('Analyse de la photo sur ton téléphone…<div class="progress"><div></div></div>');
  let data;
  try {
    data = await identify(canvas, S.products, brandName, (pct) => {
      if (run === scanRun) scanStatus(`Premier scan : téléchargement de l’IA (environ 90 Mo, une seule fois)… ${pct} %<div class="progress"><div></div></div>`);
    });
  } catch (err) {
    console.error('Baristina scan :', err);
    if (run === scanRun) scanStatus('L’IA n’a pas pu être chargée (connexion ?). Réessaie, ou cherche la capsule par son nom ci-dessous.');
    return;
  }
  if (run !== scanRun) return; // une autre photo a été prise entre-temps
  const found = data.candidates.map((c) => ({ ...c, p: S.byId.get(c.id) })).filter((c) => c.p);
  if (!found.length) { scanStatus('Capsule non reconnue. Essaie avec le nom bien visible, ou cherche-la par son nom ci-dessous.'); return; }
  const sure = found[0].confidence === 'haute';
  scanStatus(sure ? `Capsule identifiée : <b>${esc(found[0].p.name)}</b> ✓` : 'Vérifie la capsule parmi ces propositions :');
  const label = { haute: 'Confiance élevée', moyenne: 'Confiance moyenne', basse: 'Confiance faible' };
  $('scan-results').innerHTML = found.map((c) => cardHTML(c.p).replace('</strong>', `</strong><span class="confidence">${label[c.confidence] ?? ''}</span>`)).join('');
}
$('scan-manual').addEventListener('input', (e) => {
  const words = norm(e.target.value).split(' ').filter(Boolean);
  if (!words.length || !S.loaded) { $('scan-manual-results').innerHTML = ''; return; }
  const hits = S.products.filter((p) => { const h = norm(p.name + ' ' + brandName(p)); return words.every((w) => h.includes(w)); }).slice(0, 8);
  $('scan-manual-results').innerHTML = hits.length ? hits.map(cardHTML).join('') : '<div class="empty">Aucune capsule trouvée.</div>';
});

// ---------------------------------------------------------------- démarrage
async function onSession(session) {
  const uid = session?.user?.id ?? null;
  if (uid === (S.user?.id ?? null) && S.profileLoaded) return;
  S.user = session?.user ?? null;
  await loadUserData(); S.profileLoaded = true;
  if (S.user && !S.profile?.username && S.screen !== 'account') location.hash = '#/compte'; // pseudo obligatoire
  if (location.hash.startsWith('#/compte')) route(); else render(); // « Compte » : profil ou connexion selon la session
  if (S.screen === 'scan' && $('photo-preview').hidden) resetScan();
}
async function start() {
  route();
  if (!CONFIGURED) return;
  sb.auth.onAuthStateChange((event, session) => {
    if (event === 'PASSWORD_RECOVERY') { S.recovery = true; location.hash = '#/compte'; }
    setTimeout(() => onSession(session), 0); // ne pas appeler Supabase dans le rappel lui-même
  });
  await loadCatalog();
  render();
}
start();
