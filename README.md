# moonlight-client

Client de jeu **autonome** pour le serveur [Moonlight-Destiny](https://moonlight-destiny.fr),
écrit de zéro en C++23.

*A standalone game client for the Moonlight-Destiny server, written from scratch
in C++23. Documentation is in French.*

## Ce que c'est

Un programme indépendant qui lit les données d'une installation existante du
jeu et parle le protocole du serveur. Il ne s'injecte dans aucun autre
exécutable et n'en embarque aucun.

On s'y connecte, on y entre en jeu, on marche, combat, parle ; les
cartes s'affichent entières (eau, ciel, modèles animés, effets), les
compétences portent leurs effets et leurs sons, et les fenêtres du jeu sont là.

## Ce que ce dépôt ne contient pas

**Aucune donnée du jeu.** Ni archive GRF, ni sprite, ni carte, ni musique, ni
texte du jeu. Le client lit l'installation que le joueur possède déjà ; sans
elle, il n'affiche rien.

Ce dépôt est une **copie publiée** des sources du client. Le développement a
lieu ailleurs, et chaque mise à jour arrive ici en un seul commit qui nomme sa
révision d'origine. Les signalements sont bienvenus dans les *issues* ; une
*pull request* ne peut pas être fusionnée telle quelle, mais elle sera lue.

## Construire

Les dépendances (SDL3, ImGui, FreeType, spdlog, yaml-cpp…) s'installent seules
par [vcpkg](https://vcpkg.io) au premier `cmake --preset`.

### Windows

Prérequis : Visual Studio 2026 ou 2022 avec les outils C++. CMake, Ninja et
vcpkg sont livrés avec Visual Studio.

```bash
cmake --preset windows
cmake --build --preset windows-release-client
```

L'exécutable sort sous `build/windows/app/Release/Moonlight-Destiny.exe`.

### Linux

Prérequis : g++ 14, CMake, Ninja et un clone de vcpkg
désigné par `VCPKG_ROOT`. Le réseau et le coffre du mot de passe passent par
les bibliothèques du système (libcurl, OpenSSL, libsecret), et SDL3 demande les
en-têtes de la plateforme. Sur Ubuntu :

```bash
sudo apt-get install -y g++-14 cmake ninja-build pkg-config python3-venv autoconf autoconf-archive automake libtool libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev libwayland-dev libxkbcommon-dev libdecor-0-dev libegl1-mesa-dev libgl1-mesa-dev libibus-1.0-dev libdbus-1-dev libudev-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libcurl4-openssl-dev libssl-dev libsecret-1-dev
```

```bash
cmake --preset linux-gcc -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-linux-release -DVCPKG_HOST_TRIPLET=x64-linux-release
cmake --build build/linux-gcc --target moonclient
```

L'exécutable sort sous `build/linux-gcc/app/Moonlight-Destiny`. Le triplet
`x64-linux-release` ne construit que la Release des dépendances, deux fois plus
vite que le triplet par défaut.

« Se souvenir du mot de passe » demande un trousseau (GNOME Keyring, KWallet) :
sans lui, le client ne retient rien plutôt que d'écrire le mot de passe en
clair.

## Installer

Dans le dossier d'une installation du jeu, à côté des archives :

| À poser | Depuis |
|---|---|
| `Moonlight-Destiny.exe` (`Moonlight-Destiny` sous Linux) | la construction |
| `moonclient.yml` | `dist/moonclient.yml` — le serveur auquel se connecter |
| `lang/en.txt`, `lang/es.txt` | `lang/` — l'interface en anglais et en espagnol |
| `data/yaml_files/*.yml` | `dist/data/yaml_files/` — si l'installation ne les a pas déjà |

## Organisation

Une bibliothèque statique par couche, et le graphe des dépendances est imposé
par l'éditeur de liens : une couche n'inclut que celles qu'elle déclare.

```
core     ← ne connaît rien
formats  ← core                 lecture des formats de fichiers
vfs      ← core, formats        système de fichiers virtuel sur les archives
proto    ← core                 paquets du protocole
audio    ← core
sim      ← core, formats        état du monde, sans fenêtre
net      ← core, proto, sim
render   ← core, formats, vfs   rendu OpenGL, sans état de jeu
ui       ← core, render         fenêtres ImGui, en lecture seule
app      ← tout
```

## Licence

Le code est sous licence MIT (voir `LICENSE`).

Les tables de `dist/data/yaml_files/` sont des conversions : chacune dit en
tête d'où elle vient et comment la régénérer.

- `quests.yml` et `recommended_quests.yml` sont dérivés des traductions
  [ROenglishRE](https://github.com/llchrisll/ROenglishRE), dont la licence en
  permet la distribution et la modification ; ils ne sont pas revendiqués comme
  partie de ce projet.
- `item_properties.yml`, `monster_list.yml` et `pets.yml` sont exportés des
  bases d'objets, de monstres et de familiers du serveur Moonlight.
- `item_groups.yml` est exporté des bases d'objets, de groupes d'objets et
  d'invocations du serveur Moonlight.
- `achievements.yml` est exporté de la base des succès du serveur Moonlight
  (et de ses bases d'objets, de monstres et de cartes). Les textes des succès
  restent lus dans le `achievements.lub` de l'installation du joueur ; ils ne
  sont pas versés au dépôt.
- `skill_trees.yml` est exporté des arbres de compétences des métiers et de la
  guilde du serveur Moonlight (et de ses bases de compétences et de métiers).
  La disposition de la fenêtre — cases, onglets — reste lue dans les tables de
  l'installation du joueur ; elle n'est pas versée au dépôt.
- `party_booking.yml` est tiré du fichier `party_booking.lub` que livre le
  client ; sa traduction n'est pas revendiquée comme partie de ce projet.

Ragnarok Online est une marque de Gravity Co., Ltd. Ce projet n'est ni
affilié à Gravity ni approuvé par elle.
