// Baristina V1 — reconnaissance de capsule 100 % sur l'appareil, gratuite.
// Trois indices combinés :
//   1. l'apparence (modèle d'IA open source CLIP, comparé aux photos de référence) ;
//   2. la couleur de la capsule (comparée à celle des photos de référence) ;
//   3. le nom imprimé, lu par OCR (Tesseract), comparé au catalogue.
// Références : photos locales + photos officielles des marques (seules leurs empreintes sont dans data/scan-index.json).
// Le modèle (~90 Mo) est téléchargé au premier scan puis gardé en cache par le navigateur.
// Aucune photo n'est envoyée sur internet.

const TRANSFORMERS_URL = 'https://cdn.jsdelivr.net/npm/@huggingface/transformers@3.8.1';
const TESSERACT_URL = 'https://cdn.jsdelivr.net/npm/tesseract.js@5.1.1/dist/tesseract.esm.min.js';

const norm = (t) => String(t ?? '').toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9 ]/g, ' ').replace(/\s+/g, ' ').trim();
const dot = (a, b) => { let s = 0; for (let i = 0; i < a.length; i++) s += a[i] * b[i]; return s; };
const normalize = (v) => { const n = Math.hypot(...v); return v.map((x) => x / n); };
function softmax(values, temperature) {
  const m = Math.max(...values), e = values.map((x) => Math.exp((x - m) * temperature)), s = e.reduce((a, b) => a + b, 0);
  return e.map((x) => x / s);
}

// ------------------------------------------------------------ couleur
// Même calcul que scripts/build-scan-index.mjs (centre de l'image, sans reflets ni ombres).
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
function hsl([r, g, b]) {
  r /= 255; g /= 255; b /= 255;
  const mx = Math.max(r, g, b), mn = Math.min(r, g, b); let h = 0, s = 0; const l = (mx + mn) / 2;
  if (mx !== mn) {
    const d = mx - mn; s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    h = mx === r ? (g - b) / d + (g < b ? 6 : 0) : mx === g ? (b - r) / d + 2 : (r - g) / d + 4; h *= 60;
  }
  return [h, s, l];
}
// La teinte compte moins quand la photo est terne (reflet, éclairage) : elle est alors peu fiable.
function colorDistance(a, b) {
  const [h1, s1, l1] = hsl(a), [h2, s2, l2] = hsl(b);
  let hd = Math.abs(h1 - h2); hd = Math.min(hd, 360 - hd);
  const w = Math.min(1, s1 / 0.3);
  return Math.sqrt((hd * w) ** 2 + ((s1 - s2) * 100) ** 2 * 0.5 + ((l1 - l2) * 100) ** 2 * 0.35);
}

// ------------------------------------------------------------ texte (OCR)
function editSim(a, b) {
  if (!a || !b) return 0;
  let prev = Array.from({ length: b.length + 1 }, (_, i) => i);
  for (let i = 1; i <= a.length; i++) {
    const cur = [i];
    for (let j = 1; j <= b.length; j++) cur[j] = Math.min(cur[j - 1] + 1, prev[j] + 1, prev[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
    prev = cur;
  }
  return 1 - prev[b.length] / Math.max(a.length, b.length);
}
const BRAND_PATTERNS = {
  nespresso: /nespresso/, lor: /\bl ?or\b/, starbucks: /starbucks/, 'carte-noire': /carte ?noire/,
  lavazza: /lavazza/, illy: /illy/, segafredo: /segafredo/, kimbo: /kimbo/, borbone: /borbone|respresso/,
};
const GENERIC = new Set(['espresso', 'lungo', 'ristretto', 'capsule', 'capsules', 'coffee', 'cafe', 'roast', 'blend', 'the', 'and', 'des', 'les', 'aux']);
// Score 0..1 : part des mots distinctifs du nom retrouvés dans le texte lu.
function textScore(product, brand, words, fullText) {
  const name = norm(product.name);
  if (name.length >= 6 && fullText.includes(name)) return 1;
  let tokens = name.split(' ').filter((t) => t.length >= 3 && !GENERIC.has(t));
  if (!tokens.length) tokens = name.split(' ').filter((t) => t.length >= 3);
  if (!tokens.length || !words.length) return 0;
  let sum = 0;
  for (const t of tokens) {
    const best = Math.max(...words.map((w) => editSim(t, w)));
    sum += best >= 0.75 ? best : 0;
  }
  let score = sum / tokens.length;
  if (score > 0 && norm(brand).split(' ').some((b) => b.length >= 3 && words.some((w) => editSim(b, w) >= 0.8))) score = Math.min(1, score + 0.15);
  return score >= 0.6 ? score : 0;
}

// ------------------------------------------------------------ chargement des modèles
let readyPromise = null, ocrPromise = null;
export function prepare(onProgress = () => {}) {
  if (!readyPromise) {
    readyPromise = (async () => {
      const [tf, index] = await Promise.all([import(TRANSFORMERS_URL), fetch('data/scan-index.json').then((r) => r.json())]);
      tf.env.allowLocalModels = false;
      const files = {};
      const progress_callback = (e) => {
        if (e.status === 'progress' && e.total) {
          files[e.file] = [e.loaded, e.total];
          const [l, t] = Object.values(files).reduce(([a, b], [x, y]) => [a + x, b + y], [0, 0]);
          onProgress(Math.round((l / t) * 100));
        }
      };
      const [processor, vision] = await Promise.all([
        tf.AutoProcessor.from_pretrained(index.model),
        tf.CLIPVisionModelWithProjection.from_pretrained(index.model, { dtype: index.dtype, progress_callback }),
      ]);
      return { tf, index, processor, vision };
    })();
    readyPromise.catch(() => { readyPromise = null; }); // nouvel essai possible si le réseau a coupé
  }
  return readyPromise;
}
function getOCR() {
  if (!ocrPromise) {
    ocrPromise = import(TESSERACT_URL).then((m) => (m.default ?? m).createWorker('eng'));
    ocrPromise.catch(() => { ocrPromise = null; });
  }
  return ocrPromise;
}
async function readText(canvas) {
  try {
    const worker = await getOCR();
    // Image agrandie (≥ 1400 px) : les petits noms imprimés sur la capsule ou la boîte se lisent mieux.
    const k = Math.max(1, 1400 / Math.max(canvas.width, canvas.height));
    const c = document.createElement('canvas'); c.width = Math.round(canvas.width * k); c.height = Math.round(canvas.height * k);
    const x = c.getContext('2d'); x.imageSmoothingQuality = 'high'; x.filter = 'grayscale(1) contrast(1.6)'; x.drawImage(canvas, 0, 0, c.width, c.height);
    // Au-delà de 10 s (appareil lent), on continue sans le texte plutôt que de bloquer le scan.
    const result = await Promise.race([worker.recognize(c), new Promise((res) => setTimeout(() => res(null), 10000))]);
    return result?.data?.text ?? '';
  } catch { return ''; } // l'OCR est un bonus : sans lui, l'apparence et la couleur suffisent
}

// ------------------------------------------------------------ identification
// products : catalogue (avec brand_id) ; brandName(p) : nom de la marque.
export async function identify(canvas, products, brandName, onProgress) {
  const [{ tf, index, processor, vision }, text] = await Promise.all([prepare(onProgress), readText(canvas)]);
  const ctx = canvas.getContext('2d');
  const full = ctx.getImageData(0, 0, canvas.width, canvas.height);
  const image = new tf.RawImage(new Uint8ClampedArray(full.data), canvas.width, canvas.height, 4);
  const q = normalize(Array.from((await vision(await processor(image))).image_embeds.data));

  const small = document.createElement('canvas'); small.width = small.height = 60;
  small.getContext('2d').drawImage(canvas, 0, 0, 60, 60);
  const color = medianColor(small.getContext('2d').getImageData(0, 0, 60, 60).data, 60, 60, 4);

  const list = products.filter((p) => index.items[p.id]);
  const entries = list.map((p) => index.items[p.id]);
  // Plusieurs photos de référence par capsule (photo locale + photo officielle) : on garde la plus ressemblante.
  const appearance = softmax(entries.map((e) => (e.r.length ? Math.max(...e.r.map((r) => dot(q, r))) : -1)), 100);
  const colorRaw = entries.map((e) => (e.c.length ? Math.max(...e.c.map((c) => Math.exp(-colorDistance(color, c) / 20))) : 0));
  const colorSum = colorRaw.reduce((a, b) => a + b, 0) || 1;
  const byName = softmax(entries.map((e) => dot(q, e.t)), 100);
  const fullText = norm(text), words = [...new Set(fullText.split(' ').filter((w) => w.length >= 3))];

  // Marque lue sur la capsule ou la boîte : ses capsules passent devant les autres.
  const brandsSeen = new Set(Object.entries(BRAND_PATTERNS).filter(([, re]) => re.test(fullText)).map(([id]) => id));

  const scored = list.map((p, i) => {
    const ocr = textScore(p, brandName(p), words, fullText);
    // Pondération mesurée sur les photos de référence (apparence / couleur),
    // + un petit indice « nom » pour les capsules sans photo, + la marque et le nom lus, prioritaires.
    const score = 0.4 * appearance[i] + 0.6 * (colorRaw[i] / colorSum) + 0.15 * byName[i]
      + (brandsSeen.has(p.brand_id) ? 0.5 : 0) + 1.5 * ocr;
    return { p, score, ocr };
  }).sort((a, b) => b.score - a.score);

  const [first, second] = scored;
  const confidence = first.ocr >= 0.8 ? 'haute'
    : first.score > 0.35 && first.score > 2 * (second?.score ?? 0) ? 'moyenne' : 'basse';
  return { candidates: scored.slice(0, 3).map((x, i) => ({ id: x.p.id, confidence: i === 0 ? confidence : 'basse' })), text: text.trim() };
}
