// Baristina V1 — index de reconnaissance d'images (IA gratuite, exécutée sur l'appareil).
// Calcule, avec le modèle open source CLIP, une « empreinte » de chaque capsule :
//   - à partir de sa photo de référence quand elle existe (photos/…) ;
//   - à partir de son nom, de sa marque et de sa couleur (pour toutes les capsules).
// L'app compare ensuite la photo prise à ces empreintes, sans serveur ni IA payante.
// Usage (dans scripts/) :  npm install  puis  npm run index
import { AutoProcessor, AutoTokenizer, CLIPTextModelWithProjection, CLIPVisionModelWithProjection, RawImage } from '@huggingface/transformers';
import { execFileSync } from 'node:child_process';
import { writeFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

export const MODEL = process.env.SCAN_MODEL || 'Xenova/clip-vit-base-patch32';
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const { brands, products } = JSON.parse(execFileSync('node', [path.join(ROOT, 'scripts', 'build-seed.mjs'), '--json']).toString());
const brandName = Object.fromEntries(brands.map((b) => [b.id, b.name]));

const COLOR_WORDS = [
  ['black', [20, 20, 20]], ['dark brown', [70, 45, 35]], ['brown', [130, 85, 50]], ['beige', [215, 195, 160]],
  ['gold', [190, 150, 60]], ['orange', [220, 120, 40]], ['red', [160, 35, 35]], ['pink', [215, 130, 140]],
  ['purple', [95, 35, 115]], ['blue', [50, 80, 140]], ['teal', [40, 95, 105]], ['green', [40, 100, 60]],
  ['dark green', [20, 55, 35]], ['grey', [128, 128, 128]], ['silver', [190, 190, 195]], ['white', [240, 240, 235]],
];
function colorWord(hex) {
  if (!hex) return null;
  const c = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
  return COLOR_WORDS.reduce((best, [w, v]) => {
    const d = Math.hypot(c[0] - v[0], c[1] - v[1], c[2] - v[2]);
    return d < best.d ? { w, d } : best;
  }, { w: null, d: Infinity }).w;
}
// Couleur médiane du centre de l'image (même calcul que dans l'app : js/scan.js).
export function medianColor(rgb, w, h, ch) {
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

console.log('Chargement du modèle', MODEL, '…');
const [processor, tokenizer, vision, text] = await Promise.all([
  AutoProcessor.from_pretrained(MODEL),
  AutoTokenizer.from_pretrained(MODEL),
  CLIPVisionModelWithProjection.from_pretrained(MODEL, { dtype: 'q8' }),
  CLIPTextModelWithProjection.from_pretrained(MODEL, { dtype: 'q8' }),
]);

const index = {};
for (const p of products) {
  const brand = brandName[p.brand_id];
  const color = colorWord(p.color);
  const prompts = [
    `a photo of a ${brand} ${p.name} coffee capsule`,
    `${brand} ${p.name}`,
    color ? `a ${color} ${brand} coffee capsule` : `a ${brand} coffee capsule`,
  ];
  const t = await text(tokenizer(prompts, { padding: 'max_length', max_length: 77, truncation: true }));
  const rows = t.text_embeds.tolist().map(normalize);
  const entry = { t: round(normalize(rows[0].map((_, i) => rows.reduce((s, r) => s + r[i], 0)))) };

  if (p.photo_url && existsSync(path.join(ROOT, p.photo_url))) {
    const img = await RawImage.read(path.join(ROOT, p.photo_url));
    const out = await vision(await processor(img));
    entry.i = round(normalize(out.image_embeds.tolist()[0]));
    const small = await img.resize(60, 60);
    entry.c = medianColor(small.data, small.width, small.height, small.channels);
  }
  index[p.id] = entry;
  process.stdout.write('.');
}

const file = path.join(ROOT, 'data', 'scan-index.json');
writeFileSync(file, JSON.stringify({ model: MODEL, dtype: 'q8', built: new Date().toISOString().slice(0, 10), items: index }));
const withPhoto = Object.values(index).filter((e) => e.i).length;
console.log(`\nIndex écrit : ${Object.keys(index).length} capsules (${withPhoto} avec photo de référence) → data/scan-index.json`);
