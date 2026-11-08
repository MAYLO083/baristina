// Baristina V1 — index de reconnaissance d'images (IA gratuite, exécutée sur l'appareil).
// Calcule avec le modèle open source CLIP une « empreinte » de chaque capsule :
//   - à partir de ses photos de référence : photo locale (photos/…), photo officielle de la marque
//     et photos de la galerie officielle (capsule seule, côté de boîte : data/reference-images.json) ;
//   - à partir de son nom et de sa marque (indice faible, pour les capsules sans photo).
// Les photos officielles sont téléchargées dans scripts/.cache uniquement pour ce calcul :
// seules les empreintes (nombres) sont publiées dans data/scan-index.json.
// Usage (dans scripts/) :  npm install  puis  npm run index
import { AutoProcessor, AutoTokenizer, CLIPTextModelWithProjection, CLIPVisionModelWithProjection, RawImage } from '@huggingface/transformers';
import sharp from 'sharp';
import { execFileSync } from 'node:child_process';
import { writeFileSync, readFileSync, existsSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const MODEL = process.env.SCAN_MODEL || 'Xenova/clip-vit-base-patch32';
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const CACHE = path.join(ROOT, 'scripts', '.cache');
mkdirSync(CACHE, { recursive: true });
const { brands, products } = JSON.parse(execFileSync('node', [path.join(ROOT, 'scripts', 'build-seed.mjs'), '--json']).toString());
const brandName = Object.fromEntries(brands.map((b) => [b.id, b.name]));
const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36';

// Couleur médiane du centre de l'image (même calcul que dans l'app : js/scan.js).
function medianColor(rgb, w, h, ch) {
  const px = [];
  for (let y = Math.floor(h * 0.3); y < Math.floor(h * 0.7); y++) {
    for (let x = Math.floor(w * 0.2); x < Math.floor(w * 0.8); x++) {
      const i = (y * w + x) * ch, r = rgb[i], g = rgb[i + 1], b = rgb[i + 2];
      const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
      px.push({ r, g, b, l: 0.299 * r + 0.587 * g + 0.114 * b, s: mx ? (mx - mn) / mx : 0 });
    }
  }
  let m = px.filter((p) => p.l > 20 && p.l < 230); if (m.length < px.length * 0.25) m = px;
  const sat = m.filter((p) => p.s > 0.12); if (sat.length >= m.length * 0.3) m = sat;
  const med = (a) => { a = [...a].sort((x, y) => x - y); return a[a.length >> 1]; };
  return [med(m.map((p) => p.r)), med(m.map((p) => p.g)), med(m.map((p) => p.b))];
}
const normalize = (v) => { const n = Math.hypot(...v); return v.map((x) => x / n); };
const round = (v) => v.map((x) => Math.round(x * 1e4) / 1e4);

// Les photos officielles montrent souvent la boîte, avec les capsules en bas : on ajoute des recadrages
// (bas, bas-droite, bas-gauche, centre) pour qu'une photo de capsule seule trouve aussi une référence proche.
const CROPS = [null, [0, 0.55, 1, 0.45], [0.45, 0.6, 0.55, 0.4], [0, 0.6, 0.55, 0.4], [0.2, 0.25, 0.6, 0.5]];
async function loadImage(src, crop = null) {
  let buf;
  if (/^https?:/.test(src)) {
    const file = path.join(CACHE, createHash('sha1').update(src).digest('hex') + '.img');
    if (existsSync(file)) buf = readFileSync(file);
    else {
      const res = await fetch(src, { headers: { 'User-Agent': UA } });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      buf = Buffer.from(await res.arrayBuffer());
      writeFileSync(file, buf);
      await new Promise((r) => setTimeout(r, 1200)); // téléchargements espacés : certains sites refusent les rafales
    }
  } else buf = readFileSync(path.join(ROOT, src));
  if (crop) {
    const meta = await sharp(buf).metadata();
    const [x, y, w, h] = crop;
    const left = Math.floor(meta.width * x), top = Math.floor(meta.height * y);
    buf = await sharp(buf).extract({ left, top, width: Math.min(meta.width - left, Math.round(meta.width * w)), height: Math.min(meta.height - top, Math.round(meta.height * h)) }).png().toBuffer();
  }
  const { data, info } = await sharp(buf).flatten({ background: '#ffffff' }).resize(448, 448, { fit: 'contain', background: '#ffffff' })
    .removeAlpha().raw().toBuffer({ resolveWithObject: true });
  return { raw: new RawImage(new Uint8ClampedArray(data), info.width, info.height, 3), small: await sharp(buf).flatten({ background: '#ffffff' }).resize(60, 60, { fit: 'fill' }).removeAlpha().raw().toBuffer() };
}

console.log('Chargement du modèle', MODEL, '…');
const [processor, tokenizer, vision, text] = await Promise.all([
  AutoProcessor.from_pretrained(MODEL),
  AutoTokenizer.from_pretrained(MODEL),
  CLIPVisionModelWithProjection.from_pretrained(MODEL, { dtype: 'q8' }),
  CLIPTextModelWithProjection.from_pretrained(MODEL, { dtype: 'q8' }),
]);

// Photos de galerie (capsule seule, côté de boîte…) trouvées par find-reference-images.mjs.
const GALLERY_FILE = path.join(ROOT, 'data', 'reference-images.json');
const gallery = existsSync(GALLERY_FILE) ? JSON.parse(readFileSync(GALLERY_FILE, 'utf8').replace(/^﻿/, '')) : {};

const index = {}, failed = [];
for (const p of products) {
  const brand = brandName[p.brand_id];
  const prompts = [`a photo of a ${brand} ${p.name} coffee capsule`, `${brand} ${p.name}`, `a ${brand} coffee capsule box`];
  const t = await text(tokenizer(prompts, { padding: 'max_length', max_length: 77, truncation: true }));
  const rows = t.text_embeds.tolist().map(normalize);
  const entry = { t: round(normalize(rows[0].map((_, i) => rows.reduce((s, r) => s + r[i], 0)))), r: [], c: [] };

  const sources = [...new Set([p.photo_url && !/^https?:/.test(p.photo_url) ? p.photo_url : null, p.official_image, ...(gallery[p.id] ?? [])].filter(Boolean))];
  for (const src of sources) {
    const official = /^https?:/.test(src);
    // Recadrages pour la photo principale ; les photos de galerie (souvent la capsule seule) : image entière + centre.
    for (const crop of !official ? [null] : src === p.official_image ? CROPS : [null, CROPS[4]]) {
      try {
        const { raw, small } = await loadImage(src, crop);
        const out = await vision(await processor(raw));
        entry.r.push(round(normalize(out.image_embeds.tolist()[0])));
        if (!crop) entry.c.push(medianColor(small, 60, 60, 3)); // couleur : image entière seulement
      } catch (e) { failed.push(`${p.id} (${src.slice(0, 60)}… : ${e.message})`); break; }
    }
  }
  index[p.id] = entry;
  process.stdout.write(entry.r.length ? '•' : '.');
}

writeFileSync(path.join(ROOT, 'data', 'scan-index.json'), JSON.stringify({ model: MODEL, dtype: 'q8', version: 2, built: new Date().toISOString().slice(0, 10), items: index }));
const withRef = Object.values(index).filter((e) => e.r.length).length;
console.log(`\nIndex écrit : ${Object.keys(index).length} capsules, ${withRef} avec au moins une photo de référence.`);
if (failed.length) console.log('Échecs :\n' + failed.join('\n'));
