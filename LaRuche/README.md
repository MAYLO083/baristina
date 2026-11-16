# La Ruche

Extraction shooter PvE en coopération (1 à 4 joueurs), vue à la troisième personne, sous Unreal Engine 5.
Aucun serveur payant : un joueur héberge la partie sur son PC, les autres le rejoignent.

Version actuelle : **V1 prototype**. Les décors et les personnages sont des formes simples
(cubes, sphères, cylindres) et il n'y a pas encore de son : le but de cette version est de tester
les règles du jeu.

## Installation (à faire une seule fois)

1. **Unreal Engine 5** (5.4 ou plus récent) via l'Epic Games Launcher.
2. **Visual Studio 2022 Community** (gratuit) : https://visualstudio.microsoft.com/fr/vs/community/
   Pendant l'installation, cocher **« Développement de jeux en C++ »**.
   Le jeu est écrit en C++ : sans Visual Studio, Unreal ne peut pas le compiler.
3. Télécharger ce dossier `LaRuche` (bouton vert **Code > Download ZIP** sur GitHub, puis décompresser).

## Lancer le jeu dans Unreal

1. Double-cliquer sur `LaRuche.uproject`.
2. Si Unreal demande quelle version utiliser, choisir la vôtre.
3. Unreal annonce que des modules manquent et propose de les compiler : répondre **Oui**.
   La première compilation prend quelques minutes.
4. Dans l'éditeur, cliquer sur **Play** (le triangle vert en haut).

Si la compilation échoue, ouvrir le fichier `Saved/Logs/LaRuche.log`, copier les lignes
contenant `error` et les envoyer pour correction.

## Jouer à plusieurs

- **L'hôte** choisit « Héberger une partie ». Son adresse s'affiche dans la planque.
- **Les amis** choisissent « Rejoindre une partie » et tapent cette adresse.
- **Même réseau (même box)** : l'adresse affichée suffit.
- **À distance** : installer Tailscale (gratuit) sur chaque PC, se connecter au même compte ou
  réseau, et utiliser l'adresse Tailscale de l'hôte (elle commence par `100.`).
  Aucun réglage de box n'est nécessaire.
- Au premier lancement, autoriser le jeu dans le pare-feu Windows (port 7777, UDP).

Pour tester le multijoueur seul dans l'éditeur : flèche à côté de Play > **Nombre de joueurs : 2**
et **Mode réseau : Jouer en tant que serveur d'écoute**.

## Fabriquer le fichier à partager avec les amis

Les amis n'ont besoin ni d'Unreal ni de Visual Studio, seulement du jeu « empaqueté ».

- Dans l'éditeur : **Plateformes > Windows > Empaqueter le projet**, choisir un dossier de sortie.
- Ou double-cliquer sur `Package.bat` (corriger d'abord le chemin d'Unreal en haut du fichier
  si besoin). Le jeu arrive dans le dossier `Build`.

Compresser le dossier obtenu en `.zip`, puis sur GitHub : **Releases > Draft a new release**,
glisser le `.zip`, publier. Le lien de la release est le lien de téléchargement public.

## Commandes

| Touche | Action |
|---|---|
| ZQSD (ou WASD) | Se déplacer |
| Souris | Viser |
| Clic gauche | Tirer |
| R | Recharger |
| E | Interagir (ramasser, fouiller, relever, s'extraire) |
| Maj gauche | Courir (bruyant) |
| C ou Ctrl | S'accroupir (silencieux, moins visible) |
| Espace | Sauter |
| 1 / 2 / 3 / 4 | Soin / Leurre / Phéromones / Sérum |
| Tab | Sac en raid ; coffre, équipement et atelier dans la planque |
| P ou Échap | Pause |
| Page haut / bas | Sensibilité de la souris |

## Comment se déroule une partie

1. **La planque** : Tab ouvre le coffre (à gauche), l'équipement (au centre) et l'atelier (à droite).
   L'hôte choisit le contrat et le niveau de menace, puis lance le raid.
2. **Le raid** : fouillez les conteneurs, remplissez le contrat, surveillez la jauge d'**Éveil** en
   haut de l'écran. Elle monte avec le temps et avec le bruit (tirs, course, nids détruits).
3. **L'extraction** : trois sorties (égouts, sas, hélico). Elles ferment une à une quand l'Éveil monte.
4. **Le retour** : ce que vous portez est conservé si vous sortez. Sinon, seule la poche sécurisée
   (2 places) est gardée.

### Les quatre paliers d'Éveil

| Palier | Effet |
|---|---|
| 1 Dormante | Peu de créatures |
| 2 Agitée | Des cloisons organiques bouchent des rues (destructibles) |
| 3 Éveillée | Égouts fermés, nodules rares, les larves explosent en acide |
| 4 Frénésie | La Matriarche sort, seul l'hélico reste |

### Les créatures

| Créature | Particularité |
|---|---|
| Larve | Faible, en nombre |
| Pisteur | Aveugle, suit le bruit. Accroupi et immobile, il vous ignore |
| Sentinelle | Immobile. Si elle vous voit plus d'une seconde, elle hurle et alerte la zone |
| Carapace | Blindée de face : tirez dans le dos, ou utilisez la greffe acide |
| Glaneuse | Vole un objet du sac et fuit vers un nid. Détruire le nid rend le butin |
| Hôte | Ancien glaneur, tire à distance, lâche des munitions |
| Cocon piégé | Ressemble à un conteneur, mais il « respire » |
| Matriarche | Boss du palier 4, pond des larves, lâche son cœur |

## Ce qui n'est pas encore dans la V1

- Vrais modèles 3D, animations, sons et musique.
- Intérieurs de bâtiments (la ville est faite de blocs pleins).
- Raid de secours après la mort de toute l'équipe.
- Spore éclaireur (un joueur mort regarde simplement un coéquipier).
- Objectifs à porter à deux, améliorations de la planque.
- Liste d'amis et invitations (connexion par adresse IP uniquement).

## Organisation du code

Tout est dans `Source/LaRuche` :

| Fichier | Rôle |
|---|---|
| `LRItems` | Tous les objets, recettes, butin, règles du sac |
| `LRCity` | Génération de la ville et chemins des créatures |
| `LRCharacter` | Le joueur : tir, vie, interactions |
| `LREnemy` | Les créatures et leur comportement |
| `LRGameMode` | Règles du raid : Éveil, apparitions, contrats, fin de raid |
| `LRGameState` | État partagé entre les joueurs, construction du décor |
| `LRWorldActors` | Conteneurs, objets au sol, extractions, nids, cloisons |
| `LRPlayerController` | Clavier, souris, lien avec la sauvegarde |
| `LRHUD` | Toute l'interface |
| `LRGameInstance` | Sauvegarde locale |

Les valeurs d'équilibrage (dégâts, vie, vitesses, prix) sont en haut de `LRItems.cpp`
et de `LREnemy.cpp`.
