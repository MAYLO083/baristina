# Baristina V1

Application web mobile : scanner, catalogue et collection de capsules compatibles **Nespresso Original**
(Nespresso, L'Or, Starbucks, Carte Noire, Lavazza, Café Royal).

- **Supabase** : comptes, collection, catalogue, photos de la communauté.
- **IA de scan gratuite, sur le téléphone** : aucune IA payante, aucune photo envoyée sur internet.
- **GitHub Pages** : héberge seulement les fichiers du site (gratuit, aucune donnée).

```
index.html                     page unique
css/app.css                    styles
js/app.js                      application
js/scan.js                     reconnaissance de capsule (IA locale)
js/config.js                   adresse + clé publique Supabase  ← à remplir
data/scan-index.json           empreintes des capsules pour le scan (généré)
photos/                        tes photos de capsules
scripts/build-seed.mjs         source du catalogue → génère supabase/seed.sql
scripts/build-scan-index.mjs   génère data/scan-index.json
supabase/schema.sql            tables, sécurité, pseudos uniques, stockage
supabase/seed.sql              catalogue (généré)
```

## 1. Créer le projet Supabase (gratuit)

1. Crée un compte sur supabase.com → **New project** (région : Europe, par ex. Paris ou Francfort).
2. **SQL Editor** → colle et exécute `supabase/schema.sql`, puis `supabase/seed.sql`.
3. **Project Settings → API** : copie *Project URL* et la clé *anon / publishable* dans `js/config.js`.
   ⚠️ Ne mets jamais la clé `service_role` dans le site.

## 2. Connexions

**Authentication → URL Configuration**
- *Site URL* : l'adresse de ton site GitHub Pages (ex. `https://ton-pseudo.github.io/baristina/`)
- *Redirect URLs* : la même adresse (+ `http://localhost:8000/` si tu testes en local)

**Authentication → Providers**
- **Email** : activé, avec *Confirm email* activé. Dans *Password*, longueur minimale : 8.
- **Google** : crée un « ID client OAuth » (application Web) dans Google Cloud Console.
  URI de redirection autorisée : `https://<ton-projet>.supabase.co/auth/v1/callback`.
  Colle l'ID client et le secret dans Supabase.
- **Apple** : nécessite un compte Apple Developer (99 $/an). Crée un *Services ID* et une *clé Sign in with Apple*,
  avec la même URL de callback. Tant qu'Apple n'est pas configuré, le bouton Apple affichera une erreur.

Le pseudo est unique (sans tenir compte des majuscules) : vérifié pendant la saisie, puis garanti par la base de données.
Les comptes Google / Apple choisissent leur pseudo à la première connexion.

## 3. Scan : IA gratuite intégrée

Rien à configurer. Le scan fonctionne entièrement dans le navigateur du téléphone, sans compte :
- **apparence** : modèle open source CLIP (`Xenova/clip-vit-base-patch32`, via Transformers.js), comparé aux photos de référence ;
- **couleur** de la capsule, comparée aux photos de référence ;
- **nom imprimé**, lu par OCR (Tesseract.js) et comparé au catalogue — c'est l'indice principal pour les marques sans photo.

Le premier scan télécharge le modèle (environ 90 Mo, conseillé en Wi-Fi) ; il reste ensuite en cache.
Un scan prend 1 à 3 secondes. L'app propose les 3 capsules les plus probables : l'utilisateur confirme.

**Après un changement du catalogue ou de nouvelles photos de référence**, régénère l'index :

```bash
cd scripts && npm install && npm run seed && npm run index
```

Références utilisées : tes photos (`photos/`) et les **photos officielles des marques**, dont la liste est dans
`data/official-images.json` (générée par `node scripts/find-official-images.mjs`), plus les autres photos de la
galerie officielle de chaque capsule — souvent la **capsule seule**, indispensable pour reconnaître une capsule
photographiée sans sa boîte — listées dans `data/reference-images.json` (`node scripts/find-reference-images.mjs`). Les photos officielles sont
affichées dans le catalogue directement depuis le site de la marque (pas de copie) ; pour l'IA, elles sont
téléchargées dans `scripts/.cache` (non publié) et seules leurs empreintes vont dans `data/scan-index.json`.
Si une marque change ou bloque une image, l'app affiche l'illustration dessinée à la place.

Fiabilité : la reconnaissance distingue bien les marques et les couleurs ; entre deux boîtes presque identiques
d'une même marque, c'est la lecture du nom qui tranche — d'où l'intérêt de photographier le côté où le nom est écrit.

## 4. Mettre le site en ligne (GitHub Pages)

1. Crée un dépôt GitHub et envoie-y tout le dossier.
2. **Settings → Pages** → *Deploy from a branch* → `main` / `(root)`.
3. Mets l'adresse obtenue dans Supabase (étape 2).

## 5. Profils publics

Chaque membre a un profil public (`#/profil/<pseudo>`) : photo, biographie (300 caractères max.), date d'inscription,
statistiques, coups de cœur et collection complète avec étoiles et notes de dégustation. L'onglet **Membres** permet
de rechercher un pseudo. L'e-mail reste privé.
- Photos de profil : bucket public `avatars`, une photo carrée de 320 px par membre, dans le dossier `<id du compte>/`.
- Sécurité (`supabase/schema.sql`) : tout le monde peut lire profils et collections ; seul le propriétaire, connecté,
  peut modifier son profil, sa collection et sa photo.
- Modération : pour retirer une photo ou une biographie inappropriée, mets `avatar_path` ou `bio` à vide dans
  Table Editor → `profiles` (et supprime le fichier dans Storage → `avatars`).

## 6. Gérer le contenu

- **Valider une photo proposée** : Storage → `submissions`. Si elle convient, téléverse-la dans le bucket public
  `capsules`, copie son URL publique dans `products.photo_url` (Table Editor), puis passe la ligne de
  `photo_submissions` en `approved`. Pour qu'elle serve aussi au scan, copie-la dans `photos/` et régénère l'index (étape 3).
- **Mettre à jour le catalogue** : modifie `scripts/build-seed.mjs`, lance `node scripts/build-seed.mjs`, puis exécute
  le nouveau `supabase/seed.sql`. Les capsules retirées sont masquées, pas supprimées (les collections restent intactes).
  Règle : uniquement des informations vérifiées sur le site officiel de la marque ; sinon, laisser vide.

## Limites de l'offre gratuite Supabase

500 Mo de base de données, 1 Go de stockage.
Un projet gratuit **se met en pause après 7 jours sans activité** : il suffit de le relancer depuis le tableau de bord.

## Mentions

Application indépendante, non affiliée à Nestlé Nespresso SA ni aux autres marques citées.
Les noms de produits sont des marques de leurs propriétaires. Les liens « boutique officielle » renvoient vers les sites des marques.
