// Baristina — trouve l'adresse de la photo officielle de chaque capsule sur la page produit
// de la marque (balises og:image / JSON-LD). Résultat : data/official-images.json
// Usage (dans scripts/) :  node find-official-images.mjs
// Les photos ne sont pas copiées : l'app les affiche depuis le site de la marque.
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const OUT = path.join(ROOT, 'data', 'official-images.json');
const { products } = JSON.parse(execFileSync('node', [path.join(ROOT, 'scripts', 'build-seed.mjs'), '--json']).toString());
const found = existsSync(OUT) ? JSON.parse(readFileSync(OUT, 'utf8')) : {};
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36';

function extract(html, base) {
  const meta = html.match(/<meta[^>]+(?:property|name)=["'](?:og:image|og:image:secure_url|twitter:image)["'][^>]*>/gi) ?? [];
  for (const m of meta) {
    const c = m.match(/content=["']([^"']+)["']/i);
    if (c) return new URL(c[1].replace(/&amp;/g, '&'), base).href.replace(/%25([0-9A-F]{2})/gi, '%$1'); // certains sites encodent deux fois
  }
  for (const block of html.match(/<script[^>]+application\/ld\+json[^>]*>([\s\S]*?)<\/script>/gi) ?? []) {
    const img = block.match(/"image"\s*:\s*(?:\[\s*)?"([^"]+)"/);
    if (img) return new URL(img[1], base).href;
  }
  return null;
}

const urls = new Map();
for (const p of products) {
  if (found[p.id]) continue;
  if (urls.has(p.shop_url)) continue; // page de gamme partagée (ex. Café Royal) : pas une photo par capsule
  urls.set(p.shop_url, true);
  try {
    const res = await fetch(p.shop_url, { headers: { 'User-Agent': UA, 'Accept-Language': 'fr-FR,fr;q=0.9' }, redirect: 'follow' });
    const img = res.ok ? extract(await res.text(), res.url) : null;
    if (img) found[p.id] = img;
    console.log(`${img ? '✓' : '✗'} ${p.id}${res.ok ? '' : ' (HTTP ' + res.status + ')'}`);
  } catch (e) { console.log(`✗ ${p.id} (${e.message})`); }
  await new Promise((r) => setTimeout(r, 700)); // une page à la fois, pour ne pas surcharger les sites
}
writeFileSync(OUT, JSON.stringify(found, null, 1));
const missing = products.filter((p) => !found[p.id]).map((p) => p.id);
console.log(`\n${Object.keys(found).length}/${products.length} photos trouvées. Manquantes : ${missing.join(', ') || 'aucune'}`);
