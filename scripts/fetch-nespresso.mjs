// Baristina — relève le catalogue Nespresso (Original + Vertuo) sur nespresso.com.
// La page catalogue contient les fiches produit (nom, gamme, intensité, arômes, tasses, photo) :
// on garde ces faits tels quels dans data/nespresso-catalog.json, utilisé par build-seed.mjs.
// Usage (dans scripts/) :  node fetch-nespresso.mjs
import { writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const URL_LIST = 'https://www.nespresso.com/fr/fr/order/capsules/vertuo'; // la page contient les deux systèmes
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36';

const h = await (await fetch(URL_LIST, { headers: { 'User-Agent': UA, 'Accept-Language': 'fr-FR,fr;q=0.9' } })).text();

// Chaque fiche est un objet JSON contenant "modelType":"Capsule" : on retrouve ses accolades.
function objectAround(i) {
  let depth = 0, start = -1;
  for (let j = i; j >= 0; j--) {
    if (h[j] === '}') depth++;
    else if (h[j] === '{') { if (depth === 0) { start = j; break; } depth--; }
  }
  let d = 0, inStr = false;
  for (let j = start; j < h.length; j++) {
    const c = h[j];
    if (inStr) { if (c === '\\') j++; else if (c === '"') inStr = false; continue; }
    if (c === '"') inStr = true;
    else if (c === '{') d++;
    else if (c === '}' && --d === 0) return h.slice(start, j + 1);
  }
  return null;
}
const byId = new Map();
for (let i = h.indexOf('"modelType":"Capsule"'); i > 0; i = h.indexOf('"modelType":"Capsule"', i + 1)) {
  try { const o = JSON.parse(objectAround(i)); if (o.name && o.pdpURLs?.desktop) byId.set(o.id, o); } catch {}
}

const list = [...byId.values()]
  .filter((o) => !o.bundled && !/assortiment|kit |fourreau/i.test(o.name)) // pas les coffrets
  .map((o) => ({
    system: o.rootCategory === 'capsules_vertuo' ? 'vertuo' : 'original',
    slug: o.pdpURLs.desktop.split('/').pop(),
    name: o.name.trim().replace(/\s+/g, ' '),
    range: o.category?.trim() ?? null,
    intensity: o.capsuleProperties?.intensity || null,
    aromas: (o.capsuleAromatics ?? []).map((a) => a.name.trim()),
    cups: (o.extraPlpMeta?.cupSizes ?? []).map((c) => c.name.trim()),
    image: 'https://www.nespresso.com' + (o.responsiveImages?.plp || o.responsiveImages?.standard),
    url: o.pdpURLs.desktop,
  }))
  .sort((a, b) => a.system.localeCompare(b.system) || a.range.localeCompare(b.range) || a.name.localeCompare(b.name));

writeFileSync(path.join(ROOT, 'data', 'nespresso-catalog.json'), JSON.stringify({ source: URL_LIST, fetched: new Date().toISOString().slice(0, 10), items: list }, null, 1));
console.log(`${list.length} capsules : ${list.filter((x) => x.system === 'original').length} Original, ${list.filter((x) => x.system === 'vertuo').length} Vertuo.`);
