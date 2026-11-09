// Baristina V1 — source du catalogue.
// Génère supabase/seed.sql :  node scripts/build-seed.mjs
//
// Règles suivies :
//  - uniquement des faits relevés sur les sites officiels des marques (novembre 2026) ;
//  - aucune phrase marketing recopiée : seuls des mots-clés d'arômes sont gardés,
//    les descriptions affichées dans l'app sont rédigées à partir de ces faits ;
//  - une information non confirmée reste vide (null / []).
import { writeFileSync, readFileSync, existsSync } from 'node:fs';

const VERIFIED = '2026-11-09';

const BRANDS = [
  { id: 'nespresso',   name: 'Nespresso',    website: 'https://www.nespresso.com/fr/fr/order/capsules/original', sort: 1 },
  { id: 'lor',         name: "L'Or Espresso", website: 'https://www.lorespresso.com/fr_fr/capsules-de-cafe/single-capsules', sort: 2 },
  { id: 'starbucks',   name: 'Starbucks',    website: 'https://www.starbucksathome.com/fr/produits/starbucks-nespresso', sort: 3 },
  { id: 'carte-noire', name: 'Carte Noire',  website: 'https://www.cartenoire.fr/fr/cafes/capsules/nespresso', sort: 4 },
  { id: 'lavazza',     name: 'Lavazza',      website: 'https://www.lavazza.fr/fr/cafe/capsules-compatibles.html', sort: 5 },
  { id: 'illy',        name: 'illy',         website: 'https://www.illy.com/fr-fr/cafe/capsules-compatibles', sort: 6 },
  { id: 'segafredo',   name: 'Segafredo',    website: 'https://www.segafredo.fr/nos-produits/a-la-maison/cafe/capsules-compatibles-nespresso', sort: 7 },
  { id: 'kimbo',       name: 'Kimbo',        website: 'https://kimbo.it/collections/nespresso-alluminio', sort: 8 },
  { id: 'borbone',     name: 'Caffè Borbone', website: 'https://www.caffeborbone.com/en-eu/collections/compatible-capsules-nespresso', sort: 9 },
];
// Café Royal retiré (novembre 2026) : aucun visuel de capsule sur son site officiel.

const R = 'Ristretto 25 ml', E = 'Espresso 40 ml', L = 'Lungo 110 ml', C = 'Cappuccino';
const NES = 'https://www.nespresso.com/fr/fr/order/capsules/original/';

// [id, nom, gamme, intensité, tasses, arômes, extra]
const nespresso = [
  ['arpeggio', 'Arpeggio', 'Ispirazione Italiana', 9, [R, E], ['cacao'], { color: '#5a1b73', photo: 'arpeggio', url: 'arpeggio-capsule-cafe' }],
  ['roma', 'Roma', 'Ispirazione Italiana', 8, [R, E], ['boisé', 'céréales'], { color: '#493e38', photo: 'roma', url: 'roma-capsule-cafe' }],
  ['ristretto', 'Ristretto', 'Ispirazione Italiana', 10, [R, E], ['torréfaction intense', 'fruité'], { color: '#1b1b1d', photo: 'ristretto', url: 'ristretto-capsule-cafe' }],
  ['venezia', 'Venezia', 'Ispirazione Italiana', 8, [R, E], ['rond', 'caramélisé'], { color: '#8a4b2f', photo: 'venezia', url: 'venezia-capsule-cafe' }],
  ['kazaar', 'Kazaar', 'Ispirazione Italiana', 12, [R, E], ['épicé'], { color: '#183b59', photo: 'kazaar', url: 'kazaar-capsule-cafe' }],
  ['napoli', 'Napoli', 'Ispirazione Italiana', 13, [R, E], ['velouté'], { color: '#8e2316', photo: 'napoli', url: 'napoli-capsule-cafe' }],
  ['volluto', 'Volluto', 'Ispirazione Italiana', 4, [E], ['doux', 'céréales'], { color: '#b07b19', photo: 'volluto', url: 'volluto-capsule-cafe' }],
  ['cosi', 'Cosi', 'Ispirazione Italiana', 4, [E], ['équilibré', 'torréfaction légère'], { color: '#8b7160', photo: 'cosi', url: 'cosi-capsule-cafe' }],
  ['capriccio', 'Capriccio', 'Ispirazione Italiana', 5, [E], ['fine acidité', 'céréales'], { color: '#133423', photo: 'capriccio', url: 'capriccio-capsule-cafe' }],
  ['livanto', 'Livanto', 'Ispirazione Italiana', 7, [E], ['caramélisé', 'toasté'], { color: '#a9471f', photo: 'livanto', url: 'livanto-capsule-cafe' }],
  ['arpeggio-extra', 'Arpeggio Extra', 'Coffee+', 9, [R, E], [], { color: '#502769', photo: 'arpeggio-extra', url: 'arpeggio-extra-capsule-cafe', highlight: 'Extra caféiné' }],
  ['nocciola', 'Nocciola', 'Barista Creations', null, [L, C], ['noisette grillée'], { color: '#c89c62', photo: 'nocciola', url: 'barista-nocciola-caspule-cafe', flavoured: true }],
  ['caramello', 'Caramello', 'Barista Creations', null, [L, C], ['caramel'], { color: '#885446', photo: 'caramello', url: 'barista-caramello-caspule-cafe', flavoured: true }],
  ['scuro', 'Scuro', 'Barista Creations', null, [R, C], [], { color: '#9e7958', photo: 'scuro', url: 'barista-scuro-capsule-cafe', highlight: 'Pensé pour les recettes au lait' }],
  ['vaniglia', 'Vaniglia', 'Barista Creations', null, [], ['vanille'], { color: '#b5a378', photo: 'vaniglia', url: 'barista-vaniglia-caspule-cafe', flavoured: true }],
  ['paris-espresso', 'Paris Espresso', 'City Exploration', 6, [E], ['céréales', 'agrumes'], { color: '#7f7f7f', photo: 'paris-espresso', url: 'parisespresso-coffee-capsule' }],
  ['buenos-aires-lungo', 'Buenos Aires Lungo', 'City Exploration', 4, [L], ['céréales'], { color: '#838382', photo: 'buenos-aires-lungo', url: 'buenos-aires-capsule-cafe' }],
  ['shanghai-lungo', 'Shanghai Lungo', 'City Exploration', 5, [L], ['fruits rouges'], { color: '#9d2927', photo: 'shanghai-lungo', url: 'shanghai-capsule-cafe' }],
  ['tokyo-lungo', 'Tokyo Lungo', 'City Exploration', 6, [L], ['floral'], { color: '#485c8b', photo: 'tokyo-lungo', url: 'tokyo-capsule-cafe' }],
  ['vienna-lungo', 'Vienna Lungo', 'City Exploration', 6, [L], ['céréales'], { color: '#de7019', photo: 'vienna-lungo', url: 'vienna-capsule-cafe' }],
  ['stockholm-lungo', 'Stockholm Lungo', 'City Exploration', 8, [L], ['céréales maltées'], { color: '#26524b', photo: 'stockholm-lungo', url: 'stockholm-capsule-cafe' }],
  ['nicaragua', 'Nicaragua', "Cafés d'origine", 5, [E, L], [], { color: '#ba9352', photo: 'nicaragua', url: 'nicaragua-capsule-cafe', origin: 'Nicaragua' }],
  ['colombia', 'Colombia', "Cafés d'origine", 6, [E, L], ['fruité', 'doux'], { color: '#d48284', photo: 'colombia', url: 'colombia-capsule-cafe', origin: 'Colombie (Arabica)' }],
  ['indonesia', 'Indonesia', "Cafés d'origine", 8, [E, L], [], { color: '#2b5f6c', photo: 'indonesia', url: 'indonesia-capsule-cafe', origin: 'Sumatra, Indonésie (Arabica)', highlight: 'Labellisé Fairtrade' }],
  ['india', 'India', "Cafés d'origine", 11, [E, L], ['épicé'], { color: '#70855a', photo: 'india', url: 'india-capsule-cafe', origin: 'Inde' }],
  ['kahawa-ya-congo-organic', 'Kahawa ya Congo Organic', "Cafés d'origine", 7, [E, L], ['fruits', 'céréales'], { color: '#1a1a1a', photo: 'kahawa-ya-congo-organic-original', url: 'kahawa-ya-congo-organic-capsule-cafe', origin: 'Est du Congo', organic: true }],
  ['arpeggio-decaffeinato', 'Arpeggio Decaffeinato', 'Décaféinés', 9, [R, E], ['cacao'], { color: '#6a2b74', photo: 'arpeggio-decaffeinato', url: 'arpeggio-decaffeinato-capsule-cafe', decaf: true }],
  ['ristretto-decaffeinato', 'Ristretto Decaffeinato', 'Décaféinés', 10, [R, E], ['torréfaction intense', 'fruité'], { color: '#26262b', photo: 'ristretto-decaffeinato', url: 'ristretto-decaffeinato-capsule-cafe', decaf: true }],
  ['volluto-decaffeinato', 'Volluto Decaffeinato', 'Décaféinés', 4, [E], ['fruité', 'délicat'], { color: '#9d6d20', photo: 'volluto-decaffeinato', url: 'volluto-decaffeinato-coffee-capsule', decaf: true }],
  ['vienna-lungo-decaffeinato', 'Vienna Lungo Decaffeinato', 'Décaféinés', 6, [L], ['céréales maltées'], { color: '#de7019', photo: 'vienna-lungo-decaffeinato', url: 'vienna-lungo-descaffeinato', decaf: true }],
  ['hazelnut-brownie-flavoured', 'Hazelnut Brownie Flavoured', 'Édition limitée', null, [L], ['brownie', 'noisette'], { url: 'festive-hazelnut-brownie-flavoured-ol', flavoured: true, limited: true }],
  ['blueberry-cheesecake-flavoured', 'Blueberry Cheesecake Flavoured', 'Édition limitée', null, [E, L], ['cheesecake', 'myrtille'], { url: 'festive-blueberry-cheesecake-flavoured-ol', flavoured: true, limited: true }],
  ['crystal-spark-espresso', 'Crystal Spark Espresso', 'Édition limitée', 8, [E], ['cacao', 'céréales'], { url: 'festive-crystal-spark-espresso-ol', limited: true }],
  ['pumpkin-spice-cake', 'Pumpkin Spice Cake', 'Édition limitée', null, [L], ['citrouille', 'épices'], { url: 'original-pumpkin-spice-cake', flavoured: true, limited: true }],
  ['cafe-joyeux', 'Café Joyeux', 'Collaboration', null, [], [], { url: 'cafe-joyeux' }],
];

const LOR = 'https://www.lorespresso.com/fr_fr/p/';
const lor = [
  ['ristretto', 'Ristretto', 11, [R], {}, 'ristretto-1'],
  ['espresso-ultimo', 'Espresso Ultimo', 13, [E], {}, 'espresso-ultimo'],
  ['capri-dolce-vita', 'Capri Dolce Vita', 12, [], {}, 'capri'],
  ['espresso-supremo', 'Espresso Supremo', 10, [E], {}, 'espresso-supremo-10'],
  ['espresso-or-absolu', 'Espresso Or Absolu', 9, [E], {}, 'espresso-or-absolu-090'],
  ['espresso-forza', 'Espresso Forza', 9, [E], {}, 'espresso-forza-09-4029339'],
  ['espresso-colombia', 'Espresso Colombia', 8, [E], { origin: 'Colombie' }, 'espresso-colombia-8'],
  ['espresso-brazil', 'Espresso Brazil', 8, [E], { origin: 'Brésil' }, 'maxi-pack-espresso-brazil-x50'],
  ['espresso-splendente', 'Espresso Splendente', 7, [E], {}, 'espresso-splendente-7'],
  ['santorini-sunset', 'Santorini Sunset', 7, [], {}, 'santorini'],
  ['espresso-delizioso', 'Espresso Delizioso', 5, [E], {}, 'espresso-delizioso'],
  ['espresso-biondo', 'Espresso Biondo', 4, [E], {}, 'espresso-biondo'],
  ['espresso-decaffeinato', 'Espresso Decaffeinato', 6, [E], { decaf: true }, 'espresso-decaffeinato-06'],
  ['lungo-profondo', 'Lungo Profondo', 8, [L], {}, 'lungo-profondo-08'],
  ['lungo-profondo-decaffeinato', 'Lungo Profondo Decaffeinato', 8, [L], { decaf: true }, 'capsules-de-cafe-lungo-profondo-decafeine'],
  ['lungo-elegante', 'Lungo Elegante', 6, [L], {}, 'lungo-elegante-06'],
  ['lungo-mattinata', 'Lungo Mattinata', 5, [L], {}, 'lungo-mattinata-05'],
  ['espresso-noisette', 'Espresso Noisette', null, [E], { flavoured: true, aromas: ['noisette'] }, 'espresso-noisette'],
  ['espresso-caramel', 'Espresso Caramel', null, [E], { flavoured: true, aromas: ['caramel'] }, 'espresso-caramel'],
  ['espresso-vanille', 'Espresso Vanille', null, [E], { flavoured: true, aromas: ['vanille'] }, 'espresso-vanille'],
  ['espresso-chocolat', 'Espresso Chocolat', null, [E], { flavoured: true, aromas: ['chocolat'] }, 'espresso-chocolat'],
  ['dubai-chocolate-style', 'Dubai Chocolate Style', null, [E], { flavoured: true, aromas: ['chocolat'] }, 'dubai-chocolate'],
  ['coconut-iced', 'Coconut Iced', null, [], { flavoured: true, aromas: ['noix de coco'], highlight: 'Pour boisson glacée' }, 'coconut-iced'],
  ['pumpkin-spice', 'Pumpkin Spice', null, [E], { flavoured: true, aromas: ['citrouille', 'épices'] }, 'pumpkin-spice'],
];

const SBX = 'https://www.starbucksathome.com/fr/';
// roast : blonde / medium (ambrée) / dark (brune)
const starbucks = [
  ['ristretto-shot', 'Ristretto Shot', 11, [R], 'dark', ['caramel', 'fruits rouges'], {}, 'produits/starbucks-ristretto-shot'],
  ['espresso-roast', 'Espresso Roast', null, [], 'dark', [], {}, 'produits/starbucks-espresso-roast-nespresso'],
  ['decaf-espresso-roast', 'Decaf Espresso Roast', 11, [E], 'dark', ['caramel'], { decaf: true }, 'produits/decaf-espresso-roast-nespresso'],
  ['italian-style-roast', 'Italian Style Roast', null, [], 'dark', [], {}, 'produits/starbucks-italian-style-roast'],
  ['caffe-verona', 'Caffè Verona', null, [], 'dark', [], {}, 'produits/starbucks-caffe-verona-nespresso'],
  ['single-origin-sumatra', 'Single-Origin Sumatra', null, [], 'dark', [], { origin: 'Sumatra, Indonésie' }, 'produits/starbucks-sumatra'],
  ['pike-place-roast', 'Pike Place Roast', null, [], 'medium', [], {}, 'produits/starbucks-pike-place-nespresso'],
  ['house-blend', 'House Blend', null, [], 'medium', [], {}, 'produits/starbucks-house-blend-nespresso'],
  ['breakfast-blend', 'Breakfast Blend', null, [], 'medium', [], {}, 'produits/starbucks-breakfast-blend'],
  ['single-origin-colombia', 'Single-Origin Colombia', null, [], 'medium', [], { origin: 'Colombie' }, 'produits/single-origin-colombia-nespresso'],
  ['blonde-espresso-roast', 'Blonde Espresso Roast', null, [], 'blonde', [], {}, 'produits/blonde-espresso-roast-nespresso'],
  ['sunny-day-blend', 'Sunny Day Blend', null, [], 'blonde', [], {}, 'produits/starbucks-sunny-day-blend-nespresso'],
  ['single-origin-guatemala', 'Single-Origin Guatemala', null, [], 'blonde', [], { origin: 'Guatemala' }, 'produits/single-origin-guatemala-nespresso'],
  ['creamy-vanilla', 'Creamy Vanilla', 5, [E], 'blonde', ['vanille', 'biscuit'], { flavoured: true }, 'products/starbucks-vanille-nespresso'],
  ['smooth-caramel', 'Smooth Caramel', 5, [E], 'blonde', ['caramel', 'vanille'], { flavoured: true }, 'products/starbucks-caramel-nespresso'],
  ['chocolate-hazelnut', 'Chocolate Hazelnut', 5, [E], 'blonde', ['chocolat', 'noisette'], { flavoured: true }, 'products/starbucks-chocolate-hazelnut'],
];

const CN = 'https://www.cartenoire.fr/fr/cafes/capsules/nespresso/';
const carteNoire = [
  ['ristretto-puissant-12', 'Ristretto Puissant N°12', 12, [R], [], {}, 'ristretto'],
  ['espresso-puissant-11', 'Espresso Puissant N°11', 11, [E], [], {}, 'puissant.60730'],
  ['espresso-intense-9', 'Espresso Intense N°9', 9, [E], [], {}, 'intense.60727'],
  ['cuvee-de-caractere', 'Cuvée de Caractère', 8, [E], ['fruité', 'noisette'], { origin: 'Assemblage Arabica et Robusta' }, 'cuvee-de-caractere'],
  ['l-iconique', "L'Iconique", 8, [L], ['fruité', 'céréales grillées'], {}, 'iconique'],
  ['espresso-classique-7', 'Espresso Classique N°7', 7, [E], [], {}, 'classique.60728'],
  ['espresso-demi-cafeine', 'Espresso Demi-Caféiné', 7, [E], ['céréales grillées', 'fruits secs'], { highlight: 'Mi-caféiné : 50 % de café décaféiné' }, 'demi-cafeine'],
  ['lungo-classique-6', 'Espresso Lungo Classique N°6', 6, [L], [], {}, 'lungo.60729'],
  ['espresso-decafeine', 'Espresso Décaféiné', 6, [E], ['caramel', 'céréales grillées'], { decaf: true }, 'decafeine.60732'],
];

const LVZ = 'https://www.lavazza.fr/fr/capsules-dosettes-cafe/';
const lavazza = [
  ['espresso-maestro-ristretto', 'Espresso Maestro Ristretto', 'Espresso Maestro', 12, [], [], {}, 'capsules-espresso-maestro-ristretto-compatibles-avec-les-machines-nespresso'],
  ['tales-of-napoli', 'Tales of Napoli', 'Tales of Italy', 12, [E], ['cacao'], { highlight: 'Torréfaction longue' }, 'tales-of-napoli'],
  ['espresso-maestro-intenso', 'Espresso Maestro Intenso', 'Espresso Maestro', 11, [E], ['boisé', 'cacao'], {}, 'nespresso-compatible-espresso-maestro-intenso'],
  ['tales-of-roma', 'Tales of Roma', 'Tales of Italy', 10, [E], ['noisette', 'chocolat noir'], { origin: 'Assemblage Arabica et Robusta' }, 'tales-of-roma'],
  ['espresso-maestro-classico', 'Espresso Maestro Classico', 'Espresso Maestro', 9, [E], ['céréales', 'biscuit'], {}, 'nespresso-compatible-espresso-maestro-classico'],
  ['qualita-oro', 'Qualità Oro', 'Qualità Oro', 8, [E], ['fruité', 'floral'], { origin: "Arabica d'Amérique centrale et du Sud" }, 'nespresso-compatible-qualita-oro'],
  ['decaffeinato', 'Décaféiné', null, 7, [E], [], { decaf: true }, 'nespresso-compatible-decaf'],
  ['espresso-maestro-lungo', 'Espresso Maestro Lungo', 'Espresso Maestro', 5, [], [], {}, 'nespresso-compatible-espresso-maestro-lungo'],
];

// illy : intensité donnée sur 9 (échelle illy) → gardée en « particularité », pas convertie.
const ILLY = 'https://www.illy.com/fr-fr/cafe/capsules-compatibles/';
const illy = [
  ['classico', 'Classico', 5, [E], ['caramel', "fleur d'oranger", 'jasmin'], {}, 'capsules-cafe-torrefaction-classique/8908ME.html'],
  ['intenso', 'Intenso', 7, [E], ['cacao', 'fruits secs'], {}, 'capsules-cafe-torrefaction-intense/8911ME.html'],
  ['forte', 'Forte', 9, [E], ['pain grillé', 'chocolat noir'], {}, 'capsules-cafe-torrefaction-forte/8909ME.html'],
  ['decaffeinato', 'Décaféiné', 5, [E], ['caramel', 'chocolat', 'pain grillé'], { decaf: true }, 'capsules-cafe-torrefaction-decafeine/8994ST.html'],
  ['classico-lungo', 'Classico Lungo', 5, [L], ['caramel', "fleur d'oranger", 'jasmin'], {}, 'capsules-cafe-allonge/8910ST.html'],
  ['intenso-lungo', 'Intenso Lungo', 7, [L], ['cacao', 'fruits secs'], {}, 'capsules-cafe-intenso-allonge/24072ME.html'],
];

// Segafredo : intensité lue sur la boîte (échelle non précisée) → gardée en « particularité ».
const SEG = 'https://www.segafredo.fr/nos-produits/a-la-maison/cafe/capsules-compatibles-nespresso/';
const segafredo = [
  ['intenso', 'Intenso', 'Espresso', 12, ['chocolat', 'agrumes'], { origin: "Arabicas d'Amérique du Sud et d'Afrique" }, 'capsules-intenso'],
  ['ristretto', 'Ristretto', 'Espresso', 11, [], {}, 'capsules-ristretto'],
  ['classico', 'Classico', 'Espresso', 10, ['miel', 'caramel'], {}, 'capsules-classico'],
  ['100-arabica', '100% Arabica', 'Espresso', 9, ['floral', 'chocolat'], { origin: '100 % Arabica' }, 'capsules-100-arabica'],
  ['peru', 'Perù', 'Le Origini', 9, ['chocolat'], { origin: 'Pérou (100 % Arabica)' }, 'capsules-peru'],
  ['brasile', 'Brasile', 'Le Origini', 8, ['miel'], { origin: 'Brésil (100 % Arabica)' }, 'capsules-brasile'],
];

// Kimbo : intensité sur 13, torréfaction et notes aromatiques indiquées sur kimbo.it.
const KIMBO = 'https://kimbo.it/products/';
const kimbo = [
  ['espresso-napoli', 'Barista Espresso Napoli', 10, [E], ['biscuit', 'fleurs', 'vanille'], { origin: 'Arabica et Robusta', highlight: 'Torréfaction moyenne-foncée' }, 'capsule-compatibili-nespresso®-original-in-alluminio-espresso-napoli'],
  ['barista-ristretto', 'Espresso Barista Ristretto', 12, [R], ['chocolat noir', 'caramel', 'malt'], { roast: 'dark' }, 'capsule-compatibili-nespresso®-original-in-alluminio-barista-ristretto'],
  ['barista-100-arabica', 'Espresso Barista 100% Arabica', 9, [E], ['fleurs', 'pain grillé', 'miel'], { origin: '100 % Arabica', highlight: 'Torréfaction moyenne-foncée' }, 'capsule-compatibili-nespresso®-original-in-alluminio-espresso-barista-100-arabica'],
  ['barista-decaf', 'Espresso Barista Decaf', 9, [E], ['châtaigne séchée'], { roast: 'medium', decaf: true }, 'capsule-compatibili-nespresso®-original-in-alluminio-espresso-barista-decaf'],
  ['macinato-fresco', 'Macinato Fresco', 11, [E], ['chocolat noir', 'épices'], { roast: 'dark' }, 'capsule-compatibili-nespresso®-original-in-alluminio-macinato-fresco'],
];

// Caffè Borbone : gamme Respresso (la gamme aluminium n'a qu'un petit dessin de capsule : non retenue).
const BORBONE = 'https://www.caffeborbone.com/en-eu/products/';
const borbone = [
  ['respresso-gold', 'Respresso Gold', ['doux', 'raffiné'], {}, 'gold-blend-capsules-borbone-respresso-compatible-nespresso'],
  ['respresso-red', 'Respresso Red', ['corsé', 'crémeux'], {}, 'red-blend-capsules-borbone-respresso-compatible-nespresso'],
  ['respresso-blue', 'Respresso Blue', ['équilibré', 'corsé'], {}, 'blue-blend-capsules-borbone-respresso-compatible-nespresso'],
  ['respresso-black', 'Respresso Black', ['intense', 'robuste'], {}, 'black-blend-capsules-borbone-respresso-compatible-nespresso'],
  ['respresso-dek-green', 'Respresso Dek Green', [], { decaf: true }, 'dek-green-blend-capsules-borbone-respresso-compatible-nespresso'],
  ['respresso-light', 'Respresso Light', [], { highlight: 'Mi-caféiné : 50 % Blue, 50 % Dek' }, 'capsules-light-blend-borbone-respresso-compatible-nespresso'],
];

// ---------------------------------------------------------------------
const products = [];
const add = (p) => products.push({
  system: 'original', range: null, intensity: null, cup_sizes: [], aromas: [], origin: null, roast: null, highlight: null,
  decaf: false, flavoured: false, organic: false, limited: false, color: null, photo_url: null, official_image: null, ...p,
});

for (const [id, name, range, intensity, cups, aromas, x] of nespresso) {
  add({ id: 'nespresso-' + id, brand_id: 'nespresso', name, range, intensity, cup_sizes: cups, aromas,
    origin: x.origin ?? null, highlight: x.highlight ?? null, decaf: !!x.decaf, flavoured: !!x.flavoured,
    organic: !!x.organic, limited: !!x.limited, color: x.color ?? null,
    photo_url: x.photo ? `photos/${x.photo}.jpg` : null, shop_url: NES + x.url });
}
// Reste du catalogue Nespresso (Original et Vertuo), relevé par scripts/fetch-nespresso.mjs :
// toutes les capsules qui ne sont pas déjà décrites à la main ci-dessus.
const NES_FILE = new URL('../data/nespresso-catalog.json', import.meta.url);
const nesCatalog = existsSync(NES_FILE) ? JSON.parse(readFileSync(NES_FILE, 'utf8')).items : [];
const handSlugs = new Set(nespresso.map((x) => x[6].url));
const slugify = (t) => t.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase().replace(/®/g, '').replace(/&/g, ' and ').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
const CUPS = { original: { Ristretto: R, Espresso: E, Lungo: L, Recette: 'Recette au lait' }, vertuo: { Recette: 'Recette au lait' } };
const lowerFirst = (t) => (t.charAt(0).toLowerCase() + t.slice(1)).replace(/^epic/, 'épic'); // « Epicé » sans accent sur le site
for (const x of nesCatalog) {
  if (x.system === 'original' && handSlugs.has(x.slug)) continue;
  const starbucks = /^starbucks/i.test(x.name); // collaborations Starbucks vendues par Nespresso
  const name = starbucks ? x.name.replace(/®/g, '').replace(/^Starbucks\s*/i, '').trim() : x.name;
  const brand = starbucks ? 'starbucks' : 'nespresso';
  const aromas = x.aromas.map(lowerFirst);
  const highlight = [/half caffeinato/i.test(x.name) ? 'Mi-caféiné' : null, /compostable/.test(x.slug) ? 'Capsule compostable' : null,
    /over ice/i.test(x.name) ? 'Pour boisson glacée' : null].filter(Boolean).join(' · ') || null;
  add({ id: `${brand}-${x.system === 'vertuo' ? 'vertuo-' : ''}${slugify(name)}`, brand_id: brand, system: x.system, name,
    range: starbucks ? 'Starbucks by Nespresso' : x.range, intensity: x.intensity,
    cup_sizes: x.cups.map((c) => CUPS[x.system][c] ?? c), aromas, highlight,
    decaf: /decaffeinato/i.test(x.name) || x.range === 'Décaféinés', organic: /organic/i.test(x.name),
    flavoured: /flavour/i.test(x.name) || aromas.some((a) => /saveur|soupçon|noix de coco|yuzu|lavande|hibiscus|ginseng|vanille|amande/.test(a)),
    official_image: x.image, shop_url: x.url });
}

for (const [id, name, intensity, cups, x, slug] of lor) {
  add({ id: 'lor-' + id, brand_id: 'lor', name, range: x.flavoured ? 'Aromatisés' : 'Capsules classiques', intensity,
    cup_sizes: cups, aromas: x.aromas ?? [], origin: x.origin ?? null, highlight: x.highlight ?? null,
    decaf: !!x.decaf, flavoured: !!x.flavoured, shop_url: LOR + slug });
}
for (const [id, name, intensity, cups, roast, aromas, x, path] of starbucks) {
  add({ id: 'starbucks-' + id, brand_id: 'starbucks', name, range: 'Starbucks by Nespresso', intensity,
    cup_sizes: cups, aromas, roast, origin: x.origin ?? null, decaf: !!x.decaf, flavoured: !!x.flavoured, shop_url: SBX + path });
}
for (const [id, name, intensity, cups, aromas, x, slug] of carteNoire) {
  add({ id: 'carte-noire-' + id, brand_id: 'carte-noire', name, range: 'Capsules compatibles Nespresso', intensity,
    cup_sizes: cups, aromas, origin: x.origin ?? null, highlight: x.highlight ?? null, decaf: !!x.decaf, shop_url: CN + slug });
}
for (const [id, name, range, intensity, cups, aromas, x, slug] of lavazza) {
  add({ id: 'lavazza-' + id, brand_id: 'lavazza', name, range, intensity, cup_sizes: cups, aromas,
    origin: x.origin ?? null, highlight: x.highlight ?? null, decaf: !!x.decaf, shop_url: LVZ + slug });
}
for (const [id, name, i9, cups, aromas, x, path] of illy) {
  add({ id: 'illy-' + id, brand_id: 'illy', name, range: cups[0] === L ? 'Lungo' : 'Espresso', cup_sizes: cups, aromas,
    origin: '100 % Arabica', highlight: `Intensité ${i9} sur 9 (échelle illy)`, decaf: !!x.decaf, shop_url: ILLY + path });
}
for (const [id, name, range, i, aromas, x, slug] of segafredo) {
  add({ id: 'segafredo-' + id, brand_id: 'segafredo', name, range, cup_sizes: [E], aromas,
    origin: x.origin ?? null, highlight: `Intensité ${i} indiquée sur la boîte`, shop_url: SEG + slug });
}
for (const [id, name, intensity, cups, aromas, x, handle] of kimbo) {
  add({ id: 'kimbo-' + id, brand_id: 'kimbo', name, range: 'Capsules aluminium', intensity, cup_sizes: cups, aromas,
    origin: x.origin ?? null, roast: x.roast ?? null, highlight: x.highlight ?? null, decaf: !!x.decaf, shop_url: KIMBO + encodeURIComponent(handle) });
}
for (const [id, name, aromas, x, handle] of borbone) {
  add({ id: 'borbone-' + id, brand_id: 'borbone', name, range: 'Respresso', aromas,
    highlight: x.highlight ?? null, decaf: !!x.decaf, shop_url: BORBONE + handle });
}

// Photo officielle (affichée depuis le site de la marque) quand il n'y a pas de photo locale.
// Liste produite par scripts/find-official-images.mjs.
const OFFICIAL = existsSync(new URL('../data/official-images.json', import.meta.url))
  ? JSON.parse(readFileSync(new URL('../data/official-images.json', import.meta.url), 'utf8').replace(/^﻿/, '')) : {};

// Intensité sur 13 pour les capsules dont la fiche officielle française ne la donne pas.
// Valeur publiée ailleurs (autre pays, distributeur officiel, revendeur) : [valeur, false].
// Valeur estimée (même base de café, échelle de la marque ramenée sur 13, gamme de torréfaction) : [valeur, true].
// Relevé web du 2026-11-09.
const INTENSITY_FILL = {
  // Nespresso Original
  'nespresso-vaniglia': [6, false], 'nespresso-caramello': [6, true], 'nespresso-nocciola': [6, true],
  'nespresso-chiaro': [5, false], 'nespresso-scuro': [7, false], 'nespresso-corto': [11, false], 'nespresso-cafe-joyeux': [5, false],
  'nespresso-hazelnut-brownie-flavoured': [6, true], 'nespresso-blueberry-cheesecake-flavoured': [6, true], 'nespresso-pumpkin-spice-cake': [6, true],
  'nespresso-coconut-vanilla-flavour-over-ice': [6, true], 'nespresso-yuzu-vanilla-over-ice': [6, true],
  // Nespresso Vertuo
  'nespresso-vertuo-hawaii-kona': [6, false], 'nespresso-vertuo-bianco-forte': [9, true], 'nespresso-vertuo-bianco-doppio': [6, true],
  'nespresso-vertuo-golden-caramel': [6, true], 'nespresso-vertuo-roasted-hazelnut': [6, true], 'nespresso-vertuo-sweet-vanilla': [6, true],
  'nespresso-vertuo-sweet-vanilla-decaffeinato': [6, true], 'nespresso-vertuo-active': [7, true], 'nespresso-vertuo-ginseng-delight': [7, true],
  'nespresso-vertuo-oatly-barista-edition': [7, true], 'nespresso-vertuo-coconut-vanilla-flavour-over-ice': [6, true],
  'nespresso-vertuo-yuzu-vanilla-over-ice': [6, true], 'nespresso-vertuo-french-lavender-and-vanilla-decaffeinato': [5, true],
  'nespresso-vertuo-sweet-almond-and-hibiscus': [6, true],
  // L'Or aromatisés (5 sur les sites L'Or britannique et irlandais)
  'lor-espresso-caramel': [5, false], 'lor-espresso-vanille': [5, false], 'lor-espresso-noisette': [5, true], 'lor-espresso-chocolat': [5, true],
  'lor-dubai-chocolate-style': [5, true], 'lor-coconut-iced': [5, true], 'lor-pumpkin-spice': [5, true],
  // Starbucks by Nespresso (Original) — Starbucks : blonde 4-6, medium 7-9, dark 10-12
  'starbucks-espresso-roast': [11, false], 'starbucks-italian-style-roast': [11, false], 'starbucks-caffe-verona': [10, false],
  'starbucks-single-origin-sumatra': [10, false], 'starbucks-pike-place-roast': [7, false], 'starbucks-single-origin-colombia': [7, false],
  'starbucks-blonde-espresso-roast': [6, false], 'starbucks-house-blend': [8, true], 'starbucks-breakfast-blend': [7, true],
  'starbucks-sunny-day-blend': [6, true], 'starbucks-single-origin-guatemala': [6, true],
  // illy : échelle sur 9 ramenée sur 13
  'illy-classico': [7, true], 'illy-intenso': [10, true], 'illy-forte': [13, true], 'illy-decaffeinato': [7, true],
  'illy-classico-lungo': [7, true], 'illy-intenso-lungo': [10, true],
  // Segafredo : intensité sur 13 indiquée par la marque
  'segafredo-intenso': [12, false], 'segafredo-ristretto': [11, false], 'segafredo-classico': [10, false],
  'segafredo-100-arabica': [9, false], 'segafredo-peru': [9, false], 'segafredo-brasile': [8, false],
  // Borbone : échelle sur 10 ramenée sur 13
  'borbone-respresso-black': [13, true], 'borbone-respresso-red': [12, true], 'borbone-respresso-blue': [10, true],
  'borbone-respresso-gold': [9, true], 'borbone-respresso-dek-green': [10, true], 'borbone-respresso-light': [10, true],
};

for (const p of products) {
  p.official_image = OFFICIAL[p.id] ?? p.official_image;
  p.intensity_estimated = false;
  if (!p.intensity && INTENSITY_FILL[p.id]) [p.intensity, p.intensity_estimated] = INTENSITY_FILL[p.id];
  if (/^Intensité \d+ indiquée sur la boîte$/.test(p.highlight ?? '')) p.highlight = null;
  if (!p.photo_url && p.official_image) p.photo_url = p.official_image;
}

// Contrôles : identifiants uniques, URL en https.
const seen = new Set();
for (const p of products) {
  if (seen.has(p.id)) throw new Error('Identifiant en double : ' + p.id);
  seen.add(p.id);
  if (!/^https:\/\//.test(p.shop_url)) throw new Error('Lien invalide : ' + p.id);
}

// Mode test : node scripts/build-seed.mjs --json  → affiche le catalogue en JSON.
if (process.argv.includes('--json')) {
  process.stdout.write(JSON.stringify({ brands: BRANDS, products: products.map((p) => ({ ...p, verified_on: VERIFIED, active: true })) }));
  process.exit(0);
}

// ---------------------------------------------------------------------
const q = (v) => v === null || v === undefined ? 'null' : `'${String(v).replace(/'/g, "''")}'`;
const arr = (a) => `array[${a.map(q).join(', ')}]::text[]`;
const b = (v) => (v ? 'true' : 'false');

let sql = `-- Baristina V1 — catalogue (généré par scripts/build-seed.mjs, ne pas modifier à la main)
-- ${products.length} capsules, ${BRANDS.length} marques, vérifiées le ${VERIFIED}.
begin;
`;
for (const br of BRANDS) {
  sql += `insert into public.brands (id, name, website, sort) values (${q(br.id)}, ${q(br.name)}, ${q(br.website)}, ${br.sort})
  on conflict (id) do update set name = excluded.name, website = excluded.website, sort = excluded.sort;\n`;
}
sql += `\n-- Les références retirées des sites ne sont pas supprimées (collections existantes) : elles sont masquées.\n`;
sql += `update public.products set active = false where id not in (${products.map((p) => q(p.id)).join(', ')});\n\n`;
for (const p of products) {
  sql += `insert into public.products (id, brand_id, system, name, range, intensity, cup_sizes, aromas, origin, roast, highlight, decaf, flavoured, organic, limited, color, photo_url, shop_url, verified_on, active, intensity_estimated)
values (${q(p.id)}, ${q(p.brand_id)}, ${q(p.system)}, ${q(p.name)}, ${q(p.range)}, ${p.intensity ?? 'null'}, ${arr(p.cup_sizes)}, ${arr(p.aromas)}, ${q(p.origin)}, ${q(p.roast)}, ${q(p.highlight)}, ${b(p.decaf)}, ${b(p.flavoured)}, ${b(p.organic)}, ${b(p.limited)}, ${q(p.color)}, ${q(p.photo_url)}, ${q(p.shop_url)}, '${VERIFIED}', true, ${b(p.intensity_estimated)})
on conflict (id) do update set brand_id = excluded.brand_id, system = excluded.system, name = excluded.name, range = excluded.range, intensity = excluded.intensity, cup_sizes = excluded.cup_sizes, aromas = excluded.aromas, origin = excluded.origin, roast = excluded.roast, highlight = excluded.highlight, decaf = excluded.decaf, flavoured = excluded.flavoured, organic = excluded.organic, limited = excluded.limited, color = excluded.color, photo_url = coalesce(public.products.photo_url, excluded.photo_url), shop_url = excluded.shop_url, verified_on = excluded.verified_on, active = true, intensity_estimated = excluded.intensity_estimated;\n`;
}
sql += 'commit;\n';

writeFileSync(new URL('../supabase/seed.sql', import.meta.url), sql);
console.log(`seed.sql : ${products.length} capsules, ${BRANDS.length} marques`);
