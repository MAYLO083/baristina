// Baristina V1 — application (catalogue, fiches, scanner, collection, compte).
// Toutes les données passent par Supabase : rien n'est stocké sur l'appareil,
// à part la session de connexion gérée par Supabase et le choix d'affichage des listes.
import { createClient } from 'https://cdn.jsdelivr.net/npm/@supabase/supabase-js@2.45.4/+esm';
import { SUPABASE_URL, SUPABASE_ANON_KEY } from './config.js';
import { ANECDOTES } from './anecdotes.js';
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
const defaultFilters = () => ({ q: '', system: 'all', brand: 'all', range: 'all', min: 1, max: 13, sort: 'name',
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
const SYSTEM_NAME = { original: 'Nespresso Original', vertuo: 'Nespresso Vertuo' };
const ROAST = { blonde: 'blonde (légère)', medium: 'ambrée (moyenne)', dark: 'brune (foncée)' };
function intensityWord(i) { return i <= 4 ? 'doux' : i <= 7 ? 'équilibré' : i <= 10 ? 'intense' : 'très intense'; }
function describe(p) {
  const out = [];
  const range = p.range && !/^capsules/i.test(p.range) ? ` de la gamme ${p.range}` : '';
  out.push(`${p.name} est une capsule ${brandName(p)}${range}, au format ${SYSTEM_NAME[p.system] ?? SYSTEM_NAME.original}.`);
  if (p.system === 'vertuo') out.push('Elle ne fonctionne qu’avec les machines Nespresso Vertuo.');
  if (p.intensity) out.push(p.intensity_estimated
    ? `La marque ne publie pas d’intensité sur 13 : nous l’estimons à ${p.intensity}, c’est un café ${intensityWord(p.intensity)}.`
    : `Son intensité est de ${p.intensity} sur 13 : c’est un café ${intensityWord(p.intensity)}.`);
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

// ---------------------------------------------------------------- jauge d'intensité (sur 13)
function intensityRing(p, cls = '') {
  if (!p?.intensity) return '';
  const est = p.intensity_estimated ? ' estimée' : '';
  return `<span class="int-ring ${cls}${est ? ' est' : ''}" style="--p:${(p.intensity / 13 * 100).toFixed(1)}%" title="Intensité${est} ${p.intensity} sur 13" aria-label="Intensité${est} ${p.intensity} sur 13"><b>${est ? '≈' : ''}${p.intensity}</b><small>/13</small></span>`;
}

// ---------------------------------------------------------------- calque : pop-up et menus
let overlayTimer;
function openOverlay(html, kind) {
  const o = $('overlay');
  clearTimeout(overlayTimer);
  o.className = 'overlay ' + kind; o.innerHTML = `<div class="overlay-bg" data-action="close-overlay"></div>${html}`;
  o.hidden = false; document.body.classList.add('locked');
  requestAnimationFrame(() => o.classList.add('open'));
}
function closeOverlay() {
  const o = $('overlay'); if (o.hidden) return;
  o.classList.remove('open'); o.classList.add('closing');
  document.body.classList.remove('locked');
  overlayTimer = setTimeout(() => { o.hidden = true; o.innerHTML = ''; o.className = 'overlay'; }, 260);
}
document.addEventListener('keydown', (e) => { if (e.key === 'Escape') closeOverlay(); });

// ---------------------------------------------------------------- suggestion après un « j'aime »
// Même marque, même intensité sur 13 (à défaut, la plus proche), pas encore dans la collection.
function suggestionsFor(p) {
  const pool = S.products.filter((x) => x.id !== p.id && x.brand_id === p.brand_id && !S.collection.has(x.id));
  const sim = (x) => (x.system === p.system ? 8 : 0) + (x.range && x.range === p.range ? 3 : 0) + (x.decaf === p.decaf ? 6 : 0)
    + (x.aromas ?? []).filter((a) => p.aromas?.includes(a)).length * 2 + (x.photo_url ? 1 : 0);
  const sort = (list) => list.map((x) => [x, sim(x) + Math.random()]).sort((a, b) => b[1] - a[1]).map(([x]) => x);
  if (!p.intensity) return { list: sort(pool.filter((x) => !x.intensity || x.range === p.range)).slice(0, 6), exact: false };
  const rated = pool.filter((x) => x.intensity);
  const exact = rated.filter((x) => x.intensity === p.intensity);
  if (exact.length) return { list: sort(exact).slice(0, 6), exact: true };
  const best = Math.min(...rated.map((x) => Math.abs(x.intensity - p.intensity)));
  if (!rated.length || best > 2) return { list: [], exact: false };
  return { list: sort(rated.filter((x) => Math.abs(x.intensity - p.intensity) === best)).slice(0, 6), exact: false };
}
const SUG = { from: null, list: [], exact: false, i: 0 };
function openSuggestion(fromId) {
  const p = S.byId.get(fromId); if (!p) return false;
  const { list, exact } = suggestionsFor(p);
  if (!list.length) return false;
  Object.assign(SUG, { from: p, list, exact, i: 0 });
  openOverlay(`<div class="pop" role="dialog" aria-modal="true" aria-labelledby="pop-title">${suggestionHTML()}</div>`, 'center');
  return true;
}
function suggestionHTML() {
  const p = SUG.from, x = SUG.list[SUG.i];
  const why = !p.intensity ? `Même marque, dans le même esprit que ${esc(p.name)}`
    : SUG.exact ? `Même marque, même intensité : <b>${p.intensity}/13</b>`
    : `Même marque, intensité la plus proche : <b>${x.intensity}/13</b> (au lieu de ${p.intensity})`;
  const hearts = Array.from({ length: 10 }, (_, k) => `<i style="--k:${k}">♥</i>`).join('');
  const sys = x.system !== p.system ? `<span class="badge">${esc(SYSTEM_NAME[x.system])}</span>` : '';
  return `<div class="burst" aria-hidden="true">${hearts}</div>
    <button class="pop-close" data-action="close-overlay" aria-label="Fermer">×</button>
    <div class="pop-liked"><span class="heart">♥</span> ${esc(p.name)} est dans ta collection</div>
    <h3 id="pop-title">Tu devrais aussi aimer…</h3>
    <p class="pop-why">${why}</p>
    <div class="pop-card" data-k="${SUG.i}">
      <div class="pop-visual">${capsuleHTML(x, true).replace(/<p class="photo-credit">.*?<\/p>/, '')}${intensityRing(x, 'big')}</div>
      <div class="pop-brand">${esc(brandName(x))}</div>
      <div class="pop-name">${esc(x.name)}</div>
      ${x.range ? `<div class="pop-range">${esc(x.range)}</div>` : ''}
      ${x.aromas?.length ? `<div class="pop-aromas">${x.aromas.slice(0, 3).map((a) => `<span>${esc(a)}</span>`).join('')}</div>` : ''}
      ${sys}
    </div>
    <div class="pop-actions">
      <a class="btn" href="#/capsule/${encodeURIComponent(x.id)}" data-action="close-overlay">Découvrir</a>
      <button class="btn secondary" data-action="sug-add" data-id="${esc(x.id)}">♡ L’ajouter aussi</button>
    </div>
    ${SUG.list.length > 1 ? `<button class="link-btn" data-action="sug-next">Une autre idée (${SUG.i + 1}/${SUG.list.length}) ↻</button>` : ''}`;
}

// ---------------------------------------------------------------- humeurs : « pas d'idée ? »
// Chaque humeur note les capsules à partir des faits enregistrés (intensité, arômes, tasses, gamme…).
const txt = (p) => norm([...(p.aromas ?? []), p.highlight, p.name].join(' '));
const has = (p, re) => re.test(txt(p));
const cups = (p, re) => (p.cup_sizes ?? []).some((c) => re.test(c));
const ITALIAN = new Set(['lavazza', 'kimbo', 'borbone', 'segafredo', 'illy']);
const MOODS = [
  { id: 'douceur', emoji: '☁️', name: 'Tout en douceur', line: 'Rond, léger, sans amertume. Pour bien démarrer la journée.', c: ['#b08460', '#d4ae86'],
    score: (p) => (p.intensity >= 9 ? -9 : 0) + (p.intensity && p.intensity <= 6 ? 2 : 0) + (has(p, /doux|rond|velout|delicat|biscuit|cereales douces|lacte|cremeux|miel|leger|torrefaction legere/) ? 1 : 0) + (p.roast === 'blonde' ? 1 : 0) },
  { id: 'energie', emoji: '⚡', name: 'Coup de boost', line: 'Corsé, intense, plein de caractère. Pour les réveils difficiles.', c: ['#76202a', '#c4513f'],
    score: (p) => (p.decaf ? -9 : 0) + (p.intensity >= 10 ? 2 : 0) + (p.intensity >= 12 ? 1 : 0) + (has(p, /intense|corse|robusta|robuste|ginseng|intensement|torrefaction intense|caf(e|ei)ne/) ? 1 : 0) + (/coffee\+|double espresso/i.test(p.range ?? '') ? 1 : 0) + (p.roast === 'dark' ? 1 : 0) },
  { id: 'gourmand', emoji: '🍫', name: 'Gourmandise', line: 'Caramel, chocolat, vanille. Pour céder à la tentation.', c: ['#8a5a3b', '#d9a066'],
    score: (p) => (p.flavoured ? 2 : 0) + (has(p, /caramel|chocolat|vanill|noisette|biscuit|brownie|cheesecake|miel|amande|cacao|praline/) ? 1 : 0) },
  { id: 'cocooning', emoji: '🧣', name: 'Cocooning', line: 'Chaud, enveloppant, réconfortant. Pour les jours de pluie, sous un plaid.', c: ['#3e2a24', '#a0705a'], min: 2,
    score: (p) => (has(p, /cacao|chocolat|biscuit|cereales grillees|caramel|pain grille|malt|noisette|miel|fruits secs|cremeux|velout/) ? 1 : 0) + (cups(p, /lungo|mug|lait|cappuccino/i) ? 1 : 0) + (p.intensity >= 5 && p.intensity <= 9 ? 1 : 0) },
  { id: 'hiver', emoji: '❄️', name: 'Esprit d’hiver', line: 'Épicé, boisé, chaleureux. Pour se réchauffer quand il fait froid.', c: ['#2f4a5c', '#8fa9b8'],
    score: (p) => (has(p, /epic|epice|citrouille|pumpkin|chataigne|boise|fume|chene|cannelle|spice|hiver|winter|noel/) ? 2 : 0) + (has(p, /grille|cacao|torrefaction intense/) ? 1 : 0) + (p.intensity >= 8 ? 1 : 0) + (p.limited ? 1 : 0), min: 3 },
  { id: 'soleil', emoji: '☀️', name: 'Rayon de soleil', line: 'Fruité, floral, lumineux. Pour égayer la journée, chaud ou glacé.', c: ['#c77651', '#f0c27a'], min: 2,
    score: (p) => (/glac/i.test(p.highlight ?? '') ? 3 : 0) + (has(p, /fruit|agrume|citri|floral|fleur|jasmin|yuzu|baie|myrtille|acidit|tropical|coco|hibiscus/) ? 2 : 0) + (p.intensity && p.intensity <= 6 ? 1 : 0) },
  { id: 'evasion', emoji: '🌍', name: 'Évasion', line: 'Exotique, intense, aux notes venues de loin. Pour voyager sans quitter la maison.', c: ['#1e5b45', '#6e8a6b'],
    score: (p) => (p.origin ? 2 : 0) + (/origin|exploration/i.test(p.range ?? '') ? 2 : 0) + (has(p, /tropical|coco|yuzu|kona|hawaii|colombia|ethiopia|guatemala|sumatra|peru|brasil|india/) ? 1 : 0) },
  { id: 'dolcevita', emoji: '☕', name: 'Dolce vita', line: 'Serré, court. Pour savourer un ristretto comme en Italie.', c: ['#173a23', '#c6aa67'], min: 2,
    score: (p) => (/ital|qualita|origini|respresso|napoli|roma/i.test((p.range ?? '') + ' ' + p.name) ? 2 : 0) + (ITALIAN.has(p.brand_id) ? 2 : 0) + (cups(p, /ristretto/i) ? 1 : 0) + (p.intensity >= 9 ? 1 : 0) },
  { id: 'latte', emoji: '🥛', name: 'Pause latte', line: 'Doux, crémeux, une touche de lait. Pour un cappuccino, latte et macchiato.', c: ['#8f6f52', '#c9a983'],
    score: (p) => (cups(p, /lait|cappuccino/i) ? 2 : 0) + (/lait/i.test(p.highlight ?? '') ? 2 : 0) + (/barista/i.test(p.range ?? '') ? 1 : 0) + (/bianco|oatly|latte|barista/i.test(p.name) ? 1 : 0) },
  { id: 'soiree', emoji: '🌙', name: 'Douce soirée', line: 'Tout le goût du café, sans la caféine. Pour en profiter le soir sans troubler le sommeil.', c: ['#2b2540', '#6a2b72'],
    score: (p) => (p.decaf ? 3 : 0) + (/mi-caf/i.test(p.highlight ?? '') ? 2 : 0) },
];
const MOOD_ICONS = {
  douceur: '<path d="M7 18.5h10.5a4 4 0 0 0 .4-8A6 6 0 0 0 6.4 9.6 4.5 4.5 0 0 0 7 18.5z"/>',
  energie: '<path d="M13.5 2.5 4.5 14h7l-1 7.5 9-11.5h-7z"/>',
  gourmand: '<circle cx="12" cy="12" r="8.5"/><path d="M8.5 9.5h.01M14 8h.01M15.5 13.5h.01M10 15h.01M12 11.5h.01" stroke-width="2.6"/>',
  cocooning: '<path d="M4 10h12v4.5A5.5 5.5 0 0 1 10.5 20h-1A5.5 5.5 0 0 1 4 14.5z"/><path d="M16 11.5h1.3a2.5 2.5 0 0 1 0 5H16"/><path d="M8 3.5c-1 1.2 1 2 0 3.5M12 3.5c-1 1.2 1 2 0 3.5"/>',
  hiver: '<path d="M12 2.5v19M3.8 7.2l16.4 9.6M3.8 16.8l16.4-9.6"/><path d="m9.5 4 2.5 2 2.5-2M9.5 20l2.5-2 2.5 2M4 10.6l3.2-.5L6 7.2M20 13.4l-3.2.5 1.2 2.9M4 13.4l3.2.5L6 16.8M20 10.6l-3.2-.5 1.2-2.9"/>',
  soleil: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2.5M12 19v2.5M2.5 12H5M19 12h2.5M5.3 5.3l1.8 1.8M16.9 16.9l1.8 1.8M5.3 18.7l1.8-1.8M16.9 7.1l1.8-1.8"/>',
  evasion: '<circle cx="12" cy="12" r="9"/><path d="m15.8 8.2-2.3 5.3-5.3 2.3 2.3-5.3z"/>',
  dolcevita: '<path d="M3 20.5h18M4 20.5V8.5c3.5-2.5 12.5-2.5 16 0v12"/><path d="M4 12.5h16M4 16.5h16"/><path d="M7 20.5V19a1 1 0 0 1 2 0v1.5M11 20.5V19a1 1 0 0 1 2 0v1.5M15 20.5V19a1 1 0 0 1 2 0v1.5M7 12.5V11a1 1 0 0 1 2 0v1.5M11 12.5V11a1 1 0 0 1 2 0v1.5M15 12.5V11a1 1 0 0 1 2 0v1.5"/>',
  latte: '<path d="M5 8.5h12V13a6 6 0 0 1-12 0z"/><path d="M17 9.5h1a2.3 2.3 0 0 1 0 4.6h-1"/><path d="M11 13.8l-1.3-1.3a.9.9 0 0 1 1.3-1.2.9.9 0 0 1 1.3 1.2z"/><path d="M4 21h14"/>',
  soiree: '<path d="M19.5 14.5A8 8 0 1 1 9.5 4.5a6.3 6.3 0 0 0 10 10z"/><path d="M17 3.5v3M15.5 5h3"/>',
};
const moodIcon = (m, cls = '') => `<svg class="mood-ico ${cls}" viewBox="0 0 24 24" aria-hidden="true">${MOOD_ICONS[m.id] ?? ''}</svg>`;
// Photo d'ambiance (chemin absolu : une url() dans une variable CSS se lit depuis la feuille de style)
const moodImg = (m) => new URL(`img/mood-${m.id}.jpg`, document.baseURI).href;
const MOOD = { id: null, system: 'all' };
function moodList(m) {
  return S.products.map((p) => [p, m.score(p)]).filter(([, s]) => s >= (m.min ?? 1))
    .sort((a, b) => b[1] - a[1] || a[0].name.localeCompare(b[0].name, 'fr')).map(([p]) => p);
}
function openMenu() {
  const me = S.profile?.username;
  const links = [['#/scanner', 'Scanner une capsule'], ['#/collection', 'Ma collection'], ['#/catalogue', 'Catalogue'], ['#/membres', 'La communauté'],
    ['#/compte', me ? `Mon profil (${esc(me)})` : 'Se connecter']];
  openOverlay(`<div class="sheet menu-sheet" role="dialog" aria-modal="true" aria-label="Menu">
    <div class="sheet-grip" aria-hidden="true"></div>
    <button class="pop-close" data-action="close-overlay" aria-label="Fermer">×</button>
    ${links.map(([h, n]) => `<a class="menu-link" href="${h}">${n}<span aria-hidden="true">›</span></a>`).join('')}
    <button class="menu-link" data-action="open-moods">Une envie ? Choisis selon ton humeur<span aria-hidden="true">›</span></button>
  </div>`, 'bottom');
}
function openMoods() {
  const tiles = MOODS.map((m, i) => `<a class="mood" href="#/humeur/${m.id}" data-action="close-overlay" style="--c1:${m.c[0]};--c2:${m.c[1]};--d:${i};--img:url(${moodImg(m)})">
      <span class="mood-txt"><b>${esc(m.name)}</b><small>${esc(m.line)}</small></span></a>`).join('');
  openOverlay(`<div class="sheet" role="dialog" aria-modal="true" aria-labelledby="moods-title">
    <div class="sheet-grip" aria-hidden="true"></div>
    <button class="pop-close" data-action="close-overlay" aria-label="Fermer">×</button>
    <h3 id="moods-title">Quelle est ton humeur ?</h3>
    <p class="sheet-sub">Choisis une envie, on te trouve les capsules qui vont avec !</p>
    <div class="moods">${tiles}</div>
    <button class="mood mood-surprise" data-action="mood-random" style="--d:${MOODS.length};--img:url(${new URL('img/mood-surprise.jpg', document.baseURI).href})">
      <span class="mood-txt"><b>Surprends-moi</b><small>Une humeur tirée au hasard, laisse-toi porter.</small></span></button>
  </div>`, 'bottom');
}
// En carrousel : une rangée par marque ; sinon la liste suit l'ordre de pertinence.
function moodItemsHTML(list) {
  if (VIEW !== 'carousel') return `<div class="items">${list.map(cardHTML).join('')}</div>`;
  return [...S.brands.values()].map((b) => {
    const items = list.filter((p) => p.brand_id === b.id);
    return items.length ? `<h3 class="group-title">${esc(b.name)} <small>· ${items.length}</small></h3><div class="items">${items.map(cardHTML).join('')}</div>` : '';
  }).join('');
}
function renderMood() {
  const box = $('mood-content');
  const m = MOODS.find((x) => x.id === MOOD.id);
  if (!m) { box.innerHTML = '<div class="empty">Cette humeur n’existe pas.</div>'; return; }
  if (!S.loaded) { box.innerHTML = '<div class="loading">Chargement…</div>'; return; }
  const all = moodList(m);
  const list = all.filter((p) => MOOD.system === 'all' || p.system === MOOD.system);
  const tabs = [['all', 'Tous'], ['original', 'Original'], ['vertuo', 'Vertuo']]
    .map(([k, n]) => `<button class="${MOOD.system === k ? 'on' : ''}" data-msys="${k}">${n}</button>`).join('');
  box.innerHTML = `<div class="mood-hero" style="--c1:${m.c[0]};--c2:${m.c[1]};--img:url(${moodImg(m)})">
      ${moodIcon(m, 'big')}
      <h1>${esc(m.name)}</h1><p>${esc(m.line)}</p>
      <div class="mood-cta"><button class="pill" data-action="mood-surprise">🎲 Surprends-moi</button><button class="pill ghost" data-action="open-moods">Changer d’humeur</button></div>
    </div>
    <div class="content">
      <div class="tabs three">${tabs}</div>
      <div class="list-head"><p class="result-count">${list.length} capsule${list.length > 1 ? 's' : ''} pour cette humeur</p>${viewSwitchHTML()}</div>
      <div class="mood-list products view-${VIEW}">${list.length ? moodItemsHTML(list) : '<div class="empty">Aucune capsule de ce format pour cette humeur.</div>'}</div>
    </div>`;
}

// ---------------------------------------------------------------- notification d'accueil (à chaque lancement)
const WELCOME = [
  'Prête ou prêt pour ta première capsule de la journée ?', 'Ta collection t’attend, elle a hâte de te revoir.',
  'Ristretto, lungo… ou une nouvelle découverte aujourd’hui ?', 'Pas d’idée ? Laisse ton humeur choisir ta prochaine capsule.',
  'Scanne une capsule et découvre tout ce qu’elle cache.', 'Plus de 180 capsules t’attendent dans le catalogue.',
  'Le café est prêt quand tu l’es.', 'Envie de douceur ou besoin d’un coup de boost ?',
  'Une capsule goûtée, c’est une note de dégustation à partager.', 'Et si tu testais une intensité que tu n’as jamais essayée ?',
  'Les membres partagent leurs coups de cœur : va jeter un œil !', 'Qui sera la star de ta collection aujourd’hui ?',
  'Un bon café, et la journée commence déjà mieux.', 'Ta machine chauffe, ton catalogue aussi.',
  'Original ou Vertuo ? Il y en a pour toutes les machines.', 'Note tes capsules préférées pour ne jamais les oublier.',
  'Petite pause café ? On t’a gardé une place au comptoir.', 'Arabica, robusta, aromatisé… qu’est-ce qui te tente ?',
  'Chaque capsule a son histoire, à toi de la découvrir.', 'L’odeur du café n’attend plus que toi.',
  'Une tasse, un instant, rien que pour toi.', 'Ta prochaine capsule préférée est peut-être à un scan près.',
  'Colombie, Éthiopie, Inde… fais le tour du monde en quelques capsules.', 'Aujourd’hui, on ose une édition limitée ?',
  'Partage tes notes de dégustation avec la communauté Baristina.', 'Plutôt mousse de lait ou espresso bien serré aujourd’hui ?',
  'Un café, ça se savoure… et ça se collectionne.', 'Chaque capsule ajoutée fait grandir ta collection, continue !',
  'Le meilleur moment de la journée ? Celui de la capsule.', 'Bienvenue au comptoir Baristina, installe-toi.',
];
const HOME_MSG = WELCOME[Math.floor(Math.random() * WELCOME.length)];

// ---------------------------------------------------------------- anecdote du jour (accueil)
// Une anecdote au hasard à chaque lancement ; toucher la carte en tire une autre (jamais la même).
let factIdx = -1;
function showFact(animate) {
  let i; do { i = Math.floor(Math.random() * ANECDOTES.length); } while (i === factIdx && ANECDOTES.length > 1);
  factIdx = i;
  const card = $('trivia');
  const fill = () => {
    $('trivia-num').textContent = `n° ${i + 1}`;
    // mot par mot, pour une apparition en cascade
    $('trivia-text').innerHTML = ANECDOTES[i].split(' ').map((w, k) => `<span style="--k:${k}">${esc(w)}</span>`).join(' ');
  };
  if (!animate) { fill(); return; }
  card.classList.remove('flip'); void card.offsetWidth; card.classList.add('flip');
  setTimeout(fill, 260);
}
// ---------------------------------------------------------------- affichage des listes : 5 formes
// Le choix est mémorisé sur l'appareil (simple préférence d'affichage, aucune donnée personnelle).
const VIEWS = [
  ['list', 'Liste', '<path d="M4 6h16M4 12h16M4 18h16"/>'],
  ['compact', 'Compacte', '<path d="M4 5h16M4 9.5h16M4 14h16M4 18.5h16"/>'],
  ['grid', 'Grille', '<rect x="4" y="4" width="7" height="7" rx="1.5"/><rect x="13" y="4" width="7" height="7" rx="1.5"/><rect x="4" y="13" width="7" height="7" rx="1.5"/><rect x="13" y="13" width="7" height="7" rx="1.5"/>'],
  ['showcase', 'Vitrine', '<rect x="4" y="3.5" width="16" height="12" rx="2"/><path d="M4 19h16"/>'],
  ['carousel', 'Carrousel', '<rect x="7" y="5" width="10" height="14" rx="2"/><path d="M3 7v10M21 7v10"/>'],
];
let VIEW = (() => { try { const v = localStorage.getItem('baristina-view'); return VIEWS.some(([k]) => k === v) ? v : 'list'; } catch { return 'list'; } })();
function viewSwitchHTML() {
  const i = VIEWS.findIndex(([k]) => k === VIEW);
  return `<div class="view-switch" style="--i:${i}" role="radiogroup" aria-label="Affichage"><span class="vs-ind" aria-hidden="true"></span>${VIEWS.map(([k, n, svg]) =>
    `<button class="${k === VIEW ? 'on' : ''}" data-view="${k}" role="radio" aria-checked="${k === VIEW}" aria-label="Affichage ${n}" title="${n}"><svg viewBox="0 0 24 24">${svg}</svg></button>`).join('')}</div>`;
}
const viewName = () => VIEWS.find(([k]) => k === VIEW)[1];
function setView(v) {
  if (v === VIEW) return;
  VIEW = v; try { localStorage.setItem('baristina-view', v); } catch {}
  const i = VIEWS.findIndex(([k]) => k === v);
  // l'indicateur glisse d'abord, puis les cartes se réorganisent
  document.querySelectorAll('.view-switch').forEach((el) => {
    el.style.setProperty('--i', i);
    el.querySelectorAll('[data-view]').forEach((b) => { b.classList.toggle('on', b.dataset.view === v); b.setAttribute('aria-checked', b.dataset.view === v); });
  });
  const update = () => { document.body.classList.add('entering'); clearTimeout(show.t); show.t = setTimeout(() => document.body.classList.remove('entering'), 900); render(); };
  setTimeout(() => (document.startViewTransition && !matchMedia('(prefers-reduced-motion: reduce)').matches ? document.startViewTransition(update) : update()), 120);
  toast(`Affichage ${viewName()}`);
}

// ---------------------------------------------------------------- navigation
const ROUTES = { '': 'home', catalogue: 'catalog', scanner: 'scan', collection: 'collection', compte: 'account', membres: 'members' };
const SCREENS = { home: 'screen-home', catalog: 'screen-catalog', detail: 'screen-detail', scan: 'screen-scan', collection: 'screen-collection', account: 'screen-account', members: 'screen-members', profile: 'screen-profile', mood: 'screen-mood' };
let detailId = null;
function route() {
  const [a, b] = location.hash.replace(/^#\/?/, '').split('/');
  if (a === 'capsule' && b) { detailId = decodeURIComponent(b); R.delete(detailId); show('detail'); return; } // avis rechargés à chaque ouverture
  if (a === 'humeur' && b) { if (MOOD.id !== b) MOOD.system = 'all'; MOOD.id = b; show('mood'); return; }
  if (a === 'profil' && b) { openProfile(decodeURIComponent(b)); show('profile'); return; }
  // « Compte » : mon profil public (modification via #/compte/modifier) ; sinon connexion / choix du pseudo.
  if (a === 'compte') {
    if (b !== 'modifier' && S.user && S.profile?.username && !S.recovery) { openProfile(S.profile.username); show('profile'); return; }
    show('account'); return;
  }
  show(ROUTES[a] ?? 'home');
}
function show(name) {
  closeOverlay();
  if (S.screen === 'catalog') S.catalogScroll = window.scrollY;
  if (S.screen === 'scan' && name !== 'scan') stopCamera();
  S.screen = name;
  for (const [k, id] of Object.entries(SCREENS)) $(id).classList.toggle('active', k === name);
  document.body.classList.add('entering'); clearTimeout(show.t); show.t = setTimeout(() => document.body.classList.remove('entering'), 900);
  document.body.classList.toggle('on-home', name === 'home');
  const navKey = location.hash.startsWith('#/compte') ? 'account' : name;
  document.querySelectorAll('.nav a').forEach((a) => a.classList.toggle('active', a.dataset.nav === navKey));
  render();
  window.scrollTo(0, name === 'catalog' ? S.catalogScroll : 0);
  if (name === 'scan') resetScan();
}
function render() {
  // Bulle de profil en haut à droite de l'accueil : ma photo (ou initiale), sinon une silhouette (connexion).
  // Message d'accueil : tiré au hasard une fois par lancement, avec le pseudo si connecté.
  const h = new Date().getHours(), name = S.profile?.username;
  $('hn-title').textContent = `${h >= 18 || h < 5 ? 'Bonsoir' : 'Bonjour'}${name ? ' ' + name : ''} ☕`;
  $('hn-msg').textContent = HOME_MSG;
  // Icône de la notification : la photo de profil si connecté, sinon la dosette dessinée.
  const ic = document.querySelector('.hn-icon');
  ic.dataset.caps ??= ic.innerHTML;
  const key = name ? name + '|' + (avatarURL(S.profile) ?? '') : '';
  if (ic.dataset.key !== key) {
    ic.dataset.key = key;
    ic.classList.toggle('is-avatar', !!name);
    ic.innerHTML = name ? avatarHTML(S.profile) : ic.dataset.caps;
  }
  ({ catalog: renderCatalog, detail: renderDetail, collection: renderCollection, account: renderAccount, members: renderMembers, profile: renderProfile, mood: renderMood })[S.screen]?.();
}
window.addEventListener('hashchange', route);

// ---------------------------------------------------------------- catalogue
function filtered() {
  const f = S.filters, words = norm(f.q).split(' ').filter(Boolean);
  const out = S.products.filter((p) => {
    if (f.system !== 'all' && p.system !== f.system) return false;
    if (f.brand !== 'all' && p.brand_id !== f.brand) return false;
    if (f.range !== 'all' && p.range !== f.range) return false;
    if ((f.min > 1 || f.max < 13) && (!p.intensity || p.intensity < f.min || p.intensity > f.max)) return false;
    for (const k of ['decaf', 'flavoured', 'organic', 'limited']) if (f[k] && !p[k]) return false;
    if (f.owned && !S.collection.has(p.id)) return false;
    if (!words.length) return true;
    const hay = norm([p.name, brandName(p), p.range, p.origin, p.highlight, p.system, ...(p.aromas ?? [])].join(' '));
    return words.every((w) => hay.includes(w));
  });
  const byName = (a, b) => a.name.localeCompare(b.name, 'fr', { sensitivity: 'base' });
  if (f.sort === 'name') out.sort(byName);
  else out.sort((a, b) => ((f.sort === 'intensity-desc' ? -1 : 1) * ((a.intensity ?? (f.sort === 'intensity-desc' ? -1 : 99)) - (b.intensity ?? (f.sort === 'intensity-desc' ? -1 : 99)))) || byName(a, b));
  return out;
}
function cardHTML(p) {
  const item = S.collection.get(p.id);
  const tags = [p.system === 'vertuo' ? 'Vertuo' : null, p.decaf ? 'Déca' : null, p.flavoured ? 'Aromatisé' : null, p.organic ? 'Bio' : null, p.limited ? 'Édition limitée' : null].filter(Boolean);
  const aro = p.aromas?.length ? `<span class="aro">${p.aromas.slice(0, 3).map((a) => `<i>${esc(a)}</i>`).join('')}</span>` : '';
  return `<a class="card" style="--cap:${capColor(p)}" href="#/capsule/${encodeURIComponent(p.id)}">${capsuleHTML(p)}<div class="meta"><strong>${esc(p.name)}</strong><span class="sub">${esc(brandName(p))}${tags.length ? ' · ' + esc(tags.join(' · ')) : ''}</span>${aro}</div>${intensityRing(p)}${item ? `<span class="qty">×${item.quantity}</span>` : ''}</a>`;
}
function renderFilterControls() {
  const f = S.filters;
  $('brand-chips').innerHTML = [['all', 'Toutes les marques'], ...[...S.brands.values()].map((b) => [b.id, b.name])]
    .map(([id, n]) => `<button class="chip${f.brand === id ? ' on' : ''}" data-brand="${esc(id)}">${esc(n)}</button>`).join('');
  document.querySelectorAll('[data-system]').forEach((b) => b.classList.toggle('on', b.dataset.system === f.system));
  const ranges = [...new Set(S.products.filter((p) => (f.brand === 'all' || p.brand_id === f.brand) && (f.system === 'all' || p.system === f.system)).map((p) => p.range).filter(Boolean))].sort((a, b) => a.localeCompare(b, 'fr'));
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
  $('view-switch-cat').innerHTML = viewSwitchHTML();
  $('catalog-list').className = 'products view-' + VIEW;
  if (!list.length) { $('catalog-list').innerHTML = '<div class="empty">Aucune capsule ne correspond à ces critères.</div>'; return; }
  let html = '';
  for (const brand of S.brands.values()) {
    const items = list.filter((p) => p.brand_id === brand.id);
    if (!items.length) continue;
    html += `<h3 class="group-title">${esc(brand.name)} <small>· ${items.length}</small></h3>`;
    if (S.filters.sort === 'name') {
      // Groupes par gamme ; chez Nespresso, les gammes Vertuo sont repérées (« Vertuo · Barista Creations »).
      const key = (p) => (p.system === 'vertuo' ? 'Vertuo · ' : '') + (p.range ?? 'Autres');
      const ranges = [...new Set(items.map(key))].sort((a, b) => a.startsWith('Vertuo') - b.startsWith('Vertuo') || a.localeCompare(b, 'fr'));
      for (const r of ranges) {
        if (ranges.length > 1) html += `<div class="range-title">${esc(r)}</div>`;
        html += `<div class="items">${items.filter((p) => key(p) === r).map(cardHTML).join('')}</div>`;
      }
    } else html += `<div class="items">${items.map(cardHTML).join('')}</div>`;
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
  if (p.intensity) for (let i = 1; i <= 13; i++) dots += `<span class="dot${i > p.intensity ? ' off' : ''}" style="--i:${i}"></span>`;
  const badges = [`<span class="badge">${esc(SYSTEM_NAME[p.system] ?? SYSTEM_NAME.original)}</span>`, p.intensity ? `<span class="badge purple">Intensité ${p.intensity_estimated ? '≈ ' : ''}${p.intensity}/13${p.intensity_estimated ? ' (estimée)' : ''}</span>` : '',
    p.decaf ? '<span class="badge">Décaféiné</span>' : '', p.flavoured ? '<span class="badge">Aromatisé</span>' : '',
    p.organic ? '<span class="badge">Bio</span>' : '', p.limited ? '<span class="badge">Édition limitée</span>' : ''].join('');
  const facts = [
    ['Marque', brandName(p)], ['Format', SYSTEM_NAME[p.system] ?? SYSTEM_NAME.original], ['Gamme', p.range], ['Intensité', p.intensity ? `${p.intensity} / 13${p.intensity_estimated ? ' (estimée)' : ''}` : null],
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
  const vbtn = e.target.closest('[data-view]');
  if (vbtn) { setView(vbtn.dataset.view); return; }
  const msys = e.target.closest('[data-msys]');
  if (msys) { MOOD.system = msys.dataset.msys; renderMood(); return; }
  const sysTab = e.target.closest('[data-system]');
  if (sysTab) { S.filters.system = sysTab.dataset.system; renderCatalog(); return; }
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
    case 'add': if (await saveItem(id, { quantity: 1 }) && !openSuggestion(id)) toast('Ajoutée à ta collection ✓'); break;
    case 'close-overlay': closeOverlay(); break;
    case 'sug-next': {
      SUG.i = (SUG.i + 1) % SUG.list.length;
      const pop = document.querySelector('#overlay .pop'); if (pop) pop.innerHTML = suggestionHTML();
      break;
    }
    case 'sug-add': {
      if (!(await saveItem(id, { quantity: 1 }))) break;
      if (!openSuggestion(id)) { closeOverlay(); toast('Ajoutée à ta collection ✓'); }
      break;
    }
    case 'open-moods': openMoods(); break;
    case 'open-menu': openMenu(); break;
    case 'next-fact': showFact(true); break;
    case 'mood-random': { const m = MOODS[Math.floor(Math.random() * MOODS.length)]; location.hash = '#/humeur/' + m.id; break; }
    case 'mood-surprise': {
      const m = MOODS.find((x) => x.id === MOOD.id); if (!m) break;
      const list = moodList(m).filter((p) => MOOD.system === 'all' || p.system === MOOD.system);
      if (list.length) location.hash = '#/capsule/' + encodeURIComponent(list[Math.floor(Math.random() * list.length)].id);
      break;
    }
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
  } else if (input.id === 'scan-file') {
    const file = input.files?.[0]; input.value = ''; if (!file) return;
    analyze(await fileToCanvas(file));
  }
});

// ---------------------------------------------------------------- scanner
let stream = null, starting = null, scanRun = 0;
const scanStatus = (html) => { $('scan-status').innerHTML = html; };
async function startCamera() {
  if (stream || starting) return starting;
  if (!navigator.mediaDevices?.getUserMedia) { scanStatus('Caméra indisponible sur cet appareil : utilise le bouton 🖼 pour choisir une photo.'); return; }
  starting = (async () => {
    try {
      const s = await navigator.mediaDevices.getUserMedia({ video: { facingMode: { ideal: 'environment' }, width: { ideal: 1920 }, height: { ideal: 1080 } }, audio: false });
      if (S.screen !== 'scan' || !$('photo-preview').hidden) { s.getTracks().forEach((t) => t.stop()); return; } // écran quitté entre-temps
      stream = s; $('video').srcObject = s; await $('video').play().catch(() => {});
    } catch { scanStatus('Accès à la caméra refusé. Autorise-le dans les réglages, ou utilise le bouton 🖼 pour choisir une photo.'); }
  })().finally(() => { starting = null; });
  return starting;
}
function stopCamera() { stream?.getTracks().forEach((t) => t.stop()); stream = null; $('video').srcObject = null; }
function resetScan() {
  scanRun++;
  $('photo-preview').hidden = true; $('photo-preview').removeAttribute('src');
  $('video').hidden = false; $('scan-frame').hidden = false; $('scan-results').innerHTML = '';
  scanStatus('Place la capsule dans le cadre, nom visible, sans flash.');
  if (S.screen === 'scan') startCamera();
}
function capture() {
  const v = $('video');
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
  $('video').hidden = true; $('scan-frame').hidden = true; $('scan-results').innerHTML = '';
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
  showFact(false);
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
