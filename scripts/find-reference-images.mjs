// Baristina — trouve, sur la page produit officielle de chaque capsule, les autres photos de la galerie
// (capsule seule, côté de la boîte, visuel avec capsules…), en plus de la photo principale.
// Elles servent uniquement de références au scan : une photo de capsule seule doit ressembler
// à une capsule de la bonne marque, pas seulement à une boîte. Résultat : data/reference-images.json
// Usage (dans scripts/) :  node find-reference-images.mjs
import { execFileSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'data', 'reference-images.json');
const { products } = JSON.parse(execFileSync('node', [path.join(ROOT, 'scripts', 'build-seed.mjs'), '--json']).toString());
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36';

// Images de galerie propres au produit, par site (les bannières, menus et pictos sont ignorés).
const RULES = {
  lor: { re: /https:\/\/www\.lorespresso\.com\/media\/catalog\/product\/[^"'\s?]+\.(?:png|jpe?g)/g, skip: /compatibility|appliance|enjoying|machine|lifestyle/i, suffix: '?format=jpeg' },
  starbucks: { re: /https:\/\/www\.starbucksathome\.com\/fr\/sites\/default\/files\/(?:styles\/pdp_gallery_large\/public\/)?20\d\d-\d\d\/[^"'\s]+?(?:CAPSULE%20ICONS[^"'\s]*?\.png|_side_[^"'\s]*?\.png\.webp\?h=[^"'\s&]+&amp;itok=[^"'\s&]+)/g, skip: /LIFESTYLE|BACKGROUND/i },
  lavazza: { re: /\/content\/dam\/lavazza-athena\/[^"'\s]+?hero-product-banner\/[^"'\s]+?-d-[^"'\s]+?\.png/g, base: 'https://www.lavazza.fr' },
  'carte-noire': { re: /\/content\/dam\/carte-noire-athena\/[^"'\s]+?hero-product-banner\/[^"'\s]+?-d-[^"'\s]+?\.png/g, base: 'https://www.cartenoire.fr' },
};

const perPage = {}, seen = new Map(), pages = new Set();
for (const p of products) {
  const rule = RULES[p.brand_id];
  if (!rule || pages.has(p.shop_url)) continue;
  pages.add(p.shop_url);
  try {
    const res = await fetch(p.shop_url, { headers: { 'User-Agent': UA, 'Accept-Language': 'fr-FR,fr;q=0.9' } });
    const html = res.ok ? await res.text() : '';
    const urls = [...new Set((html.match(rule.re) ?? [])
      .map((u) => (rule.base ? rule.base : '') + u.replace(/&amp;/g, '&').replace(/%25([0-9A-F]{2})/gi, '%$1') + (rule.suffix ?? ''))
      .filter((u) => !rule.skip?.test(u)))];
    perPage[p.id] = urls;
    for (const u of urls) seen.set(u, (seen.get(u) ?? 0) + 1);
    console.log(`${urls.length ? '✓' : '✗'} ${p.id} : ${urls.length} image(s)${res.ok ? '' : ' (HTTP ' + res.status + ')'}`);
  } catch (e) { console.log(`✗ ${p.id} (${e.message})`); }
  await new Promise((r) => setTimeout(r, 700)); // une page à la fois
}

// Une image présente sur plusieurs pages produit est générique (pas propre à la capsule) : on l'écarte.
const out = {};
for (const [id, urls] of Object.entries(perPage)) {
  const own = urls.filter((u) => seen.get(u) < 3);
  if (own.length) out[id] = own;
}
writeFileSync(OUT, JSON.stringify(out, null, 1));
console.log(`\n${Object.keys(out).length} capsules avec des photos de galerie, ${Object.values(out).flat().length} images au total.`);
