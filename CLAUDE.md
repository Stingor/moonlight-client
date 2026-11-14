# moonlight-client — règles du dépôt

Client de jeu **autonome** pour le serveur Moonlight-Destiny, écrit de zéro en
C++23. Le `README.md` dit comment le construire et l'installer ; ce fichier dit
comment y écrire du code.

## Ce dépôt est une copie publiée

Le développement a lieu ailleurs : chaque commit de ce dépôt rejoue un commit du
dépôt de travail, et son message porte la ligne `moonclient: <révision>` qui le
désigne. Une modification faite ici ne sera donc pas fusionnée telle quelle ;
elle sera lue, et reprise là-bas si elle est bonne. Les scripts, les tests et la
documentation du dépôt de travail n'y figurent pas.

## Langue

Commentaires, documentation et messages de commit en **français**.
Identifiants, noms de fichiers et de cibles en **anglais**.

## L'architecture est imposée par l'éditeur de liens

Une bibliothèque statique par couche, et le graphe est un DAG :

```
core     ← ne connaît RIEN
formats  ← core
vfs      ← core, formats
mapedit  ← core, formats
proto    ← core
audio    ← core
sim      ← core, formats
net      ← core, proto, sim
voice    ← core, net
render   ← core, formats, vfs, mapedit
ui       ← core, render
app      ← tout
```

Une dépendance illégale est une **erreur de lien**, pas une remarque en revue.
Un lien PUBLIC est transitif : une couche n'inclut quand même que ses
dépendances **directes**, celles que déclare son `CMakeLists.txt`.

Deux frontières à ne jamais franchir :

- **`render` ne dépend pas de `sim`.** `app` construit la scène, des données
  pures, et la passe au renderer. C'est ce qui rend le rendu testable hors jeu.
- **`ui` ne possède aucun état de jeu.** Elle reçoit une vue en lecture seule et
  émet des *intentions* qu'`app` route. Une fenêtre qui ne peut pas muter l'état
  ne peut pas devenir un panneau de 3 000 lignes qui tripote l'état d'autrui.

**Pas de singleton.** Le contexte se passe par référence au constructeur : un
module reste instanciable seul, donc testable.

## Construire

```bash
cmake --preset windows
cmake --build --preset windows-release-client
```

Sous Linux, `cmake --preset linux-gcc` avec g++ 14 : le détail, les paquets
système et les options sont dans le `README.md`. L'intégration emploie g++ 14, et
pas clang avant sa version 19 : `std::expected`, dont `ro/core/error.h` dépend,
n'existe pas dans la libstdc++ sous un clang plus ancien.

Presets : `windows`, `windows-asan`, `linux-gcc`, `linux-gcc-asan`,
`linux-clang`, `linux-clang-asan`. Les dépendances passent par **vcpkg en mode
manifeste** (`vcpkg.json`), jamais par un submodule.

Les sources sont listées **explicitement** dans les `CMakeLists.txt`, sans glob :
un fichier ajouté sans être déclaré doit provoquer une erreur, pas disparaître.

Le format suit le `.clang-format` du dépôt. Le code est compilé avec les
avertissements en erreur.

## Règles d'écriture

- **Le nom porte contenu + unité + origine** : `elapsed_ms` et non `t`,
  `item_id` et non `id`. Le nom est le commentaire qui ne périme jamais.
- **Zéro constante magique.** Un nombre qui n'est ni un offset documenté ni une
  unité évidente est une constante nommée, ou il disparaît.
- **Ne pas hacher pour comparer.** Comparer des champs ne collisionne jamais.
- **Un commentaire dit la RÈGLE en vigueur, pas l'HISTOIRE du chantier.** Le
  récit va dans le message de commit. Test : « ce commentaire aide-t-il
  quelqu'un qui n'a jamais vu l'ancien code ? »
- **Jamais de `memset` sur un agrégat C++** : `x = T();`. Un champ dont le
  défaut est `-1` ne survit pas à un `memset`.
- **Jamais de `//` terminé par `\`** : la ligne suivante disparaît du programme.
- **Erreurs par `Result<T>`, pas par exception.** Les parseurs lisent des octets
  que l'on ne contrôle pas ; l'échec y est un cas normal.
- Plafond de **800 lignes** par fichier. Au-delà, on découpe — sauf pour un
  **catalogue**, une liste d'entrées indépendantes où l'on cherche sans jamais
  lire de bout en bout.

## Données

Aucune donnée du jeu (archives, sprites, cartes, musiques) n'est dans ce dépôt
ni ne doit y entrer : le client lit l'installation que le joueur possède déjà.

Les tables de `dist/data/yaml_files/` sont des conversions, et chacune porte en
tête sa mention d'origine. Une table tirée d'un tiers ne se revendique pas comme
nôtre. Les textes de l'interface sont dans `lang/` ; toute chaîne montrée au
joueur y a son entrée en anglais et en espagnol.

Aucun code tiers n'entre sans notice. Le code de ce dépôt est sous licence MIT
(voir `LICENSE`).
